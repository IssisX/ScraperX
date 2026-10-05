# AS-027 integrated service lift

ChatGPT-only successor to delivered cargo repair `560618372ad9ea18a6e037b1d7d197557fb350ad` ([green Android run](https://github.com/IssisX/ScraperX/actions/runs/37340947930)). This batch also includes separately committed CI output reuse `5f94f42`; it removes ten duplicate binary executions, retaining their existing output assertions/artifacts. No measured CI time saving is claimed.

## Implemented behavior

Actual native two-stage18m linkage provides23.3096m available travel, deck121→144.31m. Reachable deck/lower/upper controls authorize finite native effort only with real supported footing. The player boards, holds/reverses/releases the pendant and physically exits at143m; no automatic alignment or arrival. Existing single90Hz Jolt owner and four collision substeps remain authoritative. Nine moving and three static Kit bodies append after existing content, with per-constituent volume-derived mass/inertia and locally connected-pivot collision exclusions only.

Measured design:18970.2kg moving assembly;12258.1kg effective lifted mass including85kg rider design load;1.1MN motor,1.3MN passive brake,250kW electrical rating,4.75MJ capacity. Ratings derive once from source geometry, densities and specified headroom. Runtime obstruction cannot raise them. Positive estimated work debits finite energy; no regenerative refill. Ordinary checkpoint retains depletion. Explicit RESTART LIFT ATTEMPT restores the complete pre-first-command checkpoint; native route verifies this separately from touch operation.

## Causal defect and bounded repair

Two repeated uninterrupted grade→143 attempts stalled at138.266m with UP held, valid station/reach, grounded support, approximately157.5kJ still available and no energy cutoff. The force cap was reached. `campaign-guide-red.log` records the first guide1971/deck2973 contact: deck origin(-31,133.007996,-162), nominally aligned yaw pi/2, yet reported penetration0.268020m against a single37m rail convex at the intended20mm shoe clearance. Subsequent contact displaced/twisted the deck and exhausted most of the source in the jam. Fresh staged touch, aged native and native without checkpoint reset did not reproduce this exact campaign path. This is bounded collision-conditioning evidence, not a universal diagnosis of the physics engine.

Each37m rail convex was replaced by ten flush3.7m sections with the same outside envelope. Clearance, guide joints, collision masks and actuator/energy ratings remain unchanged. `rejected-long-mast.cpp.txt` retains the previous geometry. The same ordinary campaign then reached143m with zero deaths and no launcher work (`campaign-guide-green.log`), without the guide stall. Diagnostic contact/pose prints were removed before the final build. The campaign log contains diagnostics; published-source CI reruns the ordinary route without them.

## Verification receipts

- Final lift CTest:2/2 pass (`lift-final.xml`), after rail segmentation and diagnostic removal. Native real-player approach/boarding, actual rider load, hold/release, spent-energy checkpoint,143m exit, full23.3m limit, reversal/reboarding, inherited departure velocity and explicit retry pass.
- Production90Hz/four-substep component cases: near-empty/mid-source cutoff, empty source, exhausted checkpoint, actual colliding obstruction and full upper/lower return pass. Maximum near-empty drop0.526mm; other tested cutoff drop0. Full return error−16.716mm;1.23658MJ remains. Tested cases have zero energy overdraft and peak draw below250kW. No adaptive rating increase.
- One full native run:34/35 passed; sole failure was old default inventory27movers/38Kit bodies. Updated expected inventory36/50 and checkpoint assertion; full simulation-test executable then passed (`inventory-corrected.log`). All35 distinct cases have passing local results; this is not a claim that the earlier full run was green. CI must run the complete final suite.
- Fresh normal-world staged touch143 passes (`touch-normal.log`). Full ordinary grade143 passes with retained supported121 checkpoint, support11, zero deaths, no fixture relocation/death recovery/launcher work (`campaign-guide-green.log`).
- Settings/runtime3078checks,0failures; Godot editor import without script/parse errors. Independent read-only review found no blocker in ownership, retry/reach, rail envelope or preserved old render oracles. Actual-renderer gates remain in CI; no new local rendered-image proof claimed.

`source.sha256` pins the final executable-source/test/workflow files in this checkpoint. Preparation evidence is historical screening, not substituted production proof. The historical coarse90Hz/single-substep cutoff failure remains recorded there; production uses four collision substeps. Work uses impulse/displacement quadrature and an empirical velocity reserve, not a rigorous continuous energy residual. Rigid steel geometry is not stress/buckling certification. No Fold install/play/readability/performance/thermal claims.

## Delivery status

Local implementation and bounded checks complete. One exact-source ChatGPT push/Android Actions delivery is next; no AS-027 APK success is claimed until that run succeeds and its APK provenance/signature are verified. Other branches and the unrelated paused facade architecture are untouched.
