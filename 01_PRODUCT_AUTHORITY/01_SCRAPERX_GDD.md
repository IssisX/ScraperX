# SCRAPERX — GAME DESIGN DOCUMENT

**Version:** 1.1 — Product baseline + atlas integration  
**Product:** ScraperX  
**Repository:** `ScraperX`  
**Status:** Product-design baseline. v1.1 only binds spatial content to `02_ASCENT_ATLAS.md`; it does not change the governing game.

## 0. Authority and boundary

This GDD defines **what ScraperX is as a game**. It is subordinate to `00_GOVERNING_LAWS.md`; if the two conflict, the governing laws win until the conflict is deliberately resolved.

The physical layout of that game — datum, bands, braids, named modules, capability objects, and the first kernel slice — is specified in `02_ASCENT_ATLAS.md`. The atlas is subordinate to this GDD. It may not redefine the genre, weaken parkour, script consequences the world does not own, or complete the campaign by anything other than physically reaching the summit.

This document does **not** choose the engine, rigid-body backend, structural solver, rope solver, process solver, programming language boundaries, package stack, or other implementation architecture. Those decisions belong to the technical architecture document and must satisfy this GDD rather than redefine it.

No historical implementation, prior project structure, inherited location, or compatibility assumption is part of ScraperX unless explicitly re-established in the current authority set.

---

# 1. Product thesis

**ScraperX is a first-person, fast and precise industrial parkour game set inside and across one persistent, inhabited skyscraper / megastructure approximately 1.6 km tall.**

The player is an athletic climber moving physically through a tower built from large-scale, life-size machinery, mechanisms, load-bearing structure, process infrastructure, and working industrial systems. The campaign is an ascent. The player ultimately completes the main ascent by **physically reaching the summit**.

The tower is not scenery wrapped around a sequence of levels. It is the playable machine.

The player advances by learning how the tower works, reaching and operating machinery, manipulating structure and loads, using rigging, changing process/isolation state, moving objects, creating or destroying viable routes, and surviving the consequences of those changes.

ScraperX is not primarily about solving authored puzzle sequences. It is about changing authoritative world state so that new physical possibilities become real.

---

# 2. Core fantasy

The central fantasy is:

> **I am an athletic climber inside an impossible industrial skyscraper. I can run, climb, vault, hang, leap, operate serious machinery, move and rig real loads, alter structure and process state, and use the resulting physical consequences to get higher.**

The player is capable, but not magical.

Unaided physical manipulation is **slightly heroic but still grounded**. The player may push, lift, drag, brace, position, and move objects at a deliberately slightly heroic but grounded personal scale, but industrial-scale capability comes from the tower itself: forklifts, cranes, hoists, carriers, rigging, process machinery, and other appropriate mechanisms.

The player's power grows primarily through **access to capability**, not abstract character statistics.

---

# 3. The skyscraper

## 3.1 One world

ScraperX takes place in one enormous, physically coherent skyscraper / megastructure approximately 1.6 km tall.

The player moves through interiors, structural voids, work zones, inhabited bands, machinery spaces, open frames, façades, cranes, exterior machinery, and other physically connected parts of the same tower.

The game may stream, sleep, aggregate, or reduce inactive simulation cost, but the world must not become disconnected reset arenas or disposable levels.

## 3.2 Contemporary industry at impossible scale

The tower's primary technological language is **contemporary heavy industry at impossible scale**.

Machines, controls, structural systems, lifting equipment, industrial access, process systems, and work environments should be recognizable enough for physical intuition to transfer. The impossible quality comes primarily from scale, density, coupling, verticality, and the extent to which the building itself behaves as an interconnected industrial machine—not from replacing understandable mechanisms with arbitrary fantasy technology.

## 3.3 Fixed playthrough layout

The meaningful physical world is built from **authored modules with controlled recombination during development**.

Validated modules may be assembled under strict compatibility rules while building the game, but the shipped tower layout is not procedurally rearranged during a playthrough as a consequence of this decision.

The purpose of modular authorship is production leverage and verified composition, not runtime randomness.

## 3.4 Braided ascent topology

The large-scale route structure is **braided**.

Multiple ascent paths may diverge and reconnect. Machinery state, structural consequence, access, process conditions, and player-created changes may alter which routes are viable.

The tower therefore provides meaningful route choice without losing the durable campaign direction: upward.

## 3.5 Frequent exterior exposure

Exterior traversal is a major share of ordinary play, not a rare set piece.

Façades, open frames, cranes, exterior machinery, exposed structure, and long drops repeatedly place the player in direct contact with height. Interior and exterior play should reinforce each other as parts of the same tower.

---

# 4. Campaign and ascent

## 4.1 Campaign objective

The campaign has a concrete summit objective. The main ascent is complete when the player **physically reaches the summit**.

The exact narrative reason the summit matters is not defined by the current authority set and is therefore intentionally not invented here.

## 4.2 Self-directed ascent

The player is highly free to ignore the currently obvious upward objective and explore the tower.

For long stretches, the player may choose among multiple meaningful ascent opportunities, side routes, machinery opportunities, recovery paths, local objectives, and discovered situations.

The campaign provides direction without reducing the tower to a single prescribed route.

## 4.3 Common progression blocker

The most common reason upward progress is blocked is **missing physical capability or access to an appropriate tool/mechanism**.

Progress therefore comes from gaining new ways to manipulate, rig, inspect, traverse, lift, move, isolate, operate, or otherwise interact with the physical tower.

A blocker should normally be explainable in-world: the player cannot yet perform the required action, reach the required control, move the required load, establish the required connection, create the required route, or bring the right machinery into the problem.

## 4.4 Previously mastered space

Earlier regions remain part of the same world. As the player gains access and capability, movement through previously mastered vertical distance should increasingly improve through real transport, routes, machinery, or shortcuts rather than repeated compulsory traversal.

---

# 5. Opening experience

The first 10–15 minutes drop the player **directly into a real physical problem inside the functioning tower**.

There is no requirement for a long preamble before the real game begins.

The opening must communicate by play that:

- the player is physically embodied and athletic;
- the tower is a real place with usable machinery and structure;
- physical state matters;
- the player can alter that state;
- the tower extends far beyond the immediate problem;
- ascent is the durable direction.

The opening location and jammed-intake situation are specified in `02_ASCENT_ATLAS.md` band B00. Exact incident cast, dialogue, and scripted flavor beyond that physical problem remain unspecified.

---

# 6. Core gameplay loop

The recurring product loop is:

**Traverse → encounter a physical blocker/opportunity → gain or apply capability → manipulate real machinery / structure / process state → cause authoritative world change → exploit the new physical possibility → continue exploring or ascending → live with the resulting state.**

This loop can operate at multiple scales.

A small loop may involve moving a load, operating a mechanism, or creating a viable attachment.

A large loop may propagate across several systems and reshape traversal, access, inhabitant movement, or mission possibilities.

The loop is systemic rather than sequence-scripted: the important thing is the resulting world state, not whether the player followed an intended order of interactions.

---

# 7. First-person embodiment and parkour

## 7.1 Perspective

Normal traversal is **first-person**.

The perspective exists to maximize embodiment, height, machine scale, and direct physical readability. ScraperX must solve body awareness and motion comfort without abandoning first-person traversal.

## 7.2 Movement character

The parkour controller is **fast and precise**.

Player intent should feel immediate and athletic. Physics constrains what is actually reachable; sluggishness is not used as a substitute for physical credibility.

Required movement includes, where physically valid:

- walking and sprinting;
- crouching;
- jumping;
- vaulting and mantling;
- ledge grabbing and hanging;
- climbing and balancing;
- controlled dropping;
- traversing and riding moving machinery;
- moving between supports while preserving relevant momentum.

Movement assistance may improve control and reduce input friction, but it may not fabricate support, pass through invalid geometry, erase relevant momentum, or turn an impossible motion into a valid one.

## 7.3 Traversal readability

Traversable geometry communicates primarily through **world geometry and physical intuition**.

Shape, placement, clearance, motion, material context, and the player's understanding of the environment should communicate possibility. ScraperX does not rely on painting a prescribed parkour route onto the world.

## 7.4 Moving supports

Moving supports remain physically meaningful.

Jumping from a translating or rotating machine preserves relevant inherited motion. The player cannot detach from a moving mechanism and have the world silently pretend the support was stationary.

## 7.5 Fatigue

Ordinary parkour is not dominated by a universal stamina tax.

Fatigue is **situational**: long hangs, extreme climbs, injury, or exceptional exertion may matter when the situation justifies it.

---

# 8. Height, falling, parachuting, and fear

## 8.1 Falling is allowed

If the geometry permits the player to fall, the player can fall.

The game does not insert invisible safety barriers simply because a fall would be inconvenient.

A failed jump, moving support, damaged structure, open frame, exterior route, or bad recovery may genuinely send the player downward.

## 8.2 Survived falls remain real

A fall does not automatically become a reset sequence.

If the player survives on a lower structure, ledge, machine, platform, or other physically valid location, that location remains the player's real state. Recovery must come through traversal, re-entry, machinery, rescue, another route, or later failure recovery—not hidden teleportation.

## 8.3 Parachute

The player normally carries an **always-carried reusable parachute / grounded descent system**.

It is a standard recovery skill, not a guaranteed save.

Deployment uses the player's actual position, velocity, clearance, and environment. It may permit survival, redirection, or controlled descent, but it may not teleport the player, provide powered ascent, automatically return the player to the intended route, or guarantee a safe landing.

## 8.4 Fear reactions

Large falls must sound human.

The player character may yell, scream, gasp, panic, swear, plead, laugh nervously, or otherwise react according to the severity and duration of the fall.

Profanity should sound like involuntary real-world panic, not sanitized action dialogue. Variation must be broad enough that repeated falling does not expose a tiny obvious voice loop.

The humor comes from credible human terror, frustration, and relief while the physical danger remains sincere.

---

# 9. Checkpoints, failure, and committed history

## 9.1 Frequent automatic commits

Checkpoints are **frequent automatic commits**. Gaps should be short enough to encourage physical experimentation without making a bad fall or failed experiment require excessive replay.

The exact spacing is a content-tuning problem and is not fixed numerically in this GDD.

## 9.2 What a checkpoint means

A committed checkpoint records both the player's recovery location and the consequential world state needed for correct continuation.

On death or unrecoverable failure, restoration returns to that committed state. Changes after that checkpoint belong to the failed timeline and are discarded.

## 9.3 Ordinary mistakes versus hard-fail missions

The default rule is: **live with committed physical mistakes unless the player can physically recover from them.**

Mission rollback is an exception.

An objective may restore the committed checkpoint only when it is explicitly designated as a **hard-fail mission**. This exception must not become a general-purpose escape hatch from inconvenient persistent consequences.

---

# 10. Principal industrial systems

ScraperX has three principal player-manipulable macro-system families:

1. **Freight and lifting**
2. **Load-bearing architecture**
3. **Process and isolation**

They are not separate minigames. They are different faces of one industrial tower.

Cross-system coupling is **strongly region-dependent**. Some layers should become dense causal knots involving several macro systems. Other areas may focus deeply on one system or on traversal. The design must not force every problem to touch all three systems merely to prove that coupling exists.

---

# 11. Freight and lifting

Freight and lifting covers the tower's large-scale ability to move mass and people through mechanisms such as appropriate cranes, hoists, winches, carriers, counterweighted systems, forklifts, suspended loads, and related industrial equipment.

The defining requirement is not a specific machine roster. It is that these mechanisms provide **real capability** the player does not possess by hand.

They may become:

- transport;
- lifting and repositioning capability;
- route-changing machinery;
- a way to place or remove loads;
- a way to support other physical interventions;
- a contributor to larger causal chains.

Their controls and limits must correspond to real mechanism state rather than scripted animation success.

---

# 12. Load-bearing architecture

Nearly all reachable structural elements should be expected to **matter consequentially if the player manipulates them**.

This is a product expectation about world responsiveness, not a mandate that every visible member receive identical simulation cost.

Structural changes may alter:

- support;
- load paths;
- alignment;
- clearance;
- traversal;
- machine behavior;
- access;
- the usefulness or danger of nearby material.

Damage is not the primary fantasy. **Constructive manipulation comes first.** Moving, bracing, unloading, repositioning, rerouting, isolating, and recovering are more central than indiscriminate breaking.

When damage does occur, its consequential results must belong to the persistent world rather than becoming disposable spectacle.

---

# 13. Process and isolation

Process/isolation systems represent the tower's operable industrial service and process infrastructure where those systems materially affect play.

The player may alter real system connectivity, loading, isolation, access, or machine availability through appropriate controls and physical intervention.

A process system earns design complexity when changing it materially affects a player verb, hazard, route, load path, machine capability, inhabitant state, mission state, or persistent aftermath.

The game must not add process simulation merely because it sounds technically impressive.

---

# 14. Rigging and physical connection

Rigging uses a **mixed attachment model**.

Heavy or critical loads favor rated, legible attachment points. Lighter or improvised uses may allow broader attachment when geometry, compatibility, clearance, and physical conditions make the connection valid.

Rigging should be capable of supporting real uses such as repositioning, load redirection, bracing, recovery, route creation, or other physically justified interventions.

The attachment model must preserve both emergence and readability: ScraperX should not reduce rigging to one predetermined socket per puzzle, but it also should not pretend every arbitrary surface is equally suitable for critical lifting.

---

# 15. Machinery operation

Important machinery uses **richer local station controls**.

When the player takes control of a serious mechanism, ScraperX may expose explicit levers, panels, local controls, and meaningful operational sequencing rather than reducing the machine to one universal interaction button.

The touch interface must still remain workable on the target device. Richer operation means more authentic local machine verbs and sequencing, not permanent screen clutter.

The contextual Action control gets the player into a legitimate interaction. It does not solve the mechanism on the player's behalf.

Machine success must follow the authoritative machine state and physical constraints.

---

# 16. Causal stacking and Rube-Goldberg play

Large-scale causal stacking is a defining feature.

A representative chain is:

**reposition mass → alter load/alignment → change machinery or process behavior → change geometry/hazard/access → change traversal → change inhabitant or mission possibilities.**

The exact sequence is not a hidden scripted solution.

The world owns the intermediate state. If the player reaches the same valid outcome by another physically legitimate route, that result is accepted.

Causal complexity must remain legible enough that the player can form useful hypotheses from visible, audible, spatial, or operational evidence.

ScraperX should prefer mechanisms with multiple plausible uses over one-purpose puzzle devices.

---

# 17. Progression and capability

Progression is primarily **capability progression**.

The player gets higher because they can do things they could not previously do: reach machinery, operate new systems, establish different rigging, move different loads, alter new structures, traverse new geometry, gain a necessary tool, or gain access to an appropriate machine.

Portable capability may exist where established later, but the tower itself is a major source of power. Industrial-scale work should often require access to the correct machinery rather than granting the player industrial-scale strength directly.

Physical sequence breaking is valid. If the player finds a genuine way to bypass an expected blocker using authoritative mechanics, the game accepts it unless a real world constraint prevents it.

---

# 18. Missions and objectives

Objectives enter play through a **hybrid authored + discovered model**.

Important authored objectives may establish meaningful stakes and direction. At the same time, the persistent systemic world may create opportunities, problems, recovery tasks, and local goals that the player discovers through exploration and consequence.

Missions define outcomes and constraints rather than secret solution sequences.

A mission cannot fabricate a route, machine state, structural success, inhabitant movement, or other consequential fact that the world does not actually support.

No conventional combat loop is part of the ScraperX baseline. Threat and difficulty come from environment, height, machinery, hazards, physical consequence, access, people, and systemic conditions without turning the game into a shooter.

---

# 19. Inhabitants

Population density **varies strongly by tower layer**.

Industrial voids, active work zones, and more inhabited bands should not all feel equally populated.

Inhabitants are **systemic workers/operators**, not merely mission markers.

Where relevant, they can:

- travel through the real tower;
- use available access;
- operate appropriate equipment;
- evacuate from hazards;
- assist;
- respond to persistent physical changes.

Their behavior must respect actual world conditions. If access is physically lost, inhabitants cannot simply cross because a mission script wants them elsewhere.

---

# 20. Story and player personality

Story presentation is **mostly environmental and situational**.

Meaning should emerge primarily from places, people, systems, consequences, and what the player encounters while moving through the tower, with minimal interruption of embodied play.

The player character has **contextual personality** rather than constant chatter. Short remarks, frustration, observations, fear responses, and situational reactions may characterize the player without turning traversal into a continuous monologue.

The exact biography, origin, and broader fiction are not defined by the current authority set and are intentionally not invented here.

---

# 21. Tone

The dominant emotional tone outside acute falling panic is **kinetic industrial adventure**.

Discovery, daring movement, machinery, scale, physical improvisation, and upward momentum should produce excitement without undermining physical seriousness.

Height can be frightening. Machinery can be dangerous. Consequences can be harsh. Those qualities intensify the adventure rather than changing the game into a horror-first or slapstick experience.

---

# 22. Interface and touch controls

ScraperX is designed for a Galaxy Fold 6-class Android target.

Default landscape interaction remains sparse:

- **left thumb:** movement;
- **right thumb:** look;
- **contextual Action:** enter or perform a valid nearby interaction;
- **local machine controls:** appear only when meaningful machine operation requires them.

The ordinary HUD is **sparse and contextual**. Interaction, hazard, and objective information appears when relevant and recedes when it is not needed.

Traversal readability should come primarily from the world rather than permanent route overlays.

Machine-operation depth must be reconciled with touch ergonomics through context-specific controls, not by flattening machinery into fake one-button operations.

---

# 23. Persistence and world history

Consequential history persists in the committed timeline.

Strategically relevant state includes, where applicable:

- structural deformation and topology;
- connections and rigging;
- machinery and process/network state;
- useful moved or broken material;
- changed traversal;
- inhabitant state;
- mission predicates and aftermath.

Repair or recovery creates new state. It does not silently reset the world to a pristine canonical arrangement.

Persistence does not require every region to remain fully simulated at all times. Inactive state may sleep, stream, aggregate, or use lower-cost representations as long as consequential truth is preserved.

Save/load must restore the committed world sufficiently for equivalent continuation rather than quietly repairing inconvenient outcomes.

---

# 24. Presentation must expose real state

ScraperX should make physical state understandable through the world wherever possible.

Movement, alignment, load behavior, machinery response, structural change, sound, visible access, local controls, and other presentation should help the player understand what is actually happening.

Presentation may amplify real events. It may not substitute fake events for missing simulation.

The game may not claim that something bent, failed, held, moved, opened, broke, or became traversable when the authoritative state cannot support that claim.

---

# 25. Target-device and comfort constraints

The real shipping target is Galaxy Fold 6-class Android hardware with a current target of **45 FPS sustained under representative gameplay**.

Fast first-person parkour plus frequent exterior height exposure creates a real motion-comfort and accessibility burden.

That burden must be solved through camera behavior, visual stability, control response, accessibility choices, and careful presentation. It must **not** be solved by removing inherited motion, flattening height, slowing the game into unresponsiveness, or otherwise weakening the established physical traversal design.

Desktop or web success is not proof of target-device success.

---

# 26. Product anti-goals

ScraperX must not drift into any of the following as its governing identity:

- a conventional shooter;
- a gun/loot progression game;
- a sequential scripted puzzle chain;
- a simulation dashboard detached from embodiment;
- a destruction sandbox;
- a collection of isolated mechanism minigames;
- a disposable technical demo;
- a tower that is visually one building but functionally a stack of unrelated levels.

The game may contain problems, instrumentation, damage, hazards, or complex machinery. Those elements remain subordinate to athletic ascent through a persistent systemic skyscraper.

---

# 27. Technical design requirements without architecture lock-in

The later technical architecture must satisfy these product constraints:

1. **One authority per consequential fact.** Breakage, structure, machinery, process state, rigging, traversal truth, persistence, and mission-relevant outcomes cannot be double-resolved by competing systems.
2. **Finite capability.** Critical machinery obeys the relevant limits of force, torque, power, braking, travel, pressure, mass, attachment, alignment, and structural capacity.
3. **Truthful reduced models are allowed.** Numerical sophistication is not a goal by itself. A reduced model is acceptable when it is the real authoritative model and preserves the behavior ScraperX claims.
4. **No physics theater.** Scripted or cosmetic substitutes cannot impersonate consequential physical state.
5. **External technology earns its place.** Engines, solvers, middleware, packages, extensions, and tools are adopted only when they materially improve the real product and survive the actual Android shipping path.
6. **Performance optimization preserves strategic truth.** Computation may be reduced before consequential world state is discarded.

This GDD intentionally does not nominate a specific engine or solver stack.

---

# 28. Design rejection tests

A proposed feature, content direction, or implementation shortcut fails the ScraperX design if any of the following is true and the conflict cannot be resolved:

1. **It makes the tower feel like disconnected levels instead of one skyscraper.**
2. **It weakens athletic parkour into generic first-person locomotion.**
3. **It prevents physically valid falling because failure is inconvenient.**
4. **It turns parachuting into magic recovery or powered ascent.**
5. **It makes a major physical consequence happen by hidden script when the game claims it came from mechanics.**
6. **It gives presentation or mission logic authority over structural, machine, route, or process truth.**
7. **It creates industrial capability without a plausible player action, tool, machine, energy source, or physical mechanism.**
8. **It makes destruction more important than constructive manipulation.**
9. **It forces every region to combine every macro system merely for complexity theater.**
10. **It rejects a legitimate alternate solution only because the designer expected another route.**
11. **It turns serious machinery into a universal one-button interaction because touch UI is difficult.**
12. **It requires dense permanent HUD/UI to understand ordinary traversal.**
13. **It resets consequential world history merely to simplify implementation.**
14. **It creates a conventional combat loop as a primary progression system.**
15. **It adds advanced simulation, mathematics, or dependencies without buying meaningful player capability, truthful consequence, reliability, performance, or reuse.**
16. **It cannot plausibly ship and run on the actual Fold-class target without sacrificing the game's defining systems.**

If a requested feature fails one of these tests, the correct project action is to redesign or reject the feature—not to rationalize the regression.

---

# 29. GDD acceptance tests

A playable build is moving toward ScraperX rather than merely resembling it when the following are simultaneously becoming true:

- The player can traverse the tower with fast, precise first-person athletic movement grounded in real geometry and moving supports.
- Height is mechanically real: the player can fall, recover when physically possible, deploy the parachute where applicable, and suffer meaningful failure.
- The tower reads as one enormous industrial structure with frequent exterior exposure and braided upward routes.
- The player can manipulate real loads, machinery, structure, rigging, and process/isolation state rather than only activate scripted props.
- At least some interventions propagate through more than one system and create a player-visible change in access, traversal, machinery, inhabitants, or objectives.
- Constructive manipulation is as fundamental to problem solving as movement; destruction is not the dominant verb.
- Important machinery has meaningful local operation rather than animation-owned success.
- Nearly all reachable structural elements are treated as potentially consequential within the product's declared fidelity model.
- Inhabitants respond to real access and persistent changes rather than teleporting through mission logic.
- Checkpoints preserve experimentation without erasing the distinction between committed history and failed timelines.
- Save/load preserves meaningful aftermath.
- The game remains playable with sparse Fold-first touch controls and sustains the target performance on the actual device path.

A build that has attractive scenery, parkour animations, machinery models, or destruction effects but cannot satisfy these systemic conditions is not yet proving the ScraperX game.

---

# 30. Intentionally unspecified in this GDD

The following remain open because they are not required to define the product, or they belong to the atlas / TDD rather than this document:

- the exact narrative reason the summit matters;
- the player's detailed biography;
- a complete mission catalog;
- exact population counts per layer;
- exact checkpoint spacing in meters/minutes;
- exact manual lift/push mass thresholds;
- portable-tool roster beyond the atlas capability table;
- exact structural/material numerical models;
- exact engine, physics backend, solver stack, or package set (TDD);
- exact implementation-language and module boundaries (TDD);
- final art-production pipeline and audio implementation stack.

These are specified in `02_ASCENT_ATLAS.md` and must not be re-invented by a work order:

- opening location and jammed-intake problem;
- vertical datum, band cuts, braid identities, Transfer Plates;
- named modules and kernel IDs used by WO-001–008;
- physical summit predicate on the 1600 m deck.

Downstream work must use those atlas facts rather than author a second tower.

---

# 31. North star

> **ScraperX is a kinetic first-person industrial ascent through one enormous persistent skyscraper: move like an athlete, understand and operate life-size machinery, physically change structure and systems, survive height and consequence, exploit the resulting causal chains, and reach the summit.**

Every major addition should make that statement more physically true.
