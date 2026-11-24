# 第二部分：部署清单、复现与回退

前提：[核心复现已成功](../FULL_PROCESS.md)，并读过[补充概览](../PART2_SUPPLEMENT.md)。这是已有成功基线上的增量集成，不是独立 rootfs 下载包，也不是本轮重新从零编译 AOSP 的报告。

## 1. 操作边界

当前只授权 redroid13arm。redroid13repro 冻结，不拿它做 read-only 检查、部署或“再验证一次”；真机和 arch2 不重启。文档命令中的 INNER_ADB/HOST_ADB 必须指向已经重新核实的允许目标。

原工作站项目已迁移到 android-lab 下；源码和产物使用 lab.env，不依赖个人绝对路径。不要把临时文件或截图写进 ai-lan-gateway。

PID/UID/namespace、Binder节点、设备号和 ADB 状态必须现场查询。冷启动会改变 PID；不要复用历史数值杀进程。避免脚本应用自动执行游戏：本轮在冷启动前仅 force-stop 用户指定的脚本包，未永久更改其业务配置；不点击倒计时/自动运行按钮。

## 2. 构建输入

- 既定 NDK r29，API33 ARM64 编译器。
- Android SDK build-tools（本轮36）和 platform android.jar，或原 AOSP host工具。
- 管理 APK 同一签名身份才能 install -r；本地实验曾用 AOSP testkey。仓库不提交签名私钥；不要把实验 key 当生产密钥。
- 合法取得的 Magisk31.1、Next1.5.3、LSPosed2.2.1与版本匹配的原厂库。
- 外部模块输入 SHA256 和目标 native 哈希见[补充清单](../../artifacts/supplement-20261125.json)，它记录观测，不承诺换工具链按位相同。

```bash
source lab.env
bash scripts/build-native.sh
bash scripts/build-zygisk-probe.sh

# 二选一：AOSP_ROOT，或独立 SDK 两变量。
export ANDROID_BUILD_TOOLS=/path/to/android-build-tools
export ANDROID_JAR=/path/to/platforms/android-36/android.jar
export SIGN_CERT=/path/to/your/certificate.x509.pem
export SIGN_KEY=/path/to/your/signing-key.pk8
bash scripts/build-netmanager-apk.sh

python3 tests/test-proxy-transport.py
python3 tests/test-proxy-auth.py
```

构建只写 out/，不自动部署、不自动连接设备。NDK 的 sysroot 提供 JNI 头文件。管理器当前 versionCode3/versionName1.2、min/targetSDK33。

本地已执行关键 native 的 -Wall -Wextra -Werror 编译、TCP背压/半关闭和认证正反例；SDK javac 的 source/target8 及弃用 API 警告保留，不描述为零警告。

## 3. 成套部署清单

先备份目标候选镜像、将修改的文件、容器配置、原 /bin symlink 和模块目录；授权数据库只留本地。以下是映射，不是可以不核实就整段覆盖的脚本。

| 仓库输入/产物 | 容器目标 | 权限/注意 |
|---|---|---|
| config/bin/arm13-magisk-start、arm13-magisk-stage | /vendor/bin/同名 | root:root 0755 |
| config/init/redroid-magisk.rc | /vendor/etc/init/redroid-magisk.rc | 0644，按现有 init import 合并 |
| out/native/arm13-magisk-daemon-guard | /vendor/bin/arm13-magisk-daemon-guard | 0755 |
| out/native/libmagisk_container_context.so | /system/lib64/libmagisk_container_context.so | 0644 |
| out/native/arm13-magisk-module-mounts | /vendor/bin/arm13-magisk-module-mounts | root:root 0700 |
| config/init/init.zygote64-next.rc | /system/etc/init/hw/init.zygote64.rc | Next分支直接 app_process；勿与旧 wrapper叠用 |
| out/native/arm13-su-login | /vendor/bin/arm13-su-login | 0755 |
| config/bin/arm13-terminal-setup | /vendor/bin/arm13-terminal-setup | 0755，核实原 /bin 后才执行 |
| out/native/arm13-cap-init | /init.arm13-caps | root:root 0755；保留原 /init.droidspaces |
| config/arm13-boot.conf.example | /data/adb/arm13-boot.conf | 首次才作为模板；后续由bootctl原子写0600 |
| out/native/arm13-proxyctl、arm13-proxy-relay、arm13-bootctl | /vendor/bin/同名 | root:root 0755，各自检查调用身份 |
| config/init/arm13-proxy.rc | /vendor/etc/init/arm13-proxy.rc | 0644；post-fs-data目录及boot_completed恢复 |
| out/netmanager/UID-Network-Manager.apk | 包 lab.arm13.netmanager | 保持签名，install -r，不清数据 |
| 精确哈希 QTI 默认值补丁输出 | /vendor/lib64/libqcodec2_v4l2codec.so | 0644；仅容器内，保留原库 |

保留原 broker、netctl、socket preload、包名选择、图形服务和平台库，不以第二部分文件覆盖整棵 system/vendor。已有基本模块阶段仍需原 Magisk 输入目录 /system/etc/arm13-magisk 完整。

对正在执行的 ELF，不在原 inode 上直接写内容。adb push 到目标专用暂存文件，等待真正 exit0，核对大小/哈希，再由 root 在目标目录创建新文件、设置所有者/权限并 rename；保留原文件备份。本轮热替换 helper 后也核实了实际运行版本。root UI 控制调用清除继承的 LD_PRELOAD，不扩大 vendor linker namespace。

## 4. 当前实例绑定不是通用安装器

proxyctl 和 bootctl 明确包含本实验 arm 的 eth0 IPv4 校验；设备名 redroid_arm64_only、helper标记和 netns 也必须匹配。源中 LAN 代理地址只是本机预设，不是公共服务。新实验要审阅并替换**本实例的**常量后重新编译，不能删除全部保护，也不能借此操作冻结 repro。

对已有 arm 容器配置，保留自己的镜像、端口、Binder节点和设备清单，合并：

完整的第二部分参考另存为[container.arm13-part2.example.conf](../../config/container.arm13-part2.example.conf)。第一部分container.example.conf保留原核心基线，避免首次启动被可选cap-init/显示工具依赖阻断。

```ini
privileged=nocaps
allow_sandboxing=1
custom_init=/init.arm13-caps
selinux_permissive=0
enable_hw_access=0
enable_gpu_mode=0
```

本轮内存限制为7GiB、swap关闭，镜像仍12GiB；按设备实际资源设置，不把它当首次启动最低要求。allow_sandboxing 允许内层创建 namespace，不给本容器 root 自动建立 UID 映射。init helper 沿用审计过的平台参数并删除五项 capability，详见[权限边界](SCRCPY_TERMINAL_DISPLAY.md)。

## 5. 安装模块与只冷启动目标

先确认基础模块就绪，关闭内置 Zygisk，再在 Magisk 安装已核验 Next/LSPosed。保留安装输出；解压成功不能替代 native helper执行、license/网络检查或注入成功。

更换 init rc、cap-init 或早期注入组合后，要冷启动**被授权的 arm**。下面只给经过填入真实路径的目标命令，不自动执行：

```bash
# 先重新核实：实际 PID、配置路径、镜像和允许的目标。
adb -s "$HOST_ADB" shell 'su -c "/data/local/Droidspaces/bin/droidspaces --name=redroid13arm pid"'
adb -s "$HOST_ADB" shell 'su -c "/data/local/Droidspaces/bin/droidspaces --name=redroid13arm stop"'
adb -s "$HOST_ADB" shell 'su -c "/data/local/Droidspaces/bin/droidspaces -C /data/local/Droidspaces/Containers/redroid13arm/container.config start"'
```

确认 boot_completed 后，重新核实 PID/UID和连接。只针对自己的 arm 地址恢复 ADB；不执行 adb kill-server 或全局 disconnect。新的 init rc 通常需冷启动重新解析，不能靠 ctl.restart 掩盖未加载的新配置。

本轮只为文档发布做只读复核，不再重启任何容器；运行验收来自前述实际探索和已保留摘要。

## 6. 验证分层

| 层 | 判据 |
|---|---|
| 核心 | boot_completed、DHCP、system_server/zygote/netd、普通APK/HTTPS；不能被补充功能破坏 |
| 模块 | modules_ready=1，CLI/UI安装与冷启动阶段、真实覆盖与禁用卸载 |
| Zygisk | Next状态和真实 system_server/普通应用 pre/post；负例不被提升身份 |
| FD/noexec | 真SCM_RIGHTS收发、载荷可执行映射；/dev仍noexec |
| LSPosed | lspctl daemonRunning/systemServerRequested/safeMode；最后modulecount0 |
| 终端 | 实际 Droidspaces交互PTY和 terminal_ready，不只是非交互id |
| 显示 | bootctl保存/当前值/pending，init --check；改值后的实际冷启动和SurfaceFlinger模式 |
| TCP代理 | 真HTTPS状态/TLS、统计增长；不是只测试LAN端口 |
| UDP/DNS | 同一socket多目标源地址/事务ID；两种代理的DNS；故障后不主动fallback |
| 前端 | 列表滚动、保存、旧revision/非法值拒绝、重开/冷启动恢复 |
| 投屏 | stock server默认参数、QTI编码、VAAPI解码、解码像素与真实窗口 |
| 保护 | 真机bootID与Enforcing、arch2 PID/starttime保持；不查询冻结repro |

普通 UID CLI 网络探针要使用实际 UID 和其 inet supplementary GID3003；本轮首次仅 su UID 的命令因没有 inet 组而无法连接 broker socket，补齐真实应用组后通过。不能因此修改 broker 的授权或将 socket 开放给所有 UID。

认证的模拟正反例与实际无认证服务器分开记录；没有真实账号时不能补写验收。游戏、自动脚本业务、其他架构和一小时耐久不在通过项。

## 7. 回退和结束设置

只恢复目标自己的备份，不跨容器混用：

- 模块：先禁用新增 Next/LSPosed/探针，再恢复成套 Magisk/zygote配置；保留授权和日志。
- scrcpy：容器恢复精确原库可退回显式 linear option 或历史定制方案；客户端环境变量要核实。
- 代理：先在管理器关闭，确认仅自有链/精确iprule/专用路由被清理，再移除服务；不要flush全表。
- 显示：通过前端恢复已知值并 --check；需要时仅冷启动 arm。
- 终端：恢复原 /bin symlink，不删除 /system/bin/su。
- 权限：恢复对应配置/helper组合，不能只换一个标签后宣称更安全。

本轮结束设置：720×1280/DPI320/60Hz；SOCKS5 TCP开启、UDP/DNS/认证关闭；Next1.5.3+LSPosed2.2.1开启；内置Zygisk关闭；探针模块和本轮临时视频/设备暂存文件清理。repro保持冻结。
