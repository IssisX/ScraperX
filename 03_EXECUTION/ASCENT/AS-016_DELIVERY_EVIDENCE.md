# AS-016 delivery evidence — 2026-09-26

Profile: **MACRO-TRAVERSAL-STRICT**.

## Exact source and artifact

- Gameplay source: `254eb2844e57eeb3d97d6872912465bd75585df5`, branch `ChatGPT`.
- [Workflow run 36269138275](https://github.com/IssisX/ScraperX/actions/runs/36269138275), job `108479617830`, completed successfully at **2026-09-26 20:44:14 UTC**. Every required step passed; none was skipped or waived.
- [Artifact 10914754662](https://github.com/IssisX/ScraperX/actions/runs/36269138275/artifacts/10914754662), `ScraperX-build-254eb2844e57eeb3d97d6872912465bd75585df5`, expires 2026-12-25.
- Downloaded archive SHA-256 verified against GitHub's artifact digest: `d345e10fc2e034db4c99e63f9db23e5f009a481d942d99b89111116a6311361f`.
- APK: `ScraperX-254eb2844e57-arm64.apk`, **32,817,742 bytes**.
- APK SHA-256 verified against the packaged checksum: `7872749e3b7588a7fe49cd61d5f79423df53c9bf63bf9797cea7b6d527a5cadb`.
- Artifact `CHECKPOINT.txt` matches the exact source commit. Packaged `lib/arm64-v8a/libscraperx_native.so` is ELF AArch64 and contains the pipe-bridge interface. All five fall recordings and their import resources are packaged.

## Required checks

| Gate | Observed result |
|---|---|
| Native build/full suite | All eight CTest entries pass: retained regression suite plus seven pipe-bridge cases, including actual-production 90/360 Hz refinement. |
| Normal construction | 33 moving bodies, 68 Kit bodies, two direct control cords; retired campaign entities absent. Default controls/checkpoint and fourteen fallback flights pass. |
| Native full route | 20 pipes; bridge tip 8.01822 m; player centre 11.9 m on original tower support 11; no route deaths. Subsequent real fatal fall restores the deployed bridge and spent crusher (front 0.458839 m). |
| Gamepad, 60 FPS | PASS; 20 pipes; tip 8.0200 m; arrival 11.9 m; no deaths. |
| Keyboard, 60 FPS | PASS; 20 pipes; tip 8.0138 m; arrival 11.9 m; no deaths. |
| Rendered touch, 30 FPS | PASS; 20 pipes; tip 8.0013 m; arrival 11.9 m; no deaths. |
| Retained presentation/input | Legacy render and all 23 existing controller/mechanism input scenarios pass. |
| Actual audio mix | Ambience −23.3 dBFS, steps −14.6 dBFS, peak −0.8 dBFS. Native high-fall voice test: two reactions, mix peak 0.271139, zero failures. |
| Collision authority | Both normal and explicit-regression solid tables match their rendered builders. Kit mechanism geometry remains native-owned and separate from dressing export. |
| Android | Native ARM64 compilation, APK export, native-library inclusion and immutable artifact publication pass. |

## Visual inspection and limits

Inspected all four published bridge captures: rack ready, loaded pan, bridge raised and tower arrival. The raised route and original tower arrival are visible. Close control views partly occlude the loaded pan; the separately preserved local side view shows the load and receiving material. These are first-person rendered checks, not a claim that every operator viewpoint is unobstructed or that phone ergonomics have been validated.

The local ARM64 retained-suite failure at the old AS-002 mid-landing also occurs on untouched `4cdb2bc`. The exact candidate's required x86 CI suite passes; no gate was removed to obtain that result.

No APK installation, Android execution, subjective voice audition, sustained Fold frame rate or completed 1,600 m ascent is established. The delivered slice is the default grade-to-+8 m pipe bridge and its +11 m tower connection. Later mechanisms and the deferred backflip enhancement remain outside this delivery.

The subsequent delivery-record commit changes documentation only; it does not change this APK's gameplay source.
