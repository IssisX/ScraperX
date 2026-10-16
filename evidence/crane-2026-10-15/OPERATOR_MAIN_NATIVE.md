# Yard crane through real Main — native operator receipt

Result: `SCRAPERX_YARD_CRANE_OPERATOR_MAIN PASS checks=40`, process exit 0. Captured 2026-10-15T23:53:46.622616+00:00–2026-10-15T23:58:53.501736+00:00 (307 seconds). The requested test is `godot/tests/yard_crane_operator_main_native_test.gd`.

This ran real `main.tscn`, Main's input router, the current native GDExtension, finite native crane, actual player-eye camera, native world grip targets, first-person arms, native-backed spatial audio sources, and Main's keyboard/touch UI. The harness manually advances Main at 1/90 s. It stages the player above the real booth floor once at (-66.5, 1.15, -150), then actual grounded foot contact settles at (-66.5, 1.08, -150) on entity 1450 and earns controls. It does not replace the world/viewer or assign fake crane readback.

## Accepted behavior

- E enters crane mode, faces the native +X operator front, preserves actual foot/eye positions, and leaves the handheld radio hidden.
- UP reduces native hoist rest length from 30.8600006 m to 30.5104980 m. Positive hoist work rises from 0 to 819.039797 J; source use rises from 0 to 21843.910217 J against a finite 5000000 J capacity.
- Independent A/D slew and W/S trolley commands reach their actual native axes in both directions through finite ramps. The receipt records slew -1.56167996 rad and trolley 20.4699268 m.
- Winch, slew and trolley each activate their matching spatial source independently. Released inputs set native targets to zero and stop all three sources.
- Cab G/R cannot remotely rig/unrig native sling legs. This empty-rig fixture remains 0/4, with native rigging reach false.
- Actual click-to-capture mouse input permits free head yaw/pitch while world wrists remain within 2.5 cm of native grip targets and actual foot position remains within 8 cm of entry.
- Main's actual touch layout presents hoist ±, slew/trolley stick, look and Done; unreachable rig/unrig and jump buttons stay hidden.
- Escape freezes actual native time at tick 863 and clears native targets. Resume clears held axes. E/Done while three axes are held leaves the mode and clears native commands/router machine axes.

## Viewport evidence

All three files are actual Main viewport PNG captures, visually inspected. Window/capture size is 1296×1114; Main reports internal viewport 2160×1856. Captures live in `/data/data/com.termux/files/usr/tmp/scraperx-crane-main-20261015/captures/`:

| Capture | Observed content | SHA-256 |
| --- | --- | --- |
| `01-crane-native-grip-view.png` | Downward operator glance clearly shows both native-anchored gloves/forearms at real booth controls, actual tower and yard, and truthful 0/4 rigging UI. | `7d813080da893f0c199d69fbde6933893541980fb23829495bb4487386ca5c2e` |
| `02-crane-free-head-high-view.png` | Actual grounded operator eye looks up the physical mast to the high bearing and yellow crane boom through the open overhead booth frame. | `be97c1ae54d52da8f6499180686c3bcf1c4e23ca77c0eb74233640dce6b06248` |
| `03-crane-touch-operator-layout.png` | Actual touch UI and world hands: slew/trolley stick, hoist ±, right-side look and Done; no cab rig/unrig button. | `6e685df9c9398d4143d3f057b41ef7dd139f6e4cec8fb3d4adb7d887c4bc5576` |

## Source/runtime provenance and limits

- Repository inspected before edits: branch ChatGPT, HEAD 13aa36f, existing owner/concurrent WIP preserved. This worker added only its assigned test and this receipt; runtime artifacts are private.
- Godot source: owner's frozen `source-v5/godot`, copied to `/data/data/com.termux/files/usr/tmp/scraperx-crane-main-20261015/godot`. Linux descriptor architecture changed from x86_64 to arm64 in that private runtime only. Owner's frozen source was not modified.
- Native bridge: owner's `build-v5/libscraperx_native.so`, SHA-256 `152ffbe165722f3112f28ac80630785437166d507e18473888e21fe332c680ec`. Native build acceptance belongs to the owner; this worker performed no native compilation.
- Test SHA-256: `d90045fe3adc81bb7a005613636ec2a1876b2b5d0950b13dce23a5eeaf1921e6` (repository and executed private test match).
- Runtime: Godot 4.7.stable.official.5b4e0cb0f Linux ARM64, Ubuntu proot, Xvfb, Mesa 26.0.8 softpipe, compatibility renderer, Dummy audio. This provides desktop native/input/render/source-playback-state evidence, not Android device performance or audible sound quality.
- Exact command, complete output, hashes and timestamps: `/data/data/com.termux/files/usr/tmp/scraperx-crane-main-20261015/receipt.json`, `/data/data/com.termux/files/usr/tmp/scraperx-crane-main-20261015/main.log`; launcher `/data/data/com.termux/files/usr/tmp/scraperx-crane-main-20261015/run.py`. Import passed before this final run.
- Host diagnostics include proot `--shm-helper` absolute-path messages, unsupported driver V-Sync change and root-user warning. The accepted process exits 0 with no GDScript/native test errors.
- Initial harness attempt used the wrong sign convention for UP's cable-length target and too-short motor-ramp reversal durations, and omitted click-to-capture before mouse look. Those test assumptions were repaired; no production change was needed. The final run above passes all 40 checks.
- This fixture proves the real Main crane operator path with one initial booth stage. Full same-truck delivery, on-foot travel from spawn and four local sling-eye rigging are covered by separate owner fixtures; this receipt makes no claim for those routes.
