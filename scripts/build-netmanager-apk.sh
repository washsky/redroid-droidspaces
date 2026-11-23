#!/usr/bin/env bash
set -euo pipefail
: "${AOSP_ROOT:?source lab.env first}"
: "${SIGN_CERT:?set your signing certificate}"
: "${SIGN_KEY:?set your signing key}"
r=$(cd "$(dirname "$0")/.." && pwd)
p="$r/src/netmanager"
o="$r/out/netmanager"
h="$AOSP_ROOT/out/host/linux-x86"
jdk="$AOSP_ROOT/prebuilts/jdk/jdk11/linux-x86"
api="$AOSP_ROOT/prebuilts/sdk/33/public/android.jar"
export LD_LIBRARY_PATH="$h/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
mkdir -p "$o/classes" "$o/dex"
"$jdk/bin/javac" -encoding UTF-8 -source 8 -target 8 -classpath "$api" -d "$o/classes" "$p/MainActivity.java"
"$h/bin/d8" --lib "$api" --min-api 33 --output "$o/dex" "$o"/classes/lab/arm13/netmanager/*.class
"$h/bin/aapt2" compile --dir "$p/res" -o "$o/res.zip"
"$h/bin/aapt2" link -I "$api" --manifest "$p/AndroidManifest.xml" --min-sdk-version 33 --target-sdk-version 33 "$o/res.zip" -o "$o/resources.apk"
python3 - "$o" <<'PY'
import sys,zipfile,shutil
from pathlib import Path
p=Path(sys.argv[1]);shutil.copy2(p/'resources.apk',p/'unsigned.apk')
with zipfile.ZipFile(p/'unsigned.apk','a') as z:z.write(p/'dex/classes.dex','classes.dex')
PY
"$h/bin/zipalign" -p -f 4 "$o/unsigned.apk" "$o/aligned.apk"
"$jdk/bin/java" -Djava.library.path="$h/lib64" -jar "$h/framework/signapk.jar" "$SIGN_CERT" "$SIGN_KEY" "$o/aligned.apk" "$o/UID-Network-Manager.apk"
echo "$o/UID-Network-Manager.apk"
