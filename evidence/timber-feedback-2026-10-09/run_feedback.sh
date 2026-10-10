#!/data/data/com.termux/files/usr/bin/sh
set -u
base=/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/timber-feedback
proot-distro login ubuntu --shared-tmp -- /usr/bin/env GODOT_SILENCE_ROOT_WARNING=1 /usr/bin/timeout 120s /data/data/com.termux/files/usr/tmp/scraperx-godot-arm64/Godot_v4.7-stable_linux.arm64 --headless --path "$base/runtime" --audio-driver Dummy --script res://feedback_probe.gd > "$base/feedback.log" 2>&1
status=$?
printf '%s\n' "$status" > "$base/exit-code.txt"
exit "$status"
