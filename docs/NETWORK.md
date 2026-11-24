# 应用 UID 网络缺口与管理器

> **第一部分基础联网兼容。** 本页的broker仍必需。统一TCP/UDP/DNS出口、认证、管理器v1.2与独立应用页属于[第二部分](part2/PROXY_AND_MANAGER.md)，不等于原生UID直连修复。

## 定位结论

最初 shell UID2000 可以连接公网443，普通应用 UID 超时。抓包在应用连接时看不到对应流量到 ds-br0；不是所有 TCP、DNS、IPv4-mapped IPv6 或 NAT 都坏了。

后续只读 BPF 查询确认：

- 宿主根 cgroup 直接附加 ingress/egress/socket_create/connect 等程序，目标容器和 uid_* 子 cgroup 继承有效程序。
- 当时直接 attach flags=0，不能假定在子 cgroup 加程序就能覆盖父程序。
- 同一个数值 UID 在内层和宿主代表不同应用。例如内层游戏 UID10080 在宿主对应 sharedstoragebackup，宿主 permission map 值0；socket_create 路径可以直接 EPERM。
- UID10078 等还遇到宿主流量策略。配置值随宿主策略变化，不能把早期 0x1200/后期0x1208当常量，也不能套 Android13 规则位到 API35。
- program ID 和 map ID 属于不同编号空间。bpf-inspect.c 只用于读取，不 detach、修改 map 或禁用宿主过滤。

因此桥接只能改变后面的包路径，不能消除 socket_create 的 UID 身份冲突。这是 Android-in-Android 在共享宿主内核/cgroup BPF 下的隔离缺口，当前没有在 Droidspaces 根治。

## 当前兼容架构

选中应用的 libc socket() → 受控 Unix SOCK_SEQPACKET → UID2000 broker 创建真实 TCP/UDP socket → SCM_RIGHTS 返回 fd → 应用自己 connect / DNS / TLS。

- 不是 HTTP 代理，不依赖应用遵守全局 http_proxy，不把 TLS 转发到 Debian 解密。
- 默认服务器证书校验、真实响应码和应用 UID 本身不变。
- **socket 所有权/宿主计量身份变为 UID2000**，每应用原始网络 UID 隔离不再等价。
- 只支持 AF_INET/AF_INET6 的 TCP/UDP socket 创建；raw socket、直接绕过 libc 的 syscall 等不在拦截范围。
- 服务不是 root：UID2000，group inet/package_info，NoNewPrivs；按 SO_PEERCRED 校验调用者。
- 不允许用它作为开放的任意 UID socket 服务。

## 前端与持久化

前端是原生 Android APK“UID 网络管理”，不是浏览器 Web 管理页。可同时选择多个包，并非一次只能代理一个 UID。

持久配置：
```text
/data/misc/redroid-netbroker/packages
R13NET1
1
12
lab.arm13.probe
mark.via
com.mmbox.xbrowser
```

上面 revision 仅为格式示例。首次安装可从 revision=1 开始。不要覆盖正在使用的管理器配置。

服务监听 packages.list 和配置文件变化，把已选择、已安装、带 INTERNET 对应 inet GID 的包解析为当前 UID。重装导致 UID 改变时自动重算；共享 UID 按 UID 合并，仍需理解共享身份的边界。当前实现最多512个包。

运行时名单 /dev/arm13-netbroker/uids 由服务生成，不手工编辑。init 在 post-fs-data 调用 netctl boot；正常容器重启无需每次重新勾选。

前端先保存草稿，提交时包含 expected revision；netctl 使用 flock、校验、原子替换，旧 revision 返回 STALE_CONFIG，避免同时操作互相覆盖。root 控制器 chmod0700；管理器通过 su 授权运行控制器。

## 安装顺序（仅目标容器）

1. 构建三个 native 文件、APK，记录哈希。
2. 备份原 Zygote rc、将被修改的服务和配置；保留完整镜像。
3. 将 arm13-netbroker / arm13-netctl 安装到 /vendor/bin，root:root 0755 / 0700。
4. 将 libarm13_socketbroker.so 安装到 /system/lib64。
5. 按 config/init/redroid-netbroker.rc 配置服务，创建合法初始 packages 文件。
6. 在**现有 zygote service** 中合并 LD_PRELOAD；保留其他 preload。原 FD wrapper只适用于历史分支；Next1.5.3使用init.zygote64-next.rc直接app_process，见第二部分，不能叠用。不要覆盖整份设备特定rc。
7. 冷启动目标容器，安装 APK并仅授权需要 root 的管理器。
8. 查询状态、勾选测试包、保存，观察 revision/activeRevision 一致。

```bash
adb -s "$INNER_ADB" shell 'su -c "/vendor/bin/arm13-netctl status"'
# 独立重启服务，用于检查已有选择能否恢复
adb -s "$INNER_ADB" shell 'su -c "/vendor/bin/arm13-netctl restart"'
```

控制器 save 的 stdin 顺序为 expectedRevision、enabled(0/1)、每行一个包名。应先读取当前状态，保留用户现有选择再合并，不能用示例包列表覆盖。

## 验收与回退

真实 HTTPS200 / TLS_AES_128_GCM_SHA256、IPv4和映射 IPv4、局域网 UDP nonce、游戏到登录入口，以及 broker 重启和容器冷启动均曾通过。公网 UDP 游戏会话、游戏登录后业务未验收。

Android VALIDATED 使用真实可达的 HTTP/HTTPS204 探测服务；实验曾采用 connect.rom.miui.com/generate_204。Google 探测站线路超时不是伪造验证的理由。

回退时恢复**与该阶段匹配**的原 Zygote rc，移除新增 preload、停用 broker；先备份当前包名选择。仅重启目标容器。旧“固定 UID 属性白名单”已被管理器包名配置替代，不重新启用旧方案。

## 2026-11-24 独立部署发现的前端修复

管理器创建 su 控制进程前从 ProcessBuilder 环境移除继承的 LD_PRELOAD，避免 vendor 命名空间拒绝 system socket broker 库。应用本身仍使用 broker。公开源码重建 APK 在新容器验证了多包保存、服务重启和冷启动恢复，见 [复现报告](REPRODUCTION_20261124.md)。
