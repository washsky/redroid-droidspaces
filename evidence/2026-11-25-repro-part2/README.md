# repro 第二部分验收摘要（脱敏）

本页从本次实际ADB、原版scrcpy、交互PTY、安装器和UI输出整理，属于摘要，不伪装成原始连续日志。原始私有安装日志、授权环境和备份只留实验机；不公开license、密钥、账号、真机serial或完整ADB列表。

范围、失败与修正、两次冷启动及未测项见[完整报告](../../docs/part2/REPRODUCTION_REPRO_20261125.md)，哈希和结构化结果见[清单](../../artifacts/part2-repro-20261125.json)。与上一份arm摘要分开记录。

| 检查 | 结果 |
|---|---|
| Next/LSPosed安装 | 官方Magisk安装exit0；license步骤Skipped，未复制其他容器授权 |
| 实际注入 | system_server与普通应用pre/post；Next无issue；LSPosed API102/system_server接入 |
| 负例/FD | 伪造peer不提升；真实SCM_RIGHTS成功；/dev仍noexec |
| 双冷启动 | 640×1136/280/50实际运行，再恢复720×1280/320/60；服务配置恢复 |
| 基础能力 | 普通应用存储/SharedMemory/HTTPS、GPU/AHB、真实交互PTY通过 |
| 代理 | TCP/HTTPS、SOCKS5 UDP双目标、两协议DNS、relay故障拒绝及恢复 |
| UI | 真正滚动与保存、认证字段、非法配置拒绝；APK1.2.1仅repro |
| 通用控制器 | root配置严格读取；缺配置/错误目标拒绝；规则与设置不变；正确配置boot/test成功 |
| stock视频 | QTI编码+VAAPI解码720×1280；中央60%像素MAE1.342922；临时媒体删除 |
| 最终状态 | Next+LSPosed；临时探针删除；SOCKS5 TCP开，UDP/DNS/认证关；broker名单未变 |
| 保护 | 真机bootID/Enforcing保持；arm与arch2 PID/starttime保持；未做全局操作 |

配置读取测试与真实控制器的错误地址验证是两层检查；“接受arm配置”仅指相同读取器接受该格式，未在受保护arm上部署或运行新控制器。通用控制器热更新后显式boot/test通过，未为它额外做第三次冷启动。基础Magisk生命周期、认证mock、大型编译及长时/游戏测试按范围跳过。
