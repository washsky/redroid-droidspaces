# ARM64 ReDroid 13 on Droidspaces

在 **骁龙 8+ Gen 1（SM8475）/ Adreno 730、Android API 35 真机**上，用 Droidspaces v7.2.0 运行 ARM64 ReDroid 13 的实验记录、源码补丁与复现材料。

这不是通用刷机包，也不是已清空数据的可分发系统镜像。仓库记录了已经在真机跑通的组合，保留硬件、内核、厂商库和安全隔离方面的适用条件。

## 当前结果

| 功能 | 实际验收 | 边界 |
|---|---|---|
| Android 13 / ARM64 启动 | 完整启动、目标容器冷启动、DHCP | 仅操作目标容器 |
| 普通 APK | 安装、私有存储、SharedMemory、HTTPS 200 | 默认 TLS 校验保留 |
| 普通应用联网 | 游戏到登录入口；TCP/UDP；网络曾获得 VALIDATED | 通过 UID socket broker；原生应用 UID 直连隔离仍有缺口 |
| UID 网络管理前端 | Android 原生管理 APK，多包选择、保存、状态与重启 | 包名动态解析 UID；需容器内 root |
| Root | 容器内 Bootless Magisk 31.1 | 没有修补宿主 boot；不是完整 Magisk 模块环境 |
| GPU | Mesa Turnip/KGSL、ANGLE、HWUI skiavk | 不是只看 GPU 使用率；不等于全部应用/格式已兼容 |
| H.264 / H.265 | QTI YUV 硬件编解码探针 | 每类 30 帧的历史验收 |
| scrcpy H.264 Surface 硬编 | 自定义 server + 线性输入 Surface + QTI 私有 Vulkan 路径 | 有额外 GPU 中间处理；不是零拷贝保证 |
| Via / X 浏览器 | 页面、WebGL 2、滚动、点击、输入、HTTPS | 修复 QTI Gralloc 漏解锁后验收 |
| 脚本应用 | root 截屏/虚拟显示、配置页、停止操作 | 无障碍需开启；未验收自动游戏流程 |
| 存储 | 6 GiB → 12 GiB，离线文件系统检查通过 | 余量随应用数据变化，见状态快照 |

**未完成：** 原生 UID 网络隔离根治、通用硬件/ROM 兼容、全部应用 Surface 编解码、一小时耐久与游戏登录后流程、从空源码环境完整重建验收。

2026-11-24 已新增一个空数据独立容器，复用产物完成首次部署与冷启动验收，并修复公开构建的管理 APK 的预加载命名空间问题：[实际复现结果](docs/REPRODUCTION_20261124.md) · [跳过大型编译的步骤](docs/PREBUILT_REPRODUCTION.md)。

## 阅读入口

1. [完整过程与复现指南](docs/FULL_PROCESS.md)
2. [维护者 / 下一位 AI 交接](docs/HANDOFF.md)
3. [当前状态与验收矩阵](docs/STATUS.md)
4. [AOSP 源码和镜像构建](docs/BUILD.md)
5. [应用 UID 联网与管理器](docs/NETWORK.md)
6. [GPU、Gralloc 和硬件视频](docs/GRAPHICS.md)
7. [Root 与安全边界](docs/ROOT_AND_SECURITY.md)
8. [扩容与回退](docs/STORAGE_AND_ROLLBACK.md)
9. [验证步骤](docs/VALIDATION.md)
10. [时间线、失败路径与经验](docs/EXPERIMENT_LOG.md)
11. [重建边界与已知缺口](docs/REPRODUCIBILITY_GAPS.md)
12. [上游、许可证与未分发文件](THIRD_PARTY.md)

## 仓库内容

- `src/`：实际实验使用的 C / C++ / Java 源码；保留上游版权头。
- `patches/aosp/`：当前编译树相对于各项目 HEAD 的补丁与基线 SHA。
- `aosp-manifest-pinned.xml`：源码项目的 pinned manifest；未提交的改动另外保存。
- `patches/mesa/`：基于固定 Mesa for Android Container 版本的改动和新增文件。
- `patches/scrcpy/`：针对 scrcpy 5.0.1 的 server 补丁。
- `config/`：init、服务和容器配置参考；不能不加检查地覆盖现有系统。
- `scripts/`：构建、离线补丁、只读检查、扩容工具。
- `evidence/`：脱敏后的对照日志和真实画面。
- `docs/HANDOFF.md`：已验证文件、哈希、保护约束和下一步。

开始前复制 `config/lab.env.example` 为本地 `lab.env`，填入自己的路径和 ADB 目标。**所有 ADB 命令必须指定 `-s`**。

## 重要适用条件

- 当前实验使用 `privileged=full`、宿主侧已有的容器 SELinux 兼容策略，以及 `persist.zygote.app_data_isolation=false`。宿主 `getenforce` 保持 Enforcing，**不能因此声称容器隔离与原生 Android 安全等价**。
- 不要全局关闭 SELinux、解绑宿主 cgroup eBPF、清空防火墙，或暴露全部宿主设备。
- 本仓库不包含厂商 GPU/Codec blobs、WebView APK、Magisk APK、密钥、账号数据或完整 rootfs。需要从自己的合法环境取得并核对版本。
- QTI Gralloc 二进制修补只接受一个精确 SHA256，输出到新文件；不是可套用到其他手机的通用偏移补丁。
- 旧交接中的“骁龙 7+ Gen 2”“仍全程 SwiftShader”“DHCP 源码补丁未部署”等结论已经过时；本仓库以实际文件和分阶段验收为准。

整理日期：2026-11-24（Asia/Shanghai）。实际功能验收主要完成于 2026-11-21 至 2026-11-23。

<img src="evidence/image.png" alt="redroid运行补充" width="">
