# Workshop two-plant wall run

Base published source: `98ad4969b7f162dfb32ba99b7b8aab7b9459b70a`. Native baseline is unchanged from `36a4abf` outside this slice.

Sprint and Jump along the existing south workshop wall while leaning into it. The ordinary light brush remains one foot plant. Deliberate inward intent can earn one second real plant. Jump spends the currently planted foot's remaining push-off; Drop cancels even in the unloaded gap. Follow the wall onto real grade and keep moving. Floor arrows show the lane near `(-39, -40)` without adding collision.

The first foot must already have measured load and still pass its original actuator's geometry/traction validation. Measured support-relative speed predicts when to retire that row. Prediction schedules retirement only. At least one subsequent completed solve without the first row must report a fresh, distinct actual lower-wall point on the same body, entity, root shape, subshape and face. That material point must have advanced at least half the original stroke,0.15m. Only then can the existing finite foot owner begin again. Generic traction failure never authorizes refill.

Maximum two plants share an original earned-takeoff deadline derived from ballistic airtime plus two existing effort lifetimes. Reversal, release, crouch, loss of sprint eligibility, another movement owner, stale geometry, ground contact and restart cancel the episode. Jump on a current plant retains its anchor, source contact, age, work and stroke. Scoped body locks end before actuator lifecycle changes.

No actuator law, force, power, work, reach, stroke or lifetime changed. The existing per-plant bounds remain3500N,5000W,1285.625J,0.30m and0.25s. Jolt owns gravity, body velocity, contact response and reciprocal load. The new read-only `can_retain_contact()` changes no row state or ledger.

## Observed checks

[wall-run-window.json](wall-run-window.json) contains actual command arrays, outputs, source identities and failure classifications. Initial strict Release compilation rebuilt all26 users of the changed private World layout plus the foot owner. One declaration-order compile failure was repaired and only that translation unit recompiled. Final scheduling replaced only the parkour object in the coherent archive. One later World header edit only corrected its stale comment; the receipt records that difference.

- New public cases `flow`, `fast`, `jump`, `cancel`, `reverse`, `restart`: all exit0. The last cancellation-branch assertion edit leaves the other five cases and production source unchanged; their accepted results are reused.
- Existing public wall-stride `flow`, `jump`, `cancel`, `guards`, and upward-wall `flow`: all exit0, unchanged test sources.
- Normal approach: first loaded plant176/contact175; deliberate unloaded step178; second loaded plant179/contact178 at3333.92N; real grade233 at6.42213m/s tangent speed, then45ticks onward.
- Faster approach: first214/contact213; second216/contact215 at3470.21N; real grade266 at7.09751m/s, then onward.
- Drop during the unloaded gap: exactly one plant, actual grade229 and ordinary onward play. Existing public `request_release()` returns false in this gap because there is no attachment, but always queues cancellation. The first new assertion incorrectly required true; it was corrected from that independent owner contract. No API or physical success assertion was weakened.

The initial two-plant candidate missed the still-valid retirement window. A same-input read-only trace located the last valid measured point at0.146358m, before the fixed0.15m threshold. Scheduling retirement from measured next-window motion repaired that cause without changing the actuator or allowing prediction to manufacture the second contact. All four exploratory traces remain in [wall-second-window.json](wall-second-window.json).

A focused read-only consequential review found no supported material defect; that is source review, not extra runtime evidence. The public harness stages only initial real grade and then uses ordered gameplay inputs.

This is a brief reduced-order two-contact wall run. It does not establish articulated limbs, sustained wallrunning, a universal traction/speed envelope or phone feel. Full Godot import/export and the matching downloadable development APK are separate CI evidence. No full commercial-release or device-performance claim is made.
