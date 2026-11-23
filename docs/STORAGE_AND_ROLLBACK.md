# 存储扩容与回退

## 本次结果

候选镜像原逻辑大小 6,442,450,944 字节，扩至 12,884,901,888 字节，即增加6 GiB。/ 和 /data 共用该 ext4 rootfs。原镜像 rootfs.img 未覆盖。

扩容前完整备份的 SHA256 与当时活跃候选一致：
`91ffd29ca685e6e46d4ab93660e591d80baf99cc0f0b7d6deed8cdf145ba6d75`

这是**停止后、扩容前**快照哈希，不是原始构建哈希，也不是之后运行镜像的固定哈希。

## 离线操作

scripts/expand-ext4-image.py 是在能访问镜像文件的 Linux/Android环境下执行的辅助工具，不是 ADB全自动管理器。调用者先使用 Droidspaces只停止目标容器，确认外层/内层没有残留挂载和loop关联。工具遇到挂载/loop会拒绝，不替你 detach。

```bash
# 先停止正确的目标容器，检查宿主空间，并准备离线镜像文件
python3 scripts/expand-ext4-image.py /path/to/candidate.img --add-gib 6 --backup /path/to/independent-pre-expand.bak
```

顺序：拒绝覆盖备份 → 完整复制和SHA校验 → e2fsck -f -p → truncate增加逻辑长度 → resize2fs → e2fsck -f -n。若中途失败，保留原备份；工具不自动覆盖回去。

真机直接操作可用其 /system/bin/e2fsck、resize2fs、truncate，语义相同。不要对已挂载的loop运行离线e2fsck；不要把操作加到每次启动脚本。

同步 config 的 sparse_image_size_gb 为新值，但配置数字本身不会替代真正的 ext4扩容。启动后检查 df 和写入/读取，确认应用看到新容量。

## 空间规划

备份需要能容纳完整逻辑镜像（稀疏占用视文件系统/复制工具而定），resize也可能分配元数据空间。不要看到稀疏文件的小du占用就认为不需要宿主空间。当前12GiB使用量后续已增长，见STATUS.md。

## 回退分层

1. 应用配置问题：先导出当前管理器选择、无障碍设置，恢复该应用对应配置。
2. 图形库：停止目标，恢复精确原库和相关rc；保留其他设备策略。旧Gralloc会重新出现已知漏锁风险，不能作为长期“修复”。
3. Socket broker：恢复原Zygote service的preload设置，停用服务，保留包名配置备份。
4. Root：恢复原AOSP su及其init配套；不修改宿主boot/policy。
5. 镜像级：停止并确认卸载目标，将container.config切换到新的恢复副本。不要直接覆盖正在使用的镜像，也不要覆盖唯一备份。
6. 扩容后的镜像不要用truncate缩回6GiB。回退使用扩容前独立备份的新副本；如确需缩容，另行离线规划，不能按文件大小硬截断。

arch2、宿主、其他ADB设备始终不是上述回退操作的目标。
