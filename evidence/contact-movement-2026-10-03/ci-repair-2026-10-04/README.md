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

## Second Actions failure and presentation owner repair

Run `37210599552` at `dcf34382f351e8ba4a74eef04c04c1bb7dc218ee` passed all **33/33 native tests**, uninterrupted ordinary grade→121 m, historical rendered grade→110 m and the machine-authority fixture. The next input-regression step failed `touch_hang_drop`: rendered wrist error **0.218 m**. Android build/export was skipped; this run is red and supplies no new APK. [Compact actual CI output](ci-result.log) preserves both passes and failure.

The same shipping headless scenario reproduces the failure locally. At 0.3 s, player centre `(8.12816,2.211757,4)` has velocity `(-0.184712,-1.29047,0)`, camera `(8.129186,2.838926,4)`, native grips `(8.62,3.6,3.78/4.22)`. Presentation target and smoothed wrist agree at `(8.466,3.526,4±0.13)` while the solved wrist clips to `(8.408672,3.315344,4±0.131813)`: the old low camera-relative shoulders exhaust the anatomical reach. Hanging also ignores the already-exported independent native hand anchors.

`first_person_arms.gd` now converts each native interpolated grip into the existing hook-wrist offset, while Lowering retains its existing lip pose. Hanging shoulders follow the native interpolated torso and fixed structure frame, independently of head rotation, at centre+0.55 m, half-span 0.17 m and 0.08 m behind the structure-facing centre. This corresponds to shoulder height 1.45 m inside the standing 1.8 m capsule. Upper arm 0.36 m, forearm 0.33 m and capped 0.30 m lean remain. At the measured catch trough `(8.107823,2.140135,4)`, shoulder-to-hook distance is 0.945 m, below the unchanged 0.986 m reach-plus-lean bound. No native position, velocity, gravity or force changes were made. Independent review found no blocking source issue and requested that the new head-turn assertion reject missing anchors; that check is included.

The original 20 mm wrist-error assertion remains. Added checks require each target to consume its actual native hand, and require Hanging, both grip poses and nonnegative error within 20 mm after a 90° head turn with 0.2 rad pitch. Tests restore the head before drop/mantle. Full shipping input regressions and a new exact-source green Actions APK remain required. Extreme catch reach/dropout remains part of the paused broader movement work; this bounded posture does not claim unlimited anatomical reach.

**Local final verification:** [All 23 shipping input scenarios](input-repair-final.log) pass, process exit 0 and no script/parse/load errors. After tightening the missing-anchor head assertion, [both exact final hang/drop and hang/climb scenarios](hang-presentation-final.log) pass again with initial and head-turn wrist errors reported as 0.0000 m; drop remains actual downward departure and mantle lands at centre +4.52 m. [Presentation hashes](presentation-source.json) verify byte-identical shipping scripts in the actual runtime against the repository. No screenshots were required or inspected. Exact-source Actions/APK retry is pending.
