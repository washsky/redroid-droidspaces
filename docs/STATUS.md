# 状态与验收矩阵

## 2026-11-25：第二部分当前状态

第一部分仍是本项目核心；[第二部分](PART2_SUPPLEMENT.md)是成功基线上的可选补充。当前实例按以下范围解释，下文早期阶段记录不改写。

| 对象/能力 | 当前验收 |
|---|---|
| arm | Magisk31.1 + Next1.5.3(880) + LSPosed2.2.1(7912)/API102；内置Zygisk关闭；真实回调与system_server接入 |
| stock scrcpy | 5.0.1默认编码参数，QTI硬编/VAAPI硬解，720×1280像素MAE1.343 |
| 终端/显示 | 交互PTY；init参数与v1.2前端，当前720×1280/DPI320/60Hz |
| 权限 | nocaps+删五项，36项能力，无root UID映射；mask/seccomp保留 |
| 代理 | IPv4 TCP；可选SOCKS5 UDP、DNS over TCP、认证；结束时UDP/DNS/认证关闭 |
| 普通应用网络 | 仍依赖UID broker，不是原生UID直连修复 |
| repro | 经后续授权完成第二部分增量复现；Next/LSPosed、PTY、两次冷启动、stock QTI/VAAPI、显示与代理；APK1.2.1、每容器运行时控制器配置 |
| 未验收 | x86复测、真实认证账号、第三方Xposed业务hook、游戏登录/自动流程、一小时耐久、空源码全编译 |

完整输入/二进制哈希：[补充清单](../artifacts/supplement-20261125.json)。总结取自实际验收，并在发布前对arm做只读复核；没有为写文档重新部署。

上段清单对应原arm发布阶段。后续仅repro的[实际复现报告](part2/REPRODUCTION_REPRO_20261125.md)和[独立清单](../artifacts/part2-repro-20261125.json)分开保存；arm、arch2、真机未重启或升级。

## 基准设备

- 型号：23013RK75C；代号 mondrian。
- SoC：**骁龙 8+ Gen 1 / SM8475**；GPU：**Adreno 730**。
- 宿主：Android API 35 / aarch64；SELinux 全局 Enforcing。
- 运行时：Droidspaces v7.2.0。
- 内层：Android 13 / API 33 / arm64-v8a，源码目标 redroid_arm64_only-userdebug。
- 工作站：Debian x86_64；编译机 Ubuntu 20.04 x86_64。
- scrcpy：5.0.1；NDK：r29；源码/API 编译工具按 BUILD.md 固定。

Mesa 曾显示“Adreno 725 (0x07030002)”，属于驱动设备名映射，不能据此把手机改称 7+ Gen 2。本机的 SoC/GPU 认定同时参考设备、SM8475 和原厂驱动信息。

## 分阶段结论

| 阶段 | 已实际观察 | 不代表 |
|---|---|---|
| 启动 | boot_completed=1，Launcher/Settings/普通 APK，DHCP 多次冷启动 | 原生手机安全隔离等价 |
| APK | 私有文件、SharedMemory 4096 字节、保护后读取 42 | 所有 Chromium shared-memory 分支都正确 |
| WebView | 157.0.8103.78，多进程启用，RELRO 1/1，真实页面 | 可随意换任意 WebView 版本 |
| 联网 | 默认 TLS 的 HTTP 200、TLS_AES_128_GCM_SHA256；真实 UDP nonce；曾获得 VALIDATED | 原始应用 UID 已能绕过宿主 BPF |
| 游戏 | com.tencent.tmgp.supercell.clashofclans 从更新错误恢复至微信/QQ登录入口 | 已接受协议、完成登录或验收对战 |
| GPU | Turnip/KGSL、ANGLE、HWUI skiavk，原生 EGL/Vulkan/AHB 与图形探针 | 只靠游戏 GPU 占用百分比就确认全链路 |
| Codec | QTI H.264 / H.265 YUV 编码、解码各 30 帧；像素对照 | 所有格式/分辨率/应用 Surface 都支持 |
| scrcpy | c2.qti.avc.encoder，真实画面颜色正确，鼠标控制成功 | 零拷贝或所有应用都自动使用这条兼容路径 |
| 浏览器 | Via / X 的 Canvas、WebGL 2.0、滚动、点击、文本输入、HTTPS | 已确认每个 WebGL shader 都走 Adreno 硬件 |
| 脚本 | root 辅助进程及虚拟显示恢复，配置页、无障碍绑定、正常停止 | 自动游戏脚本完整业务流程已验收 |

## 存储与时间

2026-11-23：当前候选镜像从 6 GiB 扩到 12 GiB。离线 e2fsck、resize2fs、再次 e2fsck 和 8 MiB 写入/读取校验通过。当时可用约 8.6 GiB。

2026-11-24 文档采集时：仍为 12 GiB 文件系统，df 显示约 2.4 GiB 可用。期间应用数据已变化；**8.6 GiB 是扩容验收当时的余量，不是长期承诺**。本次整理没有清理或修改这些数据。

## 稳定性边界

- 已做多次目标容器冷启动；Gralloc 修补后有两轮完整停止、卸载镜像、重新挂载启动验收。
- 修补后的图形测试期间 system_server / Zygote / netd 保持稳定；冷启动前后 PID 自然变化。
- 保护对照 arch2 在最后一轮验收仍是 PID 19643、starttime 135633；宿主 HTTPS 200、Wi-Fi ADB 正常。
- 用户明确跳过的一小时耐久测试未执行；不以短时结果代替长期稳定性结论。
- 网络 VALIDATED 是此前真实 204 验收结果，不能保证任何时间和任何线路都保持；发布日也没有为获取这个标记而改动网络设置。

## 2026-11-24 独立空数据部署验收

在相同真机新增 redroid13repro，复用编译产物重放仓库配置和补丁；完成首次部署、一次冷启动及 HTTPS、GPU、Gralloc、H264 Surface、浏览器和网络管理器验证。修复前端控制命令继承 LD_PRELOAD 的问题。原容器和 arch2 未重启。实际范围与保留问题见 [本轮复现报告](REPRODUCTION_20261124.md)。

## 2026-11-24 Magisk 基础模块补齐

仅redroid13repro追加模块初始化与启动阶段、受限挂载导出。自动目录、Modules页面、官方CLI/前端ZIP安装、Action、启动脚本、system.prop、普通UID及应用namespace中的文件可见性、冷启动禁用和卸载均通过；测试模块已移除。过程做了四次新目标冷启动及离线只读fsck，原容器和arch2保持。Zygisk、自定义SELinux规则仍未支持；详情见[模块验收](MAGISK_MODULES_20261124.md)。
