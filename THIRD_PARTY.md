# 上游、许可证与分发范围

本仓库汇总实验过程与小范围兼容代码，不替代上游项目。

| 项目 | 用途 / 固定版本 |
|---|---|
| [Droidspaces-OSS](https://github.com/ravindu644/Droidspaces-OSS) | 真机运行时 v7.2.0；[图形说明](https://github.com/ravindu644/Droidspaces-OSS/blob/main/Documentation/Graphics-and-Audio.md) 提供 Turnip / KGSL 方向 |
| [ReDroid](https://github.com/remote-android) | ARM64 Android 13，目标 redroid_arm64_only-userdebug |
| [redroid-pve](https://github.com/hqw700/redroid-pve) | v0.0.4 参考补丁；实际工作树补丁另外审计，不声称整套原样应用 |
| [AOSP](https://android.googlesource.com/) | Android 13.0.0_r82；pinned manifest 和各项目基线见仓库 |
| [Mesa for Android Container](https://github.com/lfdevs/mesa-for-android-container/tree/b84f02a4514983ea9b2d96971cde72720c377320) | 27.0.0-devel，b84f02a4514983ea9b2d96971cde72720c377320 |
| [scrcpy](https://github.com/Genymobile/scrcpy/tree/v5.0.1) | 5.0.1，补丁基线 a60891aea193d92e7e5c3942700eca63f9d19a5f |
| [Magisk](https://github.com/topjohnwu/Magisk) | 31.1；仅使用容器内 root 管理，不在本仓库重新分发 APK |
| [redroid-script](https://github.com/ayasa520/redroid-script) | 研究 Bootless / 镜像修补方法时的参考，不能把其原镜像修补流程直接套到本实验 |
| [Automobili Lamborghini Recompiled](https://github.com/alondero/automobililamborghini-recomp/releases) | 检索到相似 Adreno 730 / Android 15 Gralloc 格式查询锁等待案例；本机根因另外通过反汇编和回归探针确认 |

## 许可证

保留的第三方源码和补丁上下文沿用原项目的版权及许可证：AOSP/scrcpy 通常为 Apache-2.0，Mesa 的相关文件按其文件头和上游许可证处理，Magisk 上游为 GPL-3.0。请以相应版本原文件为准。本仓库未为全部新增整理内容指定统一顶层许可证；发布本身不改变第三方权利，也不构成对厂商二进制的授权。

`src/surface/platform-headers/` 是构建 JNI helper 所需的 AOSP 头文件节选/兼容副本，保留版权头；不是稳定 NDK API 保证。

## 刻意不上传

厂商 GPU/Codec/HIDL blobs、完整系统镜像、WebView 或其他第三方 APK、Magisk 二进制、Android SDK/NDK 压缩包、签名私钥、SSH 配置、宿主账号数据和全量未脱敏日志。

厂商库应从**同一台测试手机、同一 ROM 版本**提取并生成自己的 SHA256 清单。Gralloc 输入哈希在本仓库中只用于严格识别，不能据此下载或混用其他固件。
