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
| Tall phone | 36 | Pixel 7 |
| Tablet | 36 | Pixel C |
| Foldable | 35 | SDK 7.6in Foldable |

API 24 matches the existing native compilation floor. API36 also exercises the
Android version currently targeted by the export toolchain. A green matrix would
establish only these exercised configurations, not an advertised device floor.
The compact API24 profile is an older-OS diagnostic, not a promise to support
old hardware. Android version and simulated screen size do not establish GPU,
RAM, sustained frame time or thermal capacity. Devices unable to deliver the
intended experience remain unsupported; physics and visual depth are not
reduced merely to make an old profile pass. Owner correction: no fidelity,
mechanic, content or physics cuts to widen device support. Improve efficiency
without changing the intended experience; exclude insufficient hardware.
Host acceleration and available profiles are captured before emulator launch.
The foldable driver uses the emulator's physical fold/unfold controls and
requires Android state readback, changed display bounds and retained native
identity. A green resize alone is not fold/posture evidence. The tall-phone
job enables a real Android notch overlay, checks reported insets and touch
bounds, exercises input and then restores the original display. It also runs
the ordinary forklift arrival/control/exit path. System bars, orientation
behavior, and the complete
normal forklift/crane/parkour loop remain part of R17 until actually exercised.

Android x86_64 uses the NDK ABI CPU baseline; Jolt desktop AVX/FMA/BMI
defaults are disabled for that ABI because they are optional on Android. See
[the Android ABI contract](https://developer.android.com/ndk/guides/abis).
Unchanged native compilation can be reused through ccache; CMake still
configures the actual source and links each export.

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

Observer messages are split into base64 packets below Android's per-print
limit. Only a complete observation from one instance and sequence is accepted;
a truncated or missing packet cannot count as playable state.
The CI renderer uses the supported `swiftshader` backend instead of deprecated
`swiftshader_indirect`. The previous backend rejected built-in Godot scene and
canvas shaders on all three booted profiles. Game lighting is unchanged;
successful rendering still requires actual emulator observations.
