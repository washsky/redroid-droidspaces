# 2026-11-24 真机独立容器复现验收

> **历史验收快照。** 本页数据不随arm后续变化改写；redroid13repro现已冻结。Magisk/Next/LSPosed和stock scrcpy等后续arm成果见[第二部分](PART2_SUPPLEMENT.md)。

结论：在原 `redroid13arm` 与 `arch2` 继续运行的条件下，新增 `redroid13repro`，完成空数据部署、首次启动、一次完整停止/卸载/启动，以及核心功能复测。不是仅克隆现用数据，也不是从零编译验收。

基线仓库提交：`e7cd34f0e6098a03c116cf7e00673556cc4483fb`。Debian 上 Git clone 出现 GnuTLS 接收错误，改用已发布的本地文件副本；110 个文件的 Git blob SHA 与 GitHub 当前树全部一致。因此文件内容核对通过，不能称这次 Git clone 传输已通过。

操作指南见 [复用产物复现](PREBUILT_REPRODUCTION.md)，[关键产物哈希](../artifacts/reproduction-20261124.json) 保留输入边界。

## 本次重放了什么

- 原始 6 GiB 镜像校验后建立新副本，扩到 12 GiB。
- 只复用已编译的 system/vendor、init 与版本匹配的厂商依赖；新镜像 data 清空。没有复制账号、应用数据或 Magisk 策略库。
- 新 BinderFS 三节点，由 BINDER_CTL_ADD 返回实际设备号。
- 独立 NAT 地址 172.28.41.37、转发端口 15614；run_at_boot=0。
- 仓库 init/XML/服务配置、公开源码构建的 native helper 和网络管理 APK。
- 使用仓库脚本从原库重新生成 Gralloc 修补库，结果哈希一致。
- AOSP、Mesa、Codec 私有兼容库未重新编译，历史 wrapper 的来源缺口仍然存在。

## 实际结果

| 项目 | 首次部署 | 冷启动复测 |
|---|---|---|
| boot_completed / DHCP | 1 / 172.28.41.37 | 自动恢复 |
| system_server / Zygote / netd | 476 / 300 / 299 | 468 / 299 / 298；复测期间保持 |
| 宿主侧新 init PID | 16653 | 28577 |
| APK 新安装、私有文件、SharedMemory | 通过；4096字节、只读保护、read=42 | 再次通过 |
| 普通应用 HTTPS | HTTP200、128字节、TLS_AES_128_GCM_SHA256 | 再次通过 |
| UID broker | 公共源码产物运行；包名动态解析 | revision=2、四个联网 UID 自动恢复 |
| 配置持久化 | SHA256 9b9ef72e4b3a37be2f1eb1325f109de8f399a21c21f8c3c4d2017587984264d9 | 哈希相同 |
| 网络 VALIDATED | 更换真实探测地址后等待重启读取 | 默认 Ethernet NetworkAgentInfo 明确含 VALIDATED |
| WebView157 多进程 | RELRO完成；实际 renderer 子进程 | 保持启用，Via真实页面 |
| GPU | Turnip/KGSL、ANGLE；GPU_DRAW120帧、AHB128帧 | GPU_DRAW=PASS、APP_AHB_IMPORT_RELEASE=PASS |
| Gralloc 0x22/usage0 | support=0；第二线程正常分配 | CROSS_THREAD_PASS |
| QTI H264 Surface | 30帧/EOS；解码首尾 RGB 对照通过 | 再次 SURFACE_PASS 30帧/EOS |
| scrcpy | QTI encoder、自定义线性 Surface、真实窗口与输入 | 新进程重新连接并显示浏览器 |
| Via / X 浏览器 | WebGL2、动态画面；滚动/点击；X文字输入 | Via HTTP/WebGL 与真实 HTTPS 页面 |
| Root / 管理前端 | Magisk su；修复后实际保存新增选择；服务重启 | su与管理界面正常、选择恢复 |
| 内层蓝牙 | 关闭后 OFF | 自动保持 OFF |
| 保护对象 | 原容器与 arch2 均保留 | PID与starttime一致；宿主HTTPS200、Wi-Fi ADB正常 |

GPU 驱动字符串把芯片标为 Adreno725，不能据此改写硬件型号；实机 SM8475 / Adreno730 已在此前核实。

Surface 编码得到 320x240 H264、30 帧；首尾输入 RGB 为 [0,77,179] / [255,77,179]，解码结果 [0,76,179] / [255,76,178]。这比只检查文件生成或 codec 名称更严格，但不代表所有格式、分辨率或应用均通过。

代理名单保存了 `lab.arm13.gpuprobe`，该包没有 INTERNET 权限，因此不生成对应运行 UID；其他四个实际联网 UID 为 10077、10079、10080、10082。这是权限过滤正常生效，不是单 UID 限制。UID 只代表本次安装分配，不能跨安装照抄。

## 本轮发现并修复的仓库缺陷

网络管理 APK 从 Zygote 继承 `LD_PRELOAD=/system/lib64/libarm13_socketbroker.so`。调用动态链接的 `/vendor/bin/arm13-netctl` 时，vendor 默认命名空间拒绝加载这个 system 库，界面显示“无法连接管理接口”。

修复：仅在前端的控制命令 ProcessBuilder 环境移除 LD_PRELOAD；应用自己的网络代理保留。没有扩大 linker namespace。使用仓库脚本在 Ubuntu 重新生成小 APK，保留签名安装要求，在新容器更新安装后验证状态、搜索、勾选、保存 revision=2、服务重启及冷启动恢复。原工作容器没有部署这个更新。

## 部署中发现的其他细节

- adb push 到受保护的容器目录失败，应暂存后由 root 移动。
- push 的进度行不等于进程已成功退出；完整文件大小和哈希是必要检查。
- e2fsck 自动修复返回1，旧 set-e 装配脚本误当失败；后续流程明确接受0/1。
- loop挂载的64字节路径检查和镜像SELinux标签需要分别处理，详见复现指南。
- 配置请求CPU限额时运行时明确跳过；内存限额实际在 /dev/memcg/droidspaces/redroid13repro 生效，为2684354560字节，验收期failcnt=0。本轮另有只针对新目标的低内存监测。
- 空数据初次启动需设置内层持久ADB TCP端口；冷启动后只需针对新地址重新建立离线连接。

## 保留的问题与验收边界

- 网络仍是 UID socket broker 共享2000身份；没有修复宿主eBPF/容器原生UID隔离。
- 首次启动蓝牙默认开启曾使内层模拟HAL崩溃；关闭后及冷启动后未继续发生，不需要宿主蓝牙。
- 冷启动早期 keystore2 有两次 libandroidicu.so 不可用的链接失败；APEX ready 后服务自动恢复，验收时 running/PID550。没有把启动crash buffer描述为空，也没有在本轮修改该时序。
- 脚本应用全新安装的 root/权限/业务配置需重新授权；本轮不复制旧数据、不登录、不启动自动游戏流程。初始页面可显示；对新UID单独授权后观察到 root 的 com.awcocxb:shell / :remote 进程。未启动自动游戏，完整脚本采集与游戏业务不计入本轮通过项。
- 没有重测 HEVC/全部 Buffer codec 项目、硬件解码端统计或一小时耐久；它们不能从本次H264结论外推。
- full privileges、应用数据隔离关闭、宿主设备暴露及socket身份共享的安全限制保持不变。
- 没有发布厂商二进制、完整镜像、WebView/Magisk APK、签名私钥或授权数据库。

## 证据

文本见 [evidence/2026-11-24-reproduction](../evidence/2026-11-24-reproduction)；仅收集本轮测试输出与脱敏保护检查。截图为实际 scrcpy 窗口：

- [冷启动 Via WebGL](../evidence/2026-11-24-reproduction/cold-via.png)
- [X浏览器点击与输入](../evidence/2026-11-24-reproduction/x-input.png)
- [冷启动管理器](../evidence/2026-11-24-reproduction/manager-cold.png)

原容器保护基线：redroid13arm PID24174/starttime6353308，arch2 PID19643/starttime135633。本轮未停止或重启它们。
