# Suspended ladder: receiver arrest and transfer diagnosis

Repository `ChatGPT` at `c2b2517d67d19e8ae4843db6177048db546277ad`; all existing dirty/untracked work preserved. This assignment changed private probes and this evidence file only. Root owns production `hand_transfer.cpp` and `tests/suspended_ladder_tests.cpp`.

## Frozen source and executable evidence

Private directory `/data/data/com.termux/files/usr/tmp/scraperx-ci-ladder-retention-probe-20261015/` contains the full scripts, drivers, logs and execution receipts. `final-transfer-source-receipt.json` records exact file hashes.

- Private adaptive archive SHA256 `8108c065ec800f153ed803ff674d477cd84a38783fc8a91e9e3dc9efe69479dd`: prior immutable18cc native archive plus root's adaptive lip-pair shimmy owner.
- Frozen public header SHA256 `57f165f39595c6100f3e8a094f7df7646ca6248c54a29d9be7c974c2b8285e1f`.
- Jolt archive SHA256 `8711a784f394fdf91af3366ab0b6695e80322cb73b46a8d1659e775b20aced1b`.
- Build/run: `proot-distro login ubuntu -- /usr/bin/python3 <private-directory>/final_transfer_trace.py`, then the separately motivated paid-shortening trial `paid_crouch.py`.

Both commands built successfully and executed the real public Simulation route. The first ran89.39s and exited1; the paid-crouch trial ran48.31s and exited1. These are native failure observations, not full acceptance, APK or device evidence. No production force/reach/timing changes or sweep occurred.

## Serious rivals and discriminator

The inherited arrest failure still reported foot-start1, shared-solve1 and transfer1. Serious explanations were (1) completed receiver top-out followed by walking/falling, (2) automatic transfer in progress, or (3) actual loaded-hand reach loss. One per-tick public Snapshot trace from flight through neutral arrest distinguished them without changing physics or input.

The foot-start/shared-solve/transfer counters already equal1 at tick1475, before receiver catch, and remain unchanged. Every receiver-catch sample has no transfer plan, no receiver plan identity and no grounding. They describe an earlier transfer in the same life. No receiver top-out or firm-foot completion occurred.

## First failed transition: neutral radial arrest

The actual receiver1960 catch occurs at tick1521, position `(7.47930479,118.4823532,-179.9195404)` and velocity `(3.978832006,-4.996974468,1.822597146)`m/s with two attached hands. The trace switches the public move input to neutral immediately after that observed catch.

Neutral selects the existing passive radial owner. The next sample's actual hand load is74.87N following radial reconstruction; load rises to the existing3000N combined cap by tick1534. At tick1548, position is approximately `(8.66353,117.440,-179.396)` and velocity `(3.05964,-.280295,.657067)`m/s. The original rigid anchors remain at x7.861814/8.141814, y119, z-179.28. The trailing-hand COM radius is about1.758m, above the existing1.75m reach guard.

At tick1549, traversal changes Hanging→None and hand count2→0; position `(8.69753,117.437,-179.389)`, velocity `(3.05964,-.389295,.657067)`m/s. The .109m/s vertical decrement is ordinary gravity after release. Plan/grounding remain absent and transfer counters unchanged. Shared `actual_hands_in_reach`/radial-sync owns this release; it is not a foot-row lifecycle failure.

The neutral command adds zero target debit (constant33603.2J cumulative conservative ledger). The finite hands nearly arrest descent, but sideways momentum takes the trailing attachment beyond reach first.

## One physically predicted paid-shortening trial

Added only public `set_crouch_input(true)` immediately after the same actual catch, with neutral movement. This uses existing `advance_radial_targets`: .6m/s maximum shortening, .25m posture allowance,500W positive rest-target work and1500N/hand force limits, all unchanged. The source distinguishes conservative command debit from positive supplied work.

Both actual receiver hands remain attached for the full2s trial. Maximum observed COM-to-hand radius is1.749549659m; no reach/force widening. Cumulative conservative target debit increases exactly750J, consistent with .25m×2hands×1500N. Actual per-tick supplied work/power was not printed by this driver; the500W cap is source-enforced, not a separately measured power receipt.

Descent first arrests at tick1549: position `(8.701033592,117.5004196,-179.3972015)`, velocity `(2.820786953,.2072177529,.615452528)`m/s, two actual hands, no grounding or receiver transfer. Minimum y117.4991455 at tick1548. Maximum x8.943854332 at tick1565. The actual radial arc continues around the planted anchors.

The first remaining assertion failure is `finite receiver catch reaches three slow solved intervals`: the driver requires total3D speed<.3m/s for3consecutive ticks before allowing shimmy. Only one slow sampled interval exists (tick1581, speed~.278m/s), and the two-second trial ends still Hanging with both actual hands. This observer requirement is unsupported by the radial owner, which intentionally permits tangential swing; it does not establish a mechanical attachment failure.

## Smallest useful repair boundary

Retain root's supported shared adaptive lip-pair validation fix. For this downstream failure, the next useful owner is the public route's control/observation sequence: paid radial shortening arrests the descending catch without rescue; transition to a real inboard Cartesian shimmy on observed descent arrest rather than waiting for artificial total rest. Require actual generation/anchor movement as well as COM clearance before the same physical hand-to-foot top-out. Keep original upper-receiver stable footing, finite bounds, same-life recovery/reset, and onward supported121.9m exit assertions.

This is a supported next control-path candidate, not an accepted end-to-end fix. No new production physics correction is supported by the present evidence. A continuation trial should report its first failing transition and must preserve the original upper and onward acceptance.

## Approved one-event continuation trial

`event_arrest.py` compiled and ran once,66.9396s, exit1. It preserves every original receiver/top-out/onward assertion and all physical bounds, replaces total-rest waiting with the first observed `vy>=0`, then requires actual inboard regrip before one Action. No production source changed.

At tick1549 the paid radial segment has consumed134.38687J of actual positive rest-target work, separately from269.75441J conservative debit. Maximum sampled positive work increment divided by the90Hz step is499.99950W. The mean commanded radius is0.891935706m, down from approximately0.981854m at catch. These metrics are in `event-arrest-analysis.json`; the per-tick log includes actual/commanded span and positive/absorbed work ledgers. The maximum sampled COM-to-hand radius across this continuation is1.74685064m.

At tick1550 the actual trailing hand regrips fromx7.861814 to8.358481 while the other planted anchor stays at8.141814. Actual anchor midpoint advances0.248333m and generation changes, satisfying the retained material-motion assertion. COM is `(8.731339455,117.5019073,-179.3905792)`. Thus the paid catch and genuine regrip both succeed through the original owners.

One top-out Action is issued next. At tick1551 the state remains Hanging, plan0, no receiver plan and no footing. It stays Hanging with two genuine hands through all153top-out ticks. Final COM `(8.554765701,117.5091248,-179.4819336)`. The first failing assertion is `real receiver hand-to-foot top-out completes within153ticks`; no foot transfer starts, so the transfer-lifecycle/foot-row theory remains contradicted.

The Action was issued before a demonstrated necessary entry condition. `begin_cargo_transfer` computes shoulder as COM+0.45m and requires lip distance<=1.05m. At the actual pre-Action pose, remembered lip `(8.008481026,119,-179.279998779)` is1.27798695m from that shoulder, outside the unchanged entry envelope. The driver only advances the remembered ledge/landing baseline6.667mm while a leading real hand moves0.496667m; COM moving inboard and a single regrip do not establish mantle-entry geometry.

**Evidence limit:** this trial did not instrument which internal early return rejects Action first. `begin_mantle_from_hang` checks landing-capsule clearance before entering `begin_cargo_transfer`; near-corner landing clearance is therefore a serious rival to the already numerically violated shoulder condition. No internal first-return attribution, successful upper footing, onward exit or complete route acceptance is claimed.

Next useful discriminator, if root continues this route: one private observer in existing `begin_mantle_from_hang`/`begin_cargo_transfer`, printing only the actual Action's first rejection (landing clearance versus refreshed lip/shoulder/receiver gates), plus commanded body target and existing load. Keep the event-based paid catch that demonstrably survives, preserve actual regrip, and command enough real shimmy/paid posture to satisfy the measured entry geometry before the single Action. Do not increase reach/force or accept lower catch-deck recovery as upper receiver success. Root retains integration ownership.

## Four-owner rejection repair: bounded follow-up

Root supplied immutable four-owner rejection archive31a95b421ec628e838d071534076d64be19dc21c3001f7cbff37baf7f1fdd865, with independently observed charged-Action RED-old/GREEN-new evidence. This assignment did not repeat that fixture. It found the additional early exits in `apply_traversal_commands` and `begin_mantle_from_hang`; root removed those and the Action-only radial sync exit alongside the two hand-transfer exits. Root owns all production edits.

Private subdirectory `final-gates/` reuses that archive and frozen headers, replacing only `hand_transfer.o` and `legacy_traversal.o` with read-only first-return diagnostics. Observer archive SHA256756720beb5d3a75e4058136f329cad3ea9072056d4761cf33469d9dbf810b7b2. Logs include the real Action's actual position, lip, landing, commanded body target, radial state and rejected predicate. Instrumentation adds no control, force, pose or time changes.

The first readiness candidate kept paid radial posture until actual shoulder entry range before shimmy. It ran43.729s, exit1. Shoulder range first occurs at tick1581, COM `(8.64006424,117.8199997,-179.1448975)`; after leaving radial mode the entire270tick shimmy leaves anchors and remembered ledge unchanged, ending with the same two actual hands. No receiver Action is issued. The body has swung under the plate (top capsule/underside contact is consistent with constant y117.82), but an internal shimmy first-return was not printed, so swept-clearance rejection remains a supported hypothesis, not exact attribution. This timing/control sequence is rejected; no force/reach change is justified.

A separate matched first-Action discriminator reuses the same already-compiled observer archive and original early-arrest→actual-regrip control sequence. Exact receiver Action output:

```
TOP_GATE entry radial=0 position=(8.731339455,117.501907349,-179.390579224) lip=(8.008481026,119.000000000,-179.279998779) target=(8.008481026,119.919998169,-178.929992676) command=(8.701033592,117.500419617,-179.397201538) shoulder=1.277989388
TOP_GATE passed=landing_and_actual_lip
TOP_GATE reject=shoulder distance=1.277989388
```

This closes the serious rival: actual landing clearance and refreshed lip pass; `begin_cargo_transfer`'s shoulder entry guard is the first rejection. The route requests its one Action too early in the actual hand-supported stance. The existing1.05m entry limit is unchanged.

The next bounded continuation keeps the proven early descent-arrest shimmy, retains ordinary paid Crouch while approaching, and requires actual remembered-lip baseline x>=8.25, COM x>=8.25, real generation/anchor motion and actual shoulder-to-lip<=1.05 before one Action. It uses the same observer archive, with no owner recompilation or coefficient/geometry sweep. Original upper-receiver stable footing and onward121.9m exit assertions remain present.

## Early shimmy with retained paid posture: actual transfer begins

The private `early-shimmy.py` continuation reuses the observer archive without owner recompilation. It holds public Crouch during the proven early-descent-arrest shimmy, waits for actual remembered-lip/COM inboard geometry and shoulder entry, then requests exactly one Action. No reach/force/coefficient/geometry change occurs. All finite gravity/hand/work/foot checks continue passing until the deadline assertion.

At tick1595 before Action, COM `(8.514418602,117.540847778,-179.479522705)`, remembered lip `(8.308502197,119,-179.279998779)`, landing `(8.308502197,119.919998169,-178.929992676)`. Actual shoulder distance1.049097538m passes the unchanged1.05m gate. Diagnostics explicitly print both `passed=landing_and_actual_lip` and `passed=all_transfer_validation`. At tick1596 the real owner enters Climbing, plan1:generation2, AwaitingFootContact, actual receiver1960 and two real hands. No transfer foot row is created without actual receiving contact.

The unchanged153tick top-out observer still fails. Final tick1748 COM `(8.529008865,119.7555542,-179.7619171)`, velocity approximately `(0,1.882116795,0)`m/s, two actual hands, valid receiver plan and no rejection. The body first slides clear of the receiver underside/front under real collision and finite hand load (z-179.48→-179.76 around ticks1625–1685), then rises. At the deadline its feet remain below the y119receiver plane, so no receiving-foot sample or foot transfer is asserted.

This establishes a real accepted top-out command and subsequent finite physical rise; it does **not** establish upper footing, stable braking, onward exit or complete acceptance. The new failure is the153tick observation deadline while the actual accepted transfer is still progressing, not Action rejection or foot actuator lifecycle. The original HEAD test had upper-receiver/held-footing/onward checks but did not contain this added153tick top-out deadline. Root retains authority over acceptance and any next control adjustment; this assignment does not weaken it.

A further discriminating observation, if authorized, can retain the153tick RED verdict while continuing the unchanged accepted input only to learn whether actual hand-to-foot completion follows. Alternatively, a public stance control can require actual outward clearance before the one Action, reducing time spent clearing collision within the transfer. Neither observation has been run here. No timing/force/reach sweep occurred.

## Complete original route observed; deep transfer needs223ticks

Root authorized one continuation of the **same inputs** for117additional ticks (270total,3s), retaining the original153tick RED result. Root identified153as an unvalidated shallow-top-out budget introduced in its expanded route, not a previously accepted deep-catch requirement. No physical limits, geometry, foot criteria or other checks changed.

`final-gates/deep-continuation.py` compiles only the public driver against the unchanged756720be observer archive. The run reaches actual hand-to-foot completion after223fixed intervals=2.477777778s (pre-Action tick1595→handoff1818), within270. Actual receiving foot-start2, shared-solve2 and transfer2 are observed on receiver1960, then hand count becomes0 and traversalNone. Handoff COM `(8.301127434,119.9069977,-178.9021454)`. Original receiving-support, upper height, finite laws, same-life, braking, walk connection and onward121.9m exit assertions all pass.

After original neutral braking, printed receiver COM is `(8.299991608,119.9000015,-178.8978882)` (the full log is authoritative for exact digits). Final COM `(9.000551224,121.9000015,-174.454483)`, grounded support11, hands0, no traversal, zero deaths. The final line explicitly reports `PASS AS-026 suspended_ladder support=11 walking_surface_m=121 deaths=0`, followed by `RX_DEEP_ROUTE_COMPLETE original153_red=1`.

The run takes56.302994s. At handoff the real foot is active with737.344727N of measured load. The process deliberately exits1 solely to retain the153RED diagnostic verdict; it is **not** a green process result. `deep-continuation-analysis.json` classifies that distinction, alongside the raw execution receipt and log. Root owns deriving/documenting the270deep-catch acceptance budget while retaining the separate shallow153test unchanged. This native route observation is not an integrated current-header build, APK or device claim.

Measured committed-transfer phases:

| Observation | Tick | Time after Action |
|---|---:|---:|
| Backoff clears z<=-179.70 |1667|0.8000s|
| Feet clear y119lip+0.04 |1759|1.8222s|
| Forward movement beyondz-179.60 while above lip |1777|2.0222s|
| Actual receiver shared solve and hand release |1818|2.4778s|

From the actual pre-Action ledger to handoff: positive hand rest-target work9995.22209J, absorbed hand rest-target work46.8851358J, conservative hand command debit10873.43110J. The mean per-hand target path length derived from that debit is3.624477033m (`debit/(2×1500N)`); this is a path-length derivation, not net body travel or measured muscle energy. The controls and existing6600W transfer law remain unchanged. Paid radial capture work/debit remain separately identified above.

Integration control sequence observed passing original acceptance:

1. After the real receiver catch, public Crouch=true and neutral movement until first actual vertical descent arrest (`vy>=0`, bounded2s).
2. Retain Crouch; face(0,1), move(1,0). Keep real two-hand receiver identity/force/work checks. Wait within the existing3s shimmy budget for COMx>=8.25, remembered ledgex>=8.25, actual shoulder-to-ledge<=1.05, a genuine hand generation change and actual anchor-midpoint advance>.1m.
3. One public Action with movement(0,1). Retain finite physical checks and actual receiver/shared-foot-transfer criteria; permit270ticks for this deeper catch.
4. Release Crouch immediately after actual top-out, then run the original upper-support/braking/walk/onward sequence unchanged.

No production/shared-test code was edited by this assignment. Root can integrate from `deep-continuation.cpp`, stripping only diagnostic trace/retained153exit plumbing. One meaningful affected integration run remains root-owned; no further private replay is needed unless those relevant inputs/owners change.
