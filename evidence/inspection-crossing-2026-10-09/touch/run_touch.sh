#!/data/data/com.termux/files/usr/bin/sh
set -u
base=/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/inspection-touch
label=${1:-after}
mode=${2:-primary}
case "$label" in *[!a-zA-Z0-9_-]*) exit 2;; esac
extra=''
if [ "$mode" = miss ]; then extra='--inspection-miss'; fi
proot-distro login ubuntu --shared-tmp -- /bin/bash -c "GODOT_SILENCE_ROOT_WARNING=1 timeout 360s /data/data/com.termux/files/usr/tmp/scraperx-godot-arm64/Godot_v4.7-stable_linux.arm64 --headless --audio-driver Dummy --fixed-fps 60 --resolution 432x432 --path $base/godot -- --uitest=touch_inspection_junction $extra" > "$base/$label.log" 2>&1
result=$?
printf '%s\n' "$result" > "$base/$label-exit-code.txt"
python "$base/summarize_receipt.py" "$label" --exit-code "$result"
exit "$result"
