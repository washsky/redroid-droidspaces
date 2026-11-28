# 下载已编译镜像与工具

版本：[v2026.11.28-arm64](https://github.com/washsky/redroid-droidspaces/releases/tag/v2026.11.28-arm64)。这是本机经过 redroid-pve 参考补丁编译、再集成本仓库修复的 **ARM64 ReDroid13 完整文件系统快照**。核心仍是 Droidspaces 运行 ReDroid；Next/LSPosed 属于可选的第二部分。

## 选哪个文件

| 附件 | 内容 |
|---|---|
| `redroid13-arm64-sm8475-20261128.img.zst` | 12 GiB 逻辑大小的全新 ext4；空 `/data`；包括当前 ROM 的图形/Codec 库、Mesa、Bootless Magisk 模块环境、终端兼容、显示控制及 stock scrcpy 默认 QTI 编码适配 |
| `redroid13-arm64-tools-20261128.tar.zst` | ARM64/API33 预编译 runtime helpers、库、BinderFS 创建器、gzip 稀疏写入器、验证探针、配置参考、源码映射与逐文件哈希；不自动安装 |
| `UID-Network-Manager-1.2.1.apk` | 镜像已经预装的管理前端；独立附件便于更新匹配环境 |
| `ARM13-API-Probe-20261128.apk` | 可从本仓库源码重建的普通 UID 私有存储、SharedMemory、DNS、默认 TLS HTTPS 探针 |
| `Magisk-31.1-validated.apk` | 与本次镜像匹配的历史unofficial管理APK；精确对应源码仍有缺口，见THIRD_PARTY；不用于修补宿主boot |
| `image-manifest.json` | 解压后镜像大小、SHA256、平台和装配来源 |
| `rootfs-files.sha256` / `rootfs-metadata.json` / `rootfs-xattrs.tsv` | 首次启动前的文件内容、权限/链接及扩展属性清单 |
| `optional-inputs.json` | Next/LSPosed 的固定版本、下载来源与精确输入哈希；不含模块 license |
| `SHA256SUMS`、`THIRD-PARTY-NOTICES.txt`、`AI_QUICKSTART.md` | 附件校验、来源/分发说明、给下一位 AI 的直接入口 |

镜像中没有原容器的应用数据、账号、ADB 密钥、Magisk 授权数据库、第三方模块或模块 license。用于验收的副本会产生这些状态，但不替换待下载的原始镜像。

## 适用前提

- 验证设备为 **mondrian / SM8475 / Adreno 730**，宿主 **OS3.0.5.0.VMNCNXM / Android15(API35)**；内层 **Android13(API33)、arm64-only**。
- 宿主已经具备可用的 **Droidspaces v7.2.0、相应内核/容器策略、ashmem、KGSL、dma_heap 和视频设备**。镜像不提供宿主内核，也不代替这些前提。
- 设备专用库已经包含在完整镜像中，但不能据此推定其它 SoC/ROM 兼容。`/dev/video32`、`/dev/video33` 是此 ROM 的已验证编号。
- 参考配置是 `nocaps + allow_sandboxing=1 + /init.arm13-caps`。helper 删除五项能力，仍保留36项；没有 root UID 映射。保持现有 mask/seccomp，宿主全局 SELinux 保持 Enforcing。
- **普通应用联网仍依赖 UID broker**。默认空应用名单，代理关闭；需要在管理器中选择应用。此发布不代表原生 UID 网络隔离已经修复。

硬件条件不匹配时，先读 [FULL_PROCESS.md](FULL_PROCESS.md)、[GRAPHICS.md](GRAPHICS.md) 和 [ROOT_AND_SECURITY.md](ROOT_AND_SECURITY.md)，不要通过全局关闭 SELinux、BPF 或防火墙来套用配置。

## 下载和校验

推荐无需 GitHub 登录的下载器：

```bash
python3 scripts/download-prebuilt.py --directory out/prebuilt-20261128
# 只取工具等小附件，跳过镜像：加 --tools-only
```

也可以使用已登录的 gh：

```bash
mkdir -p out/prebuilt-20261128
gh release download v2026.11.28-arm64 --repo washsky/redroid-droidspaces --dir out/prebuilt-20261128
(cd out/prebuilt-20261128 && sha256sum -c SHA256SUMS)
zstd -d --sparse out/prebuilt-20261128/redroid13-arm64-sm8475-20261128.img.zst -o out/prebuilt-20261128/rootfs.img
tar --zstd -xf out/prebuilt-20261128/redroid13-arm64-tools-20261128.tar.zst -C out/prebuilt-20261128
```

`SHA256SUMS` 校验压缩附件；`image-manifest.json` 的 `image_sha256` 校验解压后的 12884901888 字节镜像。两者不是同一个哈希。已有文件不匹配时下载器拒绝覆盖。

## 建立独立容器

先由操作者确认目标。不要覆盖活动镜像、旧配置或授权数据；不要让两个 Android 容器共用 Binder 三节点。

1. 给新实例保留独立的名称、目录、NAT IPv4 和转发端口。`config/container.release-sm8475.example.conf` 使用 `redroid13release`、示例 `172.28.41.40`、`15615:5555`；必须核实未被占用并替换 `REPLACE_*`。
2. 用工具包的 `bin/arm13-binderfs-add` 对实际 BinderFS control 执行 `BINDER_CTL_ADD`，一次创建三个全新名称。它输出真实 major/minor；不要猜设备号。只将新建三节点设为应用可打开的权限，不删除其它节点。
3. 核实真实 ashmem/KGSL/dma_heap/视频节点，填写配置。保持 `enable_hw_access=0`，使用明确的设备绑定，不暴露整棵宿主 `/dev`。
4. 先把镜像上传到自己的暂存位置，再由 root 放入新目录；不放宽 Droidspaces 父目录权限。这个 ROM 的镜像文件需要 `u:object_r:vold_data_file:s0` 标签，只对新镜像设置。
5. 复制配置到新实例的 `container.config`，检查它的 `rootfs_path` 指向新镜像；保持 `run_at_boot=0`。

一种保留稀疏洞、避免把12 GiB零块全部落盘的传输方式如下。`HOST_ADB` 必须先设置为现场核实的真机 serial；下面的新路径必须不存在：

```bash
adb -s "$HOST_ADB" push out/prebuilt-20261128/arm64-tools/bin/arm13-gzip-to-sparse /data/local/tmp/arm13-gzip-to-sparse
adb -s "$HOST_ADB" shell 'su -c "chmod 0755 /data/local/tmp/arm13-gzip-to-sparse; mkdir -p /data/local/Droidspaces/Containers/redroid13release"'
gzip -1 -c out/prebuilt-20261128/rootfs.img | adb -s "$HOST_ADB" shell -T "su -c '/data/local/tmp/arm13-gzip-to-sparse /data/local/Droidspaces/Containers/redroid13release/rootfs.img 12884901888'"
adb -s "$HOST_ADB" shell 'su -c "chcon u:object_r:vold_data_file:s0 /data/local/Droidspaces/Containers/redroid13release/rootfs.img; sha256sum /data/local/Droidspaces/Containers/redroid13release/rootfs.img"'
```

写入器检查 gzip CRC、解压后精确大小，使用 `O_EXCL` 拒绝覆盖已有文件，失败时删除自己创建的不完整输出。仍须等传输真正退出成功，再核对镜像 SHA256。也可以常规 `adb push` 原始镜像，但可能占用完整12 GiB物理空间。

## 首次启动与前端

在确认新配置及授权后：

```bash
adb -s "$HOST_ADB" shell 'su -c "/data/local/Droidspaces/bin/droidspaces -n redroid13release -C /data/local/Droidspaces/Containers/redroid13release/container.config start"'
adb -s "$HOST_ADB" forward --no-rebind tcp:15615 tcp:15615
adb connect 127.0.0.1:15615
```

新镜像会在**容器内**启用 adbd TCP5555，并在启动完成后：

- 从实际 eth0 IPv4 生成 root:root 0600 的 `/data/adb/arm13-instance.conf`，标签形如 `redroid13-172-28-41-40`；标签不是 Droidspaces UUID。
- 建立终端 `/bin/su` 兼容布局，启动空名单 broker，保留默认关闭的代理。
- 关闭不支持的虚拟蓝牙；首次启动早期可能仍出现一次虚拟蓝牙 HAL 错误，不代表宿主蓝牙被操作。
- 设置 `sys.arm13.release.ready=1`；Magisk 分别报告 `modules_ready` 和 `terminal_ready`。

```bash
adb -s 127.0.0.1:15615 shell 'getprop sys.boot_completed; getprop sys.arm13.release.ready; getprop sys.arm13.magisk.modules_ready; getprop sys.arm13.magisk.terminal_ready'
adb -s 127.0.0.1:15615 install out/prebuilt-20261128/Magisk-31.1-validated.apk
```

镜像已经预装 `lab.arm13.netmanager`。首次需在**容器内**给它授予 Magisk root，再选择联网应用；不要沿用另一实例的 UID 或授权数据库。命令行管理员可通过保留的 `/system/xbin/su.aosp 0` 引导，并在查询实际管理器 UID 后只授权该 UID。没有给所有应用默认 root。

历史Magisk管理器会显示unofficial警告，这是实际输入身份；当前上游同名v31.1的APK及签名均不同。不要无验证地替换镜像中的native文件。完整哈希和来源差异见[THIRD_PARTY.md](../THIRD_PARTY.md)。

实例配置只在缺失时生成；已有配置不被静默覆盖。NAT地址变化后，如控制器返回 `INSTANCE_ADDRESS_MISMATCH`，应由root更新本实例文件并保持0600。若首次 `release.ready` 未就绪，先检查 eth0 是否取得IPv4，再从容器root运行 `/vendor/bin/arm13-release-setup`，保留真实报错。

显示默认720×1280、DPI320、60Hz；前端保存的是下次启动参数。代理设置、UDP/DNS及认证入口均在管理器内，默认关闭。Magisk管理器中不要把“安装/修补boot”当作容器初始化步骤。

## 可选的第二部分

镜像提供Magisk模块生命周期和Next所需兼容层，但**不预装第三方模块**。按 `optional-inputs.json` 取得精确的 Next1.5.3(880) 和 LSPosed2.2.1(7912) 原包；内置Zygisk关闭，在Magisk中安装后只冷启动获准的目标。

固定版本的身份、SCM_RIGHTS、noexec及早期注入适配见 [MAGISK_ZYGISK_LSPOSED.md](part2/MAGISK_ZYGISK_LSPOSED.md)。安装退出0之后还要验证 `zygiskd status` 的注入状态及 `lspctl --json status` 的system_server接入；不伪造license，也不拷贝他人授权。模块安装中的“Unable to find preinit dir”不代表宿主policy已加载。

stock scrcpy 无需定制server或额外linear codec参数：

```bash
scrcpy -s 127.0.0.1:15615 --hwdec=vaapi
```

`--hwdec=vaapi` 是Debian电脑端解码选择；手机端QTI编码默认值已在镜像内适配。其它电脑应选自己支持的解码后端。

## 构建来源与验收范围

核心 AOSP/PVE/Mesa 输入沿用仓库的 pinned manifest、补丁和已观察哈希；这次重建的是**干净分发文件系统**，没有重新从空环境全编译AOSP。镜像保留历史已编译兼容库，完整源码仍未全部找齐，见 [REPRODUCIBILITY_GAPS.md](REPRODUCIBILITY_GAPS.md)。

镜像内profile的历史字段`aosp_source_base_commit=3b7c915...`实际指本次装配开始时的**本仓库提交**，不是AOSP项目的commit；AOSP各项目版本以`aosp-manifest-pinned.xml`为准。工具包是新编译的源码helper，不代表每个字节均与镜像内历史helper相同；逐项对照工具`manifest.json`和`rootfs-files.sha256`，不要为了统一哈希热替换活动库。

`scripts/build-release-image.py` 只在自己的私有mount namespace中只读挂载输入、新建ext4，复制静态文件并恢复xattr；`scripts/inspect-release-image.py` 校验空数据目录、核心哈希并输出清单。Android的tar诊断输出必须与二进制流分离，不能仅凭进程退出0就当作有效归档；SELinux标签以外的扩展属性也应保留。

实际首次启动、目标冷启动、模块、普通应用、PTY、默认QTI/VAAPI和保护对象复核结果见 [release-20261128.json](../artifacts/release-20261128.json)。没有验收其它ROM/x86、第三方Xposed业务hook、游戏自动流程或一小时耐久。
