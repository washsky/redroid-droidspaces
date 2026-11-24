# 第二部分验收摘要（脱敏）

本目录是根据 2026-11-24/25 实际工具输出和 11-25 发布前只读复核整理的**摘要**，不是新生成的一套原始连续日志，也不是新实例从零重放报告。不补造已删除的视频/截图，不上传含账号、授权、license 或完整安装环境的日志。

本页保留原arm发布阶段的范围。之后用户另行授权的repro增量复现见[独立报告](../../docs/part2/REPRODUCTION_REPRO_20261125.md)及[独立摘要](../2026-11-25-repro-part2/README.md)；下文“repro冻结”只描述前一阶段。

## 已实际执行

| 项目 | 观察 |
|---|---|
| 目标 | arm；repro 在用户要求后冻结，后续未连接或读取 |
| 基础模块 | 早期目录/CLI与UI安装/Action/脚本/属性/覆盖/禁用卸载，详见 ../2026-11-24-magisk-modules |
| 最终框架 | Magisk31.1，Next1.5.3(880-22a1bf5)，LSPosed2.2.1(7912) |
| 回调 | 临时探针的 system_server pre/post；lab.arm13.probe、lab.arm13.gpuprobe pre/post；探针后移除 |
| LSPosed | daemonRunning=true、systemServerRequested=true、safeMode=false、apiVersion102、installedModules0 |
| 终端 | 真实 Droidspaces enter root PTY；modules_ready=1、terminal_ready=1 |
| 权限 | init CapEff/CapBnd=000001deffecffff，Seccomp2/filter1，UID仍0→0全范围 |
| 显示 | 实际640×1136/DPI280/50Hz冷启动；恢复720×1280/DPI320/60Hz；UI保存50Hz待生效再恢复60Hz |
| 配置防护 | 奇数宽721拒绝BAD_DISPLAY_CONFIG；旧revision拒绝STALE_CONFIG；原配置不变 |
| 网络 | UID10078 + inet3003经broker，真实HTTPS200/TLS_AES_128_GCM_SHA256 |
| UDP | 同一socket同时查询1.1.1.1与8.8.8.8，源地址/端口53/事务ID相符；各61字节/2条answer（本次响应） |
| DNS | SOCKS5与HTTP两协议，经TCP上游DNS；同时向私网示例地址和公网DNS发UDP查询，正确回包 |
| 故障 | 停relay保留UDP规则，查询超时；恢复服务后成功，无主动直连fallback |
| 认证 | 真实C代码的SOCKS5/HTTP模拟服务器成功和拒绝；无真实认证账号 |
| TCP传输 | 分片握手/拒绝、双向各2MiB、背压、半关闭 |
| UI | v1.2独立应用页实际滚动；列表671px/屏1280px；代理与显示字段实际保存 |
| 冷启动 | arm独立停止/卸载/启动；Next/LSPosed、broker、代理与显示恢复 |
| stock scrcpy | 5.0.1 stock server；QTI c2.qti.avc.encoder；VAAPI解码；720×1280 |
| 像素 | 中央60% MAE1.343；原图RGB std[44.15,49.75,0.0]，解码[44.16,49.76,1.2] |
| 保护 | 真机bootID未变、Enforcing；arch2 PID/starttime保持；未改全局ADB/SELinux/BPF/防火墙 |

## 失败及修正也保留

- root-only Magisk 不足以支持模块；不以手工目录伪装支持。
- 内置 Zygisk 和 Next 的身份检查不能按原生 init/zygote 域假设。
- 用户态身份通过后仍需实际验证内核 socket SID 的 SCM_RIGHTS 通道。
- /dev noexec 同时影响安装helper和Next早期载荷；不通过全局remount解决。
- 静态 FD wrapper 干扰 Next 的早期 app_process 路径。
- UDP REDIRECT 的 IP_ORIGDSTADDR 在本内核返回改写目标，最终使用 TPROXY。
- 首版 TPROXY恢复规则缺少显式 -p udp，规则恢复被拒；修正后真实多目标与最终冷启动通过。
- 普通UID CLI探针只切UID、缺少inet supplementary组时，broker Unix socket连接EACCES；模拟真实应用组后通过，没有放宽broker。
- 画质检查初版要求每个通道都有高方差，误判原图本来恒定的蓝通道；改为同源逐通道比较，未修改视频输出来“通过”。

## 结束状态

代理revision11：SOCKS5 TCP开启，UDP/DNS/认证关闭，active=true；UID2000 relay有效能力归零，自有UDP链/规则/专用路由清理。broker既有选择保留，revision14/activeRevision14是本机快照。显示720×1280/DPI320/60Hz，pending=false。

发布前只读复核仍见上述框架和配置状态。PID/UID/revision都必须后续重新核实；不把它们当可复制配置。清单和哈希见 [supplement-20261125.json](../../artifacts/supplement-20261125.json)。

未做：x86复测、全新空data部署本补充组合、空源码AOSP全编译、真实认证账号、任意Xposed业务hook、全部视频格式/音频、游戏登录和自动流程、一小时耐久。没有查询冻结repro来扩大验收。
