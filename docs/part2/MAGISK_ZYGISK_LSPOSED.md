# Magisk 完整化与 Zygisk Next / LSPosed：重点问题记录

所属：[第二部分补充](../PART2_SUPPLEMENT.md)。实际平台为 ARM64 Android13、Droidspaces v7.2.0、同一 Android API35 宿主。**用户此前在 x86 也遇到过失败，本轮没有重新运行 x86；这里给排查依据，不宣称跨架构修复成功。**

## 1. 结论和精确组合

最终工作的组合：

- Bootless Magisk 31.1；**内置 Zygisk 关闭**，native bridge 属性为 0。
- Zygisk Next 1.5.3（880-22a1bf5-release），模块 ID 为 zygisksu。
- 用户提供的 LSPosed v2.2.1（7912），模块 ID 为 zygisk_lsposed，框架 API102。
- system_server 及两个普通应用的 pre/post specialize 探针回调实际出现。
- lspctl --json status 返回 daemonRunning=true、systemServerRequested=true、safeMode=false。
- 测试回调模块已卸载；最后第三方 Xposed 模块数为 0。

不要从模块名称、管理器“已安装”、daemon PID 或 modules 目录存在推导注入成功。至少区分 ZIP 安装、冷启动模块阶段、Zygote 注入、应用回调、LSPosed system_server 接入及实际业务 hook 六层。

本实验包按本地输入 SHA256 固定，见[产物清单](../../artifacts/supplement-20261125.json)。module.prop 的作者/版本文字不构成发行来源或 APK 签名认证；不在仓库重分发这些模块、管理 APK、license 文件或授权数据库。上游资料从 [Magisk](https://github.com/topjohnwu/Magisk)、[Zygisk Next](https://github.com/Dr-TSNG/ZygiskNext)核对；具体下载包仍要验证其实际来源。

## 2. 从 root-only 补到模块环境

最初只启动 Magisk daemon，因此 su 能工作，但没有完整执行官方模块生命周期。缺 modules 目录和前端入口是环境不完整的信号；手工 mkdir 或修改前端显示不能补齐它。

Magisk 31.1 的 check_data 还期望 /data 是挂载点；本 ReDroid 将 data 放在 rootfs 目录。最终 guard 先创建私有 mount namespace，再只在其中 bind /data，并把 SELinux/cgroup 等敏感挂载保持只读。

官方 --post-fs-data 初始化目录、合并 modules_update、运行脚本/属性和 magic mount；--service、--boot-complete 在对应阶段执行。启动状态必须区分 modules_ready=1 与 failed；失败时保留可用 root、允许 Android 启动，但不能报模块就绪。

Magisk 的私有挂载不会自然出现在容器 init / 应用视图。magisk-module-mounts.c：

1. 先在容器 init 内递归 private，切断宿主共享传播，再建立容器内部独立 shared 组。
2. 记录 root peer 和 mount namespace 身份，文件仅 root 可写。
3. 比较模块阶段前后 mountinfo，仅允许新产生的 /system、/vendor、/product、/system_ext 覆盖。
4. open_tree 克隆、move_mount 导出到经核实的 init 视图；失败回退当次已挂入部分。
5. 保留容器内部存储传播，避免全 private 使后挂载的 emulated storage 丢失。

CLI/前端安装、Action、system.prop、脚本、普通应用视图中的覆盖、禁用和卸载的早期完整验收见[模块报告](../MAGISK_MODULES_20261124.md)。后续部署到了 arm；该早期报告不代表冻结的 repro 已拥有 Next 1.5.3。

## 3. 安装失败先分清“文件存在”和“能够执行”

模块安装可能在执行自己的 native helper 时失败。Android 容器 /dev 是 noexec，chmod +x 不能越过挂载的 noexec 限制。

本轮将 Magisk 的安装临时目录从 /dev/tmp 限定改到 /data/adb/arm13-magisk-install-tmp；修改在每次启动复制到 /data/adb/magisk 的 util_functions.sh 上完成，源码位于 arm13-magisk-start。/data/adb 保持 0700，保留原校验和正常安装流程。

新版包还可能使用宿主 Python zipfile 不支持的压缩方式。本次 LSPosed 包的方法号为 95，工作站 7z 能读；Python 3.13 zipfile 的 NotImplementedError **不是设备缺陷，也不是 ZIP 必定损坏**。不要把解压器错误与 Android 运行错误混为一谈。

本次 v2.2.1 包安装脚本还执行 sign_tool 请求 license；网络或服务端拒绝要单独检查真实退出码，不通过伪造 license、删除校验或假返回成功绕过。

若安装器提示设备节点/随机源异常，应核对真实节点类型、major/minor、打开/ioctl 结果，再检查它调用的 helper 是否在 execve 或 PROT_EXEC mmap 时得到 EACCES/EPERM。**安装过程可以暴露容器环境差异，但一条提示不能证明应补造某个设备。** 不制作假 /dev、不把失败 syscall 改成成功、不对整个 /dev remount exec。

## 4. 内置 Zygisk 与外部 Next 的共同身份问题

宿主实际安全域是 u:r:droidspacesd:s0，容器中的 init、zygote 不具有原生手机相同的 u:r:init:s0 / u:r:zygote:s0。单靠界面开关无法改变这一点。

最初内置 Zygisk 也失败，说明不能直接下结论“只有外部 Zygisk 不支持”。修复内置路径时，定位到 daemon 对 IPC peer 身份的检查。

magisk-context-guard.c 只为 Magisk31.1 RequestCode::ZYGISK（10）适配 daemon 看到的 SO_PEERSEC，前提包括：

- peer 是 root，位于相同 PID namespace；
- exe 是实际 app_process64；
- 是当前 init 直接拥有的真实 zygote64 的预 specialize 子进程；
- PID 的 parent/starttime 复核一致。

SU、管理请求、任意 root 进程、shell UID 都不适配。对真正写 u:r:magisk:s0 的 attr/current 转换仍返回真实 EPERM，**没有成功切换 SELinux 域，也没有修改全局策略**。源码中的 peer-negative 探针对 root 非 zygote、shell 和其他请求作负例检查。

内置路径为诊断和回调中间验收；最终用 Next 时必须将内置 Zygisk 关闭，不能双实现同时注入。

## 5. Next 1.5.3 的额外阻断，不止一处

| 阻断层 | 现象/依据 | 最终处理 |
|---|---|---|
| Zygote 启动入口 | 静态 FD wrapper 位于 app_process 之前，影响 Next 识别/注入时序 | 使用 init.zygote64-next.rc 直接启动 app_process64，保留其他服务语义和 broker preload |
| init IPC 身份 | Next 期望 init，实际 peer 安全域为 droidspacesd | 限于真实 Next 进程、action0、同 PID namespace、init 直接子进程、exe/声明PID/UID/starttime 校验的身份适配 |
| Zygote IPC 身份 | 需要 ReadModules / Companion / Report / CleanNs / UnshareLock | 限定 action22/23/25/27/29，验证真实 init-owned zygote 或 root 预 specialize 子进程 |
| 很早的 Report | argv0 还未从原 app_process 参数改为 zygote64 | 仅 action25 接受精确原始 argv 序列及 init 父关系，避免宽泛放行 |
| FD 传输 | 用户态 peer 通过后，SCM_RIGHTS/sendmsg 仍可能被内核 SENDTO 检查拒绝 | 真实 Next 发送 socket 的 sockcreate 使用实际接收 init 域；没有仅伪造 getsockopt 或 sendmsg 成功 |
| 注入载荷 | Next 的 /dev/.zn_payload、/dev/.zn_helper 位于 noexec 挂载，执行映射失败 | 仅为核实的 Next 二进制将这两个精确路径路由到 root0700 的 /data/adb/arm13-next-exec |

关键区别：SO_PEERSEC 的用户态识别和内核 socket SID 的通信授权是不同检查。只修前者，FD 传输仍可能失败；应该保留 syscall 返回值并验证收到的 fd 真能读取/映射。

Next 特定适配只在 /data/adb/modules/zygisksu/bin/zygiskd64 为 root-owned 且不可被组/其他用户写入、当前与 init 均属于预期实际安全域时触发。action 编号是 **1.5.3 的协议边界**，升级版本必须重新对照，不能直接扩大白名单。

路径重定向保留 open/unlink 生命周期，附加 O_NOFOLLOW；/dev 仍 noexec。没有把进程域改成 init，没有加载宿主 SELinux policy，也没有将所有 peer 都报告成 zygote。

## 6. LSPosed：从安装走到 system_server

旧版 v1.9.3（7244）和 Next1.3.4 都曾参与排查，但不作为最终推荐组合，不把其早期失败宣传成永久不支持，也不把最终成功倒推为旧版已通过所有回归。

最终使用用户提供的 v2.2.1（7912）包。除了模块文件，还要看到 dex2oat/预加载相关挂载导出到容器和应用视图、zygisk 框架加载、daemon 启动以及 system_server 请求。单独打开 org.lsposed.manager 不能替代这些检查。

在**已核实的目标 arm**上：

```bash
adb -s "$INNER_ADB" shell 'su -c "/sbin/magisk -v"'
adb -s "$INNER_ADB" shell 'su -c "getprop sys.arm13.magisk.modules_ready"'
adb -s "$INNER_ADB" shell 'su -c "/data/adb/modules/zygisksu/bin/zygiskd status"'
adb -s "$INNER_ADB" shell 'su -c "/data/adb/modules/zygisk_lsposed/lspctl --json status"'
```

本轮最后返回 framework API102、daemonRunning=true、systemServerRequested=true、safeMode=false，module count=0。它证明框架接入，不证明第三方 Xposed 模块的任何业务 hook。

## 7. 增量安装顺序

先完成[补充部署清单](REPRODUCTION_AND_VALIDATION.md)，备份当前候选、脚本、init、su、库及需要回退的模块目录；授权库仅本地保管。

1. 成套部署 daemon guard、context guard、module-mounts helper、start/stage 和 redroid-magisk.rc。
2. 确认模块生命周期可用、安装临时目录在可执行的 /data 内。
3. 部署直接 app_process64 的 Next 专用 zygote rc；保留 UID broker preload。它取代本机的旧静态 FD wrapper，不给其他设备无条件覆盖整份 rc。
4. 关闭内置 Zygisk；在管理器模块页安装已核验的 Next1.5.3 和 LSPosed2.2.1。CLI 可使用官方 magisk --install-module，但仍要保留真实安装输出。
5. **只冷启动被授权的 arm 容器**；本轮需要加载 init 服务及早期注入，不能拿只重开 APK 代替。不要重启真机，不操作 repro。
6. 分别检查模块阶段、Next 状态、真实回调和 lspctl；回调探针完成后卸载并按模块生命周期清理。

探针构建：

```bash
source lab.env
bash scripts/build-zygisk-probe.sh
```

输出回调so、标准arm13-zygisk-probe.zip、负例和独立FD传输探针。ZIP内含module.prop和zygisk/arm64-v8a.so，模块ID为arm13_zygisk_probe；构建不安装到设备。安装/冷启动后再启动lab.arm13.probe、lab.arm13.gpuprobe，检索Arm13ZygiskProbe的pre/post日志。普通命令行app_process不等同于从Zygote fork的应用，不能替代该回调验收。验证后在Magisk移除这个探针模块，再按目标模块生命周期清理，不留下测试注入。

## 8. 给 x86 / x86_64 的排障检查表

用户的 x86 失败是重要参考，但未提供可在本轮重放的完整环境；以下是复核顺序，**不是 x86 修复承诺**。

1. 对照 ABI、32/64 位 zygote 和包内库；本轮探针仅 ARM64。不能将 AArch64 Codec 指令补丁用于 x86。
2. 先查安装器：压缩方法、临时目录的 noexec、helper 的 ELF interpreter、真实执行错误；再查 /data 独立挂载要求。
3. 查官方模块阶段和挂载视图；不是只 mkdir modules 或只启动 magiskd。
4. 查 init/zygote 的实际 attr/current、SO_PEERCRED、SO_PEERSEC 与 PID namespace；核对该版本的协议，而不是按“root 就应能连接”处理。
5. 若身份校验通过而 FD 失败，查发送 socket 的 SID 和实际 SCM_RIGHTS，区分 SENDTO 与身份字符串。
6. 查 /dev 载荷的 PROT_EXEC mmap、ptrace 和 capability/seccomp；不要全局放宽策略以掩盖是哪项失败。
7. 检查 FD wrapper、app_process 入口和早期 argv 时序。
8. 逐层收集：安装成功 → 模块阶段 → system_server 和应用真实 pre/post → lspctl 接入 → 一个可控 hook 模块。没有最后一步时明确只完成框架接入。

本仓库 helper 还有 redroid_arm64_only、特定域和路径校验。移植 x86 要先审查源码、编译相应架构并重新验收，不能只删保护条件。

## 9. 保留边界和回退

- 自定义 sepolicy.rule / preinit 全局策略持久化仍未接入；依赖它们的模块不计入支持范围。
- 启动阶段分区覆盖已验证；service/action 之后新增的任意动态挂载不自动再次导出。
- 不承诺隐藏、完整性认证、root 检测规避或任意第三方模块。
- 下一版 Magisk/Next/LSPosed 的协议、路径、安装器和 ABI 可能变化，当前校验不能视作通用兼容层。
- 回退时先禁用新增模块，恢复对应的一整组脚本/库/zygote rc/su，再仅冷启动目标；不要清空整个 /data/adb 或碰宿主 boot。
