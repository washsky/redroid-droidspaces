#!/usr/bin/env bash
set -euo pipefail
: "${NDK_ROOT:?source lab.env first}"
r=$(cd "$(dirname "$0")/.." && pwd)
o="$r/out/native"
cc="$NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android33-clang"
mkdir -p "$o"
"${cc}++" -O2 -Wall -Wextra -Werror -shared -fPIC -fno-exceptions -fno-rtti -static-libstdc++ "$r/src/probes/zygisk/module.cpp" -llog -o "$o/arm13-zygisk-probe.so"
"$cc" -O2 -Wall -Wextra -Werror "$r/src/probes/zygisk/peer-negative.c" -o "$o/arm13-zygisk-peer-negative"
"$cc" -O2 -Wall -Wextra -Werror "$r/src/probes/zygisk/fd-transport.c" -o "$o/arm13-fd-transport"
python3 - "$o" <<'PY'
from pathlib import Path
import sys, zipfile
out = Path(sys.argv[1])
prop = ("id=arm13_zygisk_probe\nname=ARM13 Zygisk callback probe\n"
        "version=1\nversionCode=1\nauthor=redroid-droidspaces lab\n"
        "description=Logs only selected test apps and system_server callbacks\n")
with zipfile.ZipFile(out / "arm13-zygisk-probe.zip", "w", compression=zipfile.ZIP_STORED) as archive:
    for name, data in [("module.prop", prop.encode()),
                       ("zygisk/arm64-v8a.so", (out / "arm13-zygisk-probe.so").read_bytes())]:
        entry = zipfile.ZipInfo(name, (2026, 11, 25, 0, 0, 0))
        entry.create_system = 3
        entry.external_attr = 0o100644 << 16
        archive.writestr(entry, data)
PY
echo "Built callback ZIP and negative probes into $o; nothing installed."
