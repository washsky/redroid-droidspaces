# 实验时间线与排错经验

这是按故障因果整理的过程摘要，不是完整终端录屏。无法从保存文件证实的步骤不补写成“已成功”。

## 2026-11-18 至 11-21：先获得可启动基线

1. 先前 x86 实验已经成功；本轮只继续 ARM64 真机，不重复 x86 或虚拟化方案。
2. 从 AOSP/ReDroid ARM64 源码构建，部署独立候选镜像，保存原始镜像与压缩备份。
3. 处理 Droidspaces 中属性/标签兼容；保留 opt-in 条件，不能把全局 SELinux 关闭当作完成。
4. 普通 Zygote 因 /data/user 标签处理失败，实验中持久设置 app_data_isolation=false。此设置降低内层应用数据隔离。
5. 旧 ipconfigstore 在网卡尚无地址时写出损坏的 STATIC 配置，NetworkStack NPE 导致 system_server 循环崩溃。先停用错误生成路径并清理两处存储，验证 DHCP 冷启动。
6. 后续补齐 DHCP/空地址防御源码、增量编译部署，并确认 ro.boot.redroid_net_mode=dhcp。不能沿用“补丁尚未部署”的早期结论。
7. 普通测试 APK 安装、文件、SharedMemory、scrcpy 软件路径验收成功。

## WebView：两个独立问题，分别处理

- WebView 子 Zygote 强制进行应用数据挂载；ReDroid libselinux 返回的 HACKED 是合成字符串，不能拿它重新标记内核目录。
- 最终实现读取 security.selinux 原始 xattr，再用真实标签 relabel；不是把所有 EINVAL 当成功。当前 libandroid_runtime 与编译机产物 SHA256 一致。
- Chromium protection-flags 的 ashmem ioctl 和内核 memfd 能力不能混为一谈：内核 memfd 与 Java SharedMemory 曾验证正常，不代表现代 WebView 的所有 fd 识别分支都兼容。
- 该设备有真实内核 ashmem 节点，最后采用真实 ashmem + sys.use_memfd=false 的组合，多进程 WebView 恢复。没有制造未知设备号、也没有返回虚假 ioctl 成功。

## 2026-11-23：GPU / Codec / 网络 / Root

- 先用 arch2 的 Linux 图形文档研究 Turnip/KGSL 方向；Linux 桌面路径与 Android Gralloc/Surface/Codec 路径必须分别验收。
- 使用 lfdevs Mesa 27.0.0-devel 固定提交，增加基于真实 IMapper4 metadata 的桥接，处理 QTI plane 布局；并保留 fork 的 KGSL 支持。
- 普通 UID 联网按 socket 创建、connect、桥接和上游抓包分层。shell 能联网而游戏不能，不能继续只改 DNS/NAT。
- 宿主 cgroup BPF 有效程序被容器继承；某些内层 UID 与宿主无网络权限的 UID 冲突，socket_create 已可返回 EPERM。桥接不能绕过这一阶段。
- 不修改宿主 BPF，改用容器 UID2000 socket broker。后续把固定 UID 清单升级为包名配置和动态 UID 映射，并提供原生管理 APK。
- Bootless Magisk 只承担内层 root 授权。守护程序不加载宿主 SELinux policy、不运行模块/Zygisk阶段。
- QTI YUV 硬编解码已经成功，但最初 Surface 输出全绿。收到编码包不等于画面正确。
- 为 scrcpy 编码器输入 Surface 增加真实 usage 提示，使用线性布局；Codec 私有使用原厂 Vulkan，并补齐其动态 shader 编译器库。最后用解码像素和真实窗口确认。

## 2026-11-23：浏览器 / 脚本共同卡死闭环

1. 脚本独立 root 辅助进程可在 am force-stop 主应用后残留，SurfaceFlinger 等待其 BufferQueue，导致整个投屏和输入停住。
2. 抓到 RetainBuffer / AllocateBuffer 中的 PI mutex 等待；锁持有者已经在脚本或 Chromium 的 condition variable 中睡眠。
3. 搜索到同平台 Gralloc 格式探测卡死经验后，继续核对**本机实际二进制**，发现失败分支遗漏 unlock。
4. 原版对 format=0x22 / usage=0 的查询返回不支持，但留下锁；另一个线程正常分配会超时。修补版仍返回不支持，并能完成下一线程分配。
5. 补齐原解锁调用、保留原错误码，6,643 组格式/用途检查无漏锁；Via/X 的页面、WebGL、输入、HTTPS 恢复。
6. 脚本又暴露无障碍未启用的问题；启用并验证绑定后进入配置页。通过 scrcpy 的实际点击执行“停止/退出”，而非一直强杀。
7. 6 → 12 GiB 离线扩容和两轮完整冷启动验证通过。

## 容易误判的信号

- TCP poll=0 是超时；随后 SO_ERROR=0 不能推翻未连接事实。
- HTTP 403 仍可能说明 TCP/HTTP 链路通，但不等于业务成功。
- am start -W 的 ok 不能替代实际渲染。
- WebGL VERSION 字符串不能单独证明 GPU 后端。
- H.264 有码流不能替代解码后的颜色/像素检查。
- Java SharedMemory 成功不能替代 Chromium 的 fd/protection-flags 检查。
- “cgroup 有独立目录/map”不等于有效 BPF 程序已经隔离。
- 活跃 rootfs 含 /data，哈希随使用改变；不要拿构建原始哈希判定它损坏。

## 2026-11-24 独立空数据部署验收

在相同真机新增 redroid13repro，复用编译产物重放仓库配置和补丁；完成首次部署、一次冷启动及 HTTPS、GPU、Gralloc、H264 Surface、浏览器和网络管理器验证。修复前端控制命令继承 LD_PRELOAD 的问题。原容器和 arch2 未重启。实际范围与保留问题见 [本轮复现报告](REPRODUCTION_20261124.md)。

## 2026-11-24/25：第二部分补充探索

核心运行结果保持；下列能力按[第二部分](PART2_SUPPLEMENT.md)单独记录，不提升为首次启动前置条件。

1. 用户指出Magisk缺少模块目录/前端；确认root-only启动不足，补齐官方模块阶段与受限挂载导出，在repro完成早期基础模块生命周期验收。
2. 用户随后授权arm部署并仅冷启动arm；补齐内置Zygisk身份兼容和真实回调，不能由外部Next早期失败推断“外部Zygisk一律不支持”。
3. 安装LSPosed/Next暴露安装器临时执行目录与/dev noexec问题；保留真实设备及syscall错误，不制造设备或伪造成功。
4. Next1.5.3进一步涉及静态FD wrapper、init/zygote IPC身份、内核socket SID的SCM_RIGHTS、早期argv时序和载荷PROT_EXEC映射。逐层修正后，与LSPosed2.2.1获得真实回调和system_server接入。
5. Droidspaces交互终端修复su登录参数与Magisk私有PTY视图。原版scrcpy先用显式QTI linear选项，后修正容器内编码器默认值，最终不再依赖定制server。
6. 在arm保留mask/seccomp、full改为nocaps加自定义init删除五项能力；默认九项裁剪曾使Next失败，尚未逐项最小化或建立root UID映射。
7. init显示参数外置到/data/adb/arm13-boot.conf；真实冷启动不同分辨率/刷新率后恢复基准，再将编辑器集成到管理APK。
8. 统一代理从TCP扩展到可选SOCKS5 UDP和DNS over TCP、认证。UDP REDIRECT不能取得原目标，改为仅arm网络namespace中的TPROXY；实际多目标回包和故障阻断通过。
9. UID管理器v1.2把应用名单移到独立页，验证滚动和保存；新增UDP/DNS/认证结束时关闭，保留既有TCP出口与broker名单。
10. 用户在最后授权统一写记录并推送；发布前只读复核arm和保护对象，没有解冻repro，也没有再运行游戏/一小时耐久。

用户提及x86曾在Zygisk/LSPosed失败，本轮只提供分层排障参考，不把ARM64成功写成x86成功。最后LSPosed第三方模块count0；认证仅模拟正反例，无真实账号业务验收。
