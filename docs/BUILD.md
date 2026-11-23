# 源码、构建与产物

## 固定输入

- AOSP/ReDroid：Android 13.0.0_r82，目标 redroid_arm64_only-userdebug。
- aosp-manifest-pinned.xml：2026-11-24 导出；manifest 仓库当时 HEAD 为 ed487956fff8d6b616d99ace142203d6b89aa877。
- patches/aosp/source-patches.json：每个导出项目的基线 SHA、文件列表与 git apply --cached --check 结果。
- 本次检查覆盖已知补丁目标 art、bionic、external/minijail、external/selinux、frameworks/{av,base,native}、system/{bpf,core,libbase,netd,sepolicy}、vendor/redroid。没有改动的目标没有 patch。**不是对 AOSP 所有项目未跟踪文件的完整审计**。
- Mesa：lfdevs/mesa-for-android-container b84f02a4514983ea9b2d96971cde72720c377320；源码归档 SHA256 ee1e2a78bc9f38305f54dd6f1f373bdebf1ac8417f22c89c65425f7611e43268。
- scrcpy：v5.0.1 / a60891aea193d92e7e5c3942700eca63f9d19a5f。
- NDK r29；Android API33 native target；scrcpy server 编译使用 API36 platform android.jar / framework.aidl。
- 原实验是 Ubuntu20.04 x86_64 AOSP 构建，Debian x86_64 NDK/Mesa 构建。

## AOSP 获取和补丁

先按 ReDroid 上游准备源码与环境。manifest 的项目 revision 是实际导出的输入；仅将 aosp 的相对 fetch=.. 展开为 https://android.googlesource.com/ 并清理空白，避免更换 manifest 仓库后解析到错误站点。不要盲目切到今天 main。

```bash
repo init -u https://android.googlesource.com/platform/manifest -b android-13.0.0_r82
cp /path/to/redroid-droidspaces/aosp-manifest-pinned.xml .repo/manifests/lab-pinned.xml
repo sync -m lab-pinned.xml -j4
python3 /path/to/redroid-droidspaces/scripts/apply-aosp-patches.py "$AOSP_ROOT"
```

apply 工具先检查所有基线和补丁，再进行写入；当前项目 HEAD 必须匹配清单。完整构建之前先审阅 external/selinux、system/core、system/bpf 等补丁：它们包含容器兼容和隔离退让，不能照搬到宿主 Android。

本清单是当前工作树的实际 diff，**不要再无条件叠加一整套 redroid-pve v0.0.4**，否则会重复应用或产生不同结果。它也不等于“所有 pve 补丁已原样应用”。

```bash
cd "$AOSP_ROOT"
source build/envsetup.sh
lunch redroid_arm64_only-userdebug
m -j4
# 修改对应模块后可增量：
m -j4 libandroid_runtime ipconfigstore
```

WebView 预编译仓库用 Git LFS；git lfs pull 后以 zipfile.is_zipfile 和 APK metadata 检查。签名/SDK 包从官方来源取得，不上传私钥。

## Native helpers 和管理器 APK

```bash
source lab.env
./scripts/build-native.sh
./scripts/build-netmanager-apk.sh
./scripts/build-scrcpy-server.sh
```

- Native 脚本只写本地 out/，不自动安装到设备。
- 管理器包名 lab.arm13.netmanager，target SDK33。签名证书/密钥由 SIGN_CERT / SIGN_KEY 提供；AOSP testkey 仅适合本地实验。
- 公开脚本将原实验绝对路径改成变量；会编译检查，但这次整理没有重新部署新构建产物覆盖现有成功环境。
- libarm13_mesa_gralloc 要在 AOSP vendor 模块环境构建：将 src/mesa-bridge 的源文件和头文件复制到新的 vendor 子目录，将 Android-mapper4.bp 命名为 Android.bp，再 m libarm13_mesa_gralloc。

## Mesa

```bash
git clone https://github.com/lfdevs/mesa-for-android-container.git mesa
git -C mesa checkout b84f02a4514983ea9b2d96971cde72720c377320
git -C mesa apply /path/to/repo/patches/mesa/container-local-changes.patch
cp -a /path/to/repo/patches/mesa/new-files/. mesa/
./scripts/build-mesa.sh /path/to/mesa
```

关键组合：platforms=android、freedreno-kmds=kgsl、platform-sdk-version=33、android-stub=true；只构建 Turnip Vulkan，不在这一步构建 Gallium/EGL。应用通过已有 ANGLE 使用 Vulkan。额外 Mesa bridge 从真正 IMapper4 metadata 读取 fourcc、modifier、plane offset/stride；不能猜 native_handle 私有布局。

## 镜像打包原则

1. 从构建产物读取 system.img/vendor.img；Android sparse 先 simg2img。
2. 只读 loop 挂载输入；确认 system-as-root 层级。
3. 创建**全新** ext4 镜像，复制 system/vendor；保留权限、符号链接、xattr。
4. 配置属性 opt-in marker、目标容器 init wrapper、FD 清理器、必要目录、当前 init rc。
5. 增加 broker/root/graphics 所需文件，并按本机合法厂商库清单补齐依赖；不要把宿主 /vendor 整棵目录直接覆盖进去。
6. 保存新镜像 SHA256，然后再部署到独立候选；从设备运行过的 rootfs 会因 /data 改变而产生不同哈希。

## 需要诚实保留的重建缺口

当前运行镜像由多轮增量部署形成，不是一次构建后完全未修改的 release：

- 厂商库、真实 ashmem 驱动、ROM/内核策略由具体设备提供。
- 历史 init / zygote FD wrapper、Codec 私有兼容库的来源必须与实际哈希核对；本仓库尽量保留源码和清单，但不分发这些历史二进制。
- 当前 out 产物与真机 libandroid_runtime、ipconfigstore 的 SHA256 已核对一致；这不自动证明每一个 vendor 私有库可由此 AOSP 分支重建。
- 这次整理验证的是补丁可应用、辅助代码可构建和远端提交完整；**未在空白机器上重新跑完 AOSP 全编译、打包和整机部署**。

下一步若制作可公开分发镜像，应在独立干净目录重放全部步骤、形成完整 provenance 和二进制依赖清单，再验收，不能直接发布当前含个人 /data 的镜像。
