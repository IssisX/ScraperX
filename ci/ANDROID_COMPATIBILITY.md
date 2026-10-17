# Android compatibility checks

The `Android native compatibility` workflow exports the normal Main scene with
the real native Jolt module compiled for Android x86_64. This emulator-only
export adds `android_compatibility_probe.gd` as a read-only observer; it never
advances physics, grants support, changes settings or generates input.
The shipping ARM64 preset does not install that observer. The emulator export
uses the same signing lineage and source/build identity as phone downloads;
its final signature is checked at API24 before installation.

`run_android_compatibility.py` installs the APK and sends ordinary Android touch
events through adb. Observations must show real native control, a 90 Hz step,
pause freezing ticks, background/return preserving the instance, resizing
without discarding native state, and player settings surviving process restart.
Timeouts fail; elapsed time alone never establishes success. Screenshots,
logcat, commands, source SHA and APK checksum are retained per profile.

Initial matrix:

| Profile | API | Hardware profile |
| --- | --- | --- |
| Compact phone | 24 | Nexus 5X |
| Tall phone | 35 | Pixel 7 |
| Tablet | 35 | Pixel C |
| Foldable | 35 | Pixel Fold |

API 24 matches the existing native compilation floor. A green matrix would
establish only these exercised configurations, not an advertised device floor.
Host acceleration and available profiles are captured before emulator launch.
The foldable driver requests the device's reported open/closed states and
requires Android state readback, changed display bounds and retained native
identity. A green resize alone is not fold/posture evidence. Android
cutouts/system bars, orientation behavior, and the complete
normal forklift/crane/parkour loop remain part of R17 until actually exercised.

The separate `android_responsive_controls_test.gd` checks 576 combinations of
canvas, orientation, player touch size, inset and interaction context through
the real touch controls. It covers active hit bounds, overlap, target capture,
movement and independent looking. Injected safe rectangles test geometry, not
Android's reporting of a notch or system bar.

Focused host commands:

```sh
python3 -m unittest discover -s ci -p test_android_compatibility.py
godot --headless --path godot --script res://tests/android_responsive_controls_test.gd
```

Emulator graphics are software-rendered CI evidence, never physical-device FPS,
heat or battery evidence. The separately built, signed ARM64 APK candidate is
the phone download; an emulator fixture is not a tested Play Store release.
Representative physical-device measurements and matching release acceptance
remain required before setting store exclusions or claiming broad support.
