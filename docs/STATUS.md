# 状态与验收矩阵

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
