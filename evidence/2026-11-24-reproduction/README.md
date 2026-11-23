# 2026-11-24 复现证据

命令输出为实际新容器数据，报告见 [复现验收](../../docs/REPRODUCTION_20261124.md)。

- boot/app/broker/gralloc-first：首次部署。
- cold-* / gpu-cold / surface-cold：目标停止、卸载并重新启动后的验证。
- cold-final-core 保留 Keystore 早期错误，避免隐去失败。
- protection-final：原容器/arch2 的 PID 与 starttime、宿主 HTTPS 和 Wi-Fi ADB。
- script-root-processes：新 UID 授权后的 root 引擎进程，仅证明进程存在，不代表脚本采集或游戏业务通过。
- PNG：实际 scrcpy 窗口，手工查看确认页面与操作结果。
