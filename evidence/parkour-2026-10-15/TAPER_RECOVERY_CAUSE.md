# Taper middle recovery: supported causal diagnosis

Production read-only assignment on checkout `ChatGPT`, base revision `8569c721432e4139df6286b8e8d1cce94b2db9ca`; concurrent work preserved. Parent supplied fresh Sol/high assignment. No child agents, native edits, geometry/force/gravity/catch-policy changes or broad accepted-check reruns. Relevant guidance: diagnosing-bugs and jolt-cpp-mechanics, native contact/actuator ownership reference. This report and the private diagnostic directory are the only assigned output surfaces, apart from parent-authorized guarded trial30 state/log recording.

## Supported cause

**The automatic passive trailing plant remains/acquires while ordinary input deliberately brakes and returns toward the real apron. Once compressed, its real upward/outward force unloads the capsule's actual rounded-edge contact early. The existing validated inward physical step cannot run after that real contact is lost.** The v2 passive-to-step mutex correction is insufficient because this failure loses its required receiving/contact eligibility before reaching that handoff. The eventual two-hand catch is a legitimate downstream catch of the resulting fall, not the defect owner.

The authoritative decision owners are `src/sim/simulation/parkour_intent.cpp::retain_trailing_plant` (lines33–65) and `prepare_trailing_plant` (lines6–30). The finite spring law is `PhysicalFootPush`/`FootDistanceRow::SetupVelocityConstraint` (`physical_foot_push.cpp:70–96`). The real ordinary step remains `simulation/locomotion.cpp::try_step_up` (v2 lines676–795), with the accepted v2 fresh-contact/riser/clearance/rise handoff at750–764.

## One discriminating matched observation

Saved trial28 already supplies the exact middle failure and primary/intent/buffer passes. Ranked serious rivals were (1) passive compression changes braking/contact eligibility, (2) remaining step-purpose mutex defect, (3) unchanged driver/fixture overshoot. One diagnostic pair, not an implementation or parameter sweep, distinguishes the passive integration from driver/fixture effects and shows where the physical step ceases to be available.

Actual artifacts: `/data/data/com.termux/files/usr/tmp/scraperx-taper-cause-20261015/`. `manifest.json`, `build.json`, `build-0.log` through `build-3.log`, exact `observe.cpp`, private `parkour_intent.cpp`, immutable archives/binaries, `current.log`, `no-retain.log` and `result.json` preserve provenance. All compilation/linking was sequential, at most one compiler.

- Both use exact production middle test staging, ascent, walking gap miss, fall, actual apron braking and stable observation, then its unchanged first `walk(-22.4,-157.7)` input/arrival/no-traversal requirements. No new fixture or debug staging was introduced. This invokes `middle`, never `primary` or other accepted campaigns.
- Current archive SHA256 `18af7c94f9048807d90036ce33a1b9d48c4ff38dcb38868c06171c170eaddf9b`, exact source-v2 headers, pinned Jolt source/archive. Control changes **only** `retain_trailing_plant` to return for `contact_solve_serial_ >= 1597`, after the stable actual apron arrival. All earlier approach/landing dynamics are identical. Control archive SHA256 `91c5dbb2e5c1dbe0e833337081da7d40bf4733c7bc20ba94564d2fe3a31e812e` replaces that single TU; no source/archive outside the assigned private directory changed.
- Guarded by the existing persistent `evidence/parkour-2026-10-15/coyote-window.json`: evidence-based reassessment cites actual trial28 failure and trial29 corrected compatibility all3PASS. One trial30 `taper-middle-passive-acquisition-paired-observation`, prerequisites `passive-step-corrected-compatibility`, records immutable source/archive/binary hashes. Actual command is `python /data/data/com.termux/files/usr/tmp/scraperx-taper-cause-20261015/run.py`, which executes each native observer through `proot-distro login ubuntu -- <observer> middle`, sequentially. Total45.6s. Wrapper0 means both observations executed; current gameplay result remains FAIL.

## First causal transitions

| Native tick | Current v2 | Acquisition-suppressed control |
| --- | --- | --- |
| 1597 | Stable actual apron1933, COM(-22.398,361.4,-163.999), zero hands/traversal | Identical |
| 1740 | Already reversing input z=-.81928, outward vz+3.04765 at z=-157.68227; grounded/firm1933 | Identical |
| 1746 | Firmness becomes false, grounded1933, z=-157.49730, vz+2.52445, input z≈-1 | Identical |
| 1747 | New passive plant1933, zero work/stroke, while input z≈-1 opposes outgoing vz+2.43725 | No new plant; body state still identical |
| Through1754 | Body position/velocity remains identical; passive row initially unloaded | Identical body state |
| 1755 | First actual body-state divergence: force(-.01839,+234.019,+57.5383)N; length.89921087 below original rest.89999074m; vy=-.650253,vz+1.769396 | No plant force; vy=-.657900,vz+1.767516, same commanded input |
| 1760 | Bottom contact1933 still exists; passive force(-.07365,+575.412,+210.924)N | Real contact still exists, rolls lower without new row |
| 1761 | **Actual grounded/support becomes0**, COM y361.324585,z-157.177353, force(-.08171,+590.135,+230.454)N, input z≈-1 | **Still grounded1933**, y361.312775,z-157.180908, vy-1.086944 |
| 1767–1768 | No fresh bottom contact; retained compression row cannot handoff to a contact-funded step | Grounded through1767; existing step earns vy+1.981389 at1768; Jump/step work rises62.86760→148.00739J |
| 1769 | Passive lifetime expires.25s; body is already unsupported | Actual step flight continues |
| 1814 | Exact original failure: COM(-22.400007,359.961762,-157.049988), vy-4.669935, Hanging/two hands, gravity1/deaths0 | Returning toward actual apron |
| 1846 | Original observer already exited1 | Unchanged first-walk arrival passes: grounded/firm1933, COM(-22.400032,361.399963,-157.743073), horizontal speed.114124<.12, error.043073<.08, no hands/traversal, exit0 |

The recovery apron physically ends z=-157.4 (`parkour_route.cpp:199`). Both trajectories initially overshoot the target because of ordinary finite braking; that is not alone a failure. The control demonstrates the existing physically funded step-back can recover that same overshoot. The passive row makes the recovery unavailable. This rejects blaming a newly broken driver/fixture or altering its target/tolerances. The first divergence is a real compression impulse, not an unconsumed Jump or legacy step pose. Current Jump work stays62.86760J throughout; the actual middle driver never requests Jump before this failure. Suppression does not remove a Jump motor or modify step logic.

## Smallest owner-correct repair boundary

At the passive opportunity coordinator, arbitrate deliberate return/braking input against the retained outgoing leg. **Do not newly acquire an outward trailing plant while nonzero ordinary intent points back toward its actual support/against the outgoing contact motion; relinquish an already passive row when that deliberate return gait takes ownership.** Use actual material anchor, support-relative direction and existing ordinary-input threshold, not target knowledge, entity exceptions, force tuning or a special taper rule. Cover both acquisition and retention to avoid immediately reacquiring a cleared row. Keep airborne Jump activation/finite expiry/actual material validation and powered-foot exclusion intact. No blanket active-foot mutex exemption, support grant or extra push is indicated.

This is a concrete correction candidate supported at its decision owner, not runtime proof of a shipping predicate. Root owns the minimal implementation and the original full middle/upper acceptance. The matched observation cannot distinguish every possible physically correct reversal predicate; it does establish that outgoing passive retention during the measured active return intent is the causal conflict. Generic removal of passive compression would affect broader constitutive behavior without supporting evidence.

## Evidence limits and reuse

The private observer deliberately returns after the first bad walk. Its inherited final `PASS TAPER_INSPECTION middle_miss_apron_crouched_service_rejoin` text is **not** proof of service-header/crouched/rejoin/full-middle completion; only `SUMMARY actual_first_recovery_walk_arrived` and the unchanged first-walk predicate establish control success. No downstream middle/upper route, APK, Godot or device claim is made.

Prior North primary/retry, taper primary, intent-flow/buffer, and corrected compatibility passes remain reusable for unchanged owners. This diagnosis did not rerun them. A production change to passive acquisition/retention may require a narrowly affected intent/reversal gate, but passing full routes should not be repeated without material dependency. Production implementation remains unchanged by this assignment. Existing current failure is introduced; unavailable evidence is the runtime behavior of the proposed reversal arbitration and remaining full middle/upper acceptance.

## Root-selected implementation boundary (source acceptance still pending)

Root accepted the causal diagnosis and selected a passive-only grounded lifecycle correction: reject new acquisition if deliberate ordinary horizontal input (existing magnitude threshold.001) opposes actual support-relative horizontal motion; clear an existing TrailingPlant under the same deliberate reversal only while grounded on the same fresh actual contact, before forces/ordinary stepping. Preserve neutral intent, already-commanded CoyotePush and airborne retained-plant steering/activation. This rejects the measured tick1747 acquisition before force divergence1755, and avoids expanding the repair into unrelated airborne late-Jump behavior. Investigator identified no supported flaw in this bounded decision; shipping source/runtime acceptance belongs to root and is not claimed here.
