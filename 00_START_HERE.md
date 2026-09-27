# SCRAPERX — START HERE

**Write branch: `ChatGPT` only.** Other branches and older builds are provenance, never write targets or current authority.

## 1. What the game is

A physical first-person ascent of one 1,600 m industrial tower. Keep athletic parkour, meaningful falls, the parachute and real moving-support momentum. Native C++17/Jolt owns consequential state at 90 Hz; Godot 4.7 owns input, presentation and Android delivery.

Large, visible loads, ramps, levers, pendulums and direct contacts do the heavy work. The player should see why a route changes from normal play distance. A chain is useful when its aftermath creates real traversal, not merely a show.

## 2. Current position — source through +33 m, 2026-09-27

The owner rejected the old campaign and requested its removal **in the game**, followed by new macro mechanics. This supersedes the earlier restoration/preservation instruction for campaign content. It does not authorize removal or simplification of parkour.

Route elevations below name supported walking surfaces above grade. Capsule-centre and peak airborne heights are not ascent endpoints.

| Ascent segment | Present state | Delivery boundary |
|---|---|---|
| Grade → +8 m receiver → +11 m ring | Active pipe-loaded balance bridge and supported tower connection (AS-016). | Green exact-source workflow and ARM64 APK at `254eb28`; also present in the later green `3034691` APK. No device execution proof. |
| +11 → +33 m ring | AS-017 static façade parkour is integrated in source; local native/headless touch routes reach supported +33 m. | Continuous rendered touch gate failed for the AS-017 candidate; that candidate produced no APK. The last green APK predates AS-017. |
| +33 → +44 m | Swinging stair exists only in ignored local staging. | Not integrated or delivered; no rendered route proof. Its route and checkpoint proof remain unfinished. |
| +44 → +66 m | Connection, second machine and route choice are unauthored planning targets. | No integrated route or delivery proof. The +66 m objective is paused. |
| Beyond +66 m toward the 1,600 m summit | Future campaign authorship. | The existing ordinary staircase to +154 m is an optional fallback, not proof of the new mechanism/parkour route or a completed ascent. |

| Area | Current source and claim boundary |
|---|---|
| Normal gameplay source | The native pipe-loaded bridge reaches the +8 m receiver and +11 m tower ring. AS-017 then adds static façade traversal to a supported +33 m ring. |
| Retired campaign content | Intake sequence, machine-linked stairs, Hook5 cage, ground water screw/tank/lift/dock connection and upper counterweight lifts are absent from normal gameplay. Their legacy builders remain test-only. Ordinary stairs to +154 m remain the optional fallback. |
| Presentation motion | The yard crane has clock-driven boom/hook sway and the tower gear motifs mirror the native machine phase. These are presentation cues; the crane is not simulated rigging and none of this dressing operates the playable pipe bridge. |
| Ground water decision | The owner rejected the old water screw/lift's form and pacing. It is retired. The current +8 m receiver is created by the separate AS-016 pipe bridge; it is not a retained water-lift frontier. |
| Preserved | Full native walk/run, step-up, crouch, jump, vault, mantle, hang, carry, parachute, moving-support and checkpoint code. Static tower structure and its walkable staircase to 154 m are retained. The owner clarified that ordinary ramps/stairs/ladders should remain a last-option route for players who do not want a mechanism or parkour challenge. AS-017 adds climbing on reachable thin holds; see the integration-candidate evidence boundary. |
| Parkour feel | The running vault now carries horizontal speed through entry and exit. Godot renders the camera between native 90 Hz player poses, eases head motion/FOV, adds a restrained strafe lean and softens the landing dip. These are visual responses; the native controller still owns position, collision and traversal. Backflip remains a later GDD item. |
| Regression fixtures | Legacy machinery and traversal fixtures are explicit test inputs only: native `WorldContent::RegressionFixtures`, desktop `--regression-fixtures`, old CI/UI test scenarios. No gameplay menu selects them. They are not playable campaign progress. |
| Collision ownership | Normal `src/sim/world_solids.inc` is exported from the normal rendered builders. `tests/fixtures/world_solids.inc` is separately exported from the legacy test scene. CI compares both; neither table substitutes for the other. |
| New plan | Atlas owns layout; `MECHANISM_ASCENT_PLAN.md` owns implementation order. AS-016 has a green delivery; AS-017 source adds +11→33 m. Later macro mechanisms remain unimplemented. Current acceptance results are recorded in the AS-017 candidate row below. [AS-016 integration evidence](03_EXECUTION/PLANNING/AS-016_PROTOTYPES/integrated/README.md) separates production results from older isolated fixtures. |
| Reusable module archive | [Ballast bridge source, integration patch and evidence](03_EXECUTION/PLANNING/BALLAST_BRIDGE_MODULE/README.md) preserves the separate locally built +11 m alternative. It is inactive and does not replace the pipe-loaded AS-016 plan. Corrected native and headless touch tests passed; corrected rendered/full-regression completion and APK delivery remain unproven. |
| Historical reset evidence | **GREEN cleared-foundation candidate `e33aed52662d9f414028fee2df2a9ff9de18ee4a`**, [run 36257591795](https://github.com/IssisX/ScraperX/actions/runs/36257591795), job `108447210013` (`moving-support-truth`), completed 2026-09-26 17:24 UTC. All required steps passed: native default removal/checkpoint and 14-flight fallback, full retained native suite, rendered default input/removal, regression render, 23 input scenarios, audio, both collision-table comparisons, Android arm64 compile, export and artifact publication. This is historical evidence for the reset, not a green result for the new bridge. |
| Historical reset APK | [Artifact 10910869125](https://github.com/IssisX/ScraperX/actions/runs/36257591795/artifacts/10910869125), `ScraperX-build-e33aed52662d9f414028fee2df2a9ff9de18ee4a`, contains `ScraperX-e33aed52662d-arm64.apk` (32,643,996 bytes). Downloaded archive digest, embedded checkpoint commit, ARM64 native library and APK SHA-256 verified. APK SHA-256: `6053291124891ed89658ca40e03b74840fb1e6b92312a804639d6b8b57662724`. Artifact expires 2026-12-25. |
| Prior bridge delivery | **GREEN candidate `254eb2844e57eeb3d97d6872912465bd75585df5`**, [run 36269138275](https://github.com/IssisX/ScraperX/actions/runs/36269138275), completed 2026-09-26 20:44:14 UTC. [Exact evidence and limits](03_EXECUTION/ASCENT/AS-016_DELIVERY_EVIDENCE.md). |
| AS-017 candidate | Source `13e1146` adds native climbing, shimmy, lowering, beam balance and sprint from +11 to +33 m. Local native/headless touch routes reach the supported ring with no deaths. Workflow `36280224425` completed with failure at the continuous rendered touch step; no APK was produced. Earlier run `36278331701` timed out during rendered touch traversal; the follow-up batched Kit presentation but still did not pass the rendered gate. [Slice evidence](03_EXECUTION/ASCENT/AS-017_FACADE_CLIMB.md). |
| Last green delivery | Candidate `3034691f8a2e62cb3a9849d9cc0001dd05d37e30`, [run 36271706982](https://github.com/IssisX/ScraperX/actions/runs/36271706982), completed 2026-09-26 21:29:48 UTC. Nine native tests, camera proof, retained input/render/audio/collision gates and Android build/export passed. It predates AS-017. [Exact evidence and limits](03_EXECUTION/ASCENT/PARKOUR_FEEL_DELIVERY.md). |
| Last green APK | [Artifact 10915474571](https://github.com/IssisX/ScraperX/actions/runs/36271706982/artifacts/10915474571): `ScraperX-3034691f8a2e-arm64.apk`, 32,822,007 bytes. APK SHA-256: `03097a19400d8012292deb4a62bd4491eb2a15b751ab8c1a92fa6ba3ff51b927`. This APK does not contain AS-017. |
| Device | No current-candidate APK installation, Android execution or sustained Fold 6 performance is proven. The owner's observation above is evidence about their prior build, not evidence for this candidate. |

## 3. Authorities

**Project scope:** This is the native Android ScraperX game, developed from the local clone on `ChatGPT`. Product, spatial, technical and delivery authorities are the files below.

**Selected objective:** The owner chose mixed parkour and machinery toward a connected +66 m route with a route choice, checkpoint continuation and exact-source Android proof. This objective is currently paused. Claude `f872c41` is reference-branch evidence only. Backflip remains later work.

Laws → GDD → Atlas → Execution Protocol → TDD → source/tests/runtime evidence → current bounded task. A new explicit owner decision updates affected document owners; stale plans cannot override it.

- `01_PRODUCT_AUTHORITY/00_GOVERNING_LAWS.md`: product constraints.
- `01_PRODUCT_AUTHORITY/01_SCRAPERX_GDD.md`: player experience, including macro readability in §16.
- `01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md`: single spatial/content authority.
- `03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md`: ground-up delivery order and boundaries.
- `03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md`: receiving-support and physical closure requirements.
- `Mechanism-Ideas-and-archetypes.md`: revised inspiration, not a list of already working mechanisms.

## 4. Vocabulary and retired work

`WO-000`–`WO-013` remain historical kernel work orders in `03_EXECUTION/KERNEL/`, a closed set. Their original observations are retained as historical regression provenance, never current gameplay evidence.

The old `AS-001`–`AS-015` campaign schedule is retired. Authored AS-001–007 files and `GROUND_WATER_ASCENT.md` are removed from the active tree; earlier versions remain in Git history. Do not resume their intake, needles, pin/rigging, wet-isolation or upper-machine queue. Do not reuse their IDs for different content. New bounded implementation tickets can use subsequent AS identifiers when an actual design is ready. Band names and old `MOD-*`, `KX-*` and `CAP-*` comments in fixtures are historical identities, not active orders.

`03_EXECUTION/ASCENT/` holds only new authorized tickets when needed. A reset or removal authorized directly by the owner does not require inventing a replacement mechanism ticket. `MANIFEST.txt` and `SHA256SUMS.txt` remain frozen records of the earlier delivery package, not an index of this tree.

## 5. Next boundary and delivery

AS-016 completes the green grade-to-+11 m delivery; AS-017 extends the source route upward. The current delivery record is in §2. The separate ballast prototype supplies reusable components, while its overlapping geometry remains inactive. See its [compatibility review](03_EXECUTION/PLANNING/BALLAST_BRIDGE_MODULE/COMPATIBILITY_REVIEW.md).

The owner selected **mixed parkour and machines** toward +66 m. The objective is paused. See §2 for delivery status and §6 for the local stair snapshot; neither future work nor staged code is active gameplay.

Publish code candidates to `ChatGPT` one at a time, without overlapping APK candidates or overwriting another model's branch work. Check the remote head before publication. Follow the exact SHA's workflow to terminal status and verify its required proof steps and APK artifact. A timer, old green run, document update or fixture proof is not completion.

Distinguish implemented, built, APK produced, installed, executed, observed and verified on Fold. Current-source desktop proof cannot establish device behavior. Stop at the authorized slice.

## 6. Paused checkpoint — 2026-09-27 UTC

The owner requested a clean stopping point. The +66 m goal is paused, not complete. See §2 for the current gameplay source, CI result and last green APK. The later documentation-only checkpoint commit is `88cbe5b`.

Unpublished +33→44 m swinging-stair work is saved locally in ignored `artifacts/s2-staging-20260927/`, including source, Simulation integration patch, geometry, scripts, native results and checksums. Read its `CHECKPOINT.md` first. The latest exit-waypoint repair compiled but has not run; the checkpoint test still needs correction for checkpoints on moving supports. No S2 touch/render proof exists and this stair is not active in the game. This snapshot replaces reliance on temporary files for resuming the work; it is local and is not included in the remote branch.

The snapshot's statement that AS-017 CI was still running is historical; §2 records the terminal result. A fresh clone may lack this ignored directory. Check that it exists before relying on its files, and report it as unavailable if missing; its name alone is not a recoverable implementation.

On resume: resolve AS-017's failed rendered delivery and produce its exact-source Android artifact before building on that candidate. Then finish the staged stair route and correct its checkpoint test before publishing another gameplay candidate. The +44→66 m continuation and route choice remain outstanding.
