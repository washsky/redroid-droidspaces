#!/usr/bin/env bash
set -euo pipefail
: "${NDK_ROOT:?source lab.env first}"
r=$(cd "$(dirname "$0")/.." && pwd)
o="$r/out/native"
cc="$NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android33-clang"
mkdir -p "$o"
flags=(-O2 -Wall -Wextra)
"$cc" "${flags[@]}" "$r/src/netmanager/netbroker.c" -o "$o/arm13-netbroker"
"$cc" "${flags[@]}" "$r/src/netmanager/netctl.c" -o "$o/arm13-netctl"
"$cc" "${flags[@]}" -shared -fPIC -fvisibility=hidden "$r/src/netmanager/preload.c" -ldl -o "$o/libarm13_socketbroker.so"
"$cc" "${flags[@]}" "$r/src/root/magisk-daemon-guard.c" -o "$o/arm13-magisk-daemon-guard"
"$cc" "${flags[@]}" -shared -fPIC "$r/src/root/magisk-context-guard.c" -o "$o/libmagisk_container_context.so"
"$cc" "${flags[@]}" -shared -fPIC "$r/src/surface/codec-vulkan-select.c" -ldl -o "$o/libarm13_codec_vulkan.so"
"$cc" "${flags[@]}" -shared -fPIC -I"$r/src/surface/platform-headers" "$r/src/surface/linear-window.c" -landroid -ldl -o "$o/libarm13_linear_window.so"
"$cc" "${flags[@]}" "$r/src/gralloc/ahb-is-supported.c" -landroid -o "$o/arm13-ahb-is-supported"
"$cc" "${flags[@]}" "$r/src/gralloc/find-gralloc-leak.c" -landroid -o "$o/arm13-find-gralloc-leak"
"$cc" "${flags[@]}" "$r/src/gralloc/isolated-probe.c" -o "$o/arm13-isolated-gralloc-probe"
"$cc" "${flags[@]}" "$r/src/probes/read-gralloc-mutex.c" -o "$o/arm13-gralloc-lock-read"
"$cc" "${flags[@]}" "$r/src/probes/binderfs-add.c" -o "$o/arm13-binderfs-add"
echo "Built into $o; nothing installed on a device."
