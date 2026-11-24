# 复用编译产物，在同一真机建立独立容器

> **历史独立部署指南。** 本页记录第一部分2026-11-24的实际步骤，不构成现在操作redroid13repro的授权；它当前冻结。arm后续补充功能另见[第二部分](PART2_SUPPLEMENT.md)，未在新空数据目标重新重放整套补充。

2026-11-24 实际完成一次：原容器保持运行，新增 `redroid13repro`，重建空白 `/data`，重新安装测试 APK。完整结果见 [复现验收](REPRODUCTION_20261124.md)。

这是**已有编译产物的部署复现**。它验证配置、公开小工具、二进制补丁、首次启动和冷启动，不替代从空源码环境重建 AOSP / Mesa / 厂商 Codec 的验证。缺失来源仍见 [重建边界](REPRODUCIBILITY_GAPS.md)。

## 输入与隔离

需要合法取得且版本匹配的：原始 ext4 镜像、已验证的 `system/` 和 `vendor/` 产物、`init.droidspaces`、WebView、Magisk 材料及测试 APK。不要复制正在使用的 `/data`、账号或 root 授权数据库。

本次原始 6 GiB 镜像 SHA256：

```text
ab65cb2dbc97e56829dcdce0f05a5af31a2152977194797405a6ac3a98edcc6f
```

该备份含约 912 KiB 的首次启动目录/状态，并非严格空白数据镜像。本次只在**新建且离线挂载的副本**中清空 `/data`，再安装新配置。原始备份没有改动。输入和部署结果的关键哈希在 [产物记录](../artifacts/reproduction-20261124.json)。

选择不冲突的新名称、镜像路径、Binder 三节点、NAT 地址和转发端口。本次：

| 项目 | 新目标 |
|---|---|
| 名称 | redroid13repro |
| 镜像 | /data/local/Droidspaces/Containers/redroid13repro/rootfs-repro.img |
| 大小 | 12 GiB |
| IP / 网关 | 172.28.41.37/16 / 172.28.0.1 |
| 转发 | 15614:5555/tcp |
| Debian ADB | 127.0.0.1:15614 |
| BinderFS | r13repro-binder / r13repro-hwbinder / r13repro-vndbinder |
| 自启动 | run_at_boot=0 |

使用 [配置参考](../config/container.example.conf) 替换所有目标字段及真实设备路径。不要让两个 Android 容器共用 Binder 节点。节点必须通过 `BINDER_CTL_ADD` 创建；本次使用 [binderfs-add.c](../src/probes/binderfs-add.c)，内核返回 major/minor 后才使用。重复名称会失败，不删除其他节点来重试。

## 离线装配顺序

以下涉及宿主 root，必须逐项核实目标，不能整段套用到活动镜像。

1. 在工作站保留原始镜像，校验 SHA256。上传到 `/data/local/tmp` 暂存，等待 `adb push` **真正 exit 0**，然后在手机检查完整大小和哈希。传输进度显示“pushed”时，文件仍可能尚未写完。
2. 用宿主 root 把暂存文件移入**不存在的新目标路径**。不要为了直接上传而放宽 Droidspaces 父目录权限。所有者 root:root，模式 0644。
3. 对新镜像执行 `e2fsck -f -p`。退出码 0 是无错误，1 是已经修复；仅允许这两种继续。扩展文件到 12884901888 字节，再执行 `resize2fs` 和 `e2fsck -f -n`。
4. 本机上传文件带 `shell_data_file` 标签，用户态能读取而 loop 设备报 I/O error。仅对**新镜像文件**设置与既有可用镜像一致的 `u:object_r:vold_data_file:s0` 后恢复。不改全局 SELinux，不更改其他文件标签；其他 ROM 要按实际策略判断。
5. Android Toybox loop 工具会拒绝解析后超过 64 字节的镜像路径。普通 symlink 无效，因为它解析真实路径。本次在同一文件系统为新镜像创建短路径**硬链接** `/data/local/tmp/r13repro.img`，通过短路径离线挂载。硬链接共用一个 inode，不额外占用 12 GiB。Droidspaces 的 C ioctl 挂载路径不受该 Toybox 字符串检查约束。
6. 确认挂载点为空、未被其他容器使用，并核实挂载设备与新镜像对应。在该新 ext4 的 `data/` 内清除原始备份的初始状态，确认空目录。此处绝不能误用宿主 `/data` 或原容器挂载点。
7. 只复制已验证产物的 `system/.`、`vendor/.` 和 `init.droidspaces`；不复制原容器数据。重放本仓库 `config/init`、`config/bin`、`config/media`、`config/vintf` 的匹配配置。不要用不完整的文件集合覆盖其他版本的系统。
8. 部署 `scripts/build-native.sh` 生成的 broker、netctl、socket preload、Magisk guard 和 Surface helper。控制器 `arm13-netctl` 为 root:root 0700；服务可执行文件 0755，库 0644。本次以公开源码重新生成的小工具覆盖了复用目录中的对应文件。
9. 对精确输入哈希的 Gralloc 原库运行 `scripts/patch-gralloc-unlock.py`，输出到新文件，再部署到新镜像。本次重放所得 SHA256 为 `7ba792e5becada349ca60216468b9b444c99c531ce7915db0a387e7aefb10337`。
10. 创建 `/data/misc/redroid-netbroker/packages`，格式如下，使用包名，不能沿用旧 UID。目录 0755、文件 root:root 0644。未安装或没有 INTERNET 权限的包保留选择但不获得运行 UID。

```text
R13NET1
1
1
lab.arm13.probe
mark.via
com.mmbox.xbrowser
```

11. 同步并卸载离线挂载点。只释放**本轮创建且确认未挂载、backing_file 指向新镜像**的残留 loop；绝不使用全局 detach。短路径硬链接使用完可移除，新目标镜像链接保留。
12. 写入新容器配置。首次启动前记录原容器和其他保护对象的 PID/starttime、宿主 HTTPS、Wi-Fi ADB。内存上限必须读取实际 cgroup 文件验证，不能只看配置文本。本机使用 v1 memory cgroup 实际设置 2.5 GiB，CPU 配额被运行时跳过。

## 首次启动

示例只操作新目标：

```bash
adb -s "$HOST_ADB" shell 'su -c "/data/local/Droidspaces/bin/droidspaces --name=redroid13repro -C /data/local/Droidspaces/Containers/redroid13repro/container.config start"'
adb -s "$HOST_ADB" forward --no-rebind tcp:15614 tcp:15614
adb connect 127.0.0.1:15614
```

空白数据下 adbd 可能运行但没有 TCP 端口。先通过 Droidspaces 的**目标 run 命令**进入内层 shell，设置 `persist.adb.tcp.port=5555`，首次还设置 `service.adb.tcp.port=5555`，然后仅重启内层 adbd。持久属性在本次冷启动后有效。不要改宿主 adbd。

安装新 APK，不迁移原应用私有目录。先验证 `/system/xbin/su.aosp 0 id`，安装 Magisk 管理器，再对当前实际 UID 逐个授权。测试中仅授权内层 shell、网络管理器和用户指定的脚本应用；没有复制旧授权库，也没有设置所有应用默认 root。

[网络管理器](NETWORK.md) 的前端启动 vendor 控制器前必须清除继承的 `LD_PRELOAD`。2026-11-24 的修复已写入 MainActivity；只需重建这个小 APK，不能用放开 vendor linker namespace 代替。

按用户需求关闭**内层**蓝牙并保存 `bluetooth_on=0`。本次首次启动默认开启时触发模拟 HAL 崩溃，关闭后不再继续；没有改宿主蓝牙。

默认 Google 探测站在本线路超时。本次内层使用真实可达的 HTTP 和 HTTPS `connect.rom.miui.com/generate_204`，保留探测模式与默认 TLS 验证，冷启动后获得 VALIDATED。探测服务的可达性随线路变化，不能只复制 URL 就宣称成功。

## 验收与冷启动

运行 [验证步骤](VALIDATION.md)。至少覆盖：真实普通 UID HTTPS、WebView 多进程、浏览器页面与输入、GPU 绘制/AHB、Gralloc 跨线程、QTI Surface 码流与解码颜色、真实 scrcpy 窗口控制。

`SurfaceCodecProbe` 当前输出路径在 GPU 测试包的 `files/` 下。全新安装后该目录可能还未创建；应先以对应应用 UID 创建，或在受控测试中创建并设置正确所有者。不能把输出目录缺失误判成编码器失败。

冷启动仅停止新容器，确认镜像卸载，执行只读文件系统检查，再启动。ADB 若保留旧 offline 连接，只针对新地址执行 `adb disconnect 127.0.0.1:15614` 和 connect；不重置整个 ADB server。

复测代理配置哈希、实际 UID、root、DHCP、蓝牙关闭、VALIDATED、GPU、浏览器和硬编。同步核对保护对象 PID/starttime、宿主 HTTPS 与 Wi-Fi ADB。长期测试、游戏账号和自动游戏操作不在本轮范围。

## Magisk 模块后续集成

上面的首次部署报告记录的是当时的root-only Magisk。2026-11-24后续已在新目标接入基础模块流程，需成套部署新版arm13-magisk-start、arm13-magisk-stage、redroid-magisk.rc，以及build-native.sh生成的guard与arm13-magisk-module-mounts。仅创建/data/adb/modules不能替代完整集成。构建、现场验收、挂载隔离与回退见[模块验收](MAGISK_MODULES_20261124.md)。
