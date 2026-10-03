# First gallery: rolling drum and bascule crossing

**Profile:** MACRO-TRAVERSAL-STRICT [DEFAULT].
**Status:** selected design, INTEGRATED settled-load submodel, partial native contact experiment. **Not a compiled complete encounter or a production implementation.**
**Repository verified:** IssisX/ScraperX, branch `ChatGPT`, head `2942fab1420aa2ffd600a425b590941e6a3e5916`; executable source `f23a68e`. This worker made no repository edits.

## Outcome and placement

Climb from the existing +121 m ring to an inspection landing. Align a loading tongue, then physically push one contained drum from its level rack onto a gravity chute. Actual drum/pan contact lowers a raised, counterbalanced leaf into a short crossing. Cross to real receiving footing and continue through climbing members toward approximately +141 m. The adjacent climbing supplies most elevation gain; encounter boundaries are not quotas.

**UNRESOLVED placement:** ordinary walking from the AS-026 exit around the north-west ring, world-solid interference, mast origin, transformed machine sweep and onward grips must be measured. The AS-026 receiver is X=[8,10.2], Y=[120.72,121], Z=[−177.8,−175.75] [MEASURED]. At +121 m, the north/south slabs span X=[−26,26], Z=[−176,−167]/[−133,−124]; west/east slabs span X=[−26,−17]/[17,26], Z=[−167,−133], Y=[120.5,121] [MEASURED, simulation.cpp:2996–3038]. Existing ring geometry supports a candidate connector; ordinary route continuity remains unproven here. Native grip sections must be at most 0.18 m and length at least 0.25 m; choose 0.10 m sections, 1.10 m rung length and 0.30 m vertical spacing for the initial mast screen [CHOSEN against MEASURED simulation.cpp:253–254]. Main machine/support members retain macro dimensions. Native mantle range is 0.35–1.85 m, and measured jump apex/airtime are 1.51 m/1.11 s [MEASURED source contracts]; these do not certify an untested transfer.

## Selected causal graph

Player work → tongue translation on a visible dry slide → rolling channel → loose drum descent → two-face pan contact → leaf rotation about the trunnion → timber compaction → seat/frame reaction → ordinary receiver traversal.

Gravity and player work are the only work roots. Bearings and frame carry reaction without supplying work. No motors, governors, automatic catches, occupancy forces, cargo welding, prescribed leaf poses or teleport handoffs.

Rejected alternatives:

- Opposite-tail pan raising the leaf: requires upward arrest and can become easier to unseat when rider load changes. Lowering gives underneath receivers, and ordinary rider loading assists seat closure.
- Welded drum or flagged occupancy: removes rolling, pickup momentum, slip and meaningful misalignment.
- Bare hard stop: omits finite arrest stroke and material history.
- Kit automatic catch/governor: proximity/speed capture into a fixed constraint and a velocity motor do not implement this assembly's physical argument.

## Typed local geometry and quantities

Coordinates are right-handed, +Y up. Local +X runs from pivot toward receiver; local Z is the trunnion/drum rolling axis. Positive q is clockwise: pose `Rz(−q)`; `x_world=x cos(q)+y sin(q)`, `y_world=−x sin(q)+y cos(q)` [DERIVED]. Support velocity is `v_cm + ω×r`.

| Quantity | Value and evidence |
|---|---|
| Gravity | 9.81 m/s² [MEASURED, pinned Jolt PhysicsSystem.h:392; production retains default] |
| Rider | 85 kg; dynamic translation-only capsule; radius 0.35 m, halfheight 0.9 m; fixed 90 Hz [MEASURED, simulation.cpp:174/1940–1960, simulation.hpp:414–416] |
| Front structure | 6.0×1.8×0.30 m outside hull, 800 kg nominal [CHOSEN] |
| Rear structure | 3.8 m arm, 160 kg centered x=−1.9 m; 800 kg counterweight centered x=−3.8 m, outside hull 1.2×0.8×1.2 m [CHOSEN] |
| Rotor mass | 1760 kg excluding drum and rider [DERIVED: 800+160+800] |
| Loose drum | Radius 0.60 m, axial length 1.10 m; 360 kg nominal, 330/360/390 kg band [CHOSEN]. Homogenized rigid filled barrel, not an empty thin shell. |
| Drum inertia | Axial `I=0.5 m R²=64.8 kg m²`; transverse `I=m(3R²+L²)/12=68.7 kg m²` [DERIVED]. Cylinder local Y maps to assembly Z. |
| Angular travel | Armed q=−0.18 rad; physical upper stop there; lower steel stops around q=+0.06 rad [CHOSEN]. Armed endpoint rise `6 sin(0.18)=1.074 m` [DERIVED]. |
| Pan | Two broad V faces, 35° slope, center x=3.8 m; physical confining walls [CHOSEN]. Native experiment settles drum center at local y≈1.065 m [MEASURED]; reduced screening uses y=0.9 m [CHOSEN approximation]. |
| Bearing resistance | 200 N m kinetic nominal, 100/200/300 N m band; 350 N m static [CHOSEN, ±50% kinetic DEFAULT band] |
| Contact friction | μ=0.6 nominal; screen 0.3/0.6/0.9 [CHOSEN]. Not inferred from palette Material. |
| Loading tongue | 60 kg, 1.8 m long, 1.5 m clear rolling width; lateral travel ±0.4 m; starting offset +0.35 m; 35 N dry slide friction [CHOSEN] |
| Alignment | Shift −0.25 m to offset +0.10 m. Ideal channel tolerance ±0.20 m `(1.5−1.1)/2`; dry work 8.75 J [DERIVED]. Actual rounded collider clearance remains UNRESOLVED. |
| Chute | Screen 2 m path, 0.18 m descent, 0.6 m crest fillet [CHOSEN]. Actual rolling transfer and pan-relative arrival UNRESOLVED. |
| Receiver | Nearest edge x=6.05 m [CHOSEN]; leaf maximum horizontal radius `sqrt(6²+0.15²)=6.001875 m`, leaving 0.048125 m geometry gap [DERIVED]. World height, width and walking transfer remain UNRESOLVED. |

Steel sections may use homogenized hollow-member mass properties; outside collision hulls must match visible geometry. New IDs are not allocated here. Parent must reserve them against active and fixture allocations.

## Passive material arrest and retained history

Two wide timber packs under opposite leaf chords carry the aggregate contact at local x=5.6 m. Reduced angular crush start q=−0.06 rad, available stroke 0.12 rad, resistance 22000 N m [CHOSEN]. No hidden braking actuator.

Native constitutive screening uses aggregate yield `Fy=22000/5.6=3928.57 N`, elastic stiffness `K=40000 N/m`, local contact viscosity `C=300 N s/m`, and physical compaction stroke about 0.68 m [CHOSEN]. For measured gap closure δ and actual closing point velocity:

```
p_next = max(p, min(stroke, δ − Fy/K))
δ_elastic = max(0, δ − p_next)
N = max(0, K δ_elastic + C v_close)
E_plastic += Fy (p_next − p)
E_viscous += max(0, (N − K δ_elastic) v_close) h
E_elastic = 0.5 K δ_elastic²
```

The force cannot pull. Plastic front p cannot decrease. A second arrival has only the remaining stroke. With reduced pivot inertia approximately 27623 kg m², effective point mass `I/5.6²` gives `h sqrt(K/m_eff)≈0.075` at 90 Hz [DERIVED], below DEFAULT 0.3; actual native refinement remains decisive.

Binding must derive the visible compressed free face and matching contact geometry from native deformation. If this constitutive law replaces rigid leaf/timber collision, disable only that pair; otherwise loads are counted twice. Steel backing stops stay collidable. Keep the timber free face inaccessible to unrelated loads, or extend the constitutive owner to those contacts. This is localized compaction, not a claim of general destructible timber.

Checkpoint includes plastic front, required elastic state, work accumulators, body poses/velocities, tongue and drum state. Checkpoint reconstruction is an explicit abstraction. There is no automatic physical recharge. Manual rearm requires returning the drum uphill and replacing consumed timber; that loop is not compiled.

## Physical modes, reactions and rider seam

- **Armed:** empty leaf presses into its upper stop; drum rests on level rack. Tongue alignment changes actual rolling clearance.
- **Rolling/transfer:** drum is free, with unilateral track/pan contacts. Unsupported gaps produce free flight, not artificial floor reaction.
- **Loaded rotation:** contact forces and impulses change the hinge motion. There is no pickup velocity reset or cargo attachment.
- **Arrest:** closed timber gap produces elastic/plastic reaction; underneath steel seats carry any terminal contact.
- **Departure:** actual rider contact disappears; departure retains `v_cm+ω×r`, with no lingering extra weight.

Every contact has nonnegative normal reaction and valid gap. Friction opposes relative slip. Pivot reaction enters the separate bearing and frame. The two V faces carry drum weight once. The reduced model's attached point loads are screening abstractions only; never add their mass to the real Jolt rotor or apply another 85 kg load during grounded contact.

## Reduced evaluator evidence

`settled-beam-sweep.py` calls the installed compiler evaluator unchanged. It evaluates 36 drum mass/position/rider variants, each with nominal plus three resistance samples, at h=0.001 s and h/4: **144 coarse/fine pairs**. Riders are absent or 85 kg at x=0/3/5.7 m. Drum centers are x=3.6/3.8/4.0 m.

All 144 settle `HELD_BY_CRUSH_BED`, with declared speed, hard-stop, residual and refinement checks passing [INTEGRATED within this model]. Exact numbers are in `settled-beam-summary.json` and the complete `settled-beam-report.json`.

Endpoint refinement is checked independently to 0.001 rad. The bundled evaluator's `apex_q` can be absent on one V_REST branch although both final coordinates exist; the driver compares the actual end coordinate instead of widening a tolerance. Largest endpoint difference is approximately 0.00025 rad.

The correct rotating floor projection is:

```
N = m (g cos(q) − x qddot − y ω²)
N_lower = m (g cos(max|q|) − |x|max|qddot| − 0.9 maxω²)
```

The conservative lower bound remains positive for every aboard case (approximately 564 N minimum). A conservative tip acceleration bound `6(max|qddot|+maxω²)` remains below 0.471 g [DERIVED from INTEGRATED metrics], inside DEFAULT 0.5 g.

After removing the point rider, remaining positive drive fits the holding capacity of the **same consumed front** [DERIVED over all 144 cases]. This is not an actual stepping-off test. Opposite-side loading changes the initial torque and can prevent deployment; the nominal starting mass boundary is about 353 kg for an 85 kg rider at x=−3.4 m [DERIVED]. Failed operation needs reachable lower footing.

Limits: fixed payload positions omit free rolling, contact pickup, landing impulse, pan escape, changing load location, actual support tracking and receiving transfer. Chute arrival energy must be included in the coupled native model.

## Native experiment and exact blockers

`coupled-pan-probe.cpp` links current Kit and pinned Jolt `e77f175595e64cb44218cc9d9d56fc365ad0e36a`. It contains a free cylinder, actual compound pan/leaf, bearing, upper/lower stops, receiver and native elastoplastic gap force. It starts above the pan, **not through ordinary gameplay or the tongue**. `native-cases-receipt.json` records matched source/binary hashes and 90/360 Hz logs.

Important differences:

- Native front structure is 700 kg deck plus 100 kg cradle; the reduced front is 800 kg centered at x=3 m. Exact native COM/inertia must replace that approximation before a cross-model claim.
- The optional capsule is translation-only and 85 kg but has μ=0.6 to isolate retained load. Shipping player has μ=0 plus native support-relative traction. Do not copy proxy friction or call it ordinary gameplay.
- The experiment presently lacks complete impact/sliding and dry-joint stick work accounting. Hundreds of joules appear as undeclared sinks. This is **not closed energy proof**.
- At 90 Hz, the empty-rider pad case retains angular jitter late in the run; 360 Hz settles more quietly. Contact/material integration needs repair before declaring a stable production terminal.
- The actual rolling tongue, ordinary player input, rendered deformation, route sweep, stepping off and full mechanical-limit tests are not implemented in this private experiment.

## Minimal native binding and falsifiers

Use a bounded assembly through existing Simulation/Kit owners; preserve established construction order and IDs. Native C++/Jolt owns all consequential state. Derive q from the actual body rotation; a Kit hinge reports its built reference angle, so it cannot be assumed to equal the absolute clockwise coordinate used here. Godot draws interpolated native body/part/deformation state and handles ordinary input and read-only feedback.

A **separate bearing body is required**: `Kit::add_hinge(first,second)` disables all collision between its two bodies. Using the entire frame as first would silently eliminate the very stops/receivers being claimed. Use explicit mass properties, zero undeclared damping, finite bearing friction and CCD. `Material` remains palette-only. Tongue uses a passive sliding track, with ordinary carry handle at reachable native hand height; handle effort and drum pushing need actual input proof.

Existing support `GetPointVelocity`, traversal load ownership and departure momentum remain authoritative. `load_hold` supplies weight only during traversal with player gravity disabled; grounded weight and landing impulses come through real Jolt contact once.

Remaining falsifiers, with deciding measurements:

1. **Remove drum:** raised leaf stays on its upper stop; pad front remains zero. Native experiment captured this signature.
2. **Misalign tongue:** actual contacts prevent normal transfer; drum remains on real containment/recovery footing. Run complete rolling geometry and friction corners at h and h/4.
3. **Change/remove pan faces:** contact, slip and escape outcomes change; no fixed-payload substitution. Measure relative impulses and angular momentum throughout pickup.
4. **Near/far/opposite loading; land; step off:** torque follows actual lever arms, unload removes weight, departure includes angular support velocity. Run the production player owner, not the private friction proxy.
5. **Remove arrest/support or use spent timber:** stable terminal must change; consumed stroke never resets. Complete independent impact/friction/pad ledger and calibrated solver floor; use DEFAULT tolerance `max(2% released energy,3×floor)`.
6. **Terminal and rendering:** close 90 Hz jitter, compare exact COM/inertia, verify collider/free-face/mesh together through normal play, and confirm aboard/gone load paths.
7. **Connected ascent:** ordinary shipping grade → cargo net → existing ascent → +121 ring → service mast → machine → onward receiver, with actual support, walking, reachable grips and recoverable misses. World placement waits for this geometry/route evidence.

This is a quantitative candidate with useful partial evidence. It is not yet an implementation-ready compiled Stage Contract.
