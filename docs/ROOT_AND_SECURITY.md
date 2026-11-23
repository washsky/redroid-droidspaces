# 容器 Root 与安全边界

## 为什么 AOSP su 不够

AOSP userdebug 的 su 与应用通过 Magisk 请求授权的协议/路径不同。当前镜像在 /system/xbin/su.aosp 保留原始 su；普通 root 命令使用 su -c，指定 UID/GID 测试才使用 su.aosp。不要把旧 su 0 id 语法当 Magisk 命令。

## 当前 Bootless Magisk

使用官方 Magisk 31.1，材料放在 /system/etc/arm13-magisk；本仓库不分发其 APK/二进制。config/bin/arm13-magisk-start 创建内层 /sbin tmpfs并启动 root manager 所需daemon：

- /system/bin/su、/system/xbin/su、可选 /system/bin/magisk 指向 /sbin/magisk。
- init 服务 redroid.magisk 在 post-fs-data 启动并监督。
- 不修补宿主 boot，不执行 --post-fs-data 模块阶段、Zygisk或通用模块安装流程。
- 不运行 magiskpolicy 加载策略；脚本中的 magiskpolicy 文件不可执行。
- daemon guard 先创建私有 mount namespace，再把其看到的 SELinux/cgroup 等敏感挂载 remount只读。
- context guard 对精确的 magisk context转换写入返回真实 EPERM，保持容器已有安全域；不是谎称成功。
- 这依赖实验中已有的 Droidspaces 宿主策略。不能据此推导为任意 Enforcing 宿主上的通用安全 root方法。

管理器 APK先手动确认 daemon和路径，再授权 lab.arm13.netmanager、测试工具及用户明确需要的应用。不要把所有应用默认授予 root，也不要把授权数据库/账号信息上传。

## 兼容配置的安全退让

当前 privileged=full、app_data_isolation=false、容器侧 libselinux兼容层、指定宿主设备bind、socket UID2000共享都改变了边界。源码补丁中有意减少内层部分检查，是为了该实验；宿主 Enforcing 仍为真，但不构成安全等价。

真实 xattr Zygote 修复保留了内核标签，不等于整体应用数据隔离已恢复。不要删除 marker / 改属性后直接宣称“隔离修复完成”，应在独立候选中重测完整启动和应用访问。

## 脚本应用

本机 com.awcocxb 的无障碍组件是 com.awcocxb/com.gkid.GakeService。应通过内层设置启用，并检查 Bound services。它控制的是容器内界面，不是宿主。不同 APK版本的组件名必须重新查询。

Android 强行停止应用时可能清除无障碍启用项。测试结束优先用应用自己的停止/退出；不要因为一次 root进程存在就认定节点服务已经启用。完整冷启动在正常授权/运行状态下已验证保存。

脚本配置页、root屏幕采集和控制恢复已验收；未代用户执行登录和自动游戏流程。

## 回退

保留 AOSP su原文件、Magisk前的Zygote/服务配置和完整镜像。先恢复原 su路径和与其配套的 init配置，再仅重启目标容器。保留 /data/adb 等备份供检查，不在活跃环境执行广泛 rm -rf。原实验 rollback-root.sh 的固定备份路径不能跨设备照抄。
