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
| [Magisk](https://github.com/topjohnwu/Magisk) | 31.1；容器内root管理；Release保留已验收的历史输入，管理器明确提示unofficial；不是当前上游同版本APK，精确来源对应仍有缺口 |
| [Zygisk Next](https://github.com/Dr-TSNG/ZygiskNext) | 本轮用户提供1.5.3（880-22a1bf5）包；不重分发ZIP，不把包内metadata当发行认证 |
| LSPosed（用户提供的模块包） | 本轮v2.2.1（7912）/API102；精确输入哈希见artifacts/supplement-20261125.json，不重分发管理器、模块或license |
| [Zygisk module sample API](https://github.com/topjohnwu/zygisk-module-sample) | src/probes/zygisk/zygisk.hpp保留John Wu版权和ISC风格许可头；API5，仅用于回调探针 |
| [redroid-script](https://github.com/ayasa520/redroid-script) | 研究 Bootless / 镜像修补方法时的参考，不能把其原镜像修补流程直接套到本实验 |
| [Automobili Lamborghini Recompiled](https://github.com/alondero/automobililamborghini-recomp/releases) | 检索到相似 Adreno 730 / Android 15 Gralloc 格式查询锁等待案例；本机根因另外通过反汇编和回归探针确认 |

## 许可证

保留的第三方源码和补丁上下文沿用原项目的版权及许可证：AOSP/scrcpy 通常为 Apache-2.0，Mesa 的相关文件按其文件头和上游许可证处理，Magisk 上游为 GPL-3.0。请以相应版本原文件为准。本仓库未为全部新增整理内容指定统一顶层许可证；发布本身不改变第三方权利，也不构成对厂商二进制的授权。

`src/surface/platform-headers/` 是构建 JNI helper 所需的 AOSP 头文件节选/兼容副本，保留版权头；不是稳定 NDK API 保证。

`src/probes/zygisk/zygisk.hpp`的具体授权以其文件头为准，不能因为Magisk主项目GPL-3.0而丢弃这个公开API头的原许可。新增代理实现是本仓库自有小型relay，没有复制Hev代码；研究TPROXY方向时参考了[HevSocks5TProxy文档](https://github.com/heiher/hev-socks5-tproxy)和[Linux IP_TRANSPARENT说明](https://man7.org/linux/man-pages/man2/IP_TRANSPARENT.2const.html)，不依赖其二进制。

## Release 分发与来源边界

2026-11-28起，[设备专用Release](docs/PREBUILT_RELEASES.md)提供当前mondrian/SM8475、OS3.0.5.0.VMNCNXM组合的完整静态系统，包括实际使用的QTI GPU/Codec/HIDL库、Mesa、WebView和Bootless Magisk输入；发布者已确认允许公开这些现有厂商文件。该确认不改变第三方版权或向其他ROM提供兼容保证。逐文件哈希、权限和xattr随附件提供；从自己的固件提取时仍应重新核对ABI和符号。

Magisk历史管理APK的SHA256为`9d514204826bbe3765e6dbe1e958a9db97cb5c10a439b18125e63450ddf2cc3c`，签名证书SHA256为`a0d3671e5983bd47a78cb3fb4309527ff40ca56bf22e40288cab42e5bca69b04`；其arm64 native输入与镜像一致。当前上游v31.1下载的文件SHA256为`89cefac0a318afa39fb3880c45c87b036c05bc1ebf02a7a715b7f8d7c26ac6e7`，签名证书也不同，不能宣称字节等同或来自上游正式签名。上游参考源码为[0972f67d](https://github.com/topjohnwu/Magisk/tree/0972f67d97f9b6302b67c5171283330c74114b4a)，GPL文本保存在[LICENSES/Magisk-COPYING](LICENSES/Magisk-COPYING)；尚未建立历史输入与精确对应源码的完整关系。此限制同样记录于Release清单。

Mesa参考版本许可证见[LICENSES/Mesa-license.rst](LICENSES/Mesa-license.rst)，实际文件仍按各自文件头。AOSP源码固定清单及修改见本仓库manifest和patches；这次发布复用历史构建产物，没有重新全编译。未找到精确来源的旧兼容库见[重建缺口](docs/REPRODUCIBILITY_GAPS.md)。

Next/LSPosed ZIP不重新托管；Release的`optional-inputs.json`提供实际验证过的下载来源和精确哈希。LSPosed的版本入口可能更新，哈希不匹配时必须停止，不能按相同URL自动接受新版本。没有复制、生成或公开模块授权文件。

## 刻意不上传

原容器应用数据、账号、ADB密钥、Magisk授权数据库、模块license、签名私钥、SSH配置、SDK/NDK压缩包和未脱敏全量日志。发布镜像是新建文件系统，数据目录为空，不能把运行过的验收副本上传替代它。
