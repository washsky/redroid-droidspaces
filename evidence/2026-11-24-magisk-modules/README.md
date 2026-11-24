# 2026-11-24 Magisk 基础模块证据

仅新目标redroid13repro。完整范围见[验收说明](../../docs/MAGISK_MODULES_20261124.md)，产物见[哈希记录](../../artifacts/magisk-modules-20261124.json)。没有发布Magisk二进制、授权数据库或完整镜像。

- summary.json：实测检查、保护状态和支持边界。
- ui-install-staged.txt：真实管理器ZIP安装后的待更新模块。
- cold1-app-namespace-read.txt、cold2-app-visible.txt：当前普通UID及应用mount namespace中的测试文件/属性。
- cold3-disabled-verification.txt：禁用后脚本计数不增，属性与覆盖文件不存在。
- final-removed.txt：前端移除后uninstall事件，模块目录/属性/覆盖文件已清理。
- module-frontend.txt：卸载测试模块后保留Modules和Install from storage入口。
- final-peer-isolation.json：新容器共享挂载peer与宿主及两个保护容器无交集。
- cold1-gpu-log.txt、cold1-gralloc.txt、final-probe-log.txt：GPU/AHB/Gralloc及最终普通应用TLS、SharedMemory回归。

这是短时功能验证，未执行一小时耐久、Zygisk、全局策略注入、自动游戏或登录流程。
