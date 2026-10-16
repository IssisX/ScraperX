# Resolved phase attribution — 2026-10-16 00:40 UTC

This extends the initial DIAGNOSIS.md. The distinction previously unresolved—ordinary velocity integration versus joint split position correction—is now resolved from actual-state increments. No shared repository or pinned dependency edits; no production repair implemented.

## Baseline and attribution

Private copies of pinned PhysicsSystem.cpp, ConstraintManager.cpp and SliderConstraint.cpp add read-only before/after callbacks. Only those three object members were replaced in a private copy of libJolt.a. All remaining Jolt objects and the source-v7 simulation archive remain the accepted originals. The unmodified actual suspended assembly probe is linked against that private archive. Main baseline neutral trajectory reproduces the earlier uninstrumented output bit-for-bit at every logged10s point. Receipts: window.json trial4 (integration/joints/contacts) and trial6 (separate slider subrows). Trial5 had aggregated subrow keys and should not be used to distinguish the two slider subrows; trial6 corrects only diagnostic key storage.

Actual q increments over100s, meters:

| Phase | Signed axial increment |
| --- | ---: |
| Mast velocity rotation integration | +0.00172898 |
| Carriage velocity rotation integration | +0.00317070 |
| Mast velocity translation integration | +0.0540116 |
| Carriage velocity translation integration | -0.0634694 |
| Chassis↔mast hinge position solve | -0.0309147 |
| Mast↔carriage slider position solve | -0.132195 |
| Carriage↔fork fixed position solve | +0.0779723 |
| Contacts / other constraints | 0 |
| Total measured phases | -0.0896954 |
| Actual final q minus initial q | -0.0896954 |

Integration total≈-0.0045581m, close to independently sampled integrated Jv=-0.00447778m. Coupled position solve total≈-0.0851373m accounts for the material excess. Slider position solve further separates into transverse correction+0.161545m and rotation correction-0.293740m, summing-0.132195m. Axial limits never activate.

## Supported causal chain

1. Neutral lift correctly configures motor Off and finite22kN friction. Measured reaction peaks1.102kN, so cap saturation does not cause the drop.
2. Slider friction is a velocity-only axial row. Solved axial velocity is approximately0.045mm/s, accounting for roughly4.5mm/100s.
3. In the position phase, SliderConstraint::SolvePositionConstraint executes transverse correction, RotationEulerConstraintPart correction and axial end stops, but explicitly skips its motor/friction row.
4. Rotational split correction changes actual mast/carriage orientations without corresponding physical velocity changes. Since q=(c2-c1+r2-r1)·n1, these corrections change the axial slider coordinate. To first order, their contribution is dq=n1·[delta_theta2×r2-delta_theta1×(r1+u)]. The native before/after samples directly measure this effect; the slider rotation row contributes-293.740mm across the100s. Transverse and other attached-joint projections partly cancel it, leaving85.137mm of real axial shift outside velocity integration.
5. No neutral axial position constraint compensates that correction. Repeated coupled position solves therefore let a physically sticking brake's coordinate drift. This is a coupled joint-projection/numerical defect, not a harmless absolute-world frame readout and not insufficient rated force.

Float/rotation thresholds can feed the orientation errors corrected by this loop: existing accurate integration reduced final drift89.70→38.90mm in the prior A/B, but failed to eliminate it. The current evidence identifies the phase and row producing the axial displacement; it does not attribute every input orientation error to one floating-point operation.

## Smallest authoritative repair boundary

The affected owner is the pinned Jolt slider's coupled transverse/rotation/axial sticking-friction position projection, plus any minimal solver phase state required to define its correction invariant. A supported repair must prevent split correction from creating axial displacement that the finite sticking brake did not permit, while retaining actual velocity-integrated motion, genuine force-limited slip, reciprocal reactions and end-stop behavior. An across-tick remembered-height servo, bigger friction rating, zeroed velocity, gravity cancellation, pose write from the forklift controller, or a taller starting stow does not repair this owner.

No production-ready formula or config-only fix is established here. In particular, merely preserving q before one slider rotation call would miss the measured hinge/fixed corrections, and would not by itself respect finite friction or genuine slip. A possible solver design must define an intra-substep correction coordinate from integrated physical motion and the solved stick/slip state, then satisfy that invariant through the coupled projection; this requires implementation design and its own bounded mechanical proof. Do not claim this unimplemented design is an accepted repair.

## Acceptance for a future correction

Reuse this private actual suspended fixture as the rejecting seam. Require phase closure to remain meaningful and the position correction not to accumulate unexplained axial motion. Preserve native physical integration and any genuine commanded or overloaded slip; test finite brake saturation under an actual axial overload, neutral source-work accounting, reciprocal support, release and powered lowering. Set the final duration/clearance bound from the freight operation's real tine clearance and duration, rather than promoting the1cm diagnostic detector into a product tolerance. Fullworld rail-clearance failure remains a separate route owner and was not rerun here.
