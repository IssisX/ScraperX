# Actual native crane assembly cycle — 2026-10-15

Result: **complete native loaded lift/hold/land/release/drive/restore/retry PASS trial12; individual-leg API PASS trial13**. Current acceptance is the final section below; preceding sections preserve actual rejected hypotheses and failures. World11m transfer, public hand-reach transactions, Godot and APK remain separate root integration work. Branch ChatGPT, base8569c721432e4139df6286b8e8d1cce94b2db9ca. All concurrent work preserved.

The accepted PoweredCable prerequisite was reused unchanged via `--requires articulated-hoist`. The persistent [assembly window](assembly-window.json) retains the older failed attachment trial2 and evidence-based reassessment: settled suspension left the rear eyes5.4mm below fixed cable-stock ends, so the real empty hoist must be lowered. The authored low-speed alignment now succeeds on the actual cab/tread truck; no stock stretch, body pose/velocity edit, force increase, failure-threshold relaxation or fake mass/COM was introduced.

## Build and actual execution

Private build command: `proot-distro login ubuntu -- python3 /data/data/com.termux/files/usr/tmp/scraperx-crane-mechanics-20261015/build.py`.

C++17/O3/Wall/Wextra/Wpedantic/Werror, one compiler job, pinned Jolt source/cache, accepted gate-free v2 headers first on the include path. Accepted base archive: `/data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015/build-v2/libscraperx_sim.a`, SHA256 `18af7c94f9048807d90036ce33a1b9d48c4ff38dcb38868c06171c170eaddf9b`. Jolt archive SHA256 `8711a784f394fdf91af3366ab0b6695e80322cb73b46a8d1659e775b20aced1b`.

Strict build PASS. An initial build's source-stability assertion rejected my test edit made during compilation; only the affected test was then rebuilt and linked to obtain a coherent passing receipt. Trial4 executable SHA256 `9e337fad032d73d9bfb9ac0fc3ab1336090be18bec0d06773dad43287b7ecca6`. Latest trial5 executable SHA256 `2396d1e6ddb6d6771c7103f555ed07832cee4f35e45dea0478b87c863d012e5a`. Private `build-receipt.json` records exact commands and matching object reuse.

Actual executions: `python /data/data/com.termux/files/usr/tmp/scraperx-crane-mechanics-20261015/run_cycle.py`, which invokes `ci/mechanics_trial.py --state evidence/crane-2026-10-15/assembly-window.json run --stage full-native-crane-loaded-cycle --requires articulated-hoist`, records native owners/archive/binary hashes, and runs the binary under Ubuntu proot.

- [Trial3](assembly-window.trial-3.log): alignment and actual four-eye attachment succeed; loaded raise loses front slings (mask12), so cycle FAIL.
- [Trial4](assembly-window.trial-4.log): first-transition diagnostic FAIL at lift tick51. Chassis y0.851362 from settled0.608751, mask14, hoist tension35949.9N. Front sling0 actual3.35725m versus stock3.356m, safely below unchanged1% failure threshold3.38956m; its25mm capsule cast hits body8388610 (index2). Fixture ground is index0, actual truck chassis index1 and production mast index2, matching construction order. No other sling cast is blocked at that first failure.

Supported cause: the front sling intersects the actual articulated forklift mast as the suspended assembly changes pitch. This is a physical routing/stow/load-distribution issue, not cable stock shortage, overlength failure or evidence of insufficient pull force. The2failure gate is reached; no further trial was run or silent budget reset performed. A concrete reassessment is required before another experiment.

## Changed acceptance observations

The source-energy lower bound now derives from actual chassis inverse mass and measured rise instead of obsolete3132kg pre-cab mass. Neutral hold additionally rejects positive motor event work. The lift loop captures the first consequential failure, and phase traces report real sling length/stock and obstruction BodyID. These assertions remain present but later cycle gates are unexecuted because lift fails first.

## Transport stow correction and current blocker

The first proposal to widen the fixed spreader was rejected before implementation: its vertically hanging free ends would sit58cm outboard of the real eyes, violating unchanged6cm attachment alignment. No geometry was changed. Root selected actual finite hydraulic stow at existing fully lowered forks and negative8degree forward mast stop. The negative sign follows the authored hinge axis `orientation * -AxisX`: a negative hinge angle rotates the mast top toward actual truck forward. The geometric prediction was ~0.42m forward mast-top displacement.

`YardCrane::try_attach` now requires actual truck lift below0.10m and measured mast tilt within0.02rad of the existing configured minimum. Header comments document this transport-stow contract. The fixture first physically aligns and rejects unstowed pickup, then commands actual `tilt=-1/lift=-1` until measured lift<5mm and tilt within2mrad of the stop, neutral-settles90ticks, and physically aligns the empty hoist again. No truck force, inventory, geometry, body pose or velocity was changed.

The SAME persistent window was explicitly reassessed using trial4, the mast-forward prediction and the rejected wider-spreader explanation. Strict focused native build PASS. The single bounded [trial5](assembly-window.trial-5.log) then FAILS **before lift** at unchanged four-eye hardware alignment. Actual finite stow reaches lift~-0.0000024m, tilt=-0.137687rad. Chassis settles at(-80.0313,0.645028,-150), compared with initial(-80.0041,0.608751,-150). Actual front-eye/free-end deltas are approximately(-0.0494,+0.08236,0)m:96mm distance. Rear deltas(-0.0607,+0.00103,0)m are60.7mm. Cable-stock reach itself passes: front3.27401m/3.356m, rear3.10557m/3.106m. The authored stock/free-end posture fit the old unstowed suspension pose, but the finite stow changes actual chassis pitch/position enough to violate the real6cm pickup envelope.

Lowering the empty rig increases the existing front vertical mismatch; raising enough to reduce it exceeds rear fixed-stock reach. A physical stowed-pickup stock/rig-posture correction and horizontal positioning are now the required geometry/route decision. Geometry remains outside this worker's write scope. Root received the measured values; no second run, coefficient sweep or guard change was made. Trial5 has one failure after the explicit reassessment; full lift/hold/land/release/drive/retry remains unexecuted for this correction. The previous mast-interference cause remains preserved, and the forward-stow clearance prediction has not yet been tested under load.

Root owns next correction and any further reassessment on this same persistent window. Full world11m freight transfer, bridge/Godot controls, phone and APK evidence remain separate integration work.

## Fixed-stock and finite-positioning trials6–9: decisive support failure

Root shortened each actual front leg by four20mm links to3.276m; rear stock remains3.106m. Spreader eyes/mass and actual truck remain unchanged. Root geometry SHA256 `a9950002e746e163a39a3bbccfdaa797faa04eaa6a95a97056859b7c378cbdc1`. The same persistent window was explicitly reassessed using trial5 and the physically fixed stock/finite positioning prediction.

Trial6's instantaneous spreader feedback excites the long31m pendulum; this is a **fixture guidance failure**, not evidence against the corrected stock. Trial7 uses one trolley position target derived from actual initial mean truck-eye/free-end error projected along the actual boom axis (~55mm). Finite carriage position/velocity feedback approaches that target, hoist pays out only for real stock reach, then neutral holds. Trial7 still failed; source inspection found unnecessary helper-only6mm mean centering/15mm/s axis-speed restrictions. These were removed after explicit reassessment of the SAME window. Native60mm alignment, actual per-leg150mm/s relative-point speed, fixed-stock reach, clearance, stationary truck and wheel support checks were preserved.

Trial8 still failed; the extra helper restrictions were therefore **not the complete cause**. Trial9 adds discriminating actual wheel/contact/force and point-speed observations, without changing controls or mechanics. Strict build PASS; actual trial9 FAIL before attachment. The2failure gate is reached after the latest explicit reassessment; no further run was performed.

**Supported cause, trial9:** fullforward tilt with fully lowered long forks physically unloads both front wheels. Initial all-wheel forces are4643.47/4651.29/23789.3N, all three contact=true and normalY1. After stow, frontwheel0/1 contact=false, normalY0, force0; rearwheel contact=true normalY1, force29663.8N (29672.9N after final settling). Chassis rises from y0.608751 to0.645030. At final alignment, the rig itself passes the measured envelope: front free-end errors~21mm, rear~10mm; lengths3.27227/3.276m front and3.10459/3.106m rear; actual endpoint relative speeds0.00820–0.00857m/s; chassis speed0.0000451m/s/angular0.000342rad/s, no sling obstruction cast hits. The unchanged native all-wheel support guard correctly rejects this tine-supported posture.

Root's geometric explanation is consistent: fork tips extend~2.23m beyond the tilt pivot, so forward8degrees can lower tips~0.31m, exceeding their lowered grade clearance. Fork-tip world height was not recorded in the completed discriminator; no extra run was made to fill that reporting gap.

The current lift<0.10m+fullforward stow contract is **not accepted as a usable transport posture**. Root will choose finite carriage lift sufficient to clear actual tines above grade (proposal~0.35m), preserve all-wheel support and revise the stow gate accordingly. Fixed stock derived from the tine-supported chassis posture needs reassessment against that actual supported posture. No geometry/force/support-guard change is authorized or made by this worker beyond the root-owned stock change described above. No further tuning sweep or experiment was performed.

Latest private executable SHA256 `970a4d1fe45e94b12765c5794cffd66c7bc2dccb1254b44b0f20414347e19353`; command and native owners are retained in the persistent window's trial9 receipt. Full loaded lift/hold/land/release/drive/retry remains pending.

## Actual solid clearance correction and trial10: loaded mast obstruction remains

Root restored original3.356mfront/3.106mrear fixed stock after rejecting stock fitted to the unsupported ground-jacking posture. Native pickup no longer uses a guessed scalar lift threshold. It requires the actual fork coupling, mast within0.02rad of the existing forward stop, unchanged stationary/all-three-wheel support, a nondegenerate upward walkable plane through actual wheel contact points, and every transformed corner of every actual Kit fork box at least80mm above that plane. Unknown nonbox/empty fork topology rejects pickup. The actual boxes include their authored offsets/rotations and real fork-body transform. Solved contact with any external body also rejects pickup; wheel-plane grade is not treated as omniscient obstacle clearance. The truck's own connected chassis/mast/carriage are excluded only from that external-contact check. Existing sling hardware/point-speed/stock/capsule-cast checks remain unchanged.

Fixture finite hydraulics raise actual carriage first to~0.35m with neutral mast, stop lift, then tilt to the real forward stop and settle. It reports actual all-wheel reactions and minimum fork-solid clearance before alignment and pickup. Original stock was rebuilt; no actual truck, mass, force, gravity, damping or velocity assignment changed. SAME window explicitly reassessed from trial9 with the geometric correction. Strict C++17/Werror build PASS.

[Trial10](assembly-window.trial-10.log): valid supported stow and actual four-leg acquisition PASS. Before pickup lift0.352146m, tilt−0.138481rad, minimum actual fork-box vertex height0.152178m above the actual wheel plane. Wheel contact=true for all3, normalY1; forces4768.57/4776.38/23539.2N. Chassis y0.608765. Full native rig attaches mask15 with unchanged stock/reach/speed/cast gates.

**FIRST loaded failure** at tick93: chassis y1.09173 (+0.482965m), frontleft sling0 cast hits actual mast BodyID8388610, mask14. Its length3.35746m is below the unchanged1% stock failure threshold3.38956m. Hoist35.26kN/source22572J. Correct forward stow delays the interference relative to old unstowed tick51, but does not establish a clear suspended path. All actual bodies and momentum are preserved when the failed leg removes. Full>1mlift/hold/land/detach/drive/retry remains unexecuted. There was one useful execution after this correction, no force/geometry parameter sweep and no extra replay.

Root must resolve actual suspended mast/sling routing or actual combined load balance; the80mm/tire-supported stow correction is physically supported, but it is not sufficient alone. Per-leg interaction API requested by root is deferred until the complete cycle resolves. Latest executable SHA256 `70ff1090fbdf1bd98556be8b4d37040e98bf29407ecb601f440ddd1816396823`. Exact source/archive/binary identities and command remain in the persistent window trial10 receipt.

## Material balance correction: full native cycle accepted, trial12

Root physically extended rear spreader rails to z−1.25m/crossmember−1.30m and solved the rear85.952kg plate/block location from every existing fixed material mass. Actual empty-spreader centroid and connected hook now share z−0.419814m, matching the measured actual stowed truck load center. No COM/inertia override, fake force, changed sling eye/stock, smaller probe or failure tolerance was used. Geometry SHA256 `71194bbd625df8e1b09666fc0699d1b823037e844c539a4f73f969692da65a29`. The SAME persistent window was reassessed from [LOADED_CLEARANCE_CAUSE.md](LOADED_CLEARANCE_CAUSE.md): trial11 established real mast-crossbar contact and common~19degree loaded pitch from the~4.77kNm mismatch. All earlier failures and reassessments remain preserved.

Strict C++17/Werror build PASS. [Trial12](assembly-window.trial-12.log), stage `full-native-crane-loaded-cycle`, actual command `python /data/data/com.termux/files/usr/tmp/scraperx-crane-mechanics-20261015/run_cycle.py`, **PASS** using unchanged accepted `articulated-hoist` prerequisite and accepted v2 forklift/archive/header layout.

- Real supported pickup y0.608765m; actual truck rises to y1.98731m:1.378545m ascent. Rig stays15/failure0, actual hoist force35.52kN; source draw87375.6J. The fixture's potential-energy floor derives from actual chassis inverse mass and measured rise; actual carried truck aggregate from retained mass observation3372.96kg gives~45.6kJ gravitational rise, also below measured source draw.
- Actual neutral hold y1.99379m:6.48mm height change over180ticks, below unchanged30mm gate. Source remains87375.6J; exact cumulative positive motor work remains unchanged. Suspended rig release refuses.
- Finite payout lands on real wheels, forces3594.69/3472.98/28370.1N; actual fork clearance143mm, slack rig releases mask0. Truck y0.608701m is64micrometers below pickup height. The same native truck BodyID remains unchanged and physically drives>0.2m under its existing drive command.
- Body/vehicle/crane snapshots restore with owner lifecycle; actual re-alignment and pickup return mask15 with exactly four sling joints and no duplicate constraints. Runtime operations do not assign poses or velocities; only explicit checkpoint restore restores the captured bodies as designed.

Trial12 executable SHA256 `0dda3d904753897d70926fcea675ef4fc0c43d07e6390ab303cfa62ccab940db`; accepted native owner at that run `61987e61019b8d9d251b056f757944f8ff821785edeca53833d2278d2619e5b5`. This run precedes the API extraction below; no second full-cycle campaign was performed.

## Individual-leg integration API accepted, trial13

New public APIs: `bool try_attach_leg(unsigned,const WarehouseForklift&)` and `bool try_detach_leg(unsigned,const WarehouseForklift&)`. They transact only the selected actual leg. The all-four attach and detach transactions remain. Attachment's same real support/transport-stow and selected hardware/stock/speed/obstruction logic is shared through private owner helpers, rather than duplicated authority. Remaining legs may be assembled while the rig is partial; duplicates, invalid indices and failed owner state reject without replacing existing joints. Per-leg release additionally requires real support/stow/selected hardware geometry and the existing>=20mm slack/<=150N tensile limits. Host Simulation separately owns actual grounded-player hand reach/clearance and transaction selection; these APIs grant no remote operator authority.

Strict C++17/Werror rebuild PASS. [Trial13](assembly-window.trial-13.log), stage `individual-native-rig-transactions`, actual command `python /data/data/com.termux/files/usr/tmp/scraperx-crane-mechanics-20261015/run_individual.py` invokes the real fixture with `--individual-legs` and **PASS**. It proves actual incremental mask1→9→11→15 and exact added joint counts; invalid/duplicate attach-detach and atomic partial-rig replacement negatives; rejection before sufficient slack; finite35mm payout/neutral settle; selected-only individual release in arbitrary order; restored original constraint count; preserved atomic four-leg attach/release compatibility. This is the focused acceptance for the added API and shared predicate extraction, not a repeated loaded cycle.

Latest executable SHA256 `33964e59d345bd6ffb07fc0e5b5856d8c02b2eedbbb5a0ae3518d3988b54d70c`. Both builds reuse accepted archive `18af7c94f9048807d90036ce33a1b9d48c4ff38dcb38868c06171c170eaddf9b`, pinned Jolt `8711a784f394fdf91af3366ab0b6695e80322cb73b46a8d1659e775b20aced1b`, and coherent v2 headers, with max1 compiler. Source/binary/archive binding and exact execution commands remain in the persistent window. No accepted PoweredCable primitive rerun, commit, push, dependency change or world/device claim occurred.

Remaining root work: public individual-eye hand-reach transactions and controls, fullworld11m freight receiving/drive-out path, presentation and delivery. Native full cycle and per-leg interface are complete within this worker's ownership.

## Latest tested source hashes

- `src/sim/yard_crane.cpp`: `dfdf8af443565e5f13455103762a70a5eb8d2ab929fd59a469bc270cdc872a87`
- `src/sim/yard_crane.hpp`: `fa8e5c0734fca079dd12a8666fdfe8da424dd00080330db0a7d72627ca95513e`
- `src/sim/yard_crane_geometry.cpp`: `71194bbd625df8e1b09666fc0699d1b823037e844c539a4f73f969692da65a29`
- `src/sim/warehouse_forklift_yard.cpp`: `733248d2501d2fb655055f33b83c4120b88a613aad19d42c807f1ce677bfe6d2`
- `tests/yard_crane_tests.cpp`: `b46c28e08d11fbffa7266c5a88fc1c72f32ea2416c1ec6518fd9f4db97e8997a`
