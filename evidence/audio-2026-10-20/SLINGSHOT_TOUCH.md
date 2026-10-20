# Grounded slingshot feedback — 2026-10-20

The production launcher emitted an explicit spring wobble after every launch and a swept squeak at high draw. These cues are replaced by a dry ratchet, friction creak and short band snap. Native ratchet travel and launch receipts drive the observer; presentation never advances the mechanism. Click spacing is a 0.12 m audible-spacing hypothesis, not simulated tooth geometry.

New input acceptance targets touch. Existing desktop support and release gates are retained. The owner permits simplifying insignificant simulation detail, not expanding cartoon/arcade styling.

## Actual affected acceptance

Host: Termux/proot Ubuntu, Godot 4.7 Linux ARM64, published e9c6a24 native layout plus the audio getter/bridge additions and the committed Godot audio slice. Native forces, substeps, controller laws and worker count are unchanged.

Command:

```sh
/data/data/com.termux/files/usr/bin/proot-distro login --bind /data/data/com.termux/files/usr/tmp:/data/data/com.termux/files/usr/tmp --bind /data/data/com.termux/files/home:/data/data/com.termux/files/home --bind /storage/emulated/0:/storage/emulated/0 --env GALLIUM_DRIVER=softpipe --env LP_NUM_THREADS=1 ubuntu -- xvfb-run -a -s '-screen 0 160x120x24' /data/data/com.termux/files/home/.local/share/godot/4.7/Godot_v4.7-stable_linux.arm64 --resolution 160x120 --window-size 160x120 --path /data/data/com.termux/files/usr/tmp/scraperx-touch-playability-20261020/source/godot --rendering-method gl_compatibility --fixed-fps 30 --audio-driver Dummy --write-movie /data/data/com.termux/files/usr/tmp/scraperx-touch-playability-20261020/slingshot-audio.avi --script res://tests/slingshot_audio_test.gd
```

Observed output: `SCRAPERX_SLINGSHOT_AUDIO PASS checks=145 mix_peak_db=-0.80 driver=Dummy touch_events=56`; exit 0; no SCRIPT ERROR or Parse Error. MovieMaker rendered 1,006 audio-proof frames. Three actual touch shots exercised draw, ratchet, launch, native-speed rush, Effects mute, cinematic pause/resume and recreated-scene retry. Each produced exactly one release and zero comedy cues.

This proves actual mixer output despite the Dummy driver, not Android device playback quality or gameplay frame rate. The viewport disables 3D; the approximately 208 s MovieMaker wall time is not a phone performance measurement.

Accepted supporting evidence: native-free observer checks 25 PASS; 55 unrelated PCM variants byte-identical to original HEAD and RNG state preserved. Exact receipts remain in `/data/data/com.termux/files/usr/tmp/scraperx-slingshot-audio-20261020/` and `/data/data/com.termux/files/usr/tmp/scraperx-touch-playability-20261020/` (`slingshot-audio-{command,result,source}.json`, `slingshot-audio.log`).

## Remaining

Phone listening quality and new-source Android delivery require the matching artifact. Urgent whole-game lag is a separate active fix; this audio acceptance does not prove it repaired. Rigging and unrelated hook work remain unpublished and preserved.
