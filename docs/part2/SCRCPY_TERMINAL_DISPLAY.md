# 原版 scrcpy、终端和显示启动参数

所属：[第二部分补充](../PART2_SUPPLEMENT.md)。这些改进建立在[核心图形栈](../GRAPHICS.md)已工作的基础上，不取代 GPU/Gralloc/QTI 依赖。

## 1. scrcpy 两端和最终用法

手机端 c2.qti.avc.encoder 负责硬编码；Debian --hwdec=vaapi 控制电脑硬解码。二者独立。改客户端 decoder 不能修复编码器输入 Surface 的布局错误。

历史组合是定制 server + linear-window helper，已保留在 patches/scrcpy、build-scrcpy-server.sh 和 redroid-hw-scrcpy.sh 中，供旧阶段回退。**本轮 arm 最终不再依赖定制连接脚本或定制 server。** repro 冻结，没有把这条结论同步过去。

中间可用的原版参数是：

```bash
scrcpy -s "$INNER_ADB" \
  --video-codec-options=vendor.qti-ext-enc-linear-color-format.value:int=1
```

随后定位该 QTI 库中此参数的默认值，限定修改为 1。原版 server 不显式设置它时也选择线性布局，显式 setter 仍保留。因此本机 arm 可直接：

```bash
# 先排除历史定制 server 环境变量；不是修改 server 文件。
env -u SCRCPY_SERVER_PATH scrcpy -s "$INNER_ADB"

# 若需要 Debian VAAPI 硬解，只增加客户端解码选项。
env -u SCRCPY_SERVER_PATH scrcpy -s "$INNER_ADB" --hwdec=vaapi
```

没有额外指定 video-encoder、linear format 或 OpenGLRunner；实际日志仍须确认选择 QTI。这里只验收原版 5.0.1，不能保证所有客户端版本和默认自动选码器行为。

## 2. 精确输入的容器内 QTI 默认值补丁

工具：[patch-qti-linear-default.py](../../scripts/patch-qti-linear-default.py)。目标是 **容器内** /vendor/lib64/libqcodec2_v4l2codec.so，绝不修补宿主厂商库。

| 项目 | 值 |
|---|---|
| 原库 SHA256 | e5afb835a68b970cab4e0cb9197607b6fabbeeca5f5c83cbda9138add5549d0c |
| 输出 SHA256 | c72860fed914989457ea55a8f046325ada2f4307995711575755fcff65de21dd |
| 指令偏移 | 0x7cc6c，仅对上述 AArch64 ELF |
| 原值/新值 | INT_MAX（自动布局）→ 1（线性） |
| 其他锚点 | 参数名、C2Param value store、ELF 架构 |

```bash
python3 scripts/patch-qti-linear-default.py \
  /path/from/your/device/libqcodec2_v4l2codec.so \
  /path/new/libqcodec2_v4l2codec.so
```

脚本只生成新文件，不安装；输入哈希或指令/字符串不匹配就拒绝。不同 ROM、其他架构不能放宽校验强行套偏移。保留原库，按[部署清单](REPRODUCTION_AND_VALIDATION.md)更新仅目标容器的文件，再只冷启动目标。

本补丁解决编码器默认布局选择。QTI 服务私有原厂 Vulkan、依赖库、VINTF、Gralloc 漏解锁修复仍需要保留；不能只复制这四字节就宣称完整硬编兼容。

## 3. 实际画质验证

最终以 stock 5.0.1 server、720×1280、c2.qti.avc.encoder、VAAPI 解码录制并提取帧，与同一静态图形探针的 Android screencap 对比：

- 中央 60% 区域 RGB 平均绝对误差 MAE=1.343（0–255 范围）。
- 原图通道标准差约 [44.15,49.75,0.0]，解码帧约 [44.16,49.76,1.2]。
- 蓝通道在原探针中本来恒定，不能用“每个通道都必须高方差”的规则误判。
- 记录日志确认 QTI encoder、Video decoding: vaapi、Texture (VA-API):720x1280。

只看 encoder 名字或生成了 H264 文件不够：此前确实出现过“成功输出但全绿”。应检查解码像素、实时窗口和输入。没有在本轮验收音频、所有分辨率/格式或一小时稳定性。

## 4. Droidspaces 前端终端

Droidspaces 使用 Linux 风格 su -l USER，Magisk31.1 的用户参数位置不同。新增 magisk-login-compat.c 编译为 /vendor/bin/arm13-su-login，将特定登录语法转换后交给 /sbin/su；授权仍由 Magisk 判断。

arm13-terminal-setup 在本机原 /bin → /system/bin 的前提下，将 /bin 改为对应命令符号链接目录，仅 /bin/su 指向新 wrapper，保留 /bin.arm13-original-link。不是覆盖 /system/bin/su，也不是增加无条件 root 后门。脚本对已有状态不匹配会拒绝。

另一个独立问题是 Magisk 私有 namespace 提前形成，后来 Droidspaces 的 devpts/ptmx 视图不一致。module-mounts 的 repair-pty 操作验证 init-owned daemon、exe、namespace、真实字符设备，再只在 daemon 私有视图克隆容器 devpts，同时固定两个 ptmx 别名。它不把宿主 PTY 整体共享给所有应用。

实际 Droidspaces enter 的交互 root PTY 已通过；terminal_ready=1。如果只测试 su -c id，不足以发现交互 PTY 问题。

回退恢复原 /bin symlink 和相配 helper/start 脚本，仅重启目标；不要从另一个容器复制 PTY 节点或写死 PID。

## 5. init 参数而不是临时 wm 覆盖

Droidspaces custom_init 接受可执行路径，不在这个字段塞带空格的一串参数。本机原 /init.droidspaces 是 ELF；审计其行为后，新增 cap-init.c 编译为 /init.arm13-caps，等价执行 private 根挂载和真实 /init。

显示参数来源：

1. 默认 720×1280、DPI320、60Hz。
2. root-owned、不可组/其他用户写入的 /data/adb/arm13-boot.conf。
3. helper 自身收到的四个允许 argv 参数覆盖文件值。

它只接受四个键，不允许任意 init 参数注入：

```ini
androidboot.redroid_width=720
androidboot.redroid_height=1280
androidboot.redroid_dpi=320
androidboot.redroid_fps=60
```

| 字段 | 允许范围 |
|---|---|
| width / height | 240–3840，偶数 |
| dpi | 72–640 |
| fps | 15–120 |

```bash
adb -s "$INNER_ADB" shell 'su -c "/init.arm13-caps --check"'
adb -s "$INNER_ADB" shell 'su -c "/vendor/bin/arm13-bootctl status"'
```

--check 只校验/打印；正常启动入口要求 PID1/root/已核实原 wrapper。固定的平台参数和 SELinux 兼容参数沿用既有基线，不意味着修改宿主全局 Enforcing。

本轮真正冷启动过 640×1136/DPI280/50Hz，并核对 SurfaceFlinger 模式；随后恢复 720×1280/DPI320/60Hz。管理器 v1.2 又实际保存了 50Hz 的待启动配置，再恢复 60Hz；不把单纯保存当作已经运行到新分辨率。

## 6. 前端保存与版本冲突

UID 网络管理中的“分辨率、DPI 与刷新率”同时显示当前 ro.boot 值和下次启动配置。bootctl：

- flock、固定字段和范围校验、O_NOFOLLOW、root 文件所有权检查；
- 原子写入/fsync，随后调用 /init.arm13-caps --check；失败尝试恢复旧值；
- 配置内容生成 32 位 revision，旧 revision 返回 STALE_CONFIG；
- 保存不重启容器，不对当前画面执行 wm size/density，更不重启真机。

当前源码的 bootctl/proxyctl 还固定检查本实验 arm 地址、设备名和 netns；新实例需要审查并重新编译，不能将默认检查移到冻结 repro。

## 7. 能力收紧的实际边界

本轮从 privileged=full 调整为 privileged=nocaps，保留运行时 mount mask、私有传播和 seccomp。nocaps 的名字容易误读：**它表示不做运行时默认 capability 裁剪，不是零能力。** 再由 /init.arm13-caps 删除 SYS_MODULE、SYS_RAWIO、SYS_PACCT、MAC_OVERRIDE、AUDIT_READ 五项。

最后 init CapEff/CapBnd 为 000001deffecffff，36 项；Seccomp=2，filter=1；UID 映射仍为 0→0 的全范围，**不是非特权 userns 容器，也尚未逐项最小化**。

运行时默认九项裁剪曾导致 Next 注入失败。它与当前五项削减相差 DAC_READ_SEARCH、MAC_ADMIN、WAKE_ALARM、BLOCK_SUSPEND；只证明这组差异中存在兼容依赖，没有逐项证明四项都必须保留。

redroid-pve 的非特权方案通过 LXC UID 映射提供不同的隔离；其编译补丁不会自动给 Droidspaces 建立该映射。参考[上游配置](https://github.com/hqw700/redroid-pve/blob/main/deploy/pve-configure-redroid.py)，不要把两个运行时的“非特权”标签混用。
