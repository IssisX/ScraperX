/goal Develop ScraperX into a varied, physically causal ascent through an enormous working industrial structure. Integrate the supplied machines, improve human-limited parkour, deliberately design challenging obstacles, and develop coherent places with meaningful decisions, tension, recovery, exploration, and persistent consequences. Preserve completed gameplay and ongoing work. Pursue breadth through depth: complete one coherent playable encounter or connected place, then move onward.

Everything below belongs to this goal. These requirements remain in scope until implemented and demonstrated, or explicitly changed by the owner. Phase summaries, progress percentages, builds, and passing isolated tests must not silently remove unfinished requirements.

### Phase 0 — Preserve the project and establish the execution frontier

- Work only in `IssisX/ScraperX`, on the exact write branch `ChatGPT`.
- Read the authoritative GitHub handoff first: https://github.com/IssisX/ScraperX/blob/ChatGPT/CONTINUATION_HANDOFF_CODEX.md
- Inspect the live repository, branch, HEAD, worktrees, working changes, applicable `AGENTS.md`, `00_START_HERE.md`, and `CONTINUE_HERE_CHATGPT_CODEX.md`. Live evidence takes precedence over stale handoffs.
- Treat `4364b8e825a7d4c04018755ca12a49dc9add353b` as a research baseline, never permission to reset a newer checkout. Preserve user and concurrent work.
- Preserve the completed eight repairs: responsive, stable 1.7-second mantling; usable opening ascent; supported cable endpoints; varied licensed swear recordings; playable nights; simplified audio settings; simple restart/spawn with expandable advanced controls; and the visible parachute canopy.
- Preserve existing successful cargo, monorail, scissor-lift, launcher, upper-route, moving-support, climb, balance, restart, and checkpoint behavior.
- Keep the complete requirements and their implementation status in repository documentation. Record completed, in-progress, pending, and deferred work separately. A short goal title does not replace this scope.
- Root owns difficult diagnosis, engineering decisions, integration, and delivery. Use useful bounded agents actively, with explicit non-overlapping ownership. Keep investigative agents read-only unless assigned a write surface.
- Use relevant installed Superpowers, causal-mechanism-compiler, Threespine, and Godot skills and references. Current user instructions and project physics ownership govern their application.

### Phase 1 — Finish the current playable connection

- Preserve the supplied-machine integrations already underway rather than rebuilding them.
- Finish the Crown hanging-bar connection: reachable approach, ordinary jump/catch, finite body-driven pumping, deliberate timed release, actual receiver landing, stable onward exit, missed-release recovery, and a usable retry.
- Resolve the remaining touch boarding/ride problem from actual evidence. Correct the supported cause without manufacturing footing, suppressing legitimate motion, or weakening acceptance.
- Finish the pending Pitman touch integration and its connected entry/exit.
- Preserve the completed native physical mantle correction and its responsive completion.
- Keep current reduced-order landing work accurately described. Compact posture and post-impact recovery are partial work, not proof of a completed physical roll.
- Complete the current ownership boundary before beginning another machine integration.

### Phase 2 — Make locomotion responsive and preserve momentum

The central rule is: **player intent requests an action; actual physical state determines whether and how it succeeds.**

- Improve the existing native owner rather than replacing the architecture.
- Preserve native C++17/Jolt simulation at 90 Hz, the dynamic 85 kg player, existing snapshots, physical hand coupling, contact recovery, reciprocal support reactions, and render interpolation.
- Preserve the existing 5.5 m/s standard movement ceiling, 8.0 m/s sprint ceiling, 5.5 m/s upward jump baseline, existing 22/14 m/s² controller constants, 1,500 N per-hand ceiling, and 3,000 W default hand-command budget. These are project parameters, not universal physiological constants.
- Inspect both ground/air control paths and repair demonstrated inconsistencies between force-based and velocity-assigned movement.
- Resolve movement relative to the actual support contact-point velocity, including angular motion.
- Earn acceleration, reversal, braking, and direction changes through finite effort, available traction, actual contact normal/load, slope, friction, and time.
- Preserve inertia. Zero ground input decelerates through real braking/friction; neutral airborne input does not erase momentum.
- Preserve genuine excess world velocity inherited from platforms, swings, rotating supports, and launchers. Walking and sprint ceilings are not global momentum clamps.
- Keep air control responsive but weaker than grounded control, with bounded force/work and no arbitrary lateral velocity replacement or cancellation of a committed jump arc.
- Preserve successful movement feel and route clearances until an actual comparison demonstrates improvement.
- Jump only through valid physical support. Apply finite push-off impulse/work and equal-and-opposite reaction to dynamic receivers at the actual contact point, accounting for effective inertia and rotation.
- Carry existing horizontal velocity and departure-point support velocity into jumps.
- Use short native-tick buffering for intent where useful, but execute only when valid contact exists. Do not grant free-air jumps because the player recently left an edge.
- Do not replace the player with `CharacterVirtual`, create another physics authority, or introduce decorative stamina/fatigue systems.

### Phase 3 — Make climbing, mantling, vaulting, catching, and swinging physically continuous

- Retain independent finite-force hands and reachable regrips. One-handed transitions redistribute real load.
- Require reachable, unobstructed geometry, adequate support, acceptable closing velocity, available grip force, actual constraint engagement, and collision resolution.
- Moving, rotating, and deforming holds carry the player through their actual motion. Do not add duplicate rider-weight forces.
- Allow slipping or falling when force, grip geometry, clearance, or reach is inadequate.
- Convert one remaining gravity-off or root-velocity-prescribed mantle/vault path at a time. Preserve successful paths until replacements pass equivalent behavior.
- Earn mantles through actual hand attachment/reaction, finite pull-up force/work, available foot contact or push-off, continuous clearance checks, and real receiving-footing proof.
- Preserve the repaired 1.7-second mantle and responsive completion. Do not make movement artificially slow merely to call it physical.
- Preserve legitimate vault approach momentum and receiving momentum. Do not reset speed at the beginning or end.
- Swing through actual player mass, suspended structure, grip/cable constraints, and gravity.
- Pump through finite physically represented body extension, raising/lowering, or center-of-mass changes. Correct timing can add energy; poor timing can remove energy or accomplish little. Neutral pumping must not generate energy beyond external sources.
- Bound and record supplied muscular work. Do not add arbitrary angular velocity per button press or automatically choose the pumping phase in gameplay.
- Release on the authoritative native tick. Passive release removes constraints without replacing world velocity. Powered release is a separate explicit finite-work action with a real reaction path.
- After release, preserve ballistic motion and legitimate bounded air control. Arrival depends on release phase, velocity, geometry, and reach.
- Use compliant ledge catches that absorb load over time. Fast catches may create pendulum motion, substantial deceleration, grip failure, or a miss.
- Do not magnetize, teleport, instantly delete momentum, or automatically reacquire deliberately released grips.
- Account separately for supplied work, mechanical exchange, kinetic/potential energy, elastic storage, dissipation, and numerical residual wherever evaluating closure. A command budget alone is not complete energy accounting.

### Phase 4 — Implement physical impact recovery, deliberate rolls, stumbles, and balance

- Reuse existing native energy-based landing recovery.
- Drive bracing from actual relative approach speed, surface normal, contact impulse, tangential slip, orientation, preparation, and available body support.
- Make prepared versus late or poorly oriented landings physically distinguishable through contact duration, travel, impulse distribution, deceleration, and recovery.
- Heavy landings affect subsequent movement through native recovery state without arbitrarily disabling all controls.
- Require deliberate preparation/input, compatible velocity, suitable footing, and sufficient clear forward room for a roll.
- Implement actual roll travel and time, appropriate forward momentum, collision with surrounding geometry, and a changed distribution of deceleration.
- The translation-only capsule and animation alone cannot demonstrate anatomical rolling. A reduced-order implementation must physically implement and identify its contacts, impulse exchange, energy loss, travel, and limitations.
- Preserve the native 20 m/s lethal-impact rule. Rolling must not erase fall consequences or evade that threshold.
- Cause stumbles and recoverable instability through real off-axis impacts, landing slip, support acceleration, inadequate traction, obstruction, and narrow or rotating footing.
- Give mild mistakes a short, playable correction opportunity. Severe mistakes can cause an uncontrolled fall.
- Recovery requires actual contact, finite correction force, feasible foot placement/reach, and player input. Do not substitute random animations, fixed input lockouts, or invisible rescue forces.
- Preserve narrow-beam gameplay while making correction accountable to contact and effort.
- Evaluate support footprint, body position, and horizontal velocity together. Use the reduced-order XCoM approximation where appropriate:
  `XCoM = COM + horizontal_velocity / sqrt(g / effective_COM_height)`.
- Treat XCoM as an approximation, not a full anatomical model. Losing support must resume ordinary falling.

### Phase 5 — Preserve clear controls and add tactile physical feedback

- Preserve existing touch, keyboard, and controller vocabulary.
- Recognize input promptly; physical acceleration and muscular limitations determine how quickly motion follows.
- Arbitrate context natively: supported Jump pushes off; valid hanging Jump requests mantle/contextual action; Action near a valid moving hold attempts a catch; Drop intentionally releases; directional hanging input climbs, traverses, or pumps according to actual geometry; Crouch/prepared input requests viable bracing or rolling.
- Avoid ambiguous extra touch buttons when existing controls express the action clearly.
- Sample commands into the native 90 Hz stream. Gameplay transitions must remain independent of rendering FPS.
- Extend the existing native-state-driven presentation system rather than adding a competing dispatcher.
- Communicate grip engagement, sustained strain, heavy landing compression, slips, balance corrections, mantle weight transfer, swing release/free flight, and stumble recovery through appropriate body/camera/audio/haptic responses.
- Scale feedback from actual impulses, forces, loads, slip, and native events. Coalesce events by tick and throttle vibration.
- Keep head/camera effects bounded and smooth, without excessive shaking, drift, or double interpolation.
- Honor audio, vibration, and reduced-motion settings. Distinguish Android permission/device support from merely having a vibration API.
- Preserve the existing licensed recordings and acquisition approach; no paid audio subscription is required.

### Phase 6 — Author coherent places and challenging encounters

The player is a capable, human-limited climber inside an enormous working industrial structure. Through observation, risk, manipulation, and experience, frightening places become understandable and mastery grows.

- Develop awe, curiosity, apprehension, physical urgency, human panic, recovery, relief, satisfaction, and ownership of player-made changes.
- Treat machinery/movement outpacing surrounding experience as a design hypothesis grounded in inspected source/documents—not verified phone-playtest feedback.
- Give regions recognizable industrial purposes and identities. Freight, maintenance, processing, crane work, and occupied pockets are examples, not a mandatory roster.
- Coordinate geometry, machinery, lighting, sound, materials, and selective human traces. Let boundaries follow useful function and geometry rather than arbitrary elevation bands.
- Design and build parkour as primary gameplay work. Require reading, thinking, commitment, climbing, traversing, activating, and exploiting machinery.
- Vary decisions about position, load, alignment, restraint, timing, and route. Different machines must not all become board, press UP, wait, and exit.
- Preserve physically legitimate alternatives. Where useful, offer exposed shortcuts versus longer sheltered routes with genuine tradeoffs.
- Connect neighboring encounters through useful physical consequences: repositioned loads, exposed crossings, cleared obstructions, earned access, or return routes.
- Keep useful consequences visible and persistent. Each added stage must contribute a decision, consequence, or traversal opportunity.
- Shape tension and relief through sheltered observation, anticipation, exposed commitment, demanding execution, and secure arrival. Vary enclosure, sightlines, sound, footing, and exposure.
- Near misses must emerge from real contacts, motion, and machinery. Recovery must require feasible player action.
- Add selective repairs, maintenance access, supplies, occupied spaces, and appropriate activity that explain use/history.
- Introduce workers/operators when they add meaningful interaction and respect actual access and world state. Begin with coherent places rather than a broad population framework.
- Reward exploration with useful knowledge, equipment access, shortcuts, alternate approaches, and revealing views.
- Make progress perceptible through changed surroundings, landmarks, earned access, and growing understanding.
- Record why the summit matters as an open design decision. Do not establish invented backstory as canon.

For each encounter, resolve its purpose, what the approach teaches, the meaningful choice, the physical state changed, the movement skill required, the rise and release of tension, what happens after a miss, the feasible recovery, the persistent aftermath, and how its exit prepares the next experience.

### Phase 7 — Prioritize the gravity-fed Ferris-wheel encounter

- Treat `ferris-wheel-machine.jpg` as an **important machine priority** after the current safe integration boundary.
- Preserve its defining principle: actual falling material physically loads and drives the wheel to enable player ascent.
- Stone is not mandatory. Choose stone, dirt, sand, or another coherent material based on the mechanism, setting, readability, and mobile performance.
- Give the material deliberately high-quality geometry, surfaces, collision, mass/inertia, impacts, sound, retention, and spilling. Generic low-quality debris does not satisfy this requirement.
- Author the entire encounter: approach and observation, understandable feeding controls, finite material supply, boarding decisions, exposed ascent, challenging exit, failure/recovery, reset/reload, and persistent aftermath.
- Resolve bucket geometry, loading, discharge, wheel dynamics, player loading, reactions, energy transfer, and timing before claiming integration.
- Do not substitute decorative falling material around a powered wheel.
- Reuse suitable components while preserving this encounter’s distinct mechanical and experiential identity.

### Phase 8 — Continue through the remaining supplied designs

Inspect and use these actual Android Download sources:

- `ScraperX-Vertical-Machines.zip`
- `ScraperX-Vertical-Machines-Expansion-01.zip`
- `ferris-wheel-machine.jpg`
- `cart-drag-machine.jpg`

Reuse viable archive implementations. Images establish concepts, not working geometry or energy transfer. Name missing/inaccessible sources precisely and continue with accessible sources without inventing contents.

Choose placement, elevation, dimensions, order, and adaptations autonomously. Repair weak connections and repetitive gameplay. Give 0–30 m priority when a machine genuinely fits, without forcing every design there or adding height merely to accommodate it.

Complete one machine’s approach, controls, loaded operation, ascent, stable exit, recovery/reset, and encounter experience before starting another. Aim for substantial ascent—typically 20–30 m where appropriate—while varying encounter extent and avoiding blind scaling.

Reuse physical primitives and authoring systems, not complete repetitive machines. Do not reinstate the removed parts whitelist. New components must obey native ownership, visible causality, finite forces/work, honest collision, real loading, and preserved momentum.

Continue through viable supplied designs. Defer a design when it cannot add a distinct coherent experience without disproportionate changes, and state the concrete reason.

### Phase 9 — Focused verification and exact-source Android delivery

- Never let checking, verifying, or testing become the main task or expand into redundant campaigns.
- Use checks capable of rejecting actual defects in changed behavior and connected entry/exit/recovery. Archive tests alone do not prove in-game integration.
- Demonstrate bounded movement and momentum, real contacts/reactions, momentum-preserving release, phase-sensitive finite-work pumping, failed unreachable/overloaded catches, blocked transfers, unsupported falling, physical landing/recovery differences, feasible balance corrections, and touch-to-native integration.
- Run relevant regressions for successful grade-to-Tower33, upper routes, launcher, climb, balance, moving supports, restart, and checkpoints.
- Treat native tests, Godot input/rendered evidence, Android build, and physical-device observations separately. A build is not runtime proof.
- Update only documentation affected by implementation.
- Batch completed integrations into one normal exact-source GitHub Actions APK build unless an earlier build resolves a genuine device-dependent blocker.
- Preserve normal package/signing and exact-source delivery requirements.
- Report concrete player-experience improvements, completed implementation, evidence actually observed, build/APK status, deferred designs, and remaining phone-playtest questions.
- Give occasional rough overall and phase progress percentages. Identify them as estimates.
- Continue autonomously through useful authorized work, without repeated discretionary permission requests or speculative unrelated refactors.
- Keep the goal active until the complete scope is fulfilled. Do not redefine completion around the easiest passing subset.

Use the supplied parkour landing, Hof XCoM, active swing-pumping, official Jolt architecture, and Godot interpolation references as mechanical guidance—not literal physiological parameter tables.

### Accepted visual-quality direction — 2026-10-08

This extends Phases5–8 and remains part of the active goal. The owner wants visually stunning detail throughout the game, including physics-based brick dust and some bricks breaking. Keep breadth through depth: finish the current playable encounter and integrate its material-quality slice without launching an unrelated visual redesign.

- Use deliberate art direction, high-quality coherent surfaces, fracture geometry, fine wear, lighting, depth and scale across completed places. Details should clarify purpose, causality and emotional pacing.
- For the reclaim bay, implement native-contact-driven brick dust with gravity/drag/settling, and selective impact/strength-driven brick breakage. Consequential fragments retain native mass, momentum, contact and loading; do not substitute visual disappearance or cosmetic mass deletion. Record reduced-order visual dust limitations honestly.
- Continue appropriate details through later encounters: condensation and dripping where water/cooling systems justify them; vibration in real loose fittings; load-dependent cable strain and rattling; contact scuffing/settling; wet/dry surface variation; practical light and occlusion changes; coherent repairs, fasteners, deposits and human maintenance traces. Use them where they improve that place, rather than adding a mandatory universal effect roster.
- Preserve mobile performance, settings, successful routes and native physics ownership. No random dramatic failure, free debris energy, camera excess, exhaustive polish campaign or claim of unseen phone appearance.

Status: brick native hull/PBR surfaces and recorded contact audio implemented; rendering and wheel operation pending. Dust and selective physical breakage remain pending. Wider visual-quality direction is ongoing across encounters.
