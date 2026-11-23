#!/usr/bin/env bash
set -euo pipefail
: "${NDK_ROOT:?source lab.env first}"
src=$(realpath "${1:?usage: build-mesa.sh /path/to/patched/mesa}")
r=$(cd "$(dirname "$0")/.." && pwd)
o="$r/out/mesa"
mkdir -p "$o"
ndk=$(realpath "$NDK_ROOT")/toolchains/llvm/prebuilt/linux-x86_64
python3 - "$ndk" "$o/cross.ini" <<'PY'
import sys
from pathlib import Path
n=sys.argv[1]
Path(sys.argv[2]).write_text("[constants]\nndk = '"+n+"'\n[binaries]\nc = ndk / 'bin/aarch64-linux-android33-clang'\ncpp = [ndk / 'bin/aarch64-linux-android33-clang++', '-fno-exceptions', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables', '-static-libstdc++']\nar = ndk / 'bin/llvm-ar'\nstrip = ndk / 'bin/llvm-strip'\nc_ld = 'lld'\ncpp_ld = 'lld'\npkg-config = '/usr/bin/false'\n[host_machine]\nsystem = 'android'\ncpu_family = 'aarch64'\ncpu = 'armv8'\nendian = 'little'\n")
PY
meson setup "$o/build" "$src" --cross-file "$o/cross.ini" --buildtype=release \
 -Dplatforms=android -Dplatform-sdk-version=33 -Dandroid-stub=true \
 -Dfreedreno-kmds=kgsl -Dvulkan-drivers=freedreno -Dvulkan-layers= \
 -Dgallium-drivers= -Dopengl=false -Dgles1=disabled -Dgles2=disabled \
 -Degl=disabled -Dgbm=disabled -Dglx=disabled -Dllvm=disabled \
 -Dandroid-libbacktrace=disabled -Dvalgrind=disabled -Dbuild-tests=false \
 -Dvideo-codecs= -Dxmlconfig=disabled -Dzstd=disabled -Dc_args=-fno-emulated-tls -Dcpp_args=-fno-emulated-tls
meson compile -C "$o/build" -j "${BUILD_JOBS:-4}"
echo "ICD remains in $o/build; check SONAME/NEEDED before deployment."
