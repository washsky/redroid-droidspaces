# 基础镜像来源、原始状态与修复边界

发布：[v2026.11.28-arm64-base](https://github.com/washsky/redroid-droidspaces/releases/tag/v2026.11.28-arm64-base)。本页只记录可下载的历史构建输入；[软件优先路线](SOFTWARE_FIRST.md)负责复现步骤，`docs/HANDOFF.md`继续负责正式维护交接，本次没有向其追加移植教程。

## 两个原件

| 附件 | Ubuntu20保留原名 | 解压后SHA256 | 初始状态 |
|---|---|---|---|
| `redroid13-arm64-pve-source-original-20261119.img.zst` | `rootfs13-arm64-source-ext4.img` | `d5b6961e65832aee875c15766c8ffac6ba275f3f217390853b841977785b326e` | `/data`为空；没有property opt-in标记；新设备从它的工作副本开始 |
| `redroid13-arm64-pve-property-optin-original-20261119.img.zst` | `rootfs13-arm64-source-property-optin-ext4.img` | `ab65cb2dbc97e56829dcdce0f05a5af31a2152977194797405a6ac3a98edcc6f` | 有opt-in标记与早期模拟器初始化状态；供历史对照，不是严格空数据镜像 |

两者均为6442450944字节（6 GiB）ext4。远程原件、SSH传输后解压文件和既有Debian缓存的适用哈希均已核对。仅压缩，不改原始文件内容；逐文件内容、owner/mode/链接和xattr清单随附件提供。

Ubuntu20另保留11-21的DHCP中间版，哈希`657c3b1ebbc211c9f1e89a4e5f8f34fa0d4750659da52604c6fcb3c49947b90e`。它仍是旧runtime，因此本次发布最初源码原件和已记录的opt-in原件，DHCP改动通过单独核心修复包提供，不把中间版冒充最初原件。

## 构建关系

- AOSP/ReDroid Android13.0.0_r82，`redroid_arm64_only-userdebug`，API33、arm64-only，`ro.bionic.cpu_variant=generic`，原构建日期11-19。它不把CPU限定为SM8475。
- Ubuntu20保留redroid-pve v0.0.4参考归档、补丁检查脚本以及从AOSP `system.img`/`vendor.img`打包6GiB根文件系统的脚本。原打包还加入`init.droidspaces`、zygote FD wrapper、容器hardware rc及旧模拟器辅助文件。
- 这不是redroid-pve上游发布的ARM模板。其v0.0.4公开模板为x86_64；这里是本项目参考其补丁构建的ARM64产物。不能从补丁来源推断不同运行时/内核已通过相同权限或隔离验收。
- 本仓库pinned manifest和导出patches包含后续已知修复；历史原件不是当前整棵导出源码的按位构建结果。精确构建时工作树与所有旧辅助ELF的源码对应仍有缺口，不用今天的构建输出冒充当时system.img中的每个文件。

参考上游：[redroid-pve v0.0.4](https://github.com/hqw700/redroid-pve/tree/v0.0.4)、[Droidspaces v7.2.0](https://github.com/ravindu644/Droidspaces-OSS/tree/v7.2.0)。镜像原有`/.lab-droidspaces`和`/.lab-start.sh`作为原件保留；后者含已失效的模拟器传输参数和权限设置，不作为新设备启动脚本执行。

## 已知旧版差异

| 层 | 两份原件 | 已验证的后续实现 |
|---|---|---|
| Bionic/property opt-in | 两份原件的libc/linker及相关文件有7项差异，不能仅补marker | 核心修复包记录成套转换；精确旧/新哈希，不假定历史完整构建可按位重放 |
| Zygote runtime | `libandroid_runtime.so` SHA256 `8c1a525238306509bdb33f7cb5d2960da0cfad03d1b731c1b045a01752a24b99` | 真实xattr标签修复版`41ebfbeb...`，单独核心修复包 |
| Ethernet | 旧ipconfigstore `76233866...` | DHCP/空IPv4防御版`6cc88412...`，需配套容器init模式 |
| GPU/Codec | 原ReDroid图形模块；没有本机QTI私有集成 | SM8475实例的KGSL/Mesa/Gralloc/QTI另作硬件阶段参考 |
| 应用网络 | 没有UID broker | 新平台先测原生普通UID，必要时按网络教程集成 |
| 第二部分 | 没有Magisk、Next、LSPosed、统一管理前端或代理 | 软件基线通过后按需单独集成 |

基础Release额外附带源码工具包、API探针和9文件核心兼容包（7个Bionic/相关配套文件、runtime与DHCP修复），便于少编译地推进；它们没有被写回两份历史原件，全部新文件哈希与既有成功集成一致。

## 数据与文件系统审计

source原件在journal回放前后均确认`/data`为空。opt-in原件保留11个初始普通文件，日志回放后多一个0字节的installd状态文件：包括模拟器Ethernet配置、持久属性、classpath、内核版本记录和早期日志，不含已安装业务应用数据。它不是当前真机的账号/授权快照；检查未发现ADB密钥、账号库、Magisk授权库或私钥材料。其初始网络/属性仍不应带到新设备，建议用空数据source原件建立新副本。

两份原件的只读`e2fsck -fn`都返回0，但明确跳过journal recovery并报告空闲块/inode计数警告。本次在**两个独立Debian副本**执行日志回放/修复（退出1表示已修复），随后检查为0且无这些警告，并再次完成敏感材料及文件清单检查。下载原件仍保持原始哈希；不要把退出码0单独写成原件文件系统完全干净。

本轮只完成原件来源/字节/文件系统/内容审计与发布，没有在手机启动原件或新“基础+修复”组合，也没有验证其它处理器。现有SM8475完整集成的通过结果继续由独立的[集成镜像验收](../../artifacts/release-20261128.json)承载，不能转写成所有基础镜像/设备都通过。
