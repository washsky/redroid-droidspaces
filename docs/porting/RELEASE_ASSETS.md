# ARM64 基础原件与跨设备复现入口

版本：[v2026.11.28-arm64-base](https://github.com/washsky/redroid-droidspaces/releases/tag/v2026.11.28-arm64-base)。这是本项目在Ubuntu20参考redroid-pve补丁编译/打包的ARM64原件，不是上游官方ARM模板。当前SM8475硬件集成镜像另外提供，两类输入分别保存。

先检出本tag，再运行：

```bash
python3 scripts/download-prebuilt.py --profile base
# 只下载小附件：追加 --tools-only
# 历史opt-in原件：追加 --image optin；两个原件都要：--image all
```

| 文件 | 内容 |
|---|---|
| `redroid13-arm64-pve-source-original-20261119.img.zst` | 6GiB源码根文件系统原件，默认的软件阶段起点；缺少property opt-in标记，后续修复须在工作副本集成 |
| `redroid13-arm64-pve-property-optin-original-20261119.img.zst` | 6GiB历史opt-in原件，有早期模拟器初始化状态，用于对照，不是空数据Release |
| `base-images.json` | 原名、完整镜像SHA256、平台信息、已知差异、journal审计和验收边界 |
| `source-original-*` / `property-optin-original-*` | 两份原件分别对应的逐文件哈希、元数据、xattr和只读检查结果 |
| `redroid13-arm64-portable-core-fixes-20261128.tar.zst` | 9个已核对文件：Bionic/linker配套转换、runtime及DHCP修复；源码参考、旧/新哈希与适用条件，不自动安装 |
| `redroid13-arm64-tools-20261128.tar.zst` | ARM64/API33源码helper、BinderFS与稀疏传输工具、探针；不含QTI私有库 |
| `ARM13-API-Probe-20261128.apk` | 普通UID存储/SharedMemory/DNS/默认TLS HTTPS探针；源码在本tag |
| `BASE_README.md` / `THIRD-PARTY-NOTICES.txt` / `SHA256SUMS` | 本说明、来源许可证与压缩附件校验 |

所有下载器实际选中的文件均逐个校验。默认不会下载未选中的另一个大镜像；若要执行整份`sha256sum -c SHA256SUMS`，需先用`--image all`取得所有附件。解压后的SHA256查看base-images.json，不与压缩附件哈希混淆。

**复现顺序：先软件系统、普通APK、存储/SharedMemory、网络、软件画面及冷启动，再GPU/Gralloc，最后硬件Codec和可选第二部分。** 新设备移植教程独立放在docs/porting，正式维护交接不混入这些操作步骤。

永久文档入口：

- [软件优先复现](https://github.com/washsky/redroid-droidspaces/blob/v2026.11.28-arm64-base/docs/porting/SOFTWARE_FIRST.md)
- [GPU与Codec阶段](https://github.com/washsky/redroid-droidspaces/blob/v2026.11.28-arm64-base/docs/porting/GPU_AND_CODEC.md)
- [原件来源、旧版差异、初始数据及journal](https://github.com/washsky/redroid-droidspaces/blob/v2026.11.28-arm64-base/docs/porting/BASE_IMAGE_PROVENANCE.md)

两份原件保留待回放ext4 journal；不要仅凭只读fsck返回0就认定完全干净。保留下载原件，在新副本上回放/修复并集成必要核心修复；不要执行旧`/.lab-start.sh`或覆盖宿主运行时。完整机型验收没有外推给这些原件或其它处理器，本轮发布未启动任何手机容器。
