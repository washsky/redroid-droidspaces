# 2026-11-24 新目标 Magisk 基础模块验收

> **第二部分的早期阶段记录。** 下文仅描述当时repro基础模块验收；随后用户授权arm承接深入探索，Next1.5.3+LSPosed2.2.1见[后续重点记录](part2/MAGISK_ZYGISK_LSPOSED.md)。当前repro冻结，不连接或修改它。

本次把redroid13repro从root-only补齐至基础模块生命周期。目录由官方Magisk初始化创建，现有管理器自动显示Modules；没有修改APK伪造可用状态。原redroid13arm与arch2未部署更新，也未停止或重启。基准为fd3892d981dfe989a74a11a5523ff1d7a21d4fc4，本地新增修改尚未提交。

## 原因与实现

原脚本只运行daemon，明确跳过模块阶段；/data/adb/modules不存在，界面不显示Modules。31.1的check_data还要求独立/data挂载，而该ReDroid把data目录放在根文件系统。单独运行root命令成功，不构成模块验收。

1. guard仍先unshare并递归private，在其私有命名空间内给/data做bind挂载，保留SELinux/cgroup敏感挂载只读和context guard。
2. 初始化新辅助程序时，先使新容器init的挂载递归private，断开宿主peer关系，再建立新的容器内部共享组，保留Android存储所需的传播。root peer与namespace inode记在仅root可写的/dev标记中。
3. 官方magisk --post-fs-data负责初始化目录、合并modules_update、脚本、属性与magic mount。保存阶段前的daemon mountinfo，只处理阶段后新增的Android分区挂载。
4. 新辅助程序通过open_tree克隆覆盖树，再用move_mount挂入容器init；仅允许/system、/vendor、/product、/system_ext范围，核对namespace及root peer。保留daemon私有保护；挂入失败时回退本次已挂入的覆盖。
5. init在启动Zygote前有界等待模块准备；class late_start运行service阶段，boot_completed运行boot-complete并重置官方启动计数。模块阶段失败会明确标记failed，不能报ready。daemon已运行后的模块失败保留root并让Android继续启动。

首版将init挂载传播全部保留为private，后续调整为容器内部独立共享组。最终实际检查新容器全部共享组与宿主init、redroid13arm、arch2无交集。Magisk私有root shell不具备后续FUSE视图，不能仅据root shell的/sdcard为空判断应用存储损坏；已通过真实DocumentsUI选取ZIP验证应用存储视图。测试ZIP经root暂存至/data/media/0/Download，未放宽已有目录权限。

## 构建与部署集合

使用既定NDK r29/API33运行scripts/build-native.sh；本轮两个修改的native程序另外以-Wall -Wextra -Werror编译通过。部署前保留精确文件备份，核对目标身份和哈希。

| 仓库文件/产物 | 目标位置 |
|---|---|
| config/bin/arm13-magisk-start | /vendor/bin/arm13-magisk-start，0755 |
| config/bin/arm13-magisk-stage | /vendor/bin/arm13-magisk-stage，0755 |
| config/init/redroid-magisk.rc | /vendor/etc/init/redroid-magisk.rc，0644 |
| out/native/arm13-magisk-daemon-guard | /vendor/bin/arm13-magisk-daemon-guard，0755 |
| out/native/arm13-magisk-module-mounts | /vendor/bin/arm13-magisk-module-mounts，0700 |

本机helper SHA256：a52248fcd9a9ab7687ecb2d75b1e789d68d104f244e9c7320a0eb699e6de6607；guard：31b7b1e99cf20553a7b014d325d11c986449f12acc49ceb828589e982d43dfc7。完整记录见[产物](../artifacts/magisk-modules-20261124.json)。不是任意设备可直接套用的通用包；挂载克隆依赖本机内核支持。

## 实际验收

自建探针不改内核、策略、账号或业务配置。scripts/build-magisk-probe.py可生成确定性ZIP，其文件内容与本轮实装ZIP逐一一致；ZIP时间戳不同的历史测试包与确定性输出分别记录哈希。

| 项目 | 观察结果 |
|---|---|
| 自动目录与前端 | 官方初始化生成modules、post-fs-data.d、service.d；Modules及Install from storage可见 |
| CLI安装 | 官方--install-module成功，生成modules_update与update标记 |
| 前端安装 | DocumentsUI Downloads选中ZIP、确认后成功生成待更新模块 |
| 启用后冷启动 | 官方合并模块；post-fs-data.sh与service.sh执行，system.prop读到唯一标记 |
| 文件覆盖 | /system/etc测试文件被导出；真实普通应用namespace中可见，在该namespace切换到实测UID10077后可读 |
| Action | 前端按钮执行action.sh，事件文件新增action |
| 禁用并冷启动 | UI开关写disable；升级保留禁用，事件计数不增，属性和init可见测试文件消失 |
| 移除并冷启动 | UI写remove；uninstall.sh执行，模块目录删除，测试属性/文件保持不存在 |
| 完成状态 | 测试模块卸载，Modules入口及安装按钮保留；测试ZIP和事件目录清理 |

本轮总计四次仅针对新目标的停止、确认卸载、只读fsck、启动。核心回归包含普通应用HTTPS200/默认TLS/SharedMemory、GPU120帧、AHB128帧、Gralloc跨线程；代理revision2和配置哈希不变。最终又复测HTTPS及基础状态。H264/HEVC、scrcpy实际窗口、浏览器压力、一小时耐久与游戏业务没有在本模块工作中重测。

## 边界与回退

- Zygisk保持关闭，未接入/验收；自定义sepolicy.rule与preinit策略持久化未支持。没有运行magiskpolicy修改宿主策略或修补宿主boot。依赖这些能力的模块不计入支持范围。
- 仅验收启动阶段的Android分区文件覆盖；service/action脚本运行后新增的动态挂载不自动再次导出。任意第三方模块、厂商ABI、内核修改与隐藏功能不能从探针结论外推。
- 日志仍包含/sbin标签设置被拒绝、只读SELinux context访问、worker清理EINVAL及preinit不存在提示。本轮未绕过这些保护，也未宣称日志完全无错误；上述已验收路径实际通过。
- 现有管理器显示unofficial版本警告，本轮没有重新核实APK签名/发行来源，也没有替换它。版本号不等于官方发行认证。
- privileged=full等原安全限制仍在；UID socket broker仍共享2000身份，网络原生隔离没有修复。
- 若回退，只恢复新目标备份的启动脚本、init参考和guard，移开新stage/helper，再冷启动新目标，使导出挂载随namespace销毁。保留/data/adb授权数据库，先禁用新增模块；不要全局清空目录或触碰宿主boot。

## 保护与临时应用状态

宿主仍Enforcing，boot ID与接手基线相同。原redroid13arm PID24174/starttime6353308、arch2 PID19643/starttime135633保持。最终新目标host init PID15310，内层system_server488/zygote317/netd316；这些只是当时快照。内存限额2.5GiB、failcnt0；宿主HTTPS及Wi-Fi ADB正常。

首次模块冷启动时，已有脚本包com.awcocxb自行弹出自动运行倒计时。没有点击业务按钮或登录；随即停止应用和已重新核实的root辅助进程，并仅在新容器设disabled-user，后续冷启动未再运行。该包暂时保持禁用，数据没有删除；原容器不受影响。需要业务时再按用户要求恢复，不能为模块测试启动自动游戏。

脱敏摘要见[evidence](../evidence/2026-11-24-magisk-modules/README.md)。原始本地验证、UI层级及精确备份保存在本次实验的独立目录中；未向ai-lan-gateway写入内容。
