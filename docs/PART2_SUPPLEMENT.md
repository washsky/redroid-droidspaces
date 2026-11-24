# 第二部分：功能补充与兼容性探索

整理日期：2026-11-25（Asia/Shanghai）。本部分记录 2026-11-24 至 11-25 的增量探索。

## 与第一部分的关系

**项目核心仍是让 Droidspaces 成功运行 ARM64 ReDroid，以及必要的基础环境配置。**
先读 [第一部分核心过程](FULL_PROCESS.md)、[复用产物部署](PREBUILT_REPRODUCTION.md)和[独立部署验收](REPRODUCTION_20261124.md)。启动、DHCP、属性区、普通 APK、基础 UID 联网兼容、GPU/Gralloc 等基础依赖仍以这些文档为入口。

本部分是在成功基线上追加的能力，**不应作为第一次启动 Android 的前置条件**。完整 Magisk 模块、Zygisk/Xposed 框架、免定制 server 的投屏、显示参数前端和统一代理，都属于可选补充。基础过程中的旧配置、旧投屏方案和历史验收保留其时间范围；不能把本部分的新结论倒写成早期就已实现。

只重放第一部分时，可检出已验证核心提交[fd3892d](https://github.com/washsky/redroid-droidspaces/tree/fd3892d981dfe989a74a11a5523ff1d7a21d4fc4)。当前main收录两部分的源码，核心container.example.conf与第二部分container.arm13-part2.example.conf分别保留，不能无条件混配旧脚本与新helper。

## 阅读顺序

1. [Magisk 完整化、内置 Zygisk、Zygisk Next 1.5.3 与 LSPosed 2.2.1](part2/MAGISK_ZYGISK_LSPOSED.md)：本轮重点，包含失败层次、修复、安装顺序、x86 排障参考。
2. [原版 scrcpy、Droidspaces 终端和显示启动参数](part2/SCRCPY_TERMINAL_DISPLAY.md)。
3. [UID 管理器 v1.2 与统一 TCP/UDP/DNS 代理](part2/PROXY_AND_MANAGER.md)。
4. [部署清单、验证和回退](part2/REPRODUCTION_AND_VALIDATION.md)。
5. [脱敏验收摘要](../evidence/2026-11-25-supplement/README.md)及[补充产物清单](../artifacts/supplement-20261125.json)。

早期 [Magisk 基础模块验收](MAGISK_MODULES_20261124.md)也是本部分的前置实验记录；它与后续外部 Zygisk 成功状态分开保存。

## 实例边界：不要混用两台容器的状态

| 对象 | 已有验收范围 | 本部分结束状态 |
|---|---|---|
| redroid13arm | 原始成功环境；后经用户授权承接 Magisk、Next/LSPosed、scrcpy、终端、显示和代理改进 | 本部分实际目标；允许的冷启动只针对它 |
| redroid13repro | 复用编译产物、清空新镜像应用数据后的独立部署；早期基础模块验收 | **冻结：未经用户后续指令，不连接、读取、修改或重启**；未同步本部分后续改进 |
| 真机与 arch2 | 保护对象 | 不重启真机，不停止或重启 arch2；不改宿主全局 SELinux/BPF/防火墙 |

文中的 PID、UID、netns inode、窗口句柄、端口连接状态都不是长期配置；实际操作前重新核实。历史记录中的“原容器未部署/未重启”只描述当时，后续用户已明确允许在 arm 实验。

## 本部分实际成果与限制

| 项目 | 已验证 | 不能外推 |
|---|---|---|
| Magisk 31.1 | 基础模块目录、官方模块阶段、脚本/属性、启动覆盖、管理器、禁用卸载 | 任意模块、宿主 boot 修补、通用 sepolicy.rule |
| 内置 Zygisk | 受限身份兼容及真实探针回调；为诊断中间阶段 | 与 Next 同时开启；所有 Magisk 版本 |
| Zygisk Next | 1.5.3（880-22a1bf5），真实 system_server/应用 pre/post 回调 | 旧版全部已修复、x86 已修复、32 位 ABI 已验收 |
| LSPosed | 用户提供的 v2.2.1（7912）包；daemonRunning、systemServerRequested=true，API102 | 第三方 Xposed 模块业务/隐藏功能；发行来源认证 |
| 原版 scrcpy | 5.0.1 stock server，默认编码参数选择 QTI；Debian VAAPI，720×1280 颜色比对 | 所有 scrcpy 版本、ROM、编解码格式、音频、零拷贝 |
| 终端/显示 | 登录 su 参数兼容、Magisk 私有 PTY 修复；init 读取显示参数，前端保存 | 任意 Linux su 参数、任意刷新率都能达到硬件吞吐 |
| 权限收紧 | full 改为 nocaps + 自定义 init 删五项 capability，保留 mask/seccomp | UID 映射的真正非特权容器、最小能力集 |
| 统一代理 | IPv4 TCP；SOCKS5 UDP；DNS over TCP；认证模拟正反例；冷启动恢复 | IPv6、逐应用代理、真实认证账号、所有 UDP 应用 |
| UID 管理器 | v1.2；独立应用页、显示参数和代理配置；实际滚动与保存 | 原生应用 UID 直连已经修复 |

结束时 SOCKS5 TCP 代理保持开启，新增 UDP、DNS、认证选项关闭；显示为 720×1280、DPI320、60Hz。普通应用联网仍依赖 UID broker。

本轮跳过一小时耐久，不代用户登录、选择游戏账号或运行自动游戏流程。最终 LSPosed 第三方模块数为 0；这是框架接入验收，不是业务 hook 验收。
