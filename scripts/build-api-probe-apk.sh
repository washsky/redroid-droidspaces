#!/usr/bin/env bash
set -euo pipefail
: "${ANDROID_BUILD_TOOLS:?set ANDROID_BUILD_TOOLS}"
: "${ANDROID_JAR:?set ANDROID_JAR}"
: "${SIGN_CERT:?set SIGN_CERT; never commit a signing key}"
: "${SIGN_KEY:?set SIGN_KEY}"
r=$(cd "$(dirname "$0")/.." && pwd)
o="$r/out/api-probe"
mkdir -p "$o/classes" "$o/dex"
javac -encoding UTF-8 -source 8 -target 8 -classpath "$ANDROID_JAR" -d "$o/classes" "$r/src/probes/api-apk/Probe.java"
"$ANDROID_BUILD_TOOLS/d8" --lib "$ANDROID_JAR" --min-api 33 --output "$o/dex" "$o"/classes/lab/arm13/probe/*.class
"$ANDROID_BUILD_TOOLS/aapt2" link -I "$ANDROID_JAR" --manifest "$r/src/probes/api-apk/AndroidManifest.xml" --min-sdk-version 33 --target-sdk-version 33 -o "$o/resources.apk"
python3 - "$o" <<'PY'
import pathlib,sys,shutil,zipfile
p=pathlib.Path(sys.argv[1]);shutil.copy2(p/'resources.apk',p/'unsigned.apk')
with zipfile.ZipFile(p/'unsigned.apk','a') as z:z.write(p/'dex/classes.dex','classes.dex')
PY
"$ANDROID_BUILD_TOOLS/zipalign" -p -f 4 "$o/unsigned.apk" "$o/aligned.apk"
"$ANDROID_BUILD_TOOLS/apksigner" sign --key "$SIGN_KEY" --cert "$SIGN_CERT" --out "$o/ARM13-API-Probe.apk" "$o/aligned.apk"
"$ANDROID_BUILD_TOOLS/apksigner" verify "$o/ARM13-API-Probe.apk"
echo "$o/ARM13-API-Probe.apk"
