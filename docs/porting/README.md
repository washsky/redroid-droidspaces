# 跨设备复现：先软件，再硬件

本目录给在**其它ARM64处理器、GPU、ROM或内核**上复现的操作者/AI使用。先建立可重复的软件运行基线，再按新设备适配GPU和硬件编解码；SM8475是已验收实例，不是项目允许使用的唯一处理器。

本目录是独立的复现/移植教程。正式维护交接仍由`docs/HANDOFF.md`记录已有实例、已验收状态与操作授权；复现教程不代替交接，也不把新设备计划写成现有设备的验收结果。

| 入口 | 适用任务 | 实际提供 |
|---|---|---|
| [ARM64基础镜像Release](https://github.com/washsky/redroid-droidspaces/releases/tag/v2026.11.28-arm64-base) | 新处理器/ROM先跑软件基线 | Ubuntu20保留的PVE参考补丁ARM64源码构建原件、opt-in历史原件、通用核心修复、源码工具包与API探针 |
| [SM8475集成示例Release](https://github.com/washsky/redroid-droidspaces/releases/tag/v2026.11.28-arm64) | 对照已跑通的硬件集成或同组合复现 | 空数据12GiB、当前ROM的QTI库、KGSL/Mesa、Magisk、管理器及后续补充 |
| [软件优先步骤](SOFTWARE_FIRST.md) | 第一阶段 | 镜像副本、启动、普通APK、存储/SharedMemory、网络、软件画面与冷启动 |
| [GPU与编解码阶段](GPU_AND_CODEC.md) | 软件阶段通过后 | 根据实际GPU驱动与ROM ABI逐层适配，保留软件回退 |
| [基础镜像来源和已知缺口](BASE_IMAGE_PROVENANCE.md) | 下载前核对 | 原始字节、初始状态、ext4日志、历史二进制差异和验收边界 |

```bash
# 使用包含本目录的新版本源码；无需GitHub登录。
python3 scripts/download-prebuilt.py --profile base
# 默认只取source-original这一个镜像；历史opt-in原件需 --image optin 或 --image all。
```

原始镜像不包含当前手机的QTI私有集成，也不自带本项目后续Magisk、UID broker或管理器。它是ARM64适配起点；宿主内核、Binder/共享内存、容器策略和新设备实际运行仍须验收。本项目没有测过的处理器/ROM不能写成已通过。

每阶段单独记录“输入哈希、最小改动、真实结果、保留问题”。一个阶段未通过时先定位该层，不把GPU、QTI Codec或Xposed全部堆进去。新设备记录可另建自己的移植报告，不覆盖原维护交接。
