# 容器 Root 与安全边界

> **阶段说明（2026-11-25）：** 下文root-only及基础模块描述保留历史范围。arm当前已完成Next1.5.3+LSPosed2.2.1与终端修复，详见[第二部分重点](part2/MAGISK_ZYGISK_LSPOSED.md)；repro冻结，不能按arm状态推定它已同步。

## 为什么 AOSP su 不够

AOSP userdebug 的 su 与应用通过 Magisk 请求授权的协议/路径不同。当前镜像在 /system/xbin/su.aosp 保留原始 su；普通 root 命令使用 su -c，指定 UID/GID 测试才使用 su.aosp。不要把旧 su 0 id 语法当 Magisk 命令。

## 原始 Root-only 基线

原始部署使用 Magisk 31.1，材料放在 /system/etc/arm13-magisk；本仓库不分发其 APK/二进制。当时的启动脚本创建内层 /sbin tmpfs并启动 root manager 所需daemon，原 redroid13arm 保持这一版本：

- /system/bin/su、/system/xbin/su、可选 /system/bin/magisk 指向 /sbin/magisk。
- init 服务 redroid.magisk 在 post-fs-data 启动并监督。
- 原版本不修补宿主 boot，不执行 --post-fs-data 模块阶段、Zygisk或通用模块安装流程。
- 不运行 magiskpolicy 加载策略；脚本中的 magiskpolicy 文件不可执行。
- daemon guard 先创建私有 mount namespace，再把其看到的 SELinux/cgroup 等敏感挂载 remount只读。
- context guard 对精确的 magisk context转换写入返回真实 EPERM，保持容器已有安全域；不是谎称成功。
- 这依赖实验中已有的 Droidspaces 宿主策略。不能据此推导为任意 Enforcing 宿主上的通用安全 root方法。

管理器 APK先手动确认 daemon和路径，再授权 lab.arm13.netmanager、测试工具及用户明确需要的应用。不要把所有应用默认授予 root，也不要把授权数据库/账号信息上传。

## 2026-11-24 新目标基础模块支持

当前仓库的启动脚本、init参考及新辅助程序已在 redroid13repro 接入官方 post-fs-data、late_start、boot-complete 阶段。目录由官方初始化生成；前端ZIP安装、Action、脚本、system.prop、systemless文件、禁用与卸载经过实际验收。原容器未部署更新。实现、构建、回退和限制见[模块验收](MAGISK_MODULES_20261124.md)。

daemon继续使用私有命名空间及敏感挂载只读保护；新增辅助程序只将Android分区的新覆盖挂载克隆到新容器init，并在Zygote启动前完成。容器内部保留独立共享挂载组，以支持Android存储；现场检查这些组与宿主、两个保护容器无交集。没有修补宿主boot或加载全局SELinux策略。Zygisk保持关闭，自定义sepolicy.rule与preinit持久策略没有接入，不能宣称任意Magisk模块均兼容。

## 兼容配置的安全退让

当前 privileged=full、app_data_isolation=false、容器侧 libselinux兼容层、指定宿主设备bind、socket UID2000共享都改变了边界。源码补丁中有意减少内层部分检查，是为了该实验；宿主 Enforcing 仍为真，但不构成安全等价。

真实 xattr Zygote 修复保留了内核标签，不等于整体应用数据隔离已恢复。不要删除 marker / 改属性后直接宣称“隔离修复完成”，应在独立候选中重测完整启动和应用访问。

## 脚本应用

本机 com.awcocxb 的无障碍组件是 com.awcocxb/com.gkid.GakeService。应通过内层设置启用，并检查 Bound services。它控制的是容器内界面，不是宿主。不同 APK版本的组件名必须重新查询。

Android 强行停止应用时可能清除无障碍启用项。测试结束优先用应用自己的停止/退出；不要因为一次 root进程存在就认定节点服务已经启用。完整冷启动在正常授权/运行状态下已验证保存。

脚本配置页、root屏幕采集和控制恢复已验收；未代用户执行登录和自动游戏流程。

## 回退

保留 AOSP su原文件、Magisk前的Zygote/服务配置和完整镜像。先恢复原 su路径和与其配套的 init配置，再仅重启目标容器。保留 /data/adb 等备份供检查，不在活跃环境执行广泛 rm -rf。原实验 rollback-root.sh 的固定备份路径不能跨设备照抄。
