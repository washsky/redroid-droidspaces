#!/usr/bin/env bash
# Read-only target inspection. No connect/disconnect/restart or policy mutations.
set -eu
: "${HOST_ADB:?source lab.env first}"
: "${INNER_ADB:?source lab.env first}"
adb -s "$HOST_ADB" get-state
adb -s "$HOST_ADB" shell getenforce
adb -s "$INNER_ADB" shell 'getprop ro.build.version.release; getprop ro.product.cpu.abi; getprop sys.boot_completed; pidof system_server; pidof zygote64; pidof netd; df -k /data'
adb -s "$INNER_ADB" shell 'getprop ro.hardware.egl; getprop ro.hardware.vulkan; getprop debug.hwui.renderer; getprop sys.use_memfd; getprop persist.zygote.app_data_isolation'
adb -s "$INNER_ADB" shell 'su -c "ls -l /dev/ashmem; cat /proc/misc | grep ashmem; /vendor/bin/arm13-netctl status; sha256sum /vendor/lib64/libgralloccore.so /system/lib64/libandroid_runtime.so /vendor/bin/ipconfigstore"'
adb -s "$INNER_ADB" shell 'dumpsys webviewupdate'
