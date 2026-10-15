# Actual matching Linux forklift bridge — PASS, 2026-10-15

Repository `ChatGPT` at `1f92dca096c007e95681dc0f73f400a171c17f94` plus preserved dirty native/frontend work. Both `register_types.cpp` and `scraperx_simulation.cpp` freshly compiled against a private current-source snapshot. No cached bridge TU or production-file changes.

Godot `v4.7.stable.official.5b4e0cb0f` loaded the actual native bridge, constructed the real Simulation, and completed `SCRAPERX_FORKLIFT_NATIVE_BRIDGE PASS checks=133`. Import and runtime exit0; no parse/bind/script errors. Runtime: available=true, grounded=true, reachable=true, actual support2440, battery12000000J, three native wheels. Ground-spawn remote drive was suppressed; nonfinite bound input rejected. Native dictionary field types and all three bound API methods checked. Production `forklift_view.gd` consumed each native render Transform3D and width. No mock/receipt stand-in was used. One debug stage over the real platform isolates bridge readback; it is not approach, pickup, stack, withdrawal or onward-route evidence. Accepted native mechanics/station/lift/leaving tests and controlled frontend test were reused without rerunning their campaigns.

This is headless Linux host runtime evidence; no rendered-pixel, APK, installed-device or cargo-route claim. No remaining blocker for this assigned bridge/API scope. A changed native route archive needs relink and affected runtime acceptance before being called matching.

## Artifact/source identity

| Input/output | SHA256 |
|---|---|
| `/data/data/com.termux/files/usr/tmp/scraperx-forklift-bridge-20261015/libscraperx_sim.a` | `a662ca86c2417b2e074831d53fdb472799c07fb406020fdbff0c2a1ed1788954` |
| `/data/data/com.termux/files/usr/tmp/scraperx-ci-soft-velocity-20261015/jolt-build/libJolt.a` | `8711a784f394fdf91af3366ab0b6695e80322cb73b46a8d1659e775b20aced1b` |
| `/data/data/com.termux/files/usr/tmp/scraperx-restore-linux-build/bin/libgodot-cpp.linux.template_release.arm64.a` | `d16656b18a082b9238c72020ad2d3ded89e9e068a794a73c5ebcb0f225892028` |
| `/data/data/com.termux/files/usr/tmp/scraperx-forklift-bridge-20261015/register_types.cpp.o` | `3d7e7d37b7458703a8ec8a055840685ae1ad46f237128af06ae34eeb0ba108b8` |
| `/data/data/com.termux/files/usr/tmp/scraperx-forklift-bridge-20261015/scraperx_simulation.cpp.o` | `90a99197595edad1bd3850aced23c81020a6b07230db095d4b4fccf550516034` |
| `/data/data/com.termux/files/usr/tmp/scraperx-forklift-bridge-20261015/libscraperx_native.so` | `8a9375148f452cf92e39de1aa59aaa66b713c6926dbbd1d17813cd526a572544` |
| `src/sim/simulation.hpp` | `2eae6317e29f857cd1ad82d70acd2e4a735cf7a6dc8d589bfe79e4df6962f99a` |
| `src/bridge/scraperx_simulation.hpp` | `3412c1e8511e61766380227e5c850fc3c482c963866849f0e92c85e37b887324` |
| `src/bridge/scraperx_simulation.cpp` | `328cf276ba534064c2dd9f4000ea7d0c070a11e2e50b573d1fb1747c2f9b0f99` |
| `src/bridge/register_types.cpp` | `65e4f5d31131b17df921a41fc2ad2bf764cb55c8a5bf6185c35f6c6fef666627` |
| Godot binary | `8e53c8740db03581bdd3fd7835f15046a25126baf9d78458fc6a7d4d1c312f45` |
| Private real-native runtime script | `7cc73c1ec5e1590804af6082b958d00def784e75f012418009fccec27fdf6ba7` |

Full native/bridge source snapshot hashes, exact compile and link argument arrays, build exits and output hashes: `/data/data/com.termux/files/usr/tmp/scraperx-forklift-bridge-20261015/build-receipt.json`. Presentation snapshot: `/data/data/com.termux/files/usr/tmp/scraperx-forklift-bridge-20261015/presentation-source.json`. Exact runtime commands/errors/exits: `/data/data/com.termux/files/usr/tmp/scraperx-forklift-bridge-20261015/runtime-receipt.json`. Logs: `import.log`, `native-bridge.log` beside those receipts. Scripts/source/native copy and bridge objects remain in that private directory. Current source headers/bridge TUs still matched snapshot after runtime.

## Exact build and run entry commands

```sh
proot-distro login ubuntu --shared-tmp -- python3 /data/data/com.termux/files/usr/tmp/scraperx-forklift-bridge-20261015/build.py
python /data/data/com.termux/files/usr/tmp/scraperx-forklift-bridge-20261015/runtime.py
```

The build recipe uses `/usr/bin/c++`, C++17, PIC, O3/NDEBUG, Jolt/godot-cpp matching cached Linux headers and archive; strict `-Wall -Wextra -Wpedantic -Werror`, and link `--no-undefined`. Private `.gdextension` changes the Linux architecture tags from repository x86_64 to this actual arm64 host; production descriptor remains untouched.

Runtime command arrays:
```json
[
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
    "--editor",
    "--quit"
  ],
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
    "res://tests/forklift_native_bridge_test.gd"
  ]
]
```

## Dependency/relink contract

`/data/data/com.termux/files/usr/tmp/scraperx-forklift-bridge-20261015/relink.py` reuses the fresh two bridge objects and checks every captured `.hpp` plus bridge-source input against production before relinking; it refuses changed ABI/bridge inputs. Run under Ubuntu proot with an absolute replacement native archive. It writes `relink-receipt.json` and updates the private project binary. Header changes require recompilation of both TUs. Original a662 result above stays bound to its original receipts; preserve receipts before any replacement run. Root owns native yard/route integration and final delivery.
