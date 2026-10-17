# Android runtime repairs — 2026-10-17

Observed baseline: ChatGPT35ed804dc81476430d777e623eecc5653a635cda, Android compatibility run39045102640.

- Compact API24 reached real native gameplay. Its later observer JSON was truncated by the Android per-print limit; complete observations could not be accepted.
- Tall and tablet API36 crashed in the Godot mono16 WAV decoder. In both crash records, PCM base plus sample88200 times2 equals the fault address. This is the startup wind loop endpoint. Pinned engine5b4e0cb0f includes loop_end in decoding but resumes at loop_begin+1. One copied head frame makes this read valid and preserves the original period. Four procedural loop constructors are repaired; no production audio is disabled.
- The signed build_info.json had86bytes while its sparse index retained81. Updating that one entry length/MD5 lets Godot read the exact version/source; other pack bytes remain unchanged.
- Legacy SwiftShader rejected built-in scene and canvas shaders. Use the supported swiftshader backend, retaining game lighting. Runtime rendering acceptance is pending.
- The SDK has no pixel_fold profile. Use actual7.6in Foldable with hinge metadata and physical console fold/unfold, retaining Android state/bounds/instance checks.

## Focused evidence

- python3 -m unittest discover -s ci -p test_android_compatibility.py:6tests, OK (ABI/export isolation and complete/incomplete/interleaved packet decoding).
- Actual Godot4.7 probe parse: ANDROID_PROBE_PARSE PASS. Actual Godot Unicode packet emission plus Python reassembly: REAL_GODOT_PACKETS PASS count=6 max_bytes=787.
- Signing/build identity: python -m unittest discover -s ci -p test_build_info.py:9tests, OK. Actual fixture sparse-resource reproduction reads truncated81bytes before repair and complete86bytes afterward; BuildInfo.read_identity returns the exact source/version. Fixture prepare+verify passes.
- New PCM regression on pinned Godot4.7 LinuxARM64: baseline exit1,139checks/42failures, including31 wrong repeated samples; repaired exit0,403checks/0failures. Existing grapple/forklift/crane/ambience audio checks exit0. Decoder/resampler execution is host evidence, not Android audio-hardware proof.

Exact audio commands, logs and source hashes: /data/data/com.termux/files/usr/tmp/scraperx-audio-loop-20261017/results.json and source-receipt.json. Identity reproduction: /data/data/com.termux/files/usr/tmp/scraperx-build-identity-35ed804-repro/. Android artifacts: /data/data/com.termux/files/usr/tmp/scraperx-android-compatibility-20261017/emulator-35ed804/artifacts/.

## Remaining acceptance

Rebuild and observe the real native game on each emulator, then complete ordinary controls/lifecycle/fold/cutout/machine/parkour acceptance and deliver its matching phone APK. Physical devices establish sustainable frame time, memory and heat limits. Old OS diagnostics do not promise old hardware support. No physics, fidelity, mechanic or content cuts merely to widen device support; insufficient hardware remains unsupported. Existing catch/inspection scope remains deferred.

## Actual Android follow-up and input coordinates

faf3bf8 run39046689920: all four profiles initialized the real native game with90Hz step and populated source identity. Tablet API36 completed crouch/stand/jump/move/look, pause/frozen ticks/resume, HOME/return, resize/reset and process restart with the persisted hash. Overall job still failed: the extra emulator -version diagnostic loaded desktop libpulse.so.0, unavailable on this headless host. Remove that unnecessary second emulator launch; normal launch already logs its version. Compact warm restart remains unresolved (the log shows other Android services dying too); do not attribute it to game code without discriminating evidence.

7fdee47 SwANGLE run39047046331: actual scene rendering replaces the prior gray failure; fold failure screenshot confirms a1768x1326 game window centered inside1768x2208 display. Window-relative controls were missing the441pixel Android letterbox origin, so ADB tapped above them. The native-ready signal also preceded the first interactive touch-control frame. Read actual WindowManager activity frames, require their dimensions to match the native window, and derive all ADB positions by adding the measured origin. Publish controls only after their actual input appearance threshold; wait for interactive first-person controls.

Android fullscreen tutorial setup now uses the real OS confirmation key before launch in disposable emulators, with readback. This replaces accessibility-only dismissal: API24 only exposes the focused accessibility root, modern UIA dump can fail during activity transitions. Pinned API24/35/36 source observes the OS key live. No app setting, native state or permission is manufactured. Source references and caveats: /data/data/com.termux/files/usr/tmp/scraperx-android-compatibility-20261017/android-fullscreen-source-findings.txt.

The normal prepared arrival stands on actual dynamic cargo beam2446 (source WarehouseForkliftYard::kBeamEntity), at player centerY1.86m. The existing native freight_yard_arrival_tests requires actual solved footing, not a bare-gradeY<1m. Correct the harness's false height assumption; retain actual grounding, independent cab support, reach authorization, force/work and exit checks. Crane touch coverage is implemented and integrated: ordinary service-strip approach, actual booth support1450, Action, physical suspended-load rise plus work/source use, all targets neutral, Done, grade exit. Static clearance/contract checks pass; Android machine navigation remains pending.

Focused follow-up commands: python3 -m unittest discover -s ci -p test_android_compatibility.py ->9tests OK; python3 /data/data/com.termux/files/usr/tmp/android-crane-route-check/check.py ->syntax/negative-hoist/neutral/footing checks and conservative2378solid route clearance PASS; pinned Godot4.7 probe parse ->ANDROID_PROBE_PARSE PASS. Current audio/signer evidence remains valid because those owners are unchanged. Settings restart evidence now also retains actual persisted bytes and requires a nonempty matching checksum; the previous empty-file hash alone was insufficient.
# Held Android input follow-up — 2026-10-17

Actual GitHub run39048317276 on92bd194: all four profiles initialized real
native Main. Compact, foldable and tablet passed crouch, stand and native jump
work, then failed stick displacement. Tall passed actual arrival footing but
made no walking progress in twelve short touch pulses. This is not green.

AOSP API24 Input.java starts its finite swipe clock before synchronous DOWN;
its first MOVE remains at the origin. A slow delivery can consume the duration
before a nonzero drag exists. Newer finite swipe commands have the same
wall-clock problem. These source facts support repairing the driver; the
captured run did not identify the exact failing input stage.

The next source uses separate actual DOWN/MOVE/UP events, held across native
ticks. API24 uses stock Monkey's network touch commands; API35/36 use platform
motionevent. Passive emulator-only owner/step receipts distinguish missing
drag, input lost before sampling, and accepted native intent without motion.
UP is guaranteed through cleanup and actual ownership release is required.
No game controller, physics, fidelity, displacement or work check was weakened.

Local commands and actual output before publication:

- `python3 -m unittest discover -s ci -p test_android_compatibility.py`:9 OK.
- `python3 -m unittest discover -s ci -p test_android_touch_driver.py`:4 OK,
  including stationary holds, failed MOVE, failed observation and failed UP/CANCEL cleanup.
  Initial failures exposed an overstrict cleanup counter and rounded-down
  duration; both driver defects were corrected before the passing run.
- Pinned Godot4.7 ARM64, isolated prepared touch/probe project,
  `--headless --script res://tests/parse.gd`:
  `ANDROID_TOUCH_OBSERVER dispatch_events=3 drags=1 native_tick_receipts=9 released=true`.
  This exercises observer wiring, not real Android or native movement.

Follow-up actual96e5b51 compact job117446514855: native startup/crouch/stand/jump
passed. API24 forwarded TCP connected, but first DOWN returned EOF before any
protocol readiness receipt; UP also returned EOF. This is an introduced driver
startup defect, not evidence about movement. Local correction requires stock
Monkey's harmless `listvar` response before touch and retries closed forwarded
connections. Focused driver tests now5 OK, including a connected-but-not-ready
socket followed by a valid protocol response. Android replay remains pending.

Actual96e5b51 tablet job117446514808 proves held movement: accepted tick552,
then tick651, nonzero-native-input ticks117, position z=-47.681446 from
-43.721031. It failed the newly added108-tick duration (target660) before90s,
then actual release cleared ownership at tick687. This is software-rendered
test-clock exhaustion, not absent player movement. The original displacement
assertion remains unchanged; the next driver releases on that actual response
and positive native input, rather than requiring an extra duration.

Actual tall job117446514856: four real held pulses move from normal arrival to
(-98.783974,0.90,-56.690041), with378 nonzero-input native ticks. The240s
approach deadline expires while real walking succeeds. Long machine routes now
use a real960x540 Android window and restore original full size afterward. No
app graphics, native owner, machine, displacement or work limit was changed.
This is an emulator workload correction, not a phone performance improvement.
Observer cadence becomes0.1s for closed-loop input feedback. Device job maximum
becomes40minutes; deadlock/stall and all consequential behavior gates remain.

The combined follow-up driver checks are7 OK, including a response predicate
that never becomes true and must fail/release. ABI/export/packet/frame9 OK.
Fold96e5b51 remains running; new combined Android replay is pending.
Physical-device performance and the hardware floor remain separate.

Primary source references:

- https://android.googlesource.com/platform/frameworks/base/+/android-7.0.0_r1/cmds/input/src/com/android/commands/input/Input.java
- https://android.googlesource.com/platform/development/+/android-7.0.0_r1/cmds/monkey/src/com/android/commands/monkey/MonkeySourceNetwork.java
- https://android.googlesource.com/platform/frameworks/base/+/android-16.0.0_r1/services/core/java/com/android/server/input/InputShellCommand.java

## Runtime classification and next bounded repairs

Actual fa8e3a4 run39051032030: compact API24 passed21 checks, proving the protocol
handshake repair through real Android controls and lifecycle. Tall API36 recorded
945 nonzero-input native ticks but oscillated around the forklift doorway. Tablet
API36 passed15 checks before an Android SystemUI ANR dialog covered Resume; the
retained failure screenshot, not an assumed game-menu defect, establishes the
visible obstruction. Fold API35 destroyed/recreated the activity before native
startup, then reused an engine already terminating. The original OS configuration
mask was not captured; its exact cause remains unknown.

The same immutable fa APK passed23 fold checks in targeted natural startup replay
39052199202 (harness0c966f8). No app relaunch event identified the prior trigger.
Harness dfad677 now applies an explicit real Android font-scale update after first
launch, with continuous events and OS-setting restoration. Its result is pending;
no production lifecycle fix is claimed.

Root functional-window repair retains full-size startup and actual crouch/stand,
large-window layout, physical fold at native dimensions and full-size final restore.
Extended functional checks use an800x450 OS window. The game configuration and
physics are unchanged. Ordinary host resource/memory/CPU captures retain evidence
without suppressing ANRs. Worker dimensioned walking repair uses100ms pulses and
actual native observation intervals; support/work/arrival/stall predicates remain.

Local affected evidence: `python3 -m unittest discover -s ci -p test_android_compatibility.py`
->9 OK; `python3 -m unittest discover -s ci -p test_android_touch_driver.py` ->7 OK;
`python3 -m unittest discover -s ci -p test_android_route_navigation.py -v` ->6 OK.
The route model is not Android/Jolt execution. Actual Android acceptance of these
latest functional/navigator changes remains pending.

Full proof run39048317301 on92bd194 completed110/113 native cases. The failures
were catch_net_model, catch_net_player and split_inspection_recovery, matching the
separately deferred scope. This run is red; candidate builds and successful focused
Android jobs do not replace required full release acceptance. No catch tests or
expectations are weakened, skipped or repaired by this Android change.
