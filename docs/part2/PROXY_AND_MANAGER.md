# UID 管理器 v1.2：统一代理、UDP、DNS 和认证

所属：[第二部分补充](../PART2_SUPPLEMENT.md)。基础 UID 缺口及 broker 见[第一部分网络说明](../NETWORK.md)。

## 1. 两层功能分开理解

应用名单控制的是 UID 联网兼容：被选中的实际应用 UID 经 broker 取得 UID2000 创建的 TCP/UDP socket，应用继续执行 connect、DNS、TLS。

统一代理控制的是**整个 arm 实例的 IPv4 出口**，不提供逐应用代理。没有把“勾选应用”改成代理选择器，也没有修复原生 UID 与宿主 BPF 的隔离问题。

| 前端入口 | 行为 |
|---|---|
| 应用名单与联网兼容 | 独立页面，搜索、系统/仅选择过滤、共享 UID、多包保存 |
| 统一代理出口 | TCP 总开关、SOCKS5/HTTP、服务器、端口、认证、可选 UDP/DNS |
| 分辨率、DPI 与刷新率 | 保存下次启动参数，见[显示说明](SCRCPY_TERMINAL_DISPLAY.md) |

独立应用页的实际列表高度为 671/1280px，已验证滚动；首页概览可整体滚动。保留既有包名配置和 root 授权，不需要重选应用。

## 2. 支持矩阵

| 功能 | SOCKS5 | HTTP CONNECT |
|---|---|---|
| IPv4 公网 TCP | 支持 | 支持 |
| 公网 UDP | 可选，需 UDP ASSOCIATE | 不支持，前端关闭该选项 |
| UDP53 → 经代理 DNS over TCP | 可选 | 可选 |
| 认证 | RFC1929 用户名/密码 | HTTP Basic |
| IPv6 代理 | 未实现 | 未实现 |
| 逐应用出口 | 未实现 | 未实现 |

一般局域网、回环、保留地址及代理服务器地址绕过统一转发，保持管理连接可达。**DNS 开关优先于私网绕过**：开启后，即使原查询发往局域网 DNS，其 UDP53 也送往用户配置的上游 IPv4:53，经代理使用 TCP。

DNS 开关不是 DoH，也不是自动加密 DNS；应用自带 DoH/DoT 不受该开关改写，但其公网 IPv4 TCP 仍可能走统一 TCP 代理。TCP53 自身也按一般 TCP 规则处理。不要把这个开关写成“所有 DNS 协议都被接管”。

## 3. 后端与权限

| 文件 | 用途 |
|---|---|
| /vendor/bin/arm13-proxyctl | root 管理；status/save/boot/test |
| /vendor/bin/arm13-proxy-relay | UID2000 的 TCP/UDP/DNS 转发 |
| /vendor/etc/init/arm13-proxy.rc | init 生命周期、运行 UID 与能力 |
| /data/misc/redroid-netbroker/proxy | R13PROXY2 持久配置 |
| /dev/arm13-proxy/stats | revision、PID 和聚合计数；不保存认证内容 |

管理器的固定命令经 su 调用，输入走 stdin，不拼接密码到 shell 命令行。移除控制进程继承的 LD_PRELOAD，避免 vendor namespace 拒绝 system broker 库。

配置 root:shell 0640，relay 可以读，普通应用 UID 不可以。密码不在 status 中返回；界面只知道 hasPassword，留空可保留原密码；关闭认证清除已存认证字段。十六进制是序列化格式，**不是密码加密**。用户名/密码各最多255字节，不允许控制字符；HTTP Basic 用户名不允许冒号。

本轮未提供真实认证账号，认证的成功、错误凭据拒绝和分片响应在本地模拟服务器验证；实际 LAN 服务器测试的是无认证模式。不要将其写成真实账号已验收。

## 4. TCP：透明重定向与握手

只在已核实 arm netns 中操作自有 NAT 链 ARM13_PROXY 及精确 OUTPUT 跳转。relay 在回环10809取得 SO_ORIGINAL_DST，建立 SOCKS5 CONNECT 或 HTTP CONNECT，双向转发，保留应用 TLS。

实现包含握手超时、非阻塞连接、128会话上限、背压、半关闭、空闲超时和计数。真实代码测试覆盖分片握手、拒绝响应及双向各2MiB传输；不是只看端口能建立连接。

安装规则在启动 relay 之前完成，避免 relay 缺失时主动回退原路径。规则已生效后服务故障，受管路径连接失败；这不等于对启动配置应用失败、任意 netd 重写或其他协议有全局防泄漏保证。

## 5. UDP：为什么没有沿用 TCP 的 REDIRECT

初版尝试 UDP REDIRECT + IP_ORIGDSTADDR，但本内核 recvmsg 中得到的是改写后的 127.0.0.1:10809，无法恢复公网原目标。规则计数增长和接收到数据包不代表转发成功。

最终改用 TPROXY，保留原目标；按 client/目标分离 UDP 会话，SOCKS5 UDP ASSOCIATE 保持控制 TCP，回包核对地址后通过透明 socket 绑定原目标地址返回。保留 SOCKS5 FRAG=0；未实现 SOCKS UDP 分片重组。当前仅接受 SOCKS 回复的 IPv4 relay 地址为配置的服务器地址（0.0.0.0 归一化到它），其他格式/不同 relay IP 会拒绝。

专用资源仅存在于 arm 的网络 namespace：

| 资源 | 本实验值 |
|---|---|
| mangle 链 | ARM13_UDP_OUT、ARM13_UDP_IN |
| 保留标记位 | 0x40000000/0x40000000，保留其他 netd 标记位 |
| 专用路由表 | 41136，local default dev lo |
| 精确策略规则 | pref9001，匹配该标记位 |
| 监听 | 回环10809 UDP |

控制器在改变任何规则前验证：自己与 PID1 的 netns 一致、root-owned init helper、设备为 redroid_arm64_only、eth0 为本实验 arm 地址。不会全局清空规则、操作宿主 BPF 或覆盖 netd 的原路由表。

TPROXY 的透明回包需要能力。init 只给 UID2000 relay 配 NET_RAW 和 NET_BIND_SERVICE；不授予 NET_ADMIN/root。关闭 UDP 时进程主动清空 permitted/effective 能力，最终实测 CapEff=0、NoNewPrivs=1。规则仍由受目标校验保护的 root 控制器管理。

同一个普通 UID 的 UDP socket 同时访问1.1.1.1:53和8.8.8.8:53，收到分别对应正确源地址、事务ID的应答；不是只测一次单目标 DNS。实际服务器先通过独立 UDP ASSOCIATE 探针确认支持，再验收透明路径。

## 6. DNS：固定上游使 HTTP 也能使用

DNS 开启时 UDP53 重定向回环10810；读取 query，通过所选 TCP 代理连接用户配置的 DNS IPv4:53，加两字节 DNS TCP 长度前缀，读取应答，再交给原 UDP 客户端。此固定上游路径不依赖 REDIRECT 恢复原目标，因此可用；conntrack 负责恢复应答源。

实际同时向一个私网示例地址和公网 DNS 地址查询，收到原目标地址对应的回复；SOCKS5 与 HTTP CONNECT 两种模式都通过。关闭普通 UDP、单独开启 DNS 时 relay 有效能力仍为0。

上游必须接受 DNS TCP53，代理也必须允许 CONNECT 到该端口；“HTTP 代理能连网页443”不保证53允许。前端“测试已保存 TCP”仅验证到测试地址443的代理握手，不能替代 DNS 和 UDP 检查。

## 7. 配置版本与 CLI

旧 R13PROXY1 自动按无认证、无 UDP、无 DNS 读取；首次保存写 R13PROXY2。save 使用 expectedRevision、flock、严格解析、原子替换/fsync；应用失败尝试恢复旧配置/服务，并返回错误。

save 的 stdin 顺序为：

```text
expectedRevision
enabled
protocol
hostIPv4
port
udp
dns
dnsHostIPv4
auth
usernameHexOrDash
passwordHexOrDash
```

开关为0/1，protocol 为 socks5/http，空字符串编码为单个 -。已存密码保留标记是 __KEEP_STORED_PASSWORD__ 的十六进制编码；不能把未编码文本放进该字段。优先使用前端，不用示例覆盖真实认证或配置。

```bash
adb -s "$INNER_ADB" shell 'su -c "/vendor/bin/arm13-proxyctl status"'
adb -s "$INNER_ADB" shell 'su -c "/vendor/bin/arm13-proxyctl test"'
```

status 中 connections 是聚合转发计数，UDP/DNS 应答也累计；不能解释为唯一 TCP 连接数。failures 同样是聚合错误计数。

## 8. 实际验收和结束状态

通过：SOCKS/HTTP公网 HTTPS200/204，默认 TLS；普通 UID 经 broker 的 UDP 双目标；两协议 DNS over TCP；关闭/保存/启用；无效配置拒绝；容器冷启动恢复。

故障测试：保持 UDP 捕获规则、停止 relay 后查询失败，恢复后再次收到应答，未自动直连。测试完成恢复服务。

最后通过实际 APK 关闭并保存 UDP/DNS，配置 revision11：TCP SOCKS5开启、UDP=false、DNS=false、认证=false。移除自有 UDP 链、规则和路由后，原 netd 规则保持；relay UID2000、CapEff=0。revision/UID是现场快照，不应写死在后续配置。

统一代理关闭应通过管理器/控制器完成，仅清理自身精确规则；不要手工 flush 全表。普通应用的 broker 包名名单未被代理测试覆盖或重置。游戏 UDP 会话、登录后业务和长时稳定性没有验收。
