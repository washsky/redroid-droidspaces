# 完整过程与复现指南

目标是在真机 Android 上由 Droidspaces 运行 Android 13 ARM64 容器，并让普通应用、浏览器、网络、root、GPU 与投屏形成可使用组合。当前验证对象是 mondrian / SM8475 / Adreno 730。其他 ROM/内核需要重新验收。

## 1. 准备与分层

工作站执行 ADB、scrcpy、NDK 交叉编译；Ubuntu 20.04 构建 AOSP；真机运行 Droidspaces。三者目录和地址用 lab.env 配置，不依赖原实验内网。

先确认宿主 root、USB/Wi-Fi ADB、aarch64、Droidspaces v7.2.0，以及 BinderFS、网络 namespace、cgroup、ext4 loop、KGSL 和真实 ashmem 条件。宿主必须保持 Enforcing，不能用关闭安全过滤的方式“验证成功”。

先备份原镜像，创建独立候选，再为该容器预留独立 binder / hwbinder / vndbinder 节点。节点来自合法 BinderFS 管理接口，不复用宿主或其他 Android 容器的节点，不删除正在使用的节点。

## 2. ARM64 AOSP / ReDroid 基线

按 BUILD.md 获取 pinned manifest，并使用 patches/aosp 的各项目 HEAD + 实际 diff。目标为：

```bash
source build/envsetup.sh
lunch redroid_arm64_only-userdebug
m -j4
```

确保 WebView 的 Git LFS 对象是真 APK，而不是 LFS pointer。原实验曾因 wrapper 找不到 d8.jar/apksigner.jar 失败，最终改用 AOSP out/host 的 d8、aapt2、zipalign、signapk.jar。不要因此降低 APK 签名要求。

打包 system/vendor ext4 到**新的** rootfs 镜像，不能把 Android sparse image 当普通 ext4 直接 mount。system.img 的 system-as-root 目录层级也必须核对：实验产物的 init 在挂载后的 system/system/bin/init。构建参考见 BUILD.md；本仓库不上传含 /data 的运行镜像。

属性区初始化有一个显式 marker opt-in：/system/etc/droidspaces-property-label-relax。该兼容条件只允许初始化已经完成时的标签状态错误，不把真正 AreaInit 失败改成成功。旧临时 libc 二进制 bootstrap 不是最终推荐方案，当前以 bionic 源码补丁为准。

init wrapper、Zygote FD 清理器与真实 init 是不同层。wrapper 是 ELF，不能按脚本 sed/cat。实际源码/可追溯性限制见 BUILD.md；未确认的历史临时脚本不能混入最终镜像。

## 3. Droidspaces 容器配置

从 config/container.example.conf 制作自己的配置，替换全部 REPLACE_*。静态 NAT 地址是 Droidspaces 租约/转发约定，Android Ethernet 内部仍用 DHCP。

```bash
adb -s "$HOST_ADB" shell 'su -c "/data/local/Droidspaces/bin/droidspaces --name=redroid13arm -C /data/local/Droidspaces/Containers/redroid13arm/container.config start"'
```

该示例包含 privileged=full；这不是安全隔离的通用建议。设备通过明确 bind_mounts 逐项暴露，enable_hw_access=0 / enable_gpu_mode=0 不代表没有 GPU：本实验以手工最小设备集合和专门服务接入。

不要启用与当前测试无关的宿主蓝牙或 Linux 桌面集成。不要把 arch2 的 Linux 桌面配置直接替代 Android 的 Gralloc / HIDL / Codec 配置。

## 4. 先解决启动循环与 DHCP

旧 ipconfigstore 在 eth0 没地址时写出缺少 LinkAddress 的 STATIC 存储，NetworkStack 崩溃，随后系统 watchdog 重启 system_server。

早期 workaround 是跳过错误生成器并移除两处错误存储：

- /data/misc/ethernet/ipconfig.txt
- /data/misc/apexdata/com.android.tethering/misc/ethernet/ipconfig.txt

后续已完成源码路径：初始化结构、空指针/空地址检查、DHCP 模式，以及 ro.boot.redroid_net_mode=dhcp。当前真机 ipconfigstore 和编译产物哈希一致。新复现应使用这个源码实现和 config/init/redroid.common.rc.reference 的语义，**不要重新部署只会写错误 STATIC 的旧二进制**。

新容器应冷启动就获得地址，而不是靠每次手工 ip addr / route 维持。

## 5. 普通应用与 WebView

普通应用的 app_data_isolation=false 是保留的实验兼容设置。它不完全控制 WebView 子 Zygote，后者仍会强制进行数据 bind mount。

最终 Zygote JNI 补丁直接读取 security.selinux xattr，重新创建挂载点时使用真实标签。这样避免把 ReDroid libselinux shim 合成的 HACKED 当作内核标签。真实错误仍然上报，没有泛化忽略 setfilecon 失败。

本设备真实 /dev/ashmem 与 /proc/misc 对应，当前 sys.use_memfd=false。先前 memfd_create/sealing 和 Java SharedMemory 成功是真实结果，但不能据此认定 Chromium 的所有 ashmem/protection-flags 分支兼容。用实际 fd 和 ioctl 追踪区分问题，不制造假设备或假成功。

恢复 cmd webviewupdate enable-multiprocess，确认 provider、RELRO、renderer 子进程和真实网页画面。当前 provider 为 com.android.webview 157.0.8103.78；不把长期降级旧 WebView 当正式修复。

## 6. 普通 UID 网络

shell TCP443 成功不能替代应用 UID 测试。本实验最终确认宿主 cgroup BPF 在子容器中继承，某些宿主 UID 与内层应用 UID 冲突，甚至 socket_create 就 EPERM。普通桥接/NAT/DNS 调整不能消除这个身份冲突。

保留宿主策略，通过 NETWORK.md 的 UID2000 socket broker 兼容。先构建服务、预加载库和 root 控制器，再在保留原 Zygote rc 的前提下增加 LD_PRELOAD；最后安装管理 APK，按包名勾选应用。包名配置是唯一真源，不手工维护两份 UID 清单。

测试默认 TLS、真正 HTTP 状态、内容和 UDP nonce，必要时使用线路真实可达的 Android 204 探测站。不能禁用验证或伪造 204。

## 7. Root

按 ROOT_AND_SECURITY.md 安装容器内 Bootless Magisk 31.1：保留 AOSP su，init 监督新增守护服务，限制 daemon 自身 namespace 中的敏感挂载，不加载宿主 policy、不跑模块/Zygisk阶段。管理器和需要 root 的应用单独授权，不开放无条件 root。

## 8. GPU 与 Codec

按 GRAPHICS.md：

1. 固定 Mesa for Android Container 提交，应用本地 bridge 补丁和新增文件。
2. AOSP 编译 IMapper4 metadata 桥接库；NDK 构建 Turnip / KGSL ICD。
3. 安装同 ROM 的 QTI allocator/mapper及依赖，VINTF 声明单独增加，不覆盖全部宿主 /dev。
4. CPU/真实 buffer/EGL/Vulkan/GPU APK 分层验收，不能只看占用率。
5. QTI YUV Codec 先验证，然后处理 Surface 输入布局和 Codec 私有原厂 Vulkan。
6. 定制 scrcpy server、线性 Surface helper 和 scoped Codec loader 组合起来，验证解码像素与窗口。
7. 最后补上本机 QTI Gralloc 失败分支漏解锁，避免浏览器和脚本触发全局图形卡死。

## 9. 浏览器与脚本

用本地可控 Canvas/WebGL 页面验证连续绘制，再测滚动、按钮、文字输入和公网 HTTPS。保存真实截图和帧计数。

脚本独立 root 辅助进程可能在 am force-stop 后残留；出现冻结应先保存线程栈，并核对完整 cmdline 后处理该应用自己的进程。修复后优先用应用自身“停止/退出”，避免强停导致 Android 清除无障碍启用项。

脚本的 com.gkid.GakeService 应在容器内启用并显示 Bound；不是修改宿主无障碍。配置页可用也不等于已完成自动游戏业务，本实验不替用户登录、选择账号或运行战斗。

## 10. 扩容与最终验收

按 STORAGE_AND_ROLLBACK.md 从 6 GiB 离线扩至 12 GiB。先完整备份并校验，再停止**目标容器**、确认卸载/loop 未附加，e2fsck、truncate、resize2fs、再次检查。

每次关键修改后检查：boot_completed、DHCP、system_server/Zygote/netd、普通 UID HTTPS、WebView 真实页面、硬编投屏颜色与输入、宿主 HTTPS、Wi-Fi ADB、保护容器 PID/starttime。不要把一小时测试或游戏登录后的流程补写成已完成。
