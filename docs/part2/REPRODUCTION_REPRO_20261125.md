# 第二部分在 redroid13repro 的增量复现

日期：2026-11-25（Asia/Shanghai）。基准为公开补充提交7b91023ea471599317997a78c568192a362d1c26，核心仍以fd3892d为已验证入口。本报告依据实际命令输出整理；[脱敏摘要](../../evidence/2026-11-25-repro-part2/README.md)、[输入与最终哈希](../../artifacts/part2-repro-20261125.json)单独保存，原arm报告保持历史范围。

## 范围和复用

用户明确解除本次repro复现的冻结限制，只允许修改/冷启动repro。真机未重启，redroid13arm和arch2保持原PID/starttime；全局Enforcing保持，未全局操作ADB、BPF或防火墙。临时暂停时只等待已发起的第二次冷启动完成；恢复后继续余下验收，没有从头重做。

这是既有独立空data部署成功后的**增量复现**，本次没有清空data。复用AOSP、Mesa、厂商兼容库和已验证native产物，跳过基础Magisk目录/安装/Action/禁用/卸载的重复生命周期测试，也没有重做内置Zygisk中间诊断阶段。必要例外是两个小型管理控制器及APK1.2.1的实例适配构建，不是大型镜像重编译。

目标仍是12GiB镜像、独立Binder节点、NAT和ADB端口；保留既有broker名单及revision2。repro内存限制2.5GiB、swap关闭，memory.failcnt最终为0。配置保留CPU quota字段，但运行时报告内核缺CFS带宽控制而跳过，不能写成实际已限CPU。

## 成套更新与安装

部署仓库的Magisk启动/阶段、module-mounts、context guard、直接app_process的Next zygote rc、su登录/终端修复、cap-init、proxy服务/relay及精确QTI默认线性格式补丁；保留原broker、preload及核心图形栈。先备份目标自己的原文件和容器配置，逐项校验SHA256，使用暂存新inode再rename。

使用用户提供的Next1.5.3与LSPosed2.2.1 ZIP，输入哈希与arm报告一致；官方Magisk CLI安装均exit0，没有跳过安装器检查。安装临时目录改为/data下可执行私有路径。LSPosed安装输出的license步骤为Skipped：未复制arm授权/license，也不能写成一次全新的在线license获取成功。preinit目录警告保留，不宣称通用sepolicy.rule支持。

内置Zygisk关闭，native_bridge=0。临时安装仓库的回调探针用于实际system_server和普通应用验证；验收后按模块remove生命周期，在第二次冷启动确认目录删除。

## 仓库验收发现的两个复用问题

1. 原proxyctl/bootctl在源码里绑定arm的eth0地址，复制到repro返回ARM_NETWORK_NAMESPACE_REQUIRED。先验证保护确实拒绝错误目标，再将该地址移入root:root 0600的/data/adb/arm13-instance.conf。同一通用二进制现在读取各实例自己的配置；无文件/权限不符/地址不匹配都拒绝，不能自动回退arm地址。设备、init helper、PID/net namespace检查保留。格式、升级顺序及测试见[部署说明](REPRODUCTION_AND_VALIDATION.md)。本次只替换repro控制器，arm旧版未动。
2. APK显示说明仍写死redroid13arm。改为“当前容器”，升级versionCode4/versionName1.2.1，用相同签名覆盖安装，仅部署repro，不清配置或重新选包。

配置测试覆盖缺文件、错误所有者/权限、符号链接、非法/额外行、NUL与超长；同一读取器接受arm和repro两套合法数据。真实repro控制器用arm地址执行proxy boot/display save均拒绝为INSTANCE_ADDRESS_MISMATCH；前后规则结构与显示/代理配置哈希不变。最初直接比较iptables-save输出受动态计数器/时间戳影响，改比较不含它们的规则结构，未修改实际规则来让检查通过。

## 实际验收

| 项目 | repro的实际结果 |
|---|---|
| 首次进阶冷启动 | 640×1136/DPI280/50Hz，SurfaceFlinger模式一致；boot_completed=1 |
| Magisk/终端 | modules_ready=1、terminal_ready=1；真实Droidspaces enter root交互PTY、uid0、/dev/pts正常 |
| Next | 1.5.3，inject_state=1、modules_with_issue=0；真实system_server与两个普通探针应用pre/post |
| 身份/FD负例 | 3类伪造身份未提升、attr过渡EPERM；独立SCM_RIGHTS实际收发通过 |
| LSPosed | 2.2.1/API102，daemonRunning/systemServerRequested=true、safeMode=false；模块自带manager UI显示激活、Dex包装支持 |
| 普通应用 | 私有存储、SharedMemory4096/读42、默认TLS HTTPS200；GPU_DRAW120、AHB128 |
| 代理 | SOCKS5与HTTP TCP握手/HTTPS；普通UID+inet3003经broker；最终HTTPS200/TLS_AES_128_GCM_SHA256 |
| UDP | SOCKS5，同一socket到1.1.1.1和8.8.8.8的UDP53，各自源地址/事务ID/应答正确 |
| DNS | SOCKS5、HTTP均经代理TCP上游；同时向私网示例地址和公网DNS查询，回复匹配 |
| 故障 | 仅目标relay停而规则保留，UDP查询超时；恢复成功，不主动fallback |
| 前端 | 独立应用列表601/1136px、滚动改变可见项；实际保存显示与代理；认证字段显示/密码掩码 |
| 无效设置 | 奇数宽721及端口70000被拒；原配置/revision不变 |
| 第二次冷启动 | 恢复720×1280/DPI320/60Hz，pending=false；代理revision4及brokerrevision2自动恢复，临时回调探针删除 |
| 权限 | init36项能力，CapEff/CapBnd=000001deffecffff；Seccomp2/filter1；relay UID2000、CapEff0、NoNewPrivs1；/dev仍noexec |
| 原版投屏 | stock5.0.1 server，默认编码参数选c2.qti.avc.encoder，VAAPI解码，720×1280 |
| 像素 | 同源中央60% RGB MAE=1.342922；原图std约[44.15,49.75,0]，解码[44.16,49.76,1.20] |
| 通用控制器 | Werror编译；配置负例/错误目标拒绝；正确配置status及代理boot/test通过，APK能读取 |

交互PTY首次用无TTY的管道过早输入导致超时；改为真正分配PTY、先收到提示符再输入，通过并正常exit。这是测试输入方式修正，未为该超时扩大设备权限。

第二次冷启动先验证原复现控制器的自动恢复，之后热更新为每实例配置的通用控制器，并显式执行proxyctl boot/test通过。**通用控制器替换后没有再做第三次冷启动**；热更新结果与此前冷启动持久性分别记，不混为同一次测试。

## 结束状态和边界

repro保持运行，管理器1.2.1；Next1.5.3+LSPosed2.2.1开启、内置Zygisk关闭、第三方Xposed模块0。显示720×1280/DPI320/60Hz；统一SOCKS5 TCP开启，UDP/DNS/认证关闭，relay能力归零；broker名单原样保留。脚本应用和NFC维持此前disabled-user，未运行游戏/自动流程。

无需repro定制连接脚本：显式选择自己的ADB serial，清除旧SCRCPY_SERVER_PATH后运行原版scrcpy；电脑硬解再加--hwdec=vaapi。最终留下独立repro窗口，未关闭arm或真机窗口。QA视频/图片使用临时目录后删除，只移除本轮明确命名的设备暂存文件，不清扫历史资料。

普通应用网络仍依赖UID broker。未复测x86/32位ABI、真实认证账号、第三方Xposed业务hook、所有视频格式/音频、全新空data的本补充组合、空源码全编译和一小时耐久。认证模拟正反例沿用上一报告，本次实际服务器为无认证模式。真机全局Enforcing和部分capability收紧不等于真正userns非特权隔离。
