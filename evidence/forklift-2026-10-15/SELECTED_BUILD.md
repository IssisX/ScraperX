Current status is owned by the live checkpoint. This report records the initial matching native/bridge build, before the route-only alignment correction. `route-alignment-build.json` records that later strict test-only relink; native archive and bridge bytes are unchanged. Matching Main acceptance is recorded separately in `SELECTED_MAIN_RESULT.md`.

# Matching selected source build — PASS, 2026-10-15

Private source proposal against actual HEAD `1f92dca096c007e95681dc0f73f400a171c17f94`; selective patch SHA256 `d36f270e32dde9ca54c63927ff307cee7712e4dcd499f4af656029a83be2c52d`. This refreshed patch identity supersedes the initial preparation hash in PUBLICATION_PREP.md. Only the two root route drivers were refreshed from working source. Source remained fixed throughout compilation; production files/index/branches untouched.

Exact production CMake configured selected source with `FETCHCONTENT_SOURCE_DIR_JOLT` pointing to the existing pinned patched source. Both project patch verifications passed. Built generated `scraperx_sim/fast` target at2jobs to avoid traversing/rebuilding Jolt. All 67 native translation units compiled fresh from selected source, including every Simulation method TU, baseline grappling_gun.cpp, native foot/machine/route owners. No old a662/1b99 object/archive or gate-layout object was reused. Both bridge translation units also compiled fresh against selected Simulation/bridge headers.

Native strict warnings come from unchanged production CMake `-Wall -Wextra -Wpedantic -Werror`; five prescribed TUs use `-fno-rtti`. Route test explicitly compiled with strict warnings using CMake-generated matching definitions/includes. Linking uses existing patched Jolt SHA8711 and cached matching Linux godot-cpp; Jolt and godot-cpp were not rebuilt. Bridge links `--no-undefined`.

## Artifact identities

| Artifact | SHA256 |
|---|---|
| `/data/data/com.termux/files/usr/tmp/scraperx-forklift-publication-prep-20261015/cmake-build/libscraperx_sim.a` | `ca8ce5fb26f35f16cb38f523ff115357396488646c3e22cff04bb0db3e176189` |
| `/data/data/com.termux/files/usr/tmp/scraperx-forklift-publication-prep-20261015/cmake-build/scraperx_warehouse_forklift_route_tests` | `1f11ff3eaa62f0fb9f9984733698942eb35fc1791e2b191d25eef8ed389a14a3` |
| `/data/data/com.termux/files/usr/tmp/scraperx-forklift-publication-prep-20261015/cmake-build/libscraperx_native.so` | `6b75c1bb618d8ce52c898e8c939434428e7b620fc18859d84d67ff54ddf6e329` |
| `/data/data/com.termux/files/usr/tmp/scraperx-forklift-publication-prep-20261015/cmake-build/register_types.cpp.o` | `2b2b8e9db8a34f9677e124763b2a8341d42d9890f77f3d2c2160c9fe14ad16bd` |
| `/data/data/com.termux/files/usr/tmp/scraperx-forklift-publication-prep-20261015/cmake-build/scraperx_simulation.cpp.o` | `95644e7f247f90ade3cdda79f2e80c44f9ec9813ef7b39ed371a55d8ca21a516` |
| Pinned existing patched Jolt | `8711a784f394fdf91af3366ab0b6695e80322cb73b46a8d1659e775b20aced1b` |
| Cached Linux godot-cpp | `d16656b18a082b9238c72020ad2d3ded89e9e068a794a73c5ebcb0f225892028` |
| `tests/warehouse_forklift_route_tests.cpp` | `789726158aa7e4e64ba70d4280d9f1f2f47f803c46b6eeb6402d4b8a304dd8eb` |
| `tests/forklift_route_walk.hpp` | `d9677e1f817eb6e68ab80664ebd2b05c73fbb2641ccdc51db4a6e0001bde6c73` |

Route working files still matched copied source after compilation: `{"tests/warehouse_forklift_route_tests.cpp": true, "tests/forklift_route_walk.hpp": true}`. No driver refresh/relink was needed during this build. If root changes only those files later, refresh them and rebuild/relink only route test; native/bridge artifacts remain reusable until relevant production source changes.

Full selected source hashes, exact generated native compile commands, exact route compile/link and bridge compile/link argument arrays, command exits and output hashes: `/data/data/com.termux/files/usr/tmp/scraperx-forklift-publication-prep-20261015/selected-build-receipt.json`. Source list comes directly from selected production CMake; all configured native TUs were built, not a guessed dependent subset. `compile_commands.json` and generated CMake rules live in `cmake-build/`. Logs: `configure.log`, `native-build.log`, `route-compile.log`, `route-link.log`, `register_types.cpp.log`, `scraperx_simulation.cpp.log`, `bridge-link.log`.

## Exact entry commands

```sh
proot-distro login ubuntu --shared-tmp -- cmake -S /data/data/com.termux/files/usr/tmp/scraperx-forklift-publication-prep-20261015/source -B /data/data/com.termux/files/usr/tmp/scraperx-forklift-publication-prep-20261015/cmake-build -DSCRAPERX_BUILD_BRIDGE=OFF -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DFETCHCONTENT_SOURCE_DIR_JOLT=/data/data/com.termux/files/usr/tmp/scraperx-ci-soft-velocity-20261015/jolt-src
proot-distro login ubuntu --shared-tmp -- python3 /data/data/com.termux/files/usr/tmp/scraperx-forklift-publication-prep-20261015/build_selected.py
```

Within the script, the native build command is:
```sh
/usr/bin/cmake --build /data/data/com.termux/files/usr/tmp/scraperx-forklift-publication-prep-20261015/cmake-build --target scraperx_sim/fast --parallel 2
```

## Acceptance boundaries

All build steps exit0; source stability and unchanged pinned Jolt archive hash confirmed. No native route test, other C++ test, Godot smoke, rendered frame, APK or device test was executed. Root owns the single final selected-archive connected cargo replay; route binary is ready above. Matching selected-source full Main smoke remains a later source-freeze task. Earlier a662/1b99 runtime observations do not establish this selected snapshot’s runtime result.

No build blocker remains. Root owns authoritative final route/acceptance/docs/index/commit/push integration. Selective source still excludes unfinished grapple/gate WIP and Main ParkourYard. Current private build and proposal need no production copy-over; preserving dirty owner work remains mandatory.
