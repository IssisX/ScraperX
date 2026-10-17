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
