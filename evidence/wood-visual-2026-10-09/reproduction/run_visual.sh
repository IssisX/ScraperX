#!/data/data/com.termux/files/usr/bin/sh
set -u
base=/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/wood-visual
label=${1:-baseline}
case "$label" in *[!a-zA-Z0-9_-]*) exit 2;; esac
proot-distro login ubuntu --shared-tmp -- /usr/bin/env LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=softpipe GODOT_SILENCE_ROOT_WARNING=1 /usr/bin/timeout 120s /usr/bin/xvfb-run -a -s '-screen 0 648x432x24' /data/data/com.termux/files/usr/tmp/scraperx-godot-arm64/Godot_v4.7-stable_linux.arm64 --path "$base/$label" --rendering-method gl_compatibility --audio-driver Dummy --fixed-fps 60 --resolution 648x432 --script res://wood_visual_probe.gd > "$base/$label.log" 2>&1
status=$?
printf '%s\n' "$status" > "$base/$label-exit-code.txt"
exit "$status"
