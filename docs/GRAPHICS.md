# GPU、Gralloc 与硬件视频

## 两条 Vulkan 路径

| 进程 | 路径 | 原因 |
|---|---|---|
| SurfaceFlinger / HWUI / 普通图形应用 | ANGLE + Mesa Turnip freedreno / KGSL | Android13环境下的开源 Vulkan 路径 |
| QTI Codec 服务 | 同 ROM 原厂 vulkan.adreno.so，限定私有 loader 环境 | QTI Surface 转换器与自身厂商依赖匹配 |

不要全局把 ro.hardware.vulkan 切到 adreno 来“修 Codec”。codec-vulkan-select.c 仅在该 Codec 服务 preload 中，针对这个属性的读取选择 adreno。全局实际值仍为 freedreno。RC 见 config/init/arm13-qti-codec.rc。

Droidspaces 官方 Linux 图形文档提供 Turnip/KGSL 的设备和构建方向；ReDroid 还需要 Android allocator/mapper、Gralloc、HIDL/VINTF、AHB/Surface 和 Codec，一项不能替代另一项。

## GPU 与实际 Gralloc metadata

本机只按明确清单暴露 KGSL、dma_heap、所需视频节点和独立 Binder。动态 dma_heap/qcom,system 节点由 sysfs uevent 提供真实 major/minor，config/bin/arm13-make-qcom-system-heap 不猜设备号。

AOSP 模块 libarm13_mesa_gralloc 使用 IMapper4 get() 解码真实 PixelFormatFourCC、Modifier、AllocationSize、LayerCount、PlaneLayouts。Mesa 新文件 u_gralloc_arm13_bridge.c 通过这个桥接导入 plane 信息，并对 QTI NV12 UBWC payload/metadata 布局做明确检查；不通过猜测 private_handle 布局套 offset。

TU_DEBUG=noconform / TURNIP_KMD=kgsl 是实验环境；noconform 表明没有宣称 Vulkan 完整一致性认证。HWUI 使用 skiavk。当前 boot.redroid_gpu_mode=guest 或 ro.hardware.gralloc=redroid 的旧属性值本身不足以判定实际活动驱动。

## QTI Gralloc 漏解锁：本次共同卡死的直接修复

设备原库 SHA256：
`3e4cef7a63b47c18bb1ba8283a08fdbd150a99194138e92ead3cede662c9098b`

AllocateBuffer() 的 testAlloc + size==0 路径：
- 先持有 BufferManager+8 的真实 PI mutex；
- 把返回值设置为 NO_RESOURCES(5)；
- 直接跳到 epilogue，遗漏 pthread_mutex_unlock。

原库在宿主与容器的哈希一致；不是仅因为“容器没有 memfd”。当持有线程之后休眠，其他浏览器/脚本线程进入 RetainBuffer / AllocateBuffer 就等待；SurfaceFlinger 可进一步阻塞在客户端 dequeueBuffer。

修补仅将该分支转到已确认的 ELF 可执行段尾部空白，调用**原库原有的 unlock PLT**，再回到原 epilogue。保留返回错误5、互斥锁、其余分支及真实分配行为；扩展对应 RX LOAD 长度12字节。没有强制解开其他线程的锁、没有把 ioctl 或失败结果伪造成成功。

修补输出 SHA256：
`7ba792e5becada349ca60216468b9b444c99c531ce7915db0a387e7aefb10337`

```bash
python3 scripts/patch-gralloc-unlock.py /path/from/your/device/libgralloccore.so /path/new/libgralloccore.so
```

工具对输入 SHA、ELF结构、原指令、空白和输出 SHA 都检查，拒绝覆盖。**不要修改哈希条件强行套其他 ROM**。记录输入来源，备份目标原库，原子替换到独立候选，重启目标容器再测；不修改宿主厂商库。

src/gralloc/isolated-probe.c 仅在新的 mount namespace 内 bind 测试库，用于原版/修补版对照，不影响运行容器的全局挂载。format=0x22 / usage=0 / 16x16 的原版查询返回不支持但漏锁，下一线程分配超时；修补版仍返回不支持并成功完成下一线程。6,643组检查和真实 Via/X/脚本见 evidence。

## QTI 硬编

YUV buffer 输入的 H.264/H.265 编码和解码探针先通过，各30帧并进行图像对照。后来发现 Surface 输入收到有效码流却全绿，必须单独修：

1. Codec 私有目录补齐同 ROM Vulkan 依赖及动态加载的 libllvm-qgl.so / libllvm-qcom.so。
2. 服务只在自身环境选择原厂 Vulkan，避免其转换器误用 Mesa。
3. 编码器输入 Surface 使用合法的 CPU_READ_OFTEN usage 提示，使本机 Gralloc选择线性布局，避免 Mesa UBWC 与原厂转换器布局解释不一致。
4. 定制 scrcpy 强制经 OpenGLRunner 产生本地输入 Surface，使 perform interceptor 实际留在该生产者对象上。
5. 解码后检查颜色，再检查实时窗口。

linear-window.c 使用 API33平台 ANativeWindow_setPerformInterceptor，转发原操作及原错误，仅合并 usage位。不是通用 stable NDK 接口，因此限定 API/设备并保留失败返回。有额外 GPU 中间处理，不能宣传零拷贝。

Codec 兼容目录包含部分平台兼容库、插件兼容 stub和厂商动态库；见 artifacts/hashes.json / REPRODUCIBILITY_GAPS.md。库存在并不说明可以从任意 Android15 ROM混装。

## 定制 scrcpy

```bash
git clone https://github.com/Genymobile/scrcpy.git "$SCRCPY_SRC"
git -C "$SCRCPY_SRC" checkout a60891aea193d92e7e5c3942700eca63f9d19a5f
git -C "$SCRCPY_SRC" apply /path/to/repo/patches/scrcpy/0001-arm13-linear-qti-surface.patch
./scripts/build-scrcpy-server.sh
./scripts/redroid-hw-scrcpy.sh
```

server 仅在 API33、明确 c2.qti.* encoder且 marker存在时启用 helper。helper 位于 /data/local/tmp/libarm13_linear_window.so，marker为 /data/local/tmp/arm13-linear-recording.enabled。launcher 从 vendor 私有目录复制 root-owned 0644 helper，然后设置 SCRCPY_SERVER_PATH。

配置：H264、c2.qti.avc.encoder、20fps上限、2Mbit/s；Debian显示端使用 --hwdec=vaapi。显示端 VAAPI 是 Debian GPU解码路径，与手机编码器是两个独立端点。当前正式投屏没有启用音频，不能声称音频已完整验收。

回退可以先使用原版 scrcpy server + c2.android.avc.encoder 软件路径，恢复图形库则必须恢复与原基线对应的版本。不要只回退Gralloc修补却保持会触发漏锁的浏览器/脚本然后宣称系统稳定。
