# 给下一位 AI：先选择复现路线

新处理器/ROM或从软件开始的复现，优先读[跨设备入口](porting/README.md)、[软件优先步骤](porting/SOFTWARE_FIRST.md)和[原件来源](porting/BASE_IMAGE_PROVENANCE.md)：`python3 scripts/download-prebuilt.py --profile base`。先验收软件系统与普通应用，再适配GPU/Gralloc/Codec；移植教程与正式`HANDOFF.md`分别维护，不把新设备计划写成已有实例验收。

下面保留SM8475完整集成版本的快速入口。它是硬件实例，不是其它处理器的前置条件。

固定版本：[v2026.11.28-arm64](https://github.com/washsky/redroid-droidspaces/releases/tag/v2026.11.28-arm64)。先检出这个tag，读取README、docs/PREBUILT_RELEASES.md、docs/REPRODUCIBILITY_GAPS.md和artifacts/release-20261128.json。

任务核心是Droidspaces运行ARM64 ReDroid13；Next/LSPosed等属于可选第二部分。不要为了取得缺失的历史二进制重新开始整个编译实验。

```bash
git clone --branch v2026.11.28-arm64 https://github.com/washsky/redroid-droidspaces.git
cd redroid-droidspaces
python3 scripts/download-prebuilt.py
# 如果先研究工具：python3 scripts/download-prebuilt.py --tools-only
```

下载器无需gh登录，逐个校验SHA256、拒绝覆盖不同内容，不操作手机。默认目录out/prebuilt-20261128。Release附带完整机型镜像、通用源码工具包、管理前端/API探针APK、历史匹配Magisk管理APK、空数据证明/权限/xattr清单、可选模块来源和SHA256SUMS。

部署前必须现场核实：

- mondrian/SM8475/Adreno730、宿主OS3.0.5.0.VMNCNXM/API35、Droidspaces7.2.0及必要内核/策略；镜像不是通用刷机包。
- 用户允许操作的实例与禁止触碰的对象；不要从历史记录外推授权。
- 实际ADB serial、PID/starttime、运行状态；所有adb命令指定`-s`。
- 三个独立Binder节点、真实ashmem/KGSL/dma_heap/Codec节点、未使用的NAT IPv4和转发端口。
- 原始工作镜像和配置已保留；新镜像必须放全新路径。完整命令按docs/PREBUILT_RELEASES.md执行，不盲目覆盖活跃实例。

镜像逻辑大小12884901888字节，解压后SHA256：

`beae1105bf35cc5fbc4081a3e0d6436e2a394eadb7cd0c80c520a66f1a3e1b88`

首次启动自动建立本实例地址配置、终端布局和空名单UID broker，代理默认关闭。普通应用联网必须先在管理器选择应用；原生应用UID直连仍未修复。root只在容器内授权给现场查询到的UID，不复制其它实例的授权数据库。

Magisk管理器是已验证的历史unofficial输入，与当前上游同版本APK不同；精确对应源码仍有缺口。勿操作宿主boot。Next/LSPosed不预装，按optional-inputs.json来源下载并核对哈希；不复制license。重点读docs/part2/MAGISK_ZYGISK_LSPOSED.md，安装成功还需检查注入、真实回调及system_server接入，x86不能直接推定已修复。

原版scrcpy默认QTI编码，无需定制server或linear codec参数；电脑可选`--hwdec=vaapi`。现场验证应包括普通应用存储/SharedMemory/默认TLS HTTPS、终端PTY、真实窗口画面；不能只读属性即宣称成功。

发布原图保持未启动且/data为空。验收只启动字节相同的副本；结束恢复原arm。没有操作其它容器、重启真机、登录账号或运行游戏自动流程。更多旧二进制源码、跨ROM、全部编解码格式、长期耐久仍是明确缺口。
