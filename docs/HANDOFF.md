# 接手与维护记录

更新于2026-11-25。先读[第二部分总览](PART2_SUPPLEMENT.md)及STATUS.md；本页后面的早期验收按时间保留。

## 当前接手优先事项

- 核心仍是[第一部分运行ReDroid](FULL_PROCESS.md)；深入Magisk、Next/LSPosed、stock投屏与代理属于第二部分补充。
- redroid13arm是后续授权实验目标，已集成Magisk模块完整化、Next1.5.3+LSPosed2.2.1、终端PTY、stock scrcpy、显示参数及代理v1.2。
- 用户后续明确授权在redroid13repro复现第二部分，已完成[增量复现与验收](part2/REPRODUCTION_REPRO_20261125.md)。它当前具备Next/LSPosed、stock投屏、PTY、显示和代理；管理APK1.2.1，两个控制器读取root-owned实例配置，不再将arm地址编译进程序。
- 不重启真机，不停止或重启arch2；不全局操作ADB、SELinux、BPF、防火墙。
- 最近repro复现期间，arm完全保持原状且不允许停止/重启；后续操作仍以用户当轮授权为准。重新核实PID/starttime、UID、netns、连接和窗口，历史PID不用作命令参数。
- 本地项目已迁入android-lab；不要向ai-lan-gateway放任何项目临时文件。目录通过lab.env定位，不把个人路径写入公开步骤。
- 整理文档本身不是部署授权；本次repro复现依据用户单独明确指令，不能外推为修改其他容器的授权。

| 当前arm关键点 | 状态/入口 |
|---|---|
| Magisk/Next/LSPosed | 31.1、1.5.3(880-22a1bf5)、2.2.1(7912)/API102；内置Zygisk关闭 |
| 早期注入 | init.zygote64-next.rc直接app_process；context guard对版本化请求做窄范围兼容 |
| 模块/终端 | modules_ready=1、terminal_ready=1；动态挂载/PTY边界见第二部分 |
| 显示 | /data/adb/arm13-boot.conf；720×1280/DPI320/60Hz；bootctl仅保存下次启动 |
| 权限 | nocaps + /init.arm13-caps删五项；36项能力、seccomp保留，无root UID映射 |
| 网络 | broker仍必需；统一SOCKS5 TCP开启，UDP/DNS/认证关闭；v1.2管理APK |
| 投屏 | stock5.0.1 server默认QTI编码；--hwdec=vaapi仅电脑端；不再要求定制连接脚本 |
| 产物/证据 | [清单](../artifacts/supplement-20261125.json)、[摘要](../evidence/2026-11-25-supplement/README.md) |

前一补充发布提交7b91023仅对arm做只读复核。后续repro增量复现单独记录，不倒写历史：两次仅repro冷启动、真实回调与管理器、网络、显示和stock投屏均通过。真机bootID、arm/arch2的PID与starttime连续；LSPosed第三方模块count0，x86/业务hook/一小时耐久未验收。

repro新控制器依赖/data/adb/arm13-instance.conf（root:root 0600），配置缺失/不匹配时拒绝执行。arm仍是原版本控制器与APK，未热更新；以后升级它时须先配置本实例文件，再换通用控制器。不要复制repro的/data/adb授权、license或实例配置到arm。

下面的“原容器未部署”“full”“Zygisk未支持”只描述相应历史阶段，不是上述arm当前状态。

## 不要重新开始整个实验

已经跑通 ARM64 启动、应用、UID broker、WebView 多进程、GPU 路径和定制 scrcpy 硬编。本仓库的工作是保留这一基线；没有必要重新做 x86 或虚拟机。

保护约束：

- 不重启真机宿主，不停止/重启 arch2 或其他容器。
- 不执行 adb kill-server、全局 adb disconnect；每条命令指定目标 serial。
- 不全局关闭 SELinux，不清空防火墙/路由，不 detach 宿主 cgroup BPF。
- 不删除 BinderFS 节点，不猜测 ashmem 设备号。
- 不覆盖原始镜像、扩容前备份或唯一成功候选镜像；新实验用独立候选。
- 用户不需要容器蓝牙；不要修改宿主蓝牙。
- 测试游戏仅到登录入口；脚本只验收启动/配置/截图/停止，不自行替用户登录或运行自动游戏流程。

## 当前关键文件

| 位置（容器内，除非注明） | 含义 |
|---|---|
| /init.droidspaces | ELF init wrapper，不是 shell 脚本；用 file/strings 检查 |
| /system/build.prop | app_data_isolation=false 仍生效 |
| /system/lib64/libandroid_runtime.so | 原始 xattr 标签修复后的构建产物 |
| /vendor/bin/ipconfigstore | DHCP / 空 IPv4 防御实现 |
| /vendor/etc/init/redroid.common.rc | 当前 init 参考已保存，不要回滚为早期错误生成器 |
| /dev/ashmem | 真实节点；当前环境 /proc/misc 为 127 ashmem，不能跨设备套用 |
| /vendor/bin/arm13-netctl | root 控制器，status/save/restart/boot |
| /vendor/bin/arm13-netbroker | UID2000 服务，按包名动态映射 UID |
| /system/lib64/libarm13_socketbroker.so | Zygote 预加载 socket 拦截 |
| /data/misc/redroid-netbroker/packages | 持久包名配置，revision 防并发覆盖 |
| /dev/arm13-netbroker/uids | 派生运行时名单，重启自动生成 |
| /system/bin/su、/system/xbin/su | 指向 /sbin/magisk |
| /system/xbin/su.aosp | 保留的原始 AOSP su；用于明确 UID 的测试 |
| /vendor/etc/init/arm13-qti-codec.rc | 仅 Codec 进程的原厂 Vulkan 选择 |
| /vendor/lib64/arm13-codec-compat/ | 私有兼容库与动态 shader 编译器依赖 |
| /vendor/lib64/libgralloccore.so | 已补齐漏解锁分支 |
| /vendor/lib64/libgralloccore.so.pre-unlock-20261123.bak | 原库备份 |

## 已核对哈希

见 artifacts/hashes.json。关键值：

- 原始源码 opt-in 6 GiB 镜像：ab65cb2dbc97e56829dcdce0f05a5af31a2152977194797405a6ac3a98edcc6f。
- libandroid_runtime.so：41ebfbeb2634b53fc40482d237452538bdfd662f503c965a5e20b94265895b15。
- ipconfigstore：6cc88412039ab4ebc0ddb66411d49eaa38cdaea81aac1378c0b86ce9c9ec3108。
- 原始 QTI Gralloc：3e4cef7a63b47c18bb1ba8283a08fdbd150a99194138e92ead3cede662c9098b。
- 修补后 QTI Gralloc：7ba792e5becada349ca60216468b9b444c99c531ce7915db0a387e7aefb10337。

libandroid_runtime 和 ipconfigstore 已与 Ubuntu out 目录产物再次比对一致。源码树中的原始 xattr 修复已替代早期“忽略 EINVAL”的失败实验；不要重新部署旧脚本。

## 工作区定位约定

原工作站材料按 redroid13-{network,netmanager,root,mesa27,surfacefix,commonlock}-20261123 分类。本仓库已提取其中可发布的源码。具体账号目录和内网地址不进入公开交接；用自己的 lab.env。

原始 opt-in 镜像、当前候选和扩容前完整备份均保留在实验机/真机，未上传 GitHub。活跃候选原名包含 rootfs-hw-candidate-20261121；容量后来扩为 12 GiB，不要按文件名推断大小。

## 接手先做只读复核

执行 scripts/inspect.sh；确认 ADB 目标、容器名、boot_completed、网络管理器状态、WebView provider、真实 ashmem、GPU 属性、/data 容量、关键哈希。PID 要现场复核；最后一次验收 redroid host-init PID10687、内层 system_server467，均不是永久标识。

宿主 Enforcing 不代表内层隔离原生等价。当前 privileged=full、应用数据隔离关闭、厂商设备暴露和 socket UID 共享都应保留在风险说明。

## 下一步按需求开展

1. 在 Droidspaces/Android 宿主层研究父 cgroup BPF 与 UID 命名空间隔离；当前 broker 是可用兼容层，尚非根治。
2. 在独立干净镜像中重放构建与增量部署，产出新的可分发 rootfs；本次公开仓库自身尚未从零重建整台环境。
3. 用户愿意时再做长时稳定性、真实游戏登录后网络和脚本流程；不要把前述短时验证写成完成。
4. 针对其他 ROM/芯片重新定位 Gralloc/Codec；不能把本机二进制偏移或 /dev/video32/33 直接推广。

## 2026-11-24 独立复现追加

新增目标 redroid13repro，12 GiB，NAT 172.28.41.37，转发15614，run_at_boot=0。原 redroid13arm 与 arch2 保持运行。复用已有 system/vendor，重建空 data 并重新安装测试 APK；首次部署和一次冷启动的 HTTPS、GPU、Gralloc、H264 Surface、浏览器与管理器通过。新管理 APK 修复了控制进程继承 LD_PRELOAD 的问题，仅部署到新目标。详情及限制以 [复现报告](REPRODUCTION_20261124.md) 和 [装配指南](PREBUILT_REPRODUCTION.md) 为准。不要因新增目标而混用原容器的 UID、Binder 节点和端口。

## 2026-11-24 Magisk 模块追加

用户指出模块目录和前端缺失后，已把 redroid13repro 从 root-only 补齐至基础模块流程。官方初始化自动生成目录，前端ZIP安装/Action/禁用/移除与冷启动生效已验证；测试模块已卸载。源码新增 arm13-magisk-module-mounts.c、arm13-magisk-stage 和可重建的自测ZIP生成器，原容器未部署。详见 [模块验收](MAGISK_MODULES_20261124.md)。

本轮最终目标host init PID15310、内层system_server488/zygote317/netd316，只是最后一次快照，后续仍须重核。原redroid13arm PID24174/starttime6353308、arch2 PID19643/starttime135633保持。新容器脚本包com.awcocxb冷启动曾自行显示倒计时，因此仅在新容器暂时disabled-user并停止，避免自动流程；数据未删除，后续按用户需要再启用。

当前本地代码是在fd3892d基准之上的新增修改，尚未提交/推送；本地副本没有.git。UID broker依赖、厂商来源、完整重建和长期稳定性边界均未改变。
