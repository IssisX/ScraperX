# Android compatibility checks

The `Android native compatibility` workflow exports the normal Main scene with
the real native Jolt module compiled for Android x86_64. This emulator-only
export adds `android_compatibility_probe.gd` as a read-only observer; it never
advances physics, grants support, changes settings or generates input.
The shipping ARM64 preset does not install that observer. The emulator export
uses the same signing lineage and source/build identity as phone downloads;
its final signature is checked at API24 before installation.

ChatGPT's final manifest adds only `fontScale` to the pinned Godot activity's
existing configuration mask (`0x3ff4` to `0x40003ff4`). A controlled early font
change reproduced engine teardown on activity recreation. The existing
viewport-sized game UI handles the change without replacing the activity;
this does not establish handling for future Android-native text widgets.
Signing rejects an unexpected package, activity or baseline mask, and verifies
that the final manifest differs only in the authorized version/configuration
fields. Other model packages retain their existing configuration policy.

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
the ordinary forklift arrival/control/exit and crane booth/hoist/neutral/exit
paths. Crane acceptance requires actual suspended-load rise and positive native
work/source use, not just a shorter cable setting. System bars, orientation
behavior, and the remaining normal parkour loop remain part of R17 until
actually exercised.

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
The CI renderer uses the supported `swangle` backend instead of deprecated
`swiftshader_indirect`. Plain `swiftshader` also selects the old implementation
on Linux; SwANGLE uses ANGLE over the newer SwiftShader Vulkan backend.
The previous backend rejected built-in Godot scene and
canvas shaders on all three booted profiles. Game lighting is unchanged;
successful rendering still requires actual emulator observations.
The driver rejects shader-link/script/resource failures even when the native
simulation starts. Disposable emulators start with Android's one-time fullscreen
tutorial already acknowledged through its OS setting. Game input remains
ordinary touch. This replaces unreliable accessibility-only tutorial dismissal;
it changes no app setting or permission.

Godot's points are pixels inside its window. ADB uses the whole display. The
driver reads the actual activity frame from Android's window manager, requires
dimensions matching the native observation, and adds its measured origin to
all touch points. It never guesses letterboxing or scaling. This handles the
foldable's observed441pixel vertical offset and records frame evidence.
Only controls past the actual touch owner's appearance threshold are published.
Startup waits for those controls, not merely the earlier native-ready signal.
Settings restart evidence also retains the actual persisted bytes and checks
their nonempty checksum against the observer; an empty-file hash is insufficient.

Held gestures use separate Android DOWN/MOVE/UP events. API35/36 use the
platform `input touchscreen motionevent` command. API24 uses its stock Monkey
network touch protocol because that shell predates `motionevent`. The driver
waits for actual input-owner observations and advancing native ticks, holds
for a route pulse or until the unchanged required gameplay response, releases in `finally`, and requires
observed cleared touch ownership. Cleanup errors remain failures. A slow
software-rendered frame cannot silently consume the whole finite shell swipe.
The displacement, machine work and real-support assertions remain unchanged.

Only the emulator export inserts passive receipts after TouchControls handles
an event and after Main advances the native world. These record actual drag
vectors, accepted world input and native ticks; they never issue commands.
The ordinary phone export retains the original scripts. The tablet also
exercises the real notification shade and emulator rotation, requiring OS
rotation readback, retained native identity and usable touch afterward.
These paths remain unverified until the corresponding Android jobs pass.

Production disables Android's scroll deadzone so small joystick/look drags
reach the existing input owner. CI retains its original gestures and native
movement checks. Startup focus is observed before touch: only the captured
bundled Messenger crash dialog can be dismissed once in a disposable emulator;
game crashes, unknown dialogs and missing focus return still fail. Existing
Resume controls handle any resulting pause.

Main also reads the Android activity's actual window focus before consuming
input or advancing physics. The notification shade can take focus while the
activity stays resumed, so activity pause notifications alone are insufficient.
Focus loss uses the existing pause/held-input cleanup owner; focus return never
automatically resumes play. The tablet test still requires a frozen native
clock under the real shade and an ordinary Resume afterward.

The API24 server must answer a harmless `listvar` handshake before any touch;
an accepted ADB-forwarded TCP connection alone does not prove device readiness.
At full tablet resolution, the new held driver produced117 actual nonzero-input
native ticks and almost4m of movement, but its added108-tick hold exceeded90s
on the software GPU. Basic movement now releases after the original displacement
check and actual nonzero native input, rather than waiting an unnecessary extra
duration. Camera input similarly waits for its original yaw response.

Each profile starts at its actual full-size window and must crouch/stand through
real touch before extended functional checks use a real800x450 Android window.
The API36 tablet previously displayed Android's “System UI isn't responding”
dialog over the game; its screenshot reported0.1FPS and14937.5ms frame p95.
Reducing only the disposable emulator's display bounds controls software-rendering
cost. The driver still requires a separate large-window layout check, restores
native display dimensions before physical fold/unfold, and requires the original
window again after process restart. All transitions must preserve the expected
native instance. Failures restore the owned display override during cleanup.
The app's quality, render scale, shadows, content, forces and solver are unchanged.
This provides no physical-phone performance claim.

Machine-route walking uses100ms held pulses, observed native time between
samples, and dimensioned normalized speed requests. The previous650ms controller
oscillated around the forklift door on the actual tall emulator. The sampled
finite-acceleration regression can reject that controller, but Android arrival,
actual cab/booth support, work, neutral and exit checks remain authoritative.
Device jobs have a40-minute maximum for software-rendered functional checks;
all position, support, force/work, cleanup and persistence conditions still apply.

`Android startup replay (diagnostic, not release)` can reuse an already built,
identified emulator APK while changing only diagnostic harness code. It records
both source SHAs, the APK checksum, raw Android configuration before/after, and
continuous events-buffer logs around the same complete compatibility driver.
API35 `wm_relaunch_resume_activity`/`wm_relaunch_activity` events include the
configuration-change mask; `wm_destroy_activity` records a reason. This is
necessary when ordinary logcat shows stock Godot teardown before startup but
does not identify why Android recreated the activity. No failed launch is
retried, and a diagnostic run never replaces required release checks.

The startup replay optionally applies a real Android system font-size change
immediately after the first activity launch. It records original/requested/readback
values, captures continuous lifecycle events, and restores the original OS setting.
A natural replay of the identified APK passed23 checks without reproducing the
spontaneous startup teardown; that pass does not identify the original trigger.
The controlled configuration input is diagnostic, not a game-state setter or a
launch retry. No activity configuration mask is changed without supporting evidence.

The foldable matrix now runs that same full compatibility driver through the
startup replay, with the real early `font_scale=1.25` input. Its existing
fold/unfold, control, persistence and same-instance requirements remain intact.
The reproduced failure and proposed repair do not establish a repaired Android
run until the edited-source matrix completes.
