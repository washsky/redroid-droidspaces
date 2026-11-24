# 复现后的验证

逐步验证，失败时先保存现场再改下一层。下面的shell命令均针对 lab.env 指定目标；su是容器当前Magisk语法。

## 基线

```bash
source lab.env
./scripts/inspect.sh
adb -s "$INNER_ADB" shell 'getprop sys.boot_completed; pidof system_server; pidof zygote64; pidof netd'
adb -s "$INNER_ADB" shell 'su -c "/vendor/bin/arm13-netctl status"'
adb -s "$INNER_ADB" shell 'dumpsys webviewupdate'
```

记录宿主 Enforcing、保护容器PID/starttime和宿主HTTPS，再进行任何目标冷启动。禁止用全局adb操作恢复单一转发。

## Gralloc原版/修补版对照

先构建脚本中的三个probe，上传到目标/data/local/tmp并设置正确执行权限。

isolated-gralloc-probe 的参数为：要测试的库路径、probe可执行文件、probe参数。它先unshare mount namespace并设为private，再bind该库，最后exec探针。只在root受控实验中使用。

```bash
# 以下在内层 root shell；替换为自己保存的原库
timeout -s KILL 3 /data/local/tmp/arm13-isolated-gralloc-probe \
  /vendor/lib64/libgralloccore.so.pre-unlock-20261123.bak \
  /data/local/tmp/arm13-ahb-is-supported 0x22 0 16
# 原版：support=0之后下一线程超时。137是本例超时KILL结果。
# 修补版：
timeout -s KILL 3 /data/local/tmp/arm13-ahb-is-supported 0x22 0 16
# 必须有 second-thread alloc=0 和 CROSS_THREAD_PASS
```

find-gralloc-leak对511种格式和13种usage共6,643组检查；保留 unsupported=false，不能为了通过测试假报支持。read-gralloc-mutex是**该精确库布局**的只读辅助工具，其他SHA不可按同偏移读取。

## 浏览器真实画面

使用自己控制的测试站，不依赖某个公网示例站长期提供压力流量。仓库附有原测试页：

```bash
# LAB_HTTP_BIND 填工作站在可信测试网络中可被容器访问的地址
python3 -m http.server 18083 --bind "$LAB_HTTP_BIND" --directory src/probes/browser
```

测试结束停止这个服务。需要观察：

- requestAnimationFrame计数持续增长；
- Canvas颜色/运动以及WebGL context建立；
- 滚动、按钮计数、输入文本；
- 访问实际HTTPS页面；
- 比较 native线程栈，确认没有卡在Gralloc PI lock；
- 冷启动后重新打开；am start返回ok不是画面通过。

evidence/2026-11-23中的图像是实际截图。WebGL版本字符串本身不证明GPU后端，要另看driver、renderer、实际设备调用和GPU探针。

## 网络

原实验 lab.arm13.probe/.Probe 在冷启动后记录：
```text
PRIVATE STORAGE: true
SHARED MEMORY: 4096 protect=true read=42
TLS https://example.com/ HTTP 200 bytes=128 TLS_AES_128_GCM_SHA256
```

仓库保留 NetProbe.java 用于补充同UID TCP/TLS/UDP检查。使用自己的可达测试端点并保留默认TLS校验；同UID测试必须实时核对包名到UID映射。socket是否实际已连接以poll和返回状态判断，不依赖perror打印Success。

验证服务重启、容器重启后选择仍生效，且没修改宿主BPF策略。VALIDATED要看真实204探测与Connectivity状态；不以删除验证条件换取标记。

## Codec与scrcpy

- Buffer输入：QTI AVC/HEVC编码/解码各30帧，并做像素对照。
- Surface输入：SurfaceCodecProbe的合成图案颜色正确；有效码流但全绿/黑仍失败。
- 实时：定制server + c2.qti.avc.encoder，真实窗口显示页面；通过窗口鼠标操作看到容器响应。
- 手机编码器和Debian --hwdec=vaapi分别核对，不把解码端CPU下降当编码端硬件已成功的唯一证据。
- 当前音频未启用，不扩大结论。

## 冷启动与保护对象

仅停止目标、确认镜像卸载、重新挂载启动，然后复核system_server/Zygote/netd、DHCP、代理配置、WebView、浏览器、硬编。保护容器PID/starttime、宿主HTTPS与Wi-Fi ADB应保持正常。

本轮历史验收没有执行一小时耐久测试、自动游戏脚本或登录后游戏业务。以后执行时新增日期和结果，不覆盖历史记录。

## 第二部分补充验收

Next/LSPosed真实回调、终端PTY、显示参数、stock scrcpy、代理UDP/DNS/认证的独立判据见[补充验证清单](part2/REPRODUCTION_AND_VALIDATION.md)。不要把核心启动通过等同于这些可选能力通过；repro冻结，不用本页命令连接它。
