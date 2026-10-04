# RESTART HERE — ScraperX / ChatGPT

**Latest continuation handoff:** read [CONTINUATION_HANDOFF_CODEX.md](CONTINUATION_HANDOFF_CODEX.md) first. It includes the live repository inspection and newly observed CI audio-mix failure. The detailed tooling and saved implementation information below remain useful.

**Owner-requested quick restart checkpoint, 2026-10-04.** Resume this bounded batch after starting the new session/profile. This is saved work in progress, not a completed delivery. All local workers and runtime captures are stopped. The larger movement/content goal remains paused.

## Scope and checkout

Repository `https://github.com/IssisX/ScraperX.git`, exact branch **ChatGPT**. Agent-owned clone:
`/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo`.
Verify identity, HEAD, status, origin and applicable instructions before edits. Use this clone if present; otherwise clone into a writable temporary development directory outside personal Downloads. Current explicit owner instructions take precedence over contradictory older documentation.

The three approved jobs are mechanism-render extraction, rounded/deforming cargo-net appearance, and real recorded footsteps. Geometry preservation applied to extraction; the later net change intentionally changes its geometry. Existing hand placement is owner-confirmed accurate. Native C++/Jolt owns physics; Godot owns presentation/input. Stop after this batch and its green exact-source Actions/APK. Leave simulation.cpp, world builders, camera/telemetry and broader test restructuring for later.

## Saved implementation

- Extraction `b140e05`: five routines and kit/cable/net/bucket display state moved from main into `godot/presentation/kit_view.gd`. The view is the existing direct `KitPresentation` child. Interface: `setup(native, palette, cable_material, cargo_net_material, create_sign)`, `render_view()`. Main retains startup/frame coordination and shared materials/sign creation. Existing rigid node paths, timber styling and physics responsibilities remain.
- Recorded footsteps `188f90e3774d356ae9c2c8255212fa7cc9097d65`: twelve CC0 concrete/metal/earth recordings, provenance/license files and committed PCM imports. Legacy step-generator RNG draws are consumed so later procedural cues keep their existing random sequence. Cadence, pitch, contact timing and mixer are unchanged.
- Previous restart `609c857c601340d27fab7fcff4fc0997119c8537`.
- This checkpoint adds rounded net display: **207 shared sphere knots / 382 shared cylinder links**, two MultiMesh batches at the existing `NativeSoftCargoNet` path. Connectivity comes from actual native indices; centres, orientation and in-plane widths follow interpolated native vertices. Unloaded links are nominally 80 mm wide / 70 mm deep. Collapsed frames are suppressed; bounds union actual transformed mesh AABBs; fixture removal hides the obsolete net display.
- Native net remains entity **1953**, Jolt 9×23 four-corner open ribbon weave, pinned end rows. No native/hand/physics tuning was changed. Rounded surfaces are a render proxy, not new cylindrical collision: Jolt vertex radius .035 m is particle standoff, while native ray/grip queries use triangles. Affine knots and uniform cylinders approximate warping/taper/twist; do not claim exact collision matching or finite reciprocal hand-constraint proof.

## Actual evidence and limits

The scoped ledger is `evidence/kit-net-audio-2026-10-04/README.md`. Preserve historical receipts and their source boundaries.

- Extraction: normal/fixture renderer parity for 29/15 bodies, mesh channels/materials/signs, poses/cables and original flat net. Both generated collision tables matched byte for byte. Twelve recorded PCM resources and five contact-timing cases passed before this checkpoint.
- New actual-renderer ordinary-touch net route **exited 0**: entry → loaded climb → native top-out → supported first ring, Y=11.9, zero deaths, no launcher work. Observed maximum native displacement **0.28759 m**. Log: `net-green-touch.log`. Its runtime loaded the initial rounded implementation/test; final degeneracy/bounds/empty-net guards and strengthened wrist/profile assertions were edited during that run and therefore still need a fresh combined run.
- The first 192×164 test stalled because the minimum UI scale placed CLIMB over stick home: home(71.5,92.5), action(76.5,65), action hit radius29.9. The touch harness now uses **432×371**. This is a test viewport repair, not physics tuning. 192×164 is only suitable for passive render checks.
- Fresh Godot 4.7 ARM64 `--check-only` passed all four final scripts: kit_view, kit_view_test, kit_route_render_test, kit_mechanism_render_test. Parsing is not runtime proof.
- Independent local review found no material source defect. Remote Super_Agent reviewed the net frame/profile mathematics; its degeneracy/bounds advice was incorporated. Both reviews were source/advisory evidence, not runtime acceptance.
- Mechanism test `godot/tests/kit_mechanism_render_test.gd` is saved and parses, but was stopped before its first pose. It attempts a real touch pump/fill/lift/drain cycle; it is an **unverified draft**. Its earlier attempt exposed an empty-native-net update error; the new guard is present but needs execution.
- Audio worker imported a private runtime successfully but stopped Movie Maker by owner request (exit130) before mix measurements. No subjective listening or Master mix result was obtained.
- No new tests have been wired into Actions. Baseline run **37226116280** for609c857 was still in its historical rendered +110 m step at checkpoint. A checkpoint push may supersede that run. Last verified green APK remains source **b1da5f3**, run **37214576377**, artifact **11309435499**; it does not prove this checkpoint.

## Next actions, in order

1. Read this file, current AGENTS/user instructions, `00_START_HERE.md`, and `CONTINUE_HERE_CHATGPT_CODEX.md` §7. Inspect actual source/status and newest exact-source Actions run; repair a real CI failure before moving on.
2. Fresh final-source rendered `kit_route_render_test.gd` at432×371, plus normal/fixture `kit_view_test.gd`. Retain rigid golden parity. Binary ARM golden hashes may differ on x86; establish actual differences before changing baselines.
3. Close review test gaps: explicitly require nonempty native net at cargo poses, exactly two wrists, and meaningful knot orientation/bounds checks. These were reviewer recommendations, not executed assertions. Verify relevant hand/input regressions and both solids exports; preserve the proven hand code.
4. Verify moving mechanisms/cables/shackle hiding and bucket filling/draining. Inspect the saved mechanism draft before using it. A shorter test may use legitimate public machine commands in a retired fixture and real native evolution; do not assign volume/poses or repair physics to force extraction proof. Classify fixture vs normal gameplay. Do not rebuild a large route if a bounded display test answers the question.
5. Finish existing `audio_mix` shipping Master probe (Movie Maker, not Dummy). Collect actual prominence/band/peak/step metrics. Recorded-resource load proof exists; subjective listening/device quality remains separate.
6. Add only useful passing standalone tests to `.github/workflows/wo000-delivery-spine.yml` as a separate wiring change. Update authoritative docs/checkpoint and scoped receipts, push ChatGPT, verify the final exact-source green Actions run and APK provenance/checksum. APKs are built only in Actions. Stop at this batch.

## Local tooling and concurrency

Task root `/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao`.
Godot4.7ARM64 `/data/data/com.termux/files/usr/tmp/scraperx-godot-arm64/Godot_v4.7-stable_linux.arm64` exists and executes via Ubuntu/proot.
`proot-distro login ubuntu --shared-tmp -- /bin/bash -c '...'`.
Use Xvfb / `LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=softpipe`, Compatibility renderer; llvmpipe previously SIGILL. LOW/tiny local settings are test-only.

`runtime-kit` was synchronized to checkpoint source after its completed route run. `runtime-green` retains the old baseline. Native library SHA256 `9ff20444d0733bdcf6ff2353ee4d80aa2025f8c39d9b3e324d1d08a75f615198`; native source unchanged in this batch. Each future worker must use a **separate runtime**, not overwrite shared scripts during a parent's run. An interrupted mechanism worker used runtime-kit despite its assignment; loaded-script source boundaries above must be respected.

Private audio runtime/proof: `runtime-audio-recorded`, `runtime-audio-recorded-proof` (partial AVI/log, no mix result). Raw scoped proof: `proof/kit-net-audio`. Publish selected diagnostic receipts only; never raw session logs, credentials or build binaries.

Useful delegation: parent owns net/integration; one tester owns bounded mechanism proof/wiring; another owns audio mix. Final reviewer may be read-only. Remote MCP `mcp__super_agent__ask_super_agent` accepts a self-contained `prompt` and is advisory. Do not invent jobs just to occupy agents. At this checkpoint every worker is finished, with no pending background tasks promised.
