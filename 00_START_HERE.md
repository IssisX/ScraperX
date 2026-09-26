# SCRAPERX — START HERE

**Write branch: `ChatGPT` only.** Other branches and older builds are provenance, never write targets or current authority.

## 1. What the game is

A physical first-person ascent of one 1,600 m industrial tower. Keep athletic parkour, meaningful falls, the parachute and real moving-support momentum. Native C++17/Jolt owns consequential state at 90 Hz; Godot 4.7 owns input, presentation and Android delivery.

Large, visible loads, ramps, levers, pendulums and direct contacts do the heavy work. The player should see why a route changes from normal play distance. A chain is useful when its aftermath creates real traversal, not merely a show.

## 2. Current position — opening pipe bridge and parkour feel, 2026-09-26

The owner rejected the old campaign and requested its removal **in the game**, followed by new macro mechanics. This supersedes the earlier restoration/preservation instruction for campaign content. It does not authorize removal or simplification of parkour.

| Area | Current source and claim boundary |
|---|---|
| Normal play | Native pipe-loaded bridge: release twenty pipes into the pan, pull the bridge control, then cross the raised deck to the +8 m receiver and original +11 m tower ring. Kit/Jolt owns the complete mechanism. Human recorded expletives react to dangerous falls. Tower/environment and optional fallback remain. |
| Removed from normal play | Intake crane/belt/gate and stairs, swinging stair, Hook5 cage, ground water screw/tank/lift/dock connection, upper counterweight lifts and decorative moving machinery. Both presentation construction and native body/cable construction are excluded. |
| Ground water decision | The owner observed draining/moving water but climbed the spinning rectangular screw surface; they also rejected its appearance and duration. Automated water-transfer/ride proofs did not establish acceptable visual form, pacing or a credible route. It is retired; there is no retained +8 m starting frontier. |
| Preserved | Full native walk/run, step-up, crouch, jump, vault, mantle, hang, carry, parachute, moving-support and checkpoint code. Static tower structure and its walkable staircase to 154 m are retained. The owner clarified that ordinary ramps/stairs/ladders should remain a last-option route for players who do not want a mechanism or parkour challenge. No new ladder verb is claimed. |
| Parkour feel | The running vault now carries horizontal speed through entry and exit. Godot renders the camera between native 90 Hz player poses, eases head motion/FOV, adds a restrained strafe lean and softens the landing dip. These are visual responses; the native controller still owns position, collision and traversal. Backflip remains a later GDD item. |
| Regression fixtures | Legacy machinery and traversal fixtures are explicit test inputs only: native `WorldContent::RegressionFixtures`, desktop `--regression-fixtures`, old CI/UI test scenarios. No gameplay menu selects them. They are not playable campaign progress. |
| Collision ownership | Normal `src/sim/world_solids.inc` is exported from the normal rendered builders. `tests/fixtures/world_solids.inc` is separately exported from the legacy test scene. CI compares both; neither table substitutes for the other. |
| New plan | Atlas owns layout; `MECHANISM_ASCENT_PLAN.md` owns implementation order. AS-016 is implemented in source. Seven native bridge cases, real viewport input and first-person rendered captures establish the local route. [Integration evidence](03_EXECUTION/PLANNING/AS-016_PROTOTYPES/integrated/README.md) separates final production results from older isolated fixtures. Later macro chains remain unimplemented. |
| Reusable module archive | [Ballast bridge source, integration patch and evidence](03_EXECUTION/PLANNING/BALLAST_BRIDGE_MODULE/README.md) preserves the separate locally built +11 m alternative. It is inactive and does not replace the pipe-loaded AS-016 plan. Corrected native and headless touch tests passed; corrected rendered/full-regression completion and APK delivery remain unproven. |
| Historical reset evidence | **GREEN cleared-foundation candidate `e33aed52662d9f414028fee2df2a9ff9de18ee4a`**, [run 36257591795](https://github.com/IssisX/ScraperX/actions/runs/36257591795), job `108447210013` (`moving-support-truth`), completed 2026-09-26 17:24 UTC. All required steps passed: native default removal/checkpoint and 14-flight fallback, full retained native suite, rendered default input/removal, regression render, 23 input scenarios, audio, both collision-table comparisons, Android arm64 compile, export and artifact publication. This is historical evidence for the reset, not a green result for the new bridge. |
| Historical reset APK | [Artifact 10910869125](https://github.com/IssisX/ScraperX/actions/runs/36257591795/artifacts/10910869125), `ScraperX-build-e33aed52662d9f414028fee2df2a9ff9de18ee4a`, contains `ScraperX-e33aed52662d-arm64.apk` (32,643,996 bytes). Downloaded archive digest, embedded checkpoint commit, ARM64 native library and APK SHA-256 verified. APK SHA-256: `6053291124891ed89658ca40e03b74840fb1e6b92312a804639d6b8b57662724`. Artifact expires 2026-12-25. |
| Prior bridge delivery | **GREEN candidate `254eb2844e57eeb3d97d6872912465bd75585df5`**, [run 36269138275](https://github.com/IssisX/ScraperX/actions/runs/36269138275), completed 2026-09-26 20:44:14 UTC. [Exact evidence and limits](03_EXECUTION/ASCENT/AS-016_DELIVERY_EVIDENCE.md). |
| Current delivery | **GREEN parkour-feel candidate `3034691f8a2e62cb3a9849d9cc0001dd05d37e30`**, [run 36271706982](https://github.com/IssisX/ScraperX/actions/runs/36271706982), completed 2026-09-26 21:29:48 UTC. Nine native tests, camera proof, all retained input/render/audio/collision gates and Android build/export passed. [Exact evidence and limits](03_EXECUTION/ASCENT/PARKOUR_FEEL_DELIVERY.md). |
| Current APK | [Artifact 10915474571](https://github.com/IssisX/ScraperX/actions/runs/36271706982/artifacts/10915474571): `ScraperX-3034691f8a2e-arm64.apk`, 32,822,007 bytes. Archive digest, source identity, native ARM64 library and APK checksum verified. APK SHA-256: `03097a19400d8012292deb4a62bd4491eb2a15b751ab8c1a92fa6ba3ff51b927`. Expires 2026-12-25. |
| Device | No current-candidate APK installation, Android execution or sustained Fold 6 performance is proven. The owner's observation above is evidence about their prior build, not evidence for this candidate. |

## 3. Authorities

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

The local implementation completes the first useful chain from ordinary grade input through the +8 m receiver to the +11 m tower ring. The exact-source workflow and APK verification are complete for this AS-016 slice. The separate ballast prototype supplies compatible Kit/grip/checkpoint/input/audio components; its overlapping bridge geometry remains inactive. See its [compatibility review](03_EXECUTION/PLANNING/BALLAST_BRIDGE_MODULE/COMPATIBILITY_REVIEW.md). The later Colossus chain is outside this slice.

Publish code candidates to `ChatGPT` one at a time, without overlapping APK candidates or overwriting another model's branch work. Check the remote head before publication. Follow the exact SHA's workflow to terminal status and verify its required proof steps and APK artifact. A timer, old green run, document update or fixture proof is not completion.

Distinguish implemented, built, APK produced, installed, executed, observed and verified on Fold. Current-source desktop proof cannot establish device behavior. Stop at the authorized slice.
