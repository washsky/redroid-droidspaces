#!/usr/bin/env bash
set -euo pipefail
: "${NDK_ROOT:?set NDK_ROOT}"
r=$(cd "$(dirname "$0")/.." && pwd)
o="$r/out/management"
cc="$NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android33-clang"
mkdir -p "$o"
for name in proxyctl bootctl; do
    "$cc" -O2 -Wall -Wextra -Werror -static \
        "$r/src/netmanager/$name.c" -o "$o/arm13-$name"
done
"$cc" -O2 -Wall -Wextra -Werror -static \
    "$r/tests/test-instance-guard.c" -o "$o/test-instance-guard"
echo "Built reusable controllers and config test into $o; nothing installed."
