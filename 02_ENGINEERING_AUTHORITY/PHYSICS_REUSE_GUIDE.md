# Reusing ScraperX physics

Read this when authoring or repairing consequential physical behavior. This is a compact implementation guide, not a new physics authority or a required whole-game audit. Native C++17/Jolt at 90 Hz remains the owner; Godot consumes snapshots and supplies input. The complete phased scope remains unchanged.

## Start with a prediction, then reuse the owner

For the current encounter, write six short statements in its existing work record:

1. **Outcome:** the actual approach, operation, receiving footing and recovery the player must achieve.
2. **Cause:** initial configuration, masses/inertia, geometry, materials and ordered inputs; what supplies motion and how much work is available.
3. **Reaction:** the bodies and contacts carrying each load, including the player; identify static world reactions explicitly.
4. **Storage/loss:** gravitational, kinetic and elastic energy; physical friction, material loss or impacts. Distinguish actuator command bounds from measured work and solver residual.
5. **Operating range:** justified load, approach speed, timing and geometry variation. State units and provenance of parameters. Do not call a single passing point a range.
6. **Failure/exit:** what physically stops, slips, breaks, misses or falls; how the player recovers and what state remains afterward.

Use an estimate sufficient to reject impossible force/travel/work before coding. It need not become a large simulation report. Then implement the smallest connected encounter using the existing owners below. New parts remain allowed; this is not a parts whitelist.

## Existing owners and boundaries

| Need | Authoritative implementation | Reuse contract and limit |
|---|---|---|
| Bodies, mass/inertia, hinges, guides, ropes and physical catches | `src/sim/mechanism_kit.{hpp,cpp}` | Use actual body/constraint identities and visible collision geometry. `point_inverse_mass` accounts for the implemented fixed-joint cluster. `Material` is a presentation palette, not an automatic physical material law. Explicitly author friction/mass. |
| Grip, loaded suspension, powered regrips and release | `src/sim/physical_hand_climb.{hpp,cpp}` | Reuse independent finite hands and their actual support reaction. Command work bounds and spring-target supplied/absorbed work are separate readbacks, not complete energy closure. Do not add a second rider-weight force or reset departure velocity. |
| Finite push-off on light movable supports | `src/sim/physical_foot_push.{hpp,cpp}` | `begin` retains an actual material point; the existing row limits force, stroke, duration and command budget. Losing face, traction or reach terminates the push. Respect owner-thread creation/destruction and collision-step readback. Its nominal spring diagnostic is not a full solver-energy ledger. |
| Flexible cloth, material anchors, clips and reciprocal loading | `src/sim/cargo_net.{hpp,cpp}` and pinned Jolt patch | Use `CargoNetConfig` and actual material coordinates. Clip stiffness is N/m, clip damping is N·s/m; axial damping ratio is dimensionless. Do not interchange body drag, edge strain loss and clip damping as equivalent tuning knobs. Catch-net settling/cost remain unresolved until demonstrated. |
| Elastic/breakable timber | `src/sim/deformable_plank.{hpp,cpp}` | Reuse segment bodies, joint ownership, strain/strength and fracture state. Current timber values are explicit reduced-order authoring assumptions; they are not certified wood properties or a universal beam preset. Preserve actual broken-body collision and load release. |
| Ground support, takeoff, landing and recovery | `src/sim/simulation.cpp`: `support_point_velocity`, `record_landing`, `apply_landing_recovery` | Use actual contact-point velocity, including rotation, and relative impact. Preserve support momentum, physical contact requirements and the 20 m/s lethal-impact contract. Existing recovery is not proof of a physical roll. |

Solver iterations and collision-step count are numerical settings, not material properties or free fixes for missing dissipation. Keep the existing 90 Hz integration contract; justify any numerical change with a specific solver defect and its cost.

Headers carry lifecycle, lock and measurement contracts. Read the selected owner's implementation and a relevant existing caller; do not copy an entire encounter or create a parallel controller. Native force duration and collision-step impulse are not interchangeable with render frames or outer-tick totals.

## Decide what to change

**Encounter engineering:** observed interaction obeys its contract, but the machine is too heavy, its ratio/travel is wrong, the receiver is unreachable, or a legitimate impact exceeds capacity. Change the justified geometry, material, mass, ratio or player decision. Predict the result before adjusting it.

**Foundation defect:** contact pulls when it should only push; passive release deletes velocity; reactions are missing/duplicated; internal losses create energy; or the same interaction fails across otherwise valid encounters. Capture the smallest actual native failure and correct the existing shared owner. Add a regression that would reject that defect on the old implementation. Reuse the correction rather than adding encounter-specific compensation.

Before extending a stubborn encounter investigation, compare two or three concrete physical alternatives: altered load path, distributed attachment, passive damper/tether, or counterweight. Predict each force/reaction/energy path and player consequence. Choose the simplest objective-preserving design; use a bounded independent mechanism review when uncertainty warrants it. A proven shared solver defect still belongs at its owner.

After a hypothesis is rejected, write the result and remaining uncertainty. Choose one observation that distinguishes the remaining causes before another edit. Do not continue searching damping, gains, iterations or geometry merely to get a pass. Do not assume that fixing one measured primitive also fixes every encounter symptom.

## Economical execution for GPT-6.1 Sol and other contributors

- Read the live checkpoint, this guide and only the selected owner/evidence. Do not replay historical investigations or load the full skill catalog.
- Root makes the physical/integration decision. Delegate independent ready work with one writer per file, concise evidence and a concrete acceptance behavior. Use the configured model and actual task-appropriate effort; escalate only for unresolved consequential uncertainty.
- Implement one correction or connected encounter at a time. Reuse valid evidence; invalidate it only when its source, configuration or relevant dependency changed.
- Prove the changed primitive and its affected gameplay connection, then return to construction. Expand checks only for a new risk or observed failure. Never soften acceptance to hide a failure.
- Finish with a small reusable record: owner/API; guarantee; measured operating cases; limits; regression command; affected gameplay outcome. Keep raw logs outside the startup path.
- Distinguish source review, native execution, rendered input, GREEN Android build and phone observations. A primitive test is not an APK or an in-game exit.

## Current bounded foundation repair

Pinned Jolt 5.6 soft-contact response can apply a tensile normal impulse to an overlapping vertex already separating from a static or dynamic receiver. The correction belongs in `cmake/patches/jolt-passive-soft-attachments.patch`, protected by its source-hash manifest and the existing `scraperx_sim.catch_net_contact_friction` regression group.

Reject the complete tensile normal response, including restitution history; do not simply reject every positive current normal speed, because a valid repulsive bounce may still be owed. Preserve geometric depenetration, legitimate closing response and reciprocal dynamic reactions. This correction does not by itself establish full energy closure, kinematic-contact correctness, catch-net settling, mobile performance or GREEN delivery. The live checkpoint and [scoped correction evidence](../evidence/shared-soft-contact-2026-10-13/README.md) own its current verification status.

## Reusable membrane contact and passive tether

`CargoNetSurface::ContinuousMembrane` uses actual indexed native faces and three unique barycentric masses for position, velocity, impulse, inverse mass and continuous force. Default cargo remains openweave. Jolt discrete soft/rigid contact samples vertices; visible filled triangles alone do not establish persistent collision coverage. Choose resolution from the smallest interacting contact geometry and require loaded/cell-offset evidence. The catch's.3m pitch follows the.35m capsule/.035m vertex and.55 support-normal bound; it is not a universal cloth setting.

An opt-in `CargoNet` tether reuses the native iteration owner: actual material-point weights → finite unilateral spring/dashpot → reciprocal actual frame-anchor impulse/torque. Anchor config is body-origin-local, converted to actualCOM-local; force uses real point inertia. Parameters have unitsN/m, Ns/m andm. Slack carries no force. Native render endpoints drive the cord casing. Capture/restore clears transient tether diagnostics without inventing external motion.

Ground tether is encounter engineering, not proof of a missing solver loss: dense36m caught/settled/exited before it, while45m decayed too slowly for its20s gate. The600N/m,300Ns/m,2.5m rest candidate passed the centeredARM case but failed the exact-source x86 late-settling gate. A45cm offset reproduces that failure onARM. Actual late eye/player velocities track and tension remains positive: neither a failed load path nor total contraction unloading explains the demonstrated case. A600Ns/m passive damper candidate passed center but failed the45cm-offset case and was reverted. Increasing a coefficient alone is rejected as the robust correction; full-cycle engagement remains unresolved. Mass-based critical estimates are design references, not measured cloth modal parameters. Do not use point-row inverse mass or total system mass alone as proof of modal damping. Radial predictor loss includes implicit numerical loss and does not prove complete scene energy closure. Current results live in the [checkpoint](../CONTINUE_HERE_CHATGPT_CODEX.md) and [failure/correction evidence](../evidence/canvas-contact-resolution-2026-10-13/CI_SETTLING_2a05fb4.md).

## Finite soft-to-rigid elastomer attachment

`CargoNetConfig::piston_body` names an actual dynamic rigid receiver (invalid disables the feature); `piston_material` is actual barycentric cloth space and `piston_body_local` is body-origin-local converted to actual COM. This existing owner can serve another cloth/rigid connection without a new physics authority. Finite k/c haveN/m,Ns/m units. Reuse the native fullmatrix elastomerclip law and actualmaterialpoint receiver lever: it carries force plus the finite collar moment and closes linear/angular impulse, including unequal vertex masses and displaced COM.

Apply both impulses in existing `pre_step`, BEFORE rigid andsoft positionsadvance. Do not move dynamicreceiver coupling into the after-rigid softiteration callback merely because clothclips usethat seam. A matched predictor isnot geometricenergyproof whenonebodyhasalreadyadvanced. The rejected latephase actualfixture addedenergyonfirsttick; preintegrationpair passes unchanged intermediateenergyandP/L gates. Keep physicalinitialcollisionclearance and account rigidspin plusorbitalL atoneorigin. Defaultdisabledgyroscopicforce can confound a freelyrotatinganisotropicreceiver's multi-tickL; isolate theattachment or deliberatelyconfigurethat separatephysicalcontract, not loosen tolerance.

Currenttelescopingcatch shock reusesactualKit slidingtracks, finite-mass collidingsteelparts, stroke limits and100Npassivesealfriction. The holloweye avoids fabricskin overlap; groundreactiontravels throughfiniteframe/mounts. Currentforceproof: [resolvedsourceboundresult](../evidence/canvas-contact-resolution-2026-10-13/piston-force-resolved.log). Gameplay/normalx86/Godot/Android acceptance remains separateandpending; this doesnotprove fullsceneenergyclosure or universalstability.

### Connected-body sleep/wake lifecycle

`CargoNetConfig::sleep_coupled_bodies` optionally names actual dynamic bodies mechanically connected to the cloth/frame. The native owner deduplicates these with the frame/piston, checks every member against unchanged Jolt equilibrium thresholds, and deactivates the complete assembly together. Real member activation, pose changes, loads and external contacts wake the assembly. This prevents awake lower slider stages from repeatedly waking a sleeping top through their real constraints; it does not manufacture settling. Default cargo configuration remains unchanged. [Source-bound force/sustained-sleep/wake evidence](../evidence/canvas-contact-resolution-2026-10-13/assembly-receipt.json). Rebound, render and APK acceptance remain separate.

## Grappling tool and finite practice support

Native grappling_gun owns projectile mass/contact, reciprocal cartridge launch, unilateral straight cable, finite manual takeup and momentum-preserving release. Native pure grappling_hook_geometry prisms supply both actual steel collision and Godot presentation; never substitute a mesh/flag anchor for the contact-held crook. Inventory retrieval is an explicit body-removal abstraction and never refunds cartridges. Grasp must clear full hook/world geometry; withdrawal permits separating exact tangency only, preserving real penetration/closing/obstacle rejection.

Practice bay IDs1965/1966/1968/1969 enter existing native causal-support leg-step and moving-support point-velocity owners. Use actual body frame for finite stairs/rack, not frozen authoring coordinates. New recovery gate requires finite leg work without legacy pose-raise credit. Current full same-life replayPASS4499 uses779.670319J finite stairs work and zero legacy pose raises; earlier frozen RED2387 correctly rejected0J leg work and10legacy raises. Capture attempts and validation limits are in the live encounter plan/checkpoint.

## Real crook encounter operating evidence —2026-10-14

Native gun/collision/rope remain authoritative. Removing introduced steel/concrete overlap settled the finite receiver frame; nativeprimaryPASS2137 uses ordinarylipbraking/finitepreload beforedeparture, avoiding the measured centerplatformrun/slackcatch StrengthExceeded. Existing finite air steering and reel supply separate reported work; no fullenergyclosure claim. Test observation transforms actualbar intohook coordinates and derivesfinger/collar/mouth clearance rather than cup-axis boundingbox; actualcontact/dwell/line/load gates remain independent. Frozen result/source receipt: evidence/grappling-gun-2026-10-14/powered-receiver/.

Whole-hook pickup requires actual unobstructed hand approach and strict withdrawal. The matched gravity miss proves real opposing recovery-floor overlaps; no penetration allowance was added. Released hooks now preserve independent convex-leaf contacts through BodyInterface::SetUseManifoldReduction outside Update; live-line policy and global Jolt settings remain established. One requested physical grasp can pull for0.20s with1500N/750W/1m/s bounds, paired point impulses and positive-work accounting. Completion remains bound to that hook and removes it only after strict geometry passes. Current owner fixtures18/18 include production90Hz/four-step cadence and overspeed/retargeting counterexamples. Evidence contact-pickup/RESULT.md distinguishes matched repair, full route and remaining retry/render/portable-anchor acceptance.

Automaticreelinput islatchedatnativeGunowner: one semanticstart/stop, neutralmanualvaluesmustnotcancel, actualnonzero manualrequests do. Release/linefailure/minimumlengthclearit.15nativechecks andfreshGodot111 proveowner/inputbehavior; aoneframecachedheld-axis defect wasfixedatTouchControls press/release/reset/context refresh. No new reel energy/forceauthority. Pickupdirectoverlapnowconfirmsactual1969floorleaf penetration20.118mm onbothfaces; exacttouchrestingunitcannotstandinforgravitysettledcontact. Do notacceptorignoreit atwithdrawal.

Current gun encounter frontier: native18/18, primary2137, full same-life4499 and actual portable roof firing pass; affected inspection primary/recovery remain passing. Godot input111 and real render31 bind to the same native archive2b8722159a2667bd1fbcf4669fcdc62d7065fc82e709d6ca0f2648960a4dba7e. Render images show actual native poses/line and AUTO/STOP; diagnostic viewport proof is separate from ordinary native route and Android/phone delivery. Additional compatible anchors remain a subsequent scope, never grounds for a semantic/fixed-point anchor shortcut.

## Portable gun yaw and square-rail reuse —2026-10-14

Native initial muzzle orientation now follows yaw×pitch from normalized aim, with vertical fallback; no orientation forcing during flight. Yaw covariance regression rejects old shortest-arc world-row behavior. Existing50mm square returnleg of entity1966 is collision-compatible and used from the receiver with acquired portable gun, away from rack. Root full-world replay PASS3394: capture/pause, finite AUTO403.455770J,22 consecutive airborne ticks>250N, ordinary STOP/Drop and actualreceiver90tick firmfooting, zero deaths/spentstock. Native20/20 and originalprimary2135 on archive96b121…; prior Godot111/render31 stays scoped to preserved2b87 archive. Independent geometry review found/fixed historical-loaded acceptance; all square corners must be inward of actual open tips at all finger planes, default closed-face solver tolerance only. Existing70mm rearrail perches on tips and is rejected; do not weaken check. This is same-assembly reverse-use, not separate-route or arbitrary-surface proof. Evidence portable-rail/RESULT.md and final-source-receipt.json. No APK/CI/full-native-rebuild claim; catch remains stopped.

### Compound receiver contacts during physical top-out (2026-10-14)

A reconstructed hang probe must re-probe the actual lip and carry its compound subshape before affordance validation. General ground ranking selects one contact and can hide an accepted receiver child when overlapping coplanar children both contact the capsule. The existing PlayerContactListener now has one focused actual-bottom-contact receipt, armed from the active plan before Update and reset on every collision substep. It preserves exact body/child matching, material point, normal, friction and freshness; it does not change general grounding, landing, contact response or plan acceptance. The foot row still begins from an actual solved contact and may expire only once under its existing limits. Nativef4af publicPASS1161 with unchanged153tickdeadline/firm90; affected topout, blocked, release, no-candidate, restart and fixed90/caller60/360/grouped4 checks pass. See alternating-shimmy evidence.

## Rejected traversal requests retain actual load

A neutral rejected Action or contextual Hanging Jump must retain the existing radial rows, rest targets, preload and work ledger. Input arbitration and candidate sensing cannot tear down this load path. `begin_cargo_transfer` leaves radial ownership only after hand reach, pair, receiver identity and standing clearance pass. Directional control, actual release and an accepted finite Climbing Jump retain their existing ownership transitions. `sync_radial_profile` evaluates these physical ownership conditions rather than treating an unaccepted request as a completed transition.

Rigid ledge shimmy reuses `probe_lip_pair`, including verified narrower corner grips; actual attached-hand reach, exact body identity, visibility and capsule clearance still gate movement. The west bar junction is exposed by rear-mounted real brackets, with no visibility exception. See `evidence/ci-repair-2026-10-15/west-and-radial-repair.md` and the accepted native receipts.
