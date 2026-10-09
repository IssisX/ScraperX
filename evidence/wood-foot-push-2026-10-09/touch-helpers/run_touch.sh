#!/data/data/com.termux/files/usr/bin/sh
set -u
base=/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/wood-foot-push-touch
label=${1:-first}
mode=${2:-jump}
project=${3:-$base/godot}
case "$label" in *[!a-zA-Z0-9_-]*) exit 2;; esac
case "$mode" in jump) extra='';; *) exit 2;; esac
case "$project" in *[!a-zA-Z0-9_./-]*) exit 2;; esac
proot-distro login ubuntu --shared-tmp -- /bin/bash -c "GODOT_SILENCE_ROOT_WARNING=1 timeout 360s /data/data/com.termux/files/usr/tmp/scraperx-godot-arm64/Godot_v4.7-stable_linux.arm64 --headless --audio-driver Dummy --fixed-fps 60 --resolution 432x432 --path $project -- --uitest=touch_wood_foot_push $extra" > "$base/$label.log" 2>&1
result=$?
printf '%s\n' "$result" > "$base/$label-exit-code.txt"
exit "$result"
