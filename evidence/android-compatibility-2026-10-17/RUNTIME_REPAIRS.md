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
