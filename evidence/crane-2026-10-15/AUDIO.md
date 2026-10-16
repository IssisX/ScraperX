# Yard crane receipt audio — 2026-10-15

Added `presentation/audio/yard_crane_audio.gd`, three bounded spatial Effects voices: winch, trolley motor, slew drive. Three deterministic finite .25-second mono PCM resources are cached across instances and loop without per-frame synthesis. Native signed target magnitudes gate motors; native N / N m and positive J deltas modulate bounded gain/pitch. A loaded stationary brake is silent. No invented failure/contact sound, idle drone, control state or physical authority.

Winch/trolley originate at native `lines[0].from` (actual trolley sheave). Slew originates at the current native geometry bearing root (-60, 36, -150), a documented constant pending a bearing position receipt. Invalid/missing line positions fall back to that root. Sources have inverse-distance attenuation and 100 m maximum distance. Operator mode does not gate sound: native machine demand does. Pause, unavailable receipt and exit-tree reset all voices and work history.

Main instantiates the node, updates it alongside crane rendering from the same native receipt, and resets it when pausing. No bridge/native/control/view files changed by this task.

## Focused execution

Godot 4.7 stable official 5b4e0cb0f, proot Ubuntu, headless Dummy audio; private project `/data/data/com.termux/files/usr/tmp/crane-audio-private/godot` copied from the assigned operator UI base, with only the new audio/test files overlaid. First test execution rejected the absent implementation (exit 1). After implementation, the focused fixture exited 0:

`CRANE_AUDIO failures=0 driver=Dummy receipt_fixture_only=1 heard_device_audio=0`

The fixture checks idle loaded-brake silence, three source limit, Effects routing, nonzero unclipped PCM, signed hoist demand and actual source position, force/work modulation, independent transport sources, pause/unavailable silence, nonfinite receipts and stream reuse. Engine root/proot host warnings occurred; no script or test errors in the passing run.

This is source/player/PCM/lifecycle fixture evidence. It does not establish current native Main integration at runtime, physical crane motion, audible device output, or subjective mix quality. The lead owns matching current bridge/Main runtime validation.
