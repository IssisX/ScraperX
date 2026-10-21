# Run the Fold — adversarial ascent selection, 2026-10-21

Status: selected concept for the owner's design exercise, not an implemented or playtested feature. Existing gameplay implementation remains paused. This document does not replace the active task record or authorize reviving deferred machinery.

## The player experience

A massive steel maintenance walkway curls upward through the tower. Run along its rising face, change between actual foot and hand contacts as panels steepen, and jump onto the fixed upper landing before the floor becomes a wall.

The candidate connection is the existing407m inspection approach to the418m brace. This requires a new exterior machinery bay; the current4.4m opening is not proof it fits.

From safe footing, the player sees a complete cycle: steel rises, hinges expose real tread faces, brakes slow the upper transfer, and the structure returns. The upper landing and an outboard refuge remain visible. Walking onto a particular panel and waiting must not deliver the player to the exit.

Once aboard, the player follows the travelling shoulder instead of following a HUD marker. The direct line is faster and exposes changing hinge gaps. An outboard line gives access to a protected side refuge where the player can stop, watch and re-enter on a later cycle. The last move is an ordinary short jump to fixed receiving steel, followed by actual stable footing and onward walking.

Use existing movement/look, Jump and contextual grip. No new bank of buttons, simultaneous grip-and-jump requirement, forced camera shot, automatic attachment or assigned departure velocity. Keep the camera horizon readable; let steel motion, actual hand strain, motor load and brake sound communicate the machine.

The world consequence is visible behind the player: this is working industrial machinery that keeps transforming the space they just crossed. The proposed radio guidance explains its danger once rather than narrating every movement. Import Claude's lesson that one action changes several meaningful conditions; do not import Matron, her trap, or her kinematic pipe locomotion.

## Engineering selection and corrections

Eight agents contributed across bounded waves: two source mappers, three independent designers with the same brief, and three fresh refuters with physics, touch/novelty, and integration/checkpoint lenses. Root checked source/math and selected the concept; votes were not an approval gate. Mappers used medium effort; designers and consequential physics/integration refuters used high; touch refuter used medium. No max/ultra escalation was needed.

Candidate A, Run the Fold, is the strongest overall bet: unfamiliar continuous-contact traversal with a physically plausible energy source. Candidate B, Walk the Buckle, makes the player's weight steer a compressed runway. That premise is unproven and could demand fragile instability tuning; do not silently add it to A. Candidate C, Hammerback, is cheaper to prototype but repeats Slingshot's launch-and-land decisions and assumes unsupported traction at its proposed speed.

Corrections to A:

- Remove the promised fixed apron within2m underneath the whole motion. A floor returning through that space could crush its occupant. Recovery must occupy demonstrated space beside the sweep: protected refuge/re-entry contacts and the existing403.5m lower tray where an authored miss can actually reach it. No universal safe-fall promise.
- Prefer a bounded coupled drive arrangement over eight independently authored actuator schedules. Exact linkage/drive topology is unresolved. One shared finite electrical source and honest summed work are required even if several joints are actuated. Do not claim a single-drive wave has already been engineered.
- Use real tread and rail geometry. Native support requires upward normal>=0.55, about56.6degrees from level; ordinary traction imposes a separate, sometimes stricter limit. A rough-looking steep panel is not footing.
- Exclude only this encounter's unsafe moving contacts from automatic checkpoint eligibility. Preserve existing moving-support checkpoint contracts elsewhere. Save approach/refuge/exit only when their actual contact and encounter state permit a usable restore; clear transient grips and restore controller/brake/energy state with bodies.
- No spring caps, hand force limits, collision checks or death thresholds change to rescue the design.

## Scope and physical account

Author's initial dimensions are hypotheses: eight3m by1.6m panels, approximately2000kg moving steel,11m player rise,30kW source at70percent conversion over12s. They are not surveyed geometry or tuned values.

Potential-energy estimate: (2000kg*5m +85kg*11m)*9.81m/s² =107272J. The chosen electrical source could supply252000J mechanical work over12s. This is not hinge sizing or an energy-closure result: inertia, peak torque, friction, braking and actual trajectory still matter. A straight24m uniform2000kg deck would alone impose roughly235kNm at its root when horizontal; no small generic motor is assumed.

Electrical work enters finite native joint/drive forces, then moving steel and player contact. Hinges/bearings react into the tower backbone. Braking absorbs energy as declared heat. Jolt owns body motion, contact and momentum. Godot owns input, interpolation and feedback only.

Reuse Kit bodies/hinges, PhysicalHandClimb, PhysicalFootPush where the real public contact path supports them, actual support-point velocity, native landing and existing lifecycle infrastructure. DeformablePlank is a small wood surrogate, not a ready steel folding floor. YardCrane is accounting precedent, not a generic fast drive API. New work includes the encounter owner, powered topology, collision/tread geometry, recovery/receiver construction, explicit saved controller state and presentation from native receipts.

The author's rough twelve dynamic bodies plus joints is a scope estimate, not a phone-performance claim. Coupled joint iterations, self-contact and active islands matter more than the body count alone.

## What the refuters found

Scores are subjective judgments, not measured quality or approval votes. Order: wow / physics / touch / rules / buildable.

| Candidate | Physics critic | Touch critic | Integration critic |
|---|---|---|---|
| A: travelling fold |8/7/7/8/6|8/7/5/6/5|9/7/7/8/5|
| B: weight-steered buckle |8/5/7/7/4|8/6/8/8/5|8/6/7/8/4|
| C: breaker-arm launch |9/6/5/7/5|8/7/5/4/6|8/6/6/8/7|

Their preferences differed: physics chose A, touch B, integration C for cheapest decisive experiment. Root chose A for novelty plus plausible physical ownership, while rejecting its recovery promise. No vote established playability.

Concrete checks by root and refuters:

- A fixed recovery surface at height a, below deck d(t) with clearance c, requires max(d)-2 <= a <= min(d)-c to guarantee a2m maximum fall. Thus the deck's vertical sweep in that column must be <=2-c metres. A larger sweep invalidates that universal apron claim.
- C's chosen600kg arm,12m length,8m player radius and15.5m/s tip speed give approximately28.2kJ kinetic plus10.1kJ potential energy. The44.7kJ ideal torque-work ceiling leaves limited losses, not impossible energy. However, nominal corotation needs about2240N inward player force. Current capsule friction is zero; its finite native ground-traction owner is around620–660N at that orientation. Merely roughening the art cannot provide the missing reaction. A constant-acceleration screen also completes its32degree power stroke in about0.58s; running8m during that stroke would require13.9m/s. These are analytic screens, not simulated trajectories.
- B's85kg/1200kg mass ratio does not prove impossibility. Its missing fact is whether a useful, recoverable equilibrium range exists. Energy arithmetic cannot prove that player weight can deliberately steer its fold.

## Decisive next observation

If implementation resumes on this concept, build one full-scale native greybox encounter before producing eight polished panels. Demonstrate the actual powered contact sequence, a deliberate retreat to an outboard refuge, re-entry, and fixed exit. Standing on one body must not complete it. Use the real touch path and Godot MCP to inspect the prototype, including losing contact, crush clearance and checkpoint restore. A single lucky traversal is insufficient; ordinary input variations must remain readable and usable.

Kill or materially redesign the concept if it needs a hidden staircase bypass, unavoidable crushing, a narrow memorized input sequence, force-limit changes, or extended tuning with no demonstrated player agency. Do not build a large polished machine around a failed premise.

## Evidence and limits

Source inspection only, at ChatGPT b71e2384e352c256c65f2071b7d3508da3c6af79 and freshly fetched Claude28afbc4ab142e9e2b207dd5ce909cf9f5778b879. Cached origin/ScraperX-Claude was stale and was not used for the current comparison. Read with git show/grep; no checkout changes.

Material source anchors: src/sim/mechanism_kit.hpp:177; powered_cable.hpp:12; physical_hand_climb.hpp:95; simulation/support.cpp:12; simulation/constants.hpp:23; simulation/contacts.hpp:424; simulation/locomotion.cpp:318; simulation/world.cpp:168; simulation/geometry.cpp:71; simulation/tick.cpp:556; simulation/checkpoint.cpp:93; simulation.hpp:744; inspection_junction_route.hpp:13. Claude: src/sim/shared_survival.cpp:120 and matron.cpp:1427,1514,1683.

Root independently read the zero-friction and finite-traction paths and ran a Python arithmetic screen. Observed output: C inertia15040kgm², kinetic28229.375J, potential10111.353J, ideal torque ceiling44680.429J, constant-acceleration stroke0.5765s, required inward force2240.29N. This verifies those arithmetic calculations under their stated simplifying assumptions, not gameplay.

No new Godot prototype, native gameplay test, phone performance measurement, screenshot, APK or release claim exists for this concept. Gameplay implementation remains paused; useful build/artifact workers remain preserved. Design-only publication needs no new APK; GitHub's documented [skip ci] commit instruction avoids redundant push-triggered builds without altering workflow gates: https://docs.github.com/en/actions/how-tos/manage-workflow-runs/skip-workflow-runs . Existing game CI jobs remain independent.
