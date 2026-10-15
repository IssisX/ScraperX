# Actual Main/native forklift flow — PASS, 2026-10-15

Branch `ChatGPT`, revision `1f92dca096c007e95681dc0f73f400a171c17f94` plus preserved working changes. No production edits. Private artifacts: `/data/data/com.termux/files/usr/tmp/scraperx-forklift-bridge-20261015`.

Full production `res://main.tscn` instantiated unmodified, and Main created its actual `ScraperXSimulation` extension object. No mock/receipt bridge. One initial debug stage over the real rear platform isolates UI acceptance; actual native ticks settle foot contact before operation. This is host headless runtime evidence, not physical approach/cargo-route, rendered-pixel, APK or installed-device proof. Controlled touch/pad and earlier API133-check results reused. No native cargo/mechanics test rerun.

`SCRAPERX_FORKLIFT_MAIN_NATIVE PASS checks=19`; actual runtime exit0, no parse/bind/script errors.

- Native grounded support2440 grants rear-station reach; Main offers OPERATE and keyboard E enters forklift mode / DONE action.
- Keyboard W+D feeds normalized truck drive0.707106769 and rear steer0.062222231rad; operator displacement only0.001474949m over8ticks while Main consumes ordinary movement input.
- Keyboard Up through Main raises carriage0.000003853m→0.241627991m; battery11996222.977695J→11984215.8662933J, native positive work0→271.680379364485J. Released drive applies brake1.
- Keyboard Escape enters actual pause; native tick190 remains frozen across a process frame. Actual resume clears held raise, retains brake and accepts fresh input.
- Keyboard E / DONE leaves operation while W remains held: native applied drive0 and brake1.
- Main’s actual wheel view contains all3native wheels and applies each native render Transform3D.

## Identities and commands

| Artifact | SHA256 |
|---|---|
| Corrected native archive | `1b99a1332e46e44ebee52b5b96ea8f35c8f139ff546418263c64f7e80d1214ae` |
| Matching Linux bridge | `8bac6579fa6ca301b2a1e13272eab1d0a33da4d1ab7cba96ee6e54b640e3ebd7` |
| Runtime test | `43017a026fe9d078a5065aade6a9e6f55527e6d6baccaefa76d64a96510a7dfa` |
| `godot/main.tscn` | `1a6910fdf586a7d3b0fcea52a1a84787c9ba627e6bd7fe452cd28ff769b52be2` |
| `godot/presentation/main.gd` | `f51c23071e4a691559e4eaff73f286fcd8c5dddf02407e80b7c7d96413570f72` |
| `godot/presentation/forklift_view.gd` | `bca57329671e07db5f95d76a65c6096642967294026ca53047b1efe931c31447` |
| `godot/presentation/ui/input_router.gd` | `27fa5dc39c1b8c801172f3d3fdc3fbea9f0efc6e1ebf2d3d6b8deaacd4603d1b` |
| `godot/presentation/ui/touch_controls.gd` | `4df1c5740ea9990e56f787a30fbd17f62376cf4e1c5b194dd88bf7d383637b44` |
| `src/sim/simulation.hpp` | `2eae6317e29f857cd1ad82d70acd2e4a735cf7a6dc8d589bfe79e4df6962f99a` |

Relevant production scene/frontend hashes still match staged sources after acceptance. Corrected archive source still matches copied/native-tested hash. Both fresh bridge objects from the prior build reused after guarded `.hpp`/bridge-source equality checks; only native yardTU changed. Exact original compile arrays and all source/object hashes: `build-receipt.json`; exact replacement link array/archive/SO hashes: `relink-receipt.json`; runtime command, output, exits and stability: `main-runtime-receipt.json`. Full stdout/stderr: `main-native.log`.

```sh
proot-distro login ubuntu --shared-tmp -- python3 /data/data/com.termux/files/usr/tmp/scraperx-forklift-bridge-20261015/relink.py /data/data/com.termux/files/usr/tmp/scraperx-forklift-route-20261015/libscraperx_sim.a
python /data/data/com.termux/files/usr/tmp/scraperx-forklift-bridge-20261015/main_runtime.py
```

Actual Godot command:
```json
[
  "proot-distro",
  "login",
  "ubuntu",
  "--shared-tmp",
  "--",
  "/data/data/com.termux/files/usr/tmp/scraperx-godot-arm64/Godot_v4.7-stable_linux.arm64",
  "--headless",
  "--path",
  "/data/data/com.termux/files/usr/tmp/scraperx-forklift-bridge-20261015/godot",
  "--script",
  "res://tests/forklift_main_native_test.gd"
]
```

## Failures and limits

Two private test-driver issues were corrected before final PASS: inferred Variant locals needed explicit float types (script never ran), then diagonal drive expectation>.9 conflicted with existing keyboard vector normalization to0.7071. Initial logs/receipts preserved as `main-native-driver-parse.log` / `main-runtime-driver-parse-receipt.json` and `main-native-diagonal-expectation.log` / `main-runtime-diagonal-expectation-receipt.json`. Neither required production changes. No supported frontend defect or remaining blocker for assigned operator-flow scope.

The previous API133-check receipt is preserved as `api-runtime-receipt.json`; its original a662-linked SO/native files were replaced by this corrected1b99 relink, so current private binary identity is the hash above. Source or archive changes invalidate only affected matching acceptance. Root owns cargo-route outcome, checkpoint and final publication/delivery.
