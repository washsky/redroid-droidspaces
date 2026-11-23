#!/usr/bin/env bash
set -euo pipefail
: "${AOSP_ROOT:?source lab.env first}"
: "${SCRCPY_SRC:?set patched scrcpy source tree}"
: "${SDK_PLATFORM_DIR:?set API36 platform directory}"
r=$(cd "$(dirname "$0")/.." && pwd)
s="$SCRCPY_SRC/server"
b="$r/out/scrcpy-server"
g="$b/gen"
h="$AOSP_ROOT/out/host/linux-x86"
jdk="$AOSP_ROOT/prebuilts/jdk/jdk11/linux-x86"
export LD_LIBRARY_PATH="$h/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
mkdir -p "$b/classes" "$g/com/genymobile/scrcpy"
cat > "$g/com/genymobile/scrcpy/BuildConfig.java" <<'JAVA'
package com.genymobile.scrcpy;
public final class BuildConfig { public static final boolean DEBUG=false; public static final String VERSION_NAME="5.0.1"; }
JAVA
"$h/bin/aidl" -o"$g" -I"$s/src/main/aidl" "$s/src/main/aidl/android/content/IOnPrimaryClipChangedListener.aidl"
"$h/bin/aidl" -o"$g" -I"$s/src/main/aidl" -p"$SDK_PLATFORM_DIR/framework.aidl" "$s/src/main/aidl/android/view/IDisplayWindowListener.aidl"
find "$s/src/main/java" "$g" -name '*.java' > "$b/sources.list"
"$jdk/bin/javac" -encoding UTF-8 -bootclasspath "$SDK_PLATFORM_DIR/android.jar" -classpath "$AOSP_ROOT/prebuilts/sdk/tools/core-lambda-stubs.jar:$g" -source 8 -target 8 -d "$b/classes" @"$b/sources.list"
# Do not add implicitly compiled java/lang/invoke lambda stubs to program input.
find "$b/classes/com" "$b/classes/android" -name '*.class' > "$b/classes.list"
"$h/bin/d8" --lib "$SDK_PLATFORM_DIR/android.jar" --min-api 21 --output "$b/classes.zip" @"$b/classes.list"
cp "$b/classes.zip" "$r/out/scrcpy-server-arm13-linear.jar"
echo "$r/out/scrcpy-server-arm13-linear.jar"
