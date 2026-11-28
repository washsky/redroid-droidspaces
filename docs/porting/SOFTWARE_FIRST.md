# 第一阶段：先让软件系统和普通应用工作

先读[镜像来源与已知缺口](BASE_IMAGE_PROVENANCE.md)。目标是得到可冷启动、能运行普通应用的软件基线；GPU直通、厂商Codec、Magisk模块、Next/LSPosed和代理前端在后续阶段分别处理。

## 0. 下载、保留原件、建立工作副本

```bash
python3 scripts/download-prebuilt.py --profile base
zstd -d --sparse out/prebuilt-base-20261128/redroid13-arm64-pve-source-original-20261119.img.zst -o out/prebuilt-base-20261128/source-original.img
sha256sum out/prebuilt-base-20261128/source-original.img
cp --reflink=auto --sparse=always out/prebuilt-base-20261128/source-original.img out/prebuilt-base-20261128/rootfs-working.img
sudo e2fsck -f -p out/prebuilt-base-20261128/rootfs-working.img
```

原始source镜像SHA256必须为`d5b6961e65832aee875c15766c8ffac6ba275f3f217390853b841977785b326e`，大小6442450944字节。`e2fsck`只对工作副本执行：0是正常，1是已修复，其他返回值须先处理。两个原件都保留了待回放journal，只读`-fn`的0退出码伴随计数警告，不能当成“完全干净”。本次仅在Debian副本回放日志复核，不改变下载原件。

不要执行原镜像里的`/.lab-start.sh`：它是旧模拟器传输实验脚本，包含固定端口、地址和权限参数；`/.lab-droidspaces`也不是要求替换宿主运行时的安装入口。新目标使用现场已安装的Droidspaces和自己的配置。

## 1. 核实宿主与最少设备

- 确认aarch64宿主、目标ReDroid为Android13/API33/arm64-only；不能据此运行任意32位ARM或x86应用。
- 核实Droidspaces版本、网络/挂载/PID namespace、cgroup、ext4/loop、三个独立Binder节点，以及实际ashmem或memfd实现。
- 保留宿主SELinux/BPF/防火墙与其它容器状态。节点用BinderFS管理接口新建，不猜major/minor，不复用其它实例三节点。
- 软件阶段先不绑定KGSL、DRM render node或`/dev/video*`，不导入另一处理器的厂商库。现有[软件配置参考](../../config/container.software-porting.example.conf)只有软件阶段的设备集合，需替换名称、路径、接口、Binder/ashmem节点、NAT和端口。

参考配置继承早期核心`full + /init.droidspaces`启动方式，尚不是针对新设备完成的权限最小化配置。`selinux_permissive=0`，不得为复现全局关闭宿主SELinux。该历史wrapper自身有容器启动参数与兼容逻辑；检查实际命令和源码差异，不能把配置的某个0值当成完整隔离证明。

原wrapper包含`redroid_gpu_mode=guest`、720×1280、DPI320、60fps；软件模式不是“Android没有任何图形栈”。还应观察gpu_config、SurfaceFlinger和实际renderer，确认没有误走硬件后端。wrapper是ELF，不能当shell脚本编辑，也不能假定它转发所有附加参数。

## 2. 在离线工作副本补齐已知通用修复

Release另有`redroid13-arm64-portable-core-fixes-20261128.tar.zst`，包含9个ARM64/API33配套文件：7个原source到opt-in版的Bionic/linker及相关重链接文件，以及从Ubuntu20当前AOSP输出核对的两个功能修复：

| 文件 | 作用 | 本次核对SHA256 |
|---|---|---|
| `/system/lib64/libandroid_runtime.so` | Zygote应用数据目录SELinux xattr真实标签修复 | `41ebfbeb2634b53fc40482d237452538bdfd662f503c965a5e20b94265895b15` |
| `/vendor/bin/ipconfigstore` | DHCP配置与空IPv4防御 | `6cc88412039ab4ebc0ddb66411d49eaa38cdaea81aac1378c0b86ce9c9ec3108` |

9个文件均与既有成功集成的对应文件哈希一致；两个功能修复另与Ubuntu20当前输出一致。7个Bionic/相关文件从已核对的历史opt-in原件取得，完整旧/新哈希及文件角色在包内manifest；不能把一个marker当成整个版本差异。本轮没有把“原件+修复包”的组合在新处理器上启动，新平台仍需逐项验收。包内保留源码补丁参考，没有QTI私有库或自动安装器。

在自己的私有mount namespace中确认挂载对象就是**新工作副本**后，先检查9个目标文件均为manifest记录的原始或已更新哈希，再按包内README成套复制，保持原有xattr并设置记录的owner/mode。不要向活动容器、另一版Android或宿主系统复制它们。

原source镜像缺少`/system/etc/droidspaces-property-label-relax`，而opt-in历史原件已有该标记及配套库。完成上述Bionic配套转换后，在工作副本创建空标记并沿用`system/etc`的SELinux标签；不能只touch标记就宣称转换完成。它只控制已记录的属性标签兼容分支，不把真实初始化失败当成功。

旧ipconfigstore可能把尚无地址的eth0写成错误STATIC配置。配套使用DHCP模式：在**容器init**调用新ipconfigstore之前设置`ro.boot.redroid_net_mode=dhcp`，并处理工作副本里两处旧Ethernet存储。对应语义在[redroid.common参考](../../config/init/redroid.common.rc.reference)的post-fs-data段；只移植网络相关语义，保留软件阶段的Codec设置，不能把包含后期硬件假设的整份rc无脑覆盖。真正地址、DNS、续租由Android网络栈管理。

SharedMemory选择按新内核实测。原机依赖真实ashmem且`sys.use_memfd=false`；其它内核不能照抄这个值，更不能用假节点或恒成功ioctl替代实现。普通应用启动若遇Zygote数据挂载错误，先核对上述runtime和真实xattr；`persist.zygote.app_data_isolation=false`是旧环境兼容退让，不等于已恢复原生应用隔离。

## 3. 先验收启动和本地应用

启动只使用获准的新实例；不要调用历史固定目标。TCP adbd默认未必打开，可通过Droidspaces目标run/enter设置内层`persist.adb.tcp.port=5555`与`service.adb.tcp.port=5555`，仅重启内层adbd，再建立新实例自己的转发。所有adb调用显式`-s`，目标地址由现场查询。

```bash
adb -s "$INNER_ADB" shell 'getprop sys.boot_completed; pidof system_server; pidof zygote64; pidof netd'
adb -s "$INNER_ADB" install out/prebuilt-base-20261128/ARM13-API-Probe-20261128.apk
adb -s "$INNER_ADB" shell am start -W -n lab.arm13.probe/.Probe
adb -s "$INNER_ADB" logcat -d -s Arm13ReleaseProbe:I
```

探针包名和组件可从源码manifest核对。先要求PRIVATE_STORAGE/SHARED_MEMORY通过，系统服务持续存活，Launcher/Settings或一个普通应用可真实交互；此时网络失败单独记录，不能因此宣称本地软件运行失败或已经全面通过。

## 4. 再验收普通应用网络和软件画面

先看容器eth0、默认路由、DNS、DHCP记录，再分别测试shell和普通应用UID。shell HTTPS成功不能替代应用UID；新设备如果原生应用直连可用，不需要强制引入broker。如果出现本项目曾遇到的宿主UID/BPF冲突，按[UID网络说明](../NETWORK.md)集成broker及预加载；使用基础Release附带的源码工具包，不能解绑宿主BPF或全局放行防火墙。基础原件没有预装broker，不应期待管理器开箱可用。

在实际普通UID下要求DNS成功、默认TLS校验保留、HTTPS真实200/204，再记录探针`API_AND_HTTPS=PASS`。没有Magisk时先查询原镜像的AOSP su或使用Droidspaces目标root命令；`su.aosp`是后期保存的路径，不能假定原始镜像也有同名文件。Magisk不是软件启动或网络排障的前置条件。

先使用屏幕截图或可用的软件编码器取得画面，电脑端默认软件解码即可。可运行`adb -s "$INNER_ADB" shell dumpsys media.codec`及`scrcpy -s "$INNER_ADB" --list-encoders`查实际实现，再选择实际存在的软件encoder；不要在此阶段强制QTI或VAAPI。若软件Surface编码不可用，先通过截图/屏幕录制探针验收应用显示并记录缺口；不把“有窗口”当成硬编通过。浏览器要检查页面、输入、滚动及WebView renderer进程。

## 5. 阶段通过后保存基线

| 检查 | 要求 |
|---|---|
| 系统启动 | boot_completed=1，关键进程稳定，无重复system_server重启 |
| 普通应用 | 安装、私有存储、SharedMemory、真实界面与操作 |
| 网络 | DHCP/DNS/默认TLS HTTPS，普通UID结果，注明是否依赖broker |
| 软件图形 | renderer与实际画面核对；软件投屏或替代截图验收，失败项明列 |
| 冷启动 | 仅目标停止/卸载/启动后以上状态可恢复，数据/网络配置持久化 |
| 保护对象 | 宿主bootID/网络/Enforcing及其它实例状态保持 |

保存这个工作副本的哈希、配置和独立记录。之后才进入[GPU与硬件编解码](GPU_AND_CODEC.md)。软件基线有未解释的崩溃或应用失败时，先定位当前层，不同时换GPU、Codec与Zygisk。
