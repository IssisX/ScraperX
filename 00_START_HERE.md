# SCRAPERX — START HERE

**Write branch: `ChatGPT` only.** Other branches and older builds are provenance, never write targets or current authority.

## 1. What the game is

A physical first-person ascent of one 1,600 m industrial tower. Keep athletic parkour, meaningful falls, the parachute and real moving-support momentum. Native C++17/Jolt owns consequential state at 90 Hz; Godot 4.7 owns input, presentation and Android delivery.

Large, visible loads, ramps, levers, pendulums and direct contacts do the heavy work. The player should see why a route changes from normal play distance. A chain is useful when its aftermath creates real traversal, not merely a show.

## 2. Current position — green exact-source route through +66 m; Fold play unverified, 2026-09-27

The owner rejected the old campaign and requested its removal **in the game**, followed by new macro mechanics. This supersedes the earlier restoration/preservation instruction for campaign content. It does not authorize removal or simplification of parkour.

Route elevations below name supported walking surfaces above grade. Capsule-centre and peak airborne heights are not ascent endpoints.

| Ascent segment | Present state | Delivery boundary |
|---|---|---|
| Grade → +8 m receiver → +11 m ring | Active pipe-loaded balance bridge and supported tower connection (AS-016). | First green at `254eb28`; retained in the latest green `a069fb6` route and APK. No device execution proof. |
| +11 → +33 m ring | AS-017 static façade parkour is active. The normal touch route reaches supported +33.9 m through the pipe bridge and façade. | First green at `baedf45`; retained in the latest green `a069fb6` route and APK. No install or device play proof. |
| +33 → +44 m | AS-018 swinging stair is active with one Kit/Jolt owner, a visible counterweight, pull chain, yielding receiver and supported +44 m exit. | `68e9422` passed all 15 native tests, including three grade→+44 m routes and two no-pull latch checks, plus continuous rendered touch from grade to tower support at Y=44.900 with zero deaths, retained gates and Android ARM64 export. Fold install/play/performance remain unproven. |
| +44 → +66 m | AS-019 is active: a side-pull counterweight lift seats at +55 m, then exterior cabinet, duct and vent parkour reaches the supported +66 m ring. The ordinary staircase remains a route choice. | Exact-source [run #182](https://github.com/IssisX/ScraperX/actions/runs/36302892635) passed all 18 native tests, continuous rendered touch from grade to Y=66.900 on support 11 with zero deaths, retained gates and Android APK export. Fold play and sustained performance remain unverified. |
| Beyond +66 m toward the 1,600 m summit | Future campaign authorship. | The current +154 m fallback is built from 14 continuous inclined collision slabs with cosmetic treads. It reads as ramps throughout the tower and needs a deliberate route redesign; it is not proof of the new mechanism/parkour route or a completed ascent. |

| Area | Current source and claim boundary |
|---|---|
| Normal gameplay source | The native pipe-loaded bridge reaches the +8 m receiver and +11 m tower ring. AS-017 adds static façade traversal to a supported +33 m ring. AS-018 adds the +33→44 m swinging stair. AS-019 adds the separate +44→55 m lift and exterior climb to +66 m. The full normal-input route is green on exact-source desktop CI and present in the exported APK. |
| Retired campaign content | Intake sequence, machine-linked stairs, Hook5 cage, ground water screw/tank/lift/dock connection and the old upper counterweight lifts are absent from normal gameplay. Their legacy builders remain test-only. The new AS-019 counterweight lift is distinct from those retired fixtures. Ordinary stairs to +154 m remain the optional fallback. |
| Presentation motion | The yard crane has clock-driven boom/hook sway and the tower gear motifs mirror the native machine phase. These are presentation cues; the crane is not simulated rigging and none of this dressing operates the playable pipe bridge. |
| Ground water decision | The owner rejected the old water screw/lift's form and pacing. It is retired. The current +8 m receiver is created by the separate AS-016 pipe bridge; it is not a retained water-lift frontier. |
| Preserved | Full native walk/run, step-up, crouch, jump, vault, mantle, hang, carry, parachute, moving-support and checkpoint code. Static tower structure and its walkable +154 m fallback are retained. The owner previously chose to retain an uncomplicated last-option route. Audit on 2026-09-27 found that its 14 native flights are continuous inclined slabs with cosmetic treads, producing visible ramps throughout the tower. This presentation and route-design defect is open; the fallback must not be described as real discrete stairs. AS-017 adds climbing on reachable thin holds; see its delivery evidence in §2. |
| Parkour feel | The running vault now carries horizontal speed through entry and exit. Godot renders the camera between native 90 Hz player poses, eases head motion/FOV, adds a restrained strafe lean and softens the landing dip. These are visual responses; the native controller still owns position, collision and traversal. Backflip remains a later GDD item. |
| Ledge pull-up report | On 2026-09-27 the owner reported that pull-up no longer happens when close to a ledge. Native mantle and hang paths and selected route tests still exist, but the current input path offers `CLIMB` only when the native ledge probe accepts that surface; from a hang it uses Jump or `CLIMB UP`. The reported ledge and installed build are not identified, so this is an open real-play interaction failure, not a verified fix or a proven removal. |
| Regression fixtures | Legacy machinery and traversal fixtures are explicit test inputs only: native `WorldContent::RegressionFixtures`, desktop `--regression-fixtures`, old CI/UI test scenarios. No gameplay menu selects them. They are not playable campaign progress. |
| Collision ownership | Normal `src/sim/world_solids.inc` is exported from the normal rendered builders. `tests/fixtures/world_solids.inc` is separately exported from the legacy test scene. CI compares both; neither table substitutes for the other. |
| New plan | Atlas owns layout; `MECHANISM_ASCENT_PLAN.md` owns implementation order. AS-016 through AS-019 have green exact-source deliveries through +66 m. [AS-019 delivery evidence and open limits](03_EXECUTION/ASCENT/AS-019_UPPER_COUNTERWEIGHT_AND_PARKOUR.md) is current; [AS-018 delivery evidence](03_EXECUTION/ASCENT/AS-018_SWING_STAIR_DELIVERY.md) and [AS-016 integration evidence](03_EXECUTION/PLANNING/AS-016_PROTOTYPES/integrated/README.md) retain their slice proofs. |
| Reusable module archive | [Ballast bridge source, integration patch and evidence](03_EXECUTION/PLANNING/BALLAST_BRIDGE_MODULE/README.md) preserves the separate locally built +11 m alternative. It is inactive and does not replace the pipe-loaded AS-016 plan. Corrected native and headless touch tests passed; corrected rendered/full-regression completion and APK delivery remain unproven. |
| Historical reset evidence | **GREEN cleared-foundation candidate `e33aed52662d9f414028fee2df2a9ff9de18ee4a`**, [run 36257591795](https://github.com/IssisX/ScraperX/actions/runs/36257591795), job `108447210013` (`moving-support-truth`), completed 2026-09-26 17:24 UTC. All required steps passed: native default removal/checkpoint and 14-flight fallback, full retained native suite, rendered default input/removal, regression render, 23 input scenarios, audio, both collision-table comparisons, Android arm64 compile, export and artifact publication. This is historical evidence for the reset, not a green result for the new bridge. |
| Historical reset APK | [Artifact 10910869125](https://github.com/IssisX/ScraperX/actions/runs/36257591795/artifacts/10910869125), `ScraperX-build-e33aed52662d9f414028fee2df2a9ff9de18ee4a`, contains `ScraperX-e33aed52662d-arm64.apk` (32,643,996 bytes). Downloaded archive digest, embedded checkpoint commit, ARM64 native library and APK SHA-256 verified. APK SHA-256: `6053291124891ed89658ca40e03b74840fb1e6b92312a804639d6b8b57662724`. Artifact expires 2026-12-25. |
| Prior bridge delivery | **GREEN candidate `254eb2844e57eeb3d97d6872912465bd75585df5`**, [run 36269138275](https://github.com/IssisX/ScraperX/actions/runs/36269138275), completed 2026-09-26 20:44:14 UTC. [Exact evidence and limits](03_EXECUTION/ASCENT/AS-016_DELIVERY_EVIDENCE.md). |
| AS-017 delivery | Source `13e1146` adds native climbing, shimmy, lowering, beam balance and sprint. Earlier rendered attempts `36278331701`, `36280224425` and docs run `36285233929` timed out. **GREEN exact source `baedf4562e04a8145f813538baffa14ee276a462`**, [run 36287329304](https://github.com/IssisX/ScraperX/actions/runs/36287329304), completed 2026-09-27 02:37 UTC: native grade→+33 m/checkpoint, headless and rendered 30 FPS touch route with zero deaths, retained regression input/render/audio/collision gates, Android arm64 cross-compile and APK export passed. The test temporarily used a 432×371 canvas viewport and restored the shipped Fold project settings before export. Captures show the raised bridge and supported upper arrival; close rack/pan views are partly occluded. [Slice evidence](03_EXECUTION/ASCENT/AS-017_FACADE_CLIMB.md). |
| AS-018 delivery | **GREEN exact source `68e9422cf6a1b7282264b2fd9eed7ae10ba25bf8`**, [run 36295602489](https://github.com/IssisX/ScraperX/actions/runs/36295602489): all 15 native tests, rendered grade→+44 m touch route with zero deaths and supported Y=44.900, retained input/render/audio/solids gates, Android ARM64 compile and APK export passed. [Stair delivery evidence](03_EXECUTION/ASCENT/AS-018_SWING_STAIR_DELIVERY.md) records the physical route, checkpoint test, visuals and limits. |
| AS-019 delivery | Local ARM native modes 5–7 pass: supported +66 m, no-pull latch and machine/player checkpoint restoration after a fatal fall. Continuous headless Godot touch passes grade→+66 m with zero deaths. Local ARM retained suite passed 17/18; its sole failure is the legacy AS-002 mid-landing fixture also seen on the preceding green source. [First run #181](https://github.com/IssisX/ScraperX/actions/runs/36302140280) stopped at a stale Godot body-count check after its full x86 native suite passed. Corrected [run #182](https://github.com/IssisX/ScraperX/actions/runs/36302892635) passed the full x86 suite, rendered route, retained gates and Android export. Lift ride screenshots show weak counterweight framing; strict no-weight/no-cable and Jolt energy/band proofs remain open. |
| Last green delivery | `a069fb6d7c39f85e1a59e2034548b71aadddc720`, [run 36302892635](https://github.com/IssisX/ScraperX/actions/runs/36302892635). This establishes the exact-source desktop route and Android build, not installation, device execution or sustained Fold performance. |
| Last green APK | [Artifact 10926029736](https://github.com/IssisX/ScraperX/actions/runs/36302892635/artifacts/10926029736): `ScraperX-a069fb6d7c39-arm64.apk`, 32,908,023 bytes. Downloaded checkpoint names `ChatGPT` and the full source SHA; APK archive integrity and embedded arm64 native library passed. APK SHA-256: `a361047a46caffe29476709ff8687536198d9e56d23fd4ea0b9101eeab8ccb93`. A checksum-matched copy is in Android Downloads. |
| Device | This Termux host reported SM-F956U (Fold 6), Android 16 on 2026-09-27. Direct silent installation from its app UID was denied. Installer intent returned but no prompt appeared for the owner. Installation, Android execution, touch ergonomics and sustained Fold frame-rate remain unverified. |

## 3. Authorities

**Project scope:** This is the native Android ScraperX game, developed from the local clone on `ChatGPT`. Product, spatial, technical and delivery authorities are the files below.

**Selected objective:** The owner chose mixed parkour and machinery toward a connected +66 m route with a route choice, checkpoint continuation and exact-source Android proof. The owner resumed this objective after the earlier pause; it is active and incomplete. Claude `f872c41` is reference-branch evidence only. Backflip remains later work.

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

AS-016 through AS-019 have green exact-source desktop delivery and Android APK export through the supported +66 m tower ring. Fold installation, play and sustained performance remain open, along with AS-019 presentation and strict physics checks. The 14-flight inclined-slab fallback is a major unresolved layout defect: the visible tower is dominated by easy ramp ascent, contrary to the desired discovery and traversal feel. The current record is in §2. The separate ballast prototype supplies reusable components, while its overlapping geometry remains inactive. See its [compatibility review](03_EXECUTION/PLANNING/BALLAST_BRIDGE_MODULE/COMPATIBILITY_REVIEW.md).

The owner selected **mixed parkour and machines** toward +66 m. The objective is active. See §2 for delivery status and §6 for the stair's local audit snapshot. The separate ignored snapshot is not an additional active mechanism.

Publish code candidates to `ChatGPT` one at a time, without overlapping APK candidates or overwriting another model's branch work. Check the remote head before publication. Follow the exact SHA's workflow to terminal status and verify its required proof steps and APK artifact. A timer, old green run, document update or fixture proof is not completion.

Distinguish implemented, built, APK produced, installed, executed, observed and verified on Fold. Current-source desktop proof cannot establish device behavior. Keep each delivery claim bounded to its proven slice.

## 6. Saved staging checkpoint — 2026-09-27 UTC

The earlier pause preserved the +66 m work; the owner has since resumed it. See §2 for the current gameplay source, CI result and last green APK. The earlier documentation-only checkpoint commit was `88cbe5b`.

The ignored `artifacts/s2-staging-20260927/` directory remains the original local audit snapshot, including source, Simulation integration patch, geometry, scripts, native results and checksums. Read its `CHECKPOINT.md` for provenance; current integration is in `src/sim/swing_stair.*`, the Simulation seam, Godot input test and the CI workflow. Its earlier candidate status is superseded by the green AS-018 delivery in §2. The ignored snapshot is not included in the remote branch.

Any AS-017 candidate status inside that earlier snapshot is historical; §2 records the later green delivery. A fresh clone may lack this ignored directory. Check that it exists before relying on its files, and report it as unavailable if missing; its name alone is not a recoverable implementation.

Next: redesign the 14 inclined-slab fallback and its visual dominance without casually removing the owner's requested last-option route; then verify collision, route choice and presentation together. Install the verified AS-019 APK on the Fold, play the normal route, inspect touch control and sustained frame rate, and improve the lift ride's counterweight readability. Strict no-weight/no-cable and Jolt energy/band proofs remain open. The local ARM full native suite fails the legacy AS-002 mid-landing fixture at the same point on the preceding green source; the exact-source x86 CI suite passes all 18. Do not label the game mobile-ready before installation, play and sustained performance on the Fold.
