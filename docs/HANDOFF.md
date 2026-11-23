# 接手与维护记录

整理于 2026-11-24。开始前读 STATUS.md，区分历史验收与当前状态。

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
