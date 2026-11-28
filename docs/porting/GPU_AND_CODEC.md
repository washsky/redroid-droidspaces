# 第二阶段：按新设备适配GPU，再适配硬件视频

前提是[软件阶段](SOFTWARE_FIRST.md)有可回退的实际验收副本。各步骤另建候选；一次只改一个层，保留相同应用/画面作前后对照。本页是移植路线，不代表本项目已经测过所有处理器。

| 阶段 | 先核实 | 验收后才推进 |
|---|---|---|
| GPU设备与用户态驱动 | SoC/GPU、内核接口、设备节点权限、对应ROM的EGL/Vulkan ABI | EGL/Vulkan基础绘制，真实像素；不能只读驱动名称 |
| Gralloc/Allocator/Mapper | 真实HAL版本、native_handle布局、metadata、format/modifier/stride | AHB分配/导入/释放、跨线程、持续帧更新 |
| 应用图形栈 | ANGLE/HWUI/SurfaceFlinger如何使用新驱动 | 普通APK和WebView真实绘制、窗口切换、滚动/输入，无新增死锁 |
| 视频硬件编解码 | 当前ROM的Codec服务、依赖、VINTF与真实视频设备节点 | 先Buffer encode/decode，再Surface encode、EOS和解码像素 |
| scrcpy硬件路径 | 可用encoder、Surface格式；电脑端解码后端另核实 | 原版server实际画面/输入；需要定制时明确版本和边界 |

SM8475成功案例使用Adreno/KGSL、Turnip、IMapper4桥接与QTI Codec。它为**这一类已测组合**提供源码、补丁、哈希和排障经验；其它Adreno代际也须核实内核/ROM ABI，Mali、PowerVR或其它GPU不能直接套用QTI库、KGSL节点或Gralloc二进制偏移。ARM64指令集相同不足以保证GPU/Codec ABI相同。

对新硬件先查该驱动的官方源码/文档及准确版本，再参考同GPU/内核的已验证经验。若目前没有可用的容器图形接口或厂商依赖，保留已验收的软件路径并标明硬件适配待办。不要用伪造metadata、固定native_handle偏移、假ioctl成功或暴露整棵宿主`/dev`掩盖问题。

本仓库[GRAPHICS.md](../GRAPHICS.md)、`src/mesa-bridge`、`src/surface`和`patches/mesa`作为SM8475实例研究；每一步先判断哪些机制可复用，哪些依赖当前ROM。Gralloc修补和QTI默认线性格式修补都接受精确输入哈希，拒绝其它库时应重新分析，不能去掉哈希校验继续套偏移。

原版scrcpy在SM8475集成镜像上的默认QTI编码适配，不属于原始基础镜像的默认能力，也不保证其它处理器的Surface格式。手机端编码与电脑端解码分别记录；先让默认软件解码看到正确画面，再测试电脑支持的VAAPI等后端。真实窗口、编码帧、解码像素与输入操作均要检查。

GPU/Codec验收后，再按需要进入[第二部分补充](../PART2_SUPPLEMENT.md)的Magisk模块、Next/LSPosed、管理前端、显示和代理。它们不是GPU驱动适配的替代方案；本目录不要求登录账号、运行游戏自动流程或做一小时耐久。
