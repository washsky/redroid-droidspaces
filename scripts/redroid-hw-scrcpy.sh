#!/usr/bin/env bash
set -euo pipefail
: "${INNER_ADB:?source lab.env first}"
r=$(cd "$(dirname "$0")/.." && pwd)
export SCRCPY_SERVER_PATH="${SCRCPY_SERVER_PATH:-$r/out/scrcpy-server-arm13-linear.jar}"
test -f "$SCRCPY_SERVER_PATH"
adb -s "$INNER_ADB" get-state >/dev/null
adb -s "$INNER_ADB" shell 'su -c "set -e; cp /vendor/lib64/arm13-codec-compat/libarm13_linear_window.so /data/local/tmp/libarm13_linear_window.so.new; chown 0:0 /data/local/tmp/libarm13_linear_window.so.new; chmod 0644 /data/local/tmp/libarm13_linear_window.so.new; mv /data/local/tmp/libarm13_linear_window.so.new /data/local/tmp/libarm13_linear_window.so; touch /data/local/tmp/arm13-linear-recording.enabled; chown 0:0 /data/local/tmp/arm13-linear-recording.enabled; chmod 0644 /data/local/tmp/arm13-linear-recording.enabled"'
exec scrcpy -s "$INNER_ADB" --no-audio --video-codec=h264 --video-encoder=c2.qti.avc.encoder --max-fps=20 --video-bit-rate=2M --hwdec=vaapi --window-title='ARM64 ReDroid 13 - Qualcomm Hardware H264' "$@"
