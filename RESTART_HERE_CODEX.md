# RESTART HERE — ScraperX / ChatGPT

**Owner-requested safe Codex restart checkpoint, 2026-10-04.** This file is a pickup guide. `AGENTS.md` and the existing `CONTINUE_HERE_CHATGPT_CODEX.md` §7 keep their authority and labels. Resume the bounded batch when the owner continues; the larger movement/content goal remains paused.

## Current work

Three separate jobs were approved:

1. Extract mechanism rendering from `main.gd` into `kit_view.gd`, preserving behavior during extraction.
2. Change the cargo net's geometry: substantial, rounded ropes/rungs, with visible deformation from actual native state.
3. Replace the rejected synthetic “ting-ting” footsteps with real recorded footsteps.

The owner clarified that unchanged net geometry applied only to job 1. Job 2 deliberately changes that geometry. The owner confirms current hand placement is accurate and grabs real geometry; preserve that proven behavior. Keep native C++17/Jolt physics ownership, read-only Godot presentation, real contact and the proven ordinary route. Stop after this bounded batch; leave simulation.cpp, world builders, camera/telemetry and wider test-suite restructuring for separate tasks.

## Saved code

- Exact repository `https://github.com/IssisX/ScraperX.git`, exact branch `ChatGPT`.
- Agent-owned checkout: `/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo`. Use it if present; inspect identity/status/HEAD before writing. Otherwise make a fresh owned clone in a writable temporary development location, outside personal Downloads.
- Starting source `03d50dcd2cdce65c6037ad964950ea075759826f`.
- Extraction commit **`b140e05`**: five functions plus kit/body/cable/net/water state live in `godot/presentation/kit_view.gd`. `KitPresentation` is this module itself, directly beneath main. `setup(native, palette, cable_material, cargo_net_material, create_sign)` and `render_view()` form the interface. Main keeps shared materials/sign creation and startup/frame coordination. Existing `KitBody1950/1951/2900` paths and timber styling are significant to slingshot_view.
- Audio commit **`188f90e3774d356ae9c2c8255212fa7cc9097d65`**: sound bank now preloads four concrete, four metal and four earth bootsteps; original non-step procedural RNG draws are consumed to preserve other cues. WAV/import/license/provenance files are committed under `godot/assets/audio/footsteps/`. Cadence, pitch, mixer and native contacts are unchanged.
- **No net geometry implementation yet.** Exact pending test source is `03_EXECUTION/PLANNING/KIT_NET_AUDIO_CHECKPOINT_2026-10-04/kit_route_render_test.gd`, deliberately outside the Godot test project. Its actual RED rejects missing rounded strands/knots after ordinary touch reaches the entry.

## What was proven

Read `evidence/kit-net-audio-2026-10-04/README.md` and its saved test logs. Actual renderer parity covers 29 normal and 15 fixture bodies, mesh batching/materials/signs, native interpolation/cables and the original flat net. A fresh reviewer found no material extraction defect. The twelve loaded PCM recording variants and five contact cases pass. Both regenerated collision tables match byte for byte.

These receipts do not prove loaded mechanism transfers, water fill/drain, subjective sound quality, final new geometry, Android device performance, or a new green APK. New standalone tests are not yet added to Actions. The last verified green delivery is source `b1da5f3`, [run 37214576377](https://github.com/IssisX/ScraperX/actions/runs/37214576377). Check Actions for the restart checkpoint's exact HEAD first; source on branch is newer than that last accepted APK. Every new APK is built in Actions and must come from a green exact-source run.

## First actions after restart

1. Verify repository/ChatGPT/HEAD/worktree/local changes and current Actions state. Read applicable instructions, current checkpoint and the scoped evidence; preserve concurrent edits. All local workers were finished at the restart boundary.
2. Continue the cargo-net geometry implementation in `kit_view.gd` (or an explicit bounded display helper if justified). The native net is entity 1953, one Jolt soft body: `src/sim/cargo_net.cpp`, 9×23 four-corner knots, open ribbon links, pinned upper/lower rows and 0.035 m collision vertex radius. The bridge supplies native interpolated vertices/indices. Current view draws flat double-sided triangles, explaining the paper-thin appearance.
3. An efficient candidate is shared rounded cylinder/sphere geometry through MultiMesh strands/knots. Derive knot centres from their four actual vertices and connectivity from native indices, rather than duplicate a grid or animate player presence. Resolve visible cross-section versus the actual collision/grip envelope explicitly. Native loading and interpolation own deformation; do not add decorative motion to claim physics. This candidate is a design direction, **not implemented source**.
4. Bring the saved net test into `godot/tests/`, strengthen edge-specific adjacency and actual loaded deformation checks as appropriate, then exercise ordinary-touch climb/top-out with real rendering. The test subclasses the existing shipping touch driver and renders its telling poses on slow local software hosts. Headless execution skips the net update and cannot establish its rendering. Existing flat-net assertions in `kit_view_test.gd` apply to the extraction baseline; intentionally replace them with truthful rounded-geometry assertions while retaining rigid-body parity.
5. Close the extraction's remaining meaningful rendered moving-body/cable and bucket fill/drain boundaries, then the new recorded-footstep shipping mix. Use relevant route/hand-placement regressions and both solids comparisons. Update existing authoritative docs and §7; keep changes separate and push only ChatGPT. Wire useful passing standalone tests into the available delivery path with a small separate wiring change.
6. Close final exact-source green Actions/APK provenance. Report code, actual proof and remaining device/listening boundary plainly. Stop at this batch.

## Local tools if still present

Task root: `/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao`.

- Godot 4.7 ARM64: `/data/data/com.termux/files/usr/tmp/scraperx-godot-arm64/Godot_v4.7-stable_linux.arm64`.
- Ubuntu/proot wrapper: `proot-distro login ubuntu --shared-tmp -- /bin/bash -c '...'`.
- `runtime-green` retains the original executable baseline. `runtime-kit` has extraction, new audio/PCM imports, native library and test resources; synchronize future source deliberately and run import after asset changes. Do not confuse runtime scratch with branch source.
- Rendering succeeded via `LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=softpipe xvfb-run -a`, Compatibility renderer. Prior llvmpipe on this host can SIGILL. Test-only 192×164/LOW settings are not shipped settings or device quality proof.
- Raw temporary receipts: `proof/kit-net-audio`. Useful selected receipts are committed; raw session logs and build binaries are not published.
- Existing host build/source/native caches are beneath the task root; native source did not change in this batch. APKs stay in Actions.

