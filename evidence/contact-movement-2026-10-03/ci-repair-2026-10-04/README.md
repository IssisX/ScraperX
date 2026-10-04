# Bounded native CI repair — 2026-10-04

The owner authorized repairing the red GitHub Actions APK build. The broader contact-aware movement goal remains paused. Base is `c44b476` on `ChatGPT`, native candidate `61361c6`.

## Proven cause

The unchanged immediate-release north path captures incoming `(0, -2.893, 0)` m/s, then scripted Hanging produces `(5.39995, -46.6727, 0)` m/s in one tick. Release lands on the correct entity 1901 receiving deck at centre 104.4 m but increments deaths from one to two. [The trace](north-trace.log) proves the artificial catch correction causes fatal release, rather than missing geometry or a short test deadline. [The red catch regression](north-red.log) rejects gravity-off/root-driven acquisition.

## Repair

- The existing native `PhysicalHandClimb` supplies two finite hands for validated rigid lips, including prescribed kinematic ledges. Acquisition keeps current pose/velocity and restores gravity; active hands bypass root velocity driving and separate weight forces. Real receiver contacts remain active and their incoming fall speed is sampled.
- Requested hanging posture uses the actuator's commanded rest position, not actual spring sag; otherwise finite compliance would continually charge an integral target correction. Existing 5,000 N/m, 180 N·s/m, 1,500 N vector/hand and shared conservative 3,000 W target-command bound remain.
- Shimmy validates both real lips and sweeps from actual capsule pose. Regrip preserves the acquired player-local anchor and changes separation/target equally, retaining spring error and momentum. Native local anchors also drive rendered hand placement. Lowering acquires finite hands at its final handoff; validated mantle clears them; passive release preserves actual velocity.
- CI now builds both registered physical-hand and physical-traversal executables before the full suite.
- Original north recovery/death, canopy rejection, shimmy and receiver assertions remain. Catch assertions additionally reject excessive speed change, pose relocation, gravity-off and missing constraints. The legacy first-catch-to-one-second zero-drift expectation is migrated to finite settling followed by the same 50 mm drift tolerance, plus actual gravity/constraint and stopped-fall checks. Moving-ledge tracking/release checks remain.
- A sixth module case checks real kinematic tracking, regrip neutral-position and velocity preservation, actual gravity reaction, finite force, contact-free support and passive free fall.

## Verification status

The first repaired local run passes all three original north cases. The updated broad athletic test passes its affected hanging/moving-ledge/mantle/release section, then encounters the previously documented local cached ARM64 AS-003 freight east-face failure. Its prior unchanged-baseline reproduction is retained in `evidence/as-023/device-feedback-2026-10-02/`; the full GitHub native gate remains required and unchanged. Further current-source local results and exact-source Actions delivery will be recorded here after completion.

Independent read-only physics review found no material blocker to this bounded repair. It confirms the pinned Jolt target sign, COM transforms, translation-only identity axes, preserved regrip error and ownership. Remaining broader limitations: arm-extension dropout, finite mantle/top-out transfer, flexible material adapter and closed muscle-work ledger. Those remain in the paused movement plan; this repair does not claim their completion or device acceptance.


## Local pre-publication verification

Strict current-source native/bridge build succeeds. [The remaining native gates](native-repair.log) pass 32/32 in 213.48 s, including the sixth hand-module case, passive rigid/deforming/rotating departure, suspended ladder, cargo net, launcher, landing, parkour, ascent, pipe bridge, teeter, braced bay and three north caller rates. [The hand component output](hand-component.log) preserves actual six-case force, reaction and regrip results. [The broad ARM64 athletic output](athletic-arm64.log) retains its passing affected traversal section and the documented pre-existing freight east-face boundary. The final additional no-pose-relocation assertion passes all three original north caller rates, 11.54 s; [the actual catch/recovery output](north-final-trace.log) is retained. The full unchanged Actions native suite must pass before green delivery is claimed.


## Exact repaired-source ordinary replay

Source `dcf34382f351e8ba4a74eef04c04c1bb7dc218ee` is pushed to `ChatGPT`; [Actions run 37210599552](https://github.com/IssisX/ScraperX/actions/runs/37210599552) is in progress. Fresh current-source native library and byte-identical shipping main/character/input driver pass uninterrupted ordinary grade → cargo net → retained upper machinery → repaired north catch/shimmy → loaded AS-026 → supported +121.9 m, support 11, deaths 0, launcher work 0; final position `(9.002249,121.9,-174.5659)`, reported elapsed 327189 ms. [The complete headless output](ordinary-route.log) and [source/library/driver hashes](runtime-source.json) are retained. This establishes ordinary input/runtime continuity, not rendered inspection or device performance.

## Owner screenshot-policy correction

On 2026-10-04 the owner removed screenshot requirements and reiterated that APK delivery must be green. Active agent, execution, technical, ascent and handoff instructions now make screenshots optional diagnostics. The successor workflow removes only PNG-presence assertions and related loops; gameplay commands, PASS/error checks and numerical launch-motion CSV remain. YAML parsing, unchanged step order, Bash syntax for all 17 run blocks and retention of PASS/error/CSV checks pass locally. The already-running `dcf3438` Actions job uses its immutable earlier workflow; these policy changes do not alter native or Godot executable source. A green Actions run and verified exact-source APK provenance remain required before this CI repair closes.
