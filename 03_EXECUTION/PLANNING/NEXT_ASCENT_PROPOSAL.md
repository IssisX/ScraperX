# Development direction and next ascent proposal

**Status:** Development plan, 2026-09-26. The owner selected **mixed parkour and machines** and directed us to proceed. The height targets and first mechanism below remain engineering proposals until their physical contracts close. This document authorizes no claim of additional playable height.

**Profile:** `MACRO-TRAVERSAL-STRICT` (project requirement). The Atlas remains spatial authority; `00_START_HERE.md` remains delivery/evidence authority. Superpowers supplies the design/execution workflow, Causal Mechanism Compiler supplies physical contracts, and ThreeSpine supplies relevant mathematical methods. Skills do not expand product scope by themselves.

## 1. Starting point

The current source is `c2de2f3`, with gameplay delivery `3034691`. The demonstrated new mechanism route reaches the +11 m tower ring through the pipe bridge's +8 m receiver. These are existing delivery facts, not new tests performed for this proposal. The optional ordinary route to +154 m and the 1,600 m tower objective do not count as newly authored mechanism progress. Android packaging is recorded; actual Fold execution and sustained performance remain unproven.

The next production question is whether the existing substrate lets us author useful new traversal substantially faster. Building another elaborate miniature mechanism would not answer it.

## 2. Three directions

| Direction | What the player does | Production tradeoff | Recommendation |
|---|---|---|---|
| **Mixed parkour and machinery** | Climb between freight decks, operate a few large machines, use their changed geometry, choose exposed shortcuts or broader routes that reconnect. | Requires good level layout; reuses movement and a small set of mechanical families. | **Lead with this.** Matches the GDD's braided ascent and the owner's requested balance. |
| **Machinery and chain reactions** | Read visible loads and restraints, redirect cargo, cause linked structural transformations, then traverse the aftermath. | Strong spectacle and causal depth; each extra physical handoff increases capture, persistence and testing work. | Use for selected major encounters. Making it the density of every floor would slow production sharply. |
| **Parkour with occasional machines** | Follow continuous exposed climbing lines; machinery supplies occasional major transfers and shortcuts. | Fastest likely content expansion using existing movement; repeated static obstacles can become generic. | Useful for longer connecting stretches. Keep consequential world changes substantial enough to retain ScraperX's identity. |

These are emphases within one persistent tower, not separate game modes. A legal shortcut remains a valid outcome.

## 3. Recommended next playable slice

**CHOSEN planning target:** extend the new route from +11 m to +66 m, a **DERIVED +55 m** of additional ascent. These elevations use the existing **SOURCE 11 m ring spacing**. They are destination targets, not evidence of fit or a closed mechanism design.

| Checkpoint in development | Intended playable result | What it resolves |
|---|---|---|
| **A: +11 → +33 m** (CHOSEN) | One substantial macro transfer, with a supported approach, reachable operation and stable upper exit. | Can existing machinery primitives buy multiple levels without another framework or long setup sequence? |
| **B: +33 → +44 m** (CHOSEN) | An authored exterior/interior parkour connection with a forgiving route and a more demanding shortcut that reconnect. | Does the new space feel athletic, readable and worth exploring between machine encounters? |
| **C: +44 → +66 m** (CHOSEN) | A second macro encounter using the first encounter's proven family where it fits, with a different approach, intervention or consequence. | Does reuse reduce implementation effort without producing an obvious copy? |
| **D: whole route** | Ordinary input from grade through the opening and all new connections, checkpoint continuation, rendered observation and an exact-source Android candidate. | Does the experience function as one connected piece of the game? |

The two new macro encounters are a **CHOSEN scope budget**, not a requirement for two new simulation systems. Resolve A before specifying C's detailed machinery. If the second placement cannot use the family's validated envelope, either choose a compatible placement or explicitly justify another family; do not silently scale all dimensions and declare the physics unchanged.

### First candidate: counterweighted tilting freight frame

An idle freight transfer frame has a raised, visibly restrained counterweight. A large reachable release frees the frame; the descending weight raises the boarding end toward an upper freight landing. A structural receiver arrests and supports it. The player uses real moving support and exits onto the tower. The frame and its restraint must look like working industrial equipment, with their load path visible from the approach.

This candidate favors one main assembly on a hinge, a physical release and a finite receiving contact. It reuses body construction, mass properties, hinges, control grips, contact, player support and checkpoint infrastructure. The existing pipe bridge's finite crush model is a possible starting point, not proof that its parameters or force direction fit this frame.

**Screening geometry only:** a boarding point rising symmetrically from +11 m to +33 m about a +22 m pivot, through CHOSEN angles of −25° to +25°, requires a DERIVED radius `r = (33 − 11) / (2 sin 25°) ≈ 26.03 m`. This establishes the approximate scale of the sweep, not tower clearance, boarding safety, energy closure or successful arrival. Final coordinates belong in the Atlas after screening the full swept volume, deck rings, receiver and counterweight travel.

Other candidates considered:

- **Cable counterweight carrier:** direct vertical travel and a smaller horizontal footprint, but long-stroke braking, terminal capture and visible cable/sheave behavior need explicit closure. Keep as the alternative if the hinged frame's sweep does not fit. Reusing a hidden speed governor would not resolve it.
- **Roller → pendulum → deployed bridge:** strong later spectacle, but adds multiple dynamic transfers before establishing a cheaper content workflow. Defer this chain until its extra interactions buy a distinct player decision.

**UNRESOLVED before an implementation-ready contract:** swept volume and side exit; masses/COM/inertia and drive curve; reachable release force/travel; arrival speed across resistance and rider-position variations; finite stopping stroke; stable loaded/unloaded rest; early/late boarding and missed boarding; coherent checkpoint restoration. Resolve these with a bounded model and native probe, not assumptions disguised as tuning constants.

The compiler's first checks are target support and onward route, then geometry, force, energy and reaction closure. Use its converged band evaluator where the reduced model applies; Jolt must reproduce the relevant outcomes. If the geometry demands an artificial brake or exact single-angle snap, change the geometry or topology.

## 4. Build order and concrete ownership

Work inline as the sole writer on the existing local `ChatGPT` checkout. Preserve unrelated work and check the remote before publication. This is a development plan; the next mechanism's numerical contract is intentionally not falsely labeled ready.

- [ ] **Resolve A's receiving route and physical contract.** Name actual supports, standing surfaces, control access, reaction paths, permitted variations and recovery. Screen the existing tower sweep. Create a new AS-017 ticket only for the resulting closed slice; update the Atlas with accepted placement. A rejected candidate is a design result, not a reason to activate legacy machinery.
- [ ] **Implement and prove A.** Add the selected native assembly, integrate normal-world construction and all new persistent state, render the authoritative bodies, and exercise the actual player's approach, operation, ride/crossing and upper departure. The route test must first fail because the new route is absent, then pass through normal input. Include source/link removal, unsafe boarding and checkpoint continuation cases that discriminate real failure.
- [ ] **Author and traverse B.** Compose actual ledges, beams, work platforms and clearances using existing movement. Verify both routes with normal input and rendered first-person inspection. A valid unexpected route is accepted. Do not change global reach or jump to compensate for bad local geometry.
- [ ] **Build C using demonstrated reuse.** Record which code and contracts are reused, and which spatial/load assumptions changed. Extract a shared component only when the second real use requires it. Re-run the affected physical envelope and receiver checks.
- [ ] **Deliver D and judge the experience.** Run the connected input route, required native/render/collision/checkpoint gates, and exact-source Android export. Inspect the rendered approach, causal event and arrival. Record device installation/execution/performance separately; complete a sustained representative Fold check against the existing 45 FPS target before claiming mobile readiness.

| File / seam | Planned responsibility |
|---|---|
| `src/sim/freight_frame.hpp`, `.cpp` (proposed if this candidate survives) | Assembly construction and only the finite material/history state this mechanism actually needs. Kit/Jolt owns its bodies and constraints. |
| `src/sim/mechanism_kit.hpp`, `.cpp` | Reuse existing primitives. Add or extract behavior only for a demonstrated missing capability. |
| `src/sim/simulation.cpp`, `.hpp` | Normal-world registration, unique entity allocation, stepping and checkpoint capture/restore for added state. |
| `godot/presentation/main.gd` | Static route construction and necessary presentation through existing native body readback. No second mechanism simulator. |
| `src/sim/world_solids.inc` | Regenerate from changed normal static geometry; do not hand-author a conflicting collision map. |
| `tests/freight_frame_tests.cpp` (proposed), `CMakeLists.txt` | Normal-input ascent, causal negative cases, supported upper exit and continuation after checkpoint restore. Reuse existing helpers where appropriate. |
| `godot/presentation/ui/ui_test_driver.gd`, `.github/workflows/wo000-delivery-spine.yml` | Extend the existing input/render and delivery path for the new route, with clear source identity. |

**Known reuse conflict:** Kit's optional guide governor is an artificial braking source unless a permitted physical device owns that action. Its automatic catch relatch also uses proximity/speed logic. Neither is automatically valid for the strict macro profile. A new primary load path must obtain restraint, arrest and capture from the modeled visible mechanism. Existing capabilities are a menu, not a blanket certificate.

Do not create a general mechanism editor, a second physics engine or a campaign generation framework to deliver this slice. Keep fixture-only shortcuts out of the acceptance route.

## 5. How the rest of the game grows

1. **Demonstrate the production method through +66 m.** Get a connected, readable stretch with contrasting play, useful height and repeatable authoring. Test the Android path early enough to expose a performance problem before multiplying it.
2. **Complete a coherent lower district toward +154 m** (CHOSEN subsequent milestone, aligned with existing structural/fallback extent). Recombine proven families and author new situations, shortcuts and recovery routes. Introduce a bounded inhabited work area and a physical access objective when the traversal loop is established, consistent with the GDD's inhabited tower. Do not build a general city simulation as a prerequisite.
3. **Expand toward the 1,600 m summit in connected districts.** Freight, load-bearing architecture and process/isolation supply different regional problems. Specify the next district in detail only after the previous one works. Add a new simulation capability when an authored encounter requires it; verify sleeping/streaming and committed-state restoration before multiplying active content beyond the demonstrated device budget.
4. **Close the whole ascent.** Earned return routes, fall recovery, persistence, device performance, legibility and a physically reachable summit must work together. A distant skyline or a list of planned districts never counts as this milestone.

No calendar estimate is justified yet. The next completed placement and its second use will provide evidence for content throughput. Keep the later plan coarse rather than spending implementation time pretending every future floor is designed.

## 6. Decisions I will challenge

- **A bespoke machine every few metres:** reject the production pattern. Reserve bespoke work for a new player decision or a major world change; reuse its mechanics elsewhere.
- **Height as the only score:** a lift to the roof could maximize metres and ruin the game. Track supported new route height together with hands-on traversal, useful choices, waiting and recovery cost.
- **More links because a chain sounds spectacular:** each link must contribute a readable interaction or meaningful aftermath. Otherwise remove the link.
- **A giant object assumed easier than a small one:** larger sweep, inertia and stopping work still need closure. Macro scale is about visible consequences; body count is not spectacle quality.
- **Another framework or skill hunt during a blocked build:** require a specific missing capability and an adoption payoff. Collecting tools is not level production.
- **Repeated tuning of an ill-conditioned arrival:** widen the physical receiver, shorten the transfer, reshape the drive or choose another topology. Do not disguise the problem with a snap or success flag.
- **An APK or green test treated as proof of fun:** require a normal first-person play observation. Separately verify the shipping device. Neither a numerical test nor a screenshot proves satisfying traversal by itself.

Apply this to my own proposals as strictly as the owner's. Be blunt about the cost and consequence of an idea, explain the evidence, and offer a smaller viable alternative. Personal insults are not useful design criticism.

## 7. Working cadence and completion evidence

Keep only one gameplay candidate in publication at a time. During active work, give concise updates on the current decision, new evidence, failure or pivot; do not replace progress with repeated reassurance. Surface changes to product direction or costly new subsystem scope before committing the project to them. Routine implementation choices proceed inline.

After each useful slice, report:

- highest **supported** new route arrival and its onward connection;
- what the player now does and which world state changes;
- which primitive/family was reused and what genuinely new engineering was required;
- observed operation/traversal/waiting and recovery behavior, with subjective judgments labeled as such;
- native, input, render, Android artifact and device evidence at their actual completion levels;
- the next bounded task and any unresolved decision.

**Suggested milestone goal:** Deliver a continuous, normal-input route from grade to a supported +66 m exit, with contrasting parkour and macro machinery, a meaningful route choice, correct checkpoint continuation, and an exact-source Android build; establish actual Fold play and sustained performance before calling it mobile-ready.
