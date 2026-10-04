# Contact movement implementation

**Objective:** smooth, physically coherent climbing, catching, balancing, transferring and departing on rigid, rotating and deforming structures, proved through ordinary gameplay. GDD §7.2 owns product intent; this file maps implementation. The running to-do remains in `CONTINUE_HERE_CHATGPT_CODEX.md` §7.

**Inspected starting source:** `543d13e04c615af6869d46826214878446513200`, `ChatGPT`, 2026-10-03. The planted-hand rendering repair and uninterrupted normal grade→121 route are delivered at `0ecf341`. They establish an entry point, not completion of this objective.

## Current causal gap

At the inspected starting source, `PhysicsWorld::begin_climb` disables player gravity. `drive_traversal` replaces player velocity to follow a desired root; `load_hold` applies a separate weight force to a dynamic support or the cargo mesh. Passive release then replaces player velocity with the principal support's point velocity. This can erase climbing momentum or add a net's recoil velocity that the rider did not have. These paths are authoritative source facts, with the departure discrepancy exercised in a native regression.

The replacement must carry the actual 85 kg gravity-on player through independent hand reactions and real foot contacts. Interpolation reads those contacts at the existing shared render timestamp. Preserve public traversal labels and existing identifiers.

## Native implementation sequence

1. **Rigid hand coupling.** `PhysicalHandClimb` owns two independently removable native SixDOF constraints; `PhysicsWorld` owns acquisition, reach, regrip, commands and lifecycle. Kit owns the support bodies. Acquisition initializes from current geometry and velocity. Translation is compliant and force-limited; rotation is free. Player gravity stays enabled; these grips bypass `drive_traversal` and `load_hold`.
2. **Flexible coupling.** Extend the same contact/control contract to actual cargo-net material coordinates and the long rope ladder's rung bodies. Use live deforming geometry and reciprocal force/impulse partition. Actual cloth contacts and hand reactions enter once. Regrip one hand at a time; track each support separately. Do not lower the global mass threshold to admit arbitrary loose debris.
3. **Transfers and departure.** Retain real foot contact while hand-coupled, preserve momentum on passive release, and fund powered departure through a finite reciprocal native action. Reach/occlusion reject inaccessible contacts. Complete the receiving transfer on actual support; a gravity-off mantle or scripted root cannot substitute for this contact proof.
4. **Balance and locomotion.** Moving-support point velocity includes rotation and deformation. Foot correction and jump reactions must couple back to the actual support, with finite effort and work. Preserve ordinary parkour capability and prove contact changes, unloading and edge departure.
5. **Presentation and ordinary proof.** Hands, body, camera, gait and state-driven feedback consume the same native contact history. Exercise actual touch on fixed structure, AS-026's rotating support and the cargo net/flexible ladder, followed by the ordinary connected route. Verify approach, catch, transfer, release and supported arrival through actual gameplay state and runtime checks. Screenshots are optional diagnostics. Close delivery with a green exact-source Actions APK build and verified provenance, separately from device acceptance.

## First module contract

- MEASURED source: 85 kg dynamic upright capsule, radius 0.35 m, cylinder half-height 0.55 m, translation DOFs, body friction 0; Jolt owns collision.
- CHOSEN coupling: stiffness 5,000 N/m, damping 180 N·s/m, each hand vector force at most 1,500 N; independent axis limits are `1500/sqrt(3)` N. These are finite game control parameters, not measured physiology.
- CHOSEN command ceiling: 3,000 W shared between hands. Limit `sum(F_max * length(delta_target)) <= P_max * dt`; record this as a conservative command-work bound, not measured motor work or an energy-closure result.
- Body 1 is the translation-only player, making translation motor axes fixed in world space. Body 2 is the actual rigid hold. All six axes are free except finite translation position motors; target velocity is zero. Initial world anchors coincide at the reachable grip. No collision exclusion or added rider-weight force.
- CHOSEN joint island budget: 40 velocity / 8 position iterations, retaining production global 10 / 2. Read motor impulses with the actual internal collision-step duration.
- VERIFIED in native component fixtures: compliant catch, shared command-bound enforcement, gravity-on equilibrium, reciprocal dynamic recoil and passive release. The one-hand transient has only about 32.18 N vertical force margin and needs long settling; an oblique catch drops about 1.70 m. UNRESOLVED: controller performance, full positive-work/energy closure, flexible and mixed-support contact, receiving transfer, foot balance, rendered/device behavior. See the exact module receipt.

The coupled systems are player muscle/contact control, native rigid/soft structure dynamics and contact-driven traversal/presentation. Their authoritative load path and momentum must agree; a standalone module is only a component proof.

## Acceptance that can reject the implementation

- Gravity-on supported hang and catch; no duplicate weight force or root velocity setter in that mode.
- Removing one hand redistributes reaction; removing both preserves instantaneous player velocity and resumes gravity. The support receives the reciprocal dynamic reaction.
- Positive command work is bounded across both hands. Report storage, damping and numerical residual honestly; a force ceiling alone proves no energy ledger.
- Reachable independent contacts allow hand-over-hand and mixed-support transfers; absent, occluded or unreachable geometry rejects acquisition.
- Rotating/deforming anchors remain attached to the rendered owners through interpolation and regrip; readback does not advance simulation.
- Balance, landing, stepping off and powered departure exercise actual foot support and momentum. A lost support cannot remain an invisible foothold.
- Normal touch reaches real receiving footing on rigid, rotating and deforming support paths and continues walking. Staging and fixtures remain explicitly labelled supplements.
- Restart, checkpoint restore, launcher takeover and destruction remove grips before moving or restoring their bodies. Device smoothness is claimed only with actual device evidence.

## Owner-requested pause boundary, 2026-10-03

The rigid module and its Simulation integration are implemented. Five affected native/bridge gates pass after the source-review repair. Passive release retains actual momentum on rigid, rotating and deforming paths; rigid `Climbing` uses gravity-on finite hand constraints. The ordinary reviewed-source headless campaign also passes grade→121.9 m on Tower support 11, zero deaths and zero launcher work. [The checkpoint ledger](../../evidence/contact-movement-2026-10-03/README.md) records actual sources, tests and limits.

Soft/rigid hand candidates are kept consistent with the principal selected hold before acquisition, including initial hands. Soft material transfer awaits its reciprocal adapter. Receiving top-out still clears the rigid grips and enters the existing mantle; powered jump remains the existing velocity-based action. These are explicit remaining portions of the larger objective.

All prototype assignments are finished and their exact sources/receipts are [preserved in the branch](../../evidence/west-gallery-compilation-2026-10-03/README.md). Wait for the owner's next instruction before another stage. First check the new checkpoint's exact-source Actions/Android delivery on resumption; then continue flexible contact/transfer work from this ownership contract. Keep the broader movement goal open, and preserve public traversal labels. Existing low-mass cylindrical rung eligibility and full controller energy closure still need implementation.
