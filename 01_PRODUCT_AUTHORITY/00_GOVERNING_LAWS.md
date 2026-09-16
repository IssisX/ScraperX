# SCRAPERX — GOVERNING LAWS v1.0

**Status:** FROZEN BASELINE  
**Repository:** `ScraperX`  
**Game/app title:** `ScraperX`  
**Purpose:** Define the non-negotiable design and engineering laws that all later requirements, GDD decisions, architecture, implementation, tools, content, and changes must obey.

## 1. CLEAN-ROOM AUTHORITY

`ScraperX` begins clean.

Legacy project history, old repositories, branches, builds, architecture, historical proper nouns, implementation assumptions, compatibility obligations, and absorbed context do not belong in the repository or authoritative design documents unless a requirement is explicitly re-established on its present merits.

Past experience may inform judgment. Historical baggage may not become authority.

## 2. ONE SKYSCRAPER, ONE PERSISTENT WORLD

The game takes place within, upon, around, beneath, and progressively upward through one enormous inhabited industrial skyscraper approximately **1.6 km tall**.

It is one physically coherent place.

Streaming, sleeping, aggregation, variable simulation fidelity, and inactive-region representations are allowed.

Disconnected reset arenas, disposable levels, and disguised level-select structures are not.

## 3. ASCENT IS THE CAMPAIGN

The dominant progression is upward.

The player reaches higher regions by gaining physical access, knowledge, machinery, tools, infrastructure control, traversal capability, and changed world capability.

Progression should increasingly improve movement through already-mastered regions through physically earned transport, routes, shortcuts, infrastructure, or machinery rather than forcing repetitive backtracking.

## 4. ATHLETIC INDUSTRIAL PARKOUR IS FOUNDATIONAL

Movement includes, where physically appropriate: **walking, sprinting, crouching, jumping, vaulting, mantling, ledge-grabbing, hanging, climbing, balancing, dropping, traversing moving machinery, riding moving supports, and other physically justified traversal.**

Traversal derives from actual geometry, support, contact, clearance, reachable surfaces, and momentum.

Assistance may improve responsiveness. It may not fabricate support, teleport the player, erase meaningful motion, bypass geometry, or convert invalid traversal into valid traversal.

## 5. MOVING SUPPORTS PRESERVE MOTION

Moving machinery and structures remain physically meaningful during traversal.

Jumping from a moving platform preserves relevant inherited velocity. Rotating or translating supports impart appropriate motion.

Player movement cannot silently cancel the mechanical consequences of the structure beneath them.

## 6. HEIGHT AND FALLING ARE REAL GAMEPLAY

Where geometry permits, the player can fall.

Invisible safety walls may not be inserted simply because the fall would be inconvenient.

Large drops, open shafts, exterior traversal, machinery, damaged structure, moving platforms, and extreme altitude remain genuine hazards.

Height should retain emotional weight.

## 7. PARACHUTING PRESERVES CONSEQUENCE

When physically applicable, the player may deploy a parachute or equivalent grounded descent system during a fall.

Deployment uses the player's actual position, velocity, clearance, and surrounding environment.

It may permit survival, redirection, and controlled descent.

It may not guarantee survival, cancel arbitrary momentum, teleport the player, automatically restore the intended route, or provide powered ascent.

## 8. FALLING MUST SOUND HUMAN

Significant falls produce context-sensitive human fear responses.

These may include panic, shouting, screaming, gasping, pleading, nervous laughter, and natural-sounding profanity.

Response intensity must reflect the actual severity and duration of the fall.

Variation must be sufficient to avoid obvious canned repetition.

Humor should emerge from believable human fear, frustration, and relief rather than cartoon slapstick.

## 9. CHECKPOINTS COMMIT AUTHORITATIVE STATE

A reached checkpoint records both the player's recovery position and the consequential world state necessary for correct continuation.

Death or unrecoverable failure restores that committed state.

Changes made after the checkpoint belong to the failed timeline and are discarded.

Persistent history therefore means the committed history of the surviving playthrough, not every abandoned failure branch.

## 10. REAL FALLS MAY CREATE REAL RECOVERY PROBLEMS

A survived fall may leave the player on a lower structure, ledge, machine, or region.

That resulting position remains real.

The player must continue through legitimate traversal, re-entry, rescue, machinery, another route, or checkpoint restoration when failure becomes unrecoverable.

Hidden teleportation and semantic route repair are prohibited.

## 11. THREE PRINCIPAL INDUSTRIAL SYSTEMS

The principal player-manipulable macro systems are **Freight and lifting**, **Load-bearing architecture**, and **Process and isolation**.

They must function as one coupled industrial machine rather than isolated minigames.

Supporting systems such as electrical, thermal, hydraulic, fire, ventilation, water, signaling, or other infrastructure earn complexity only when they materially alter a player verb, a load path, a route, a hazard, machine capability, mission state, or persistent aftermath.

## 12. THE PLAYER PHYSICALLY CHANGES THE TOWER

The central gameplay is not merely navigating machinery.

The player changes what the skyscraper can physically do.

Useful interventions may include moving mass, lifting, lowering, bracing, cutting, tensioning, unloading, rerigging, isolating, venting, rerouting, repositioning, supporting, repairing, deliberately damaging, or repurposing machinery and structure.

## 13. CAUSAL STACKING DEFINES SYSTEMIC PLAY

Important interventions should propagate through authoritative systems.

A representative chain is: **mass → load/alignment → machinery/process/power → geometry/hazard → traversal → inhabitants → mission possibilities**.

The exact chain is not scripted merely to produce spectacle.

The world state creates the intermediate consequences.

## 14. EMERGENCE MUST REMAIN LEGIBLE

Complexity may produce unexpected outcomes. It may not become arbitrary chaos.

Important causal transitions must remain grounded in inspectable mechanisms and observable evidence so the player can form useful hypotheses about what happened and why.

## 15. RANDOMNESS MAY NOT REPLACE MECHANISM

Randomness may enrich deliberately stochastic phenomena, presentation, and variation.

It may not arbitrarily decide consequential facts such as structural failure, machine success, route validity, topology, mission truth, or other deterministic physical outcomes when those outcomes should follow from authoritative state.

## 16. MISSIONS DESCRIBE OUTCOMES, NOT HIDDEN SOLUTIONS

Missions define desired outcomes and constraints.

They do not prescribe secret sequences that substitute for physical causality.

If a route is required, a real route must exist. If machinery must function, the actual machinery must function. If people must escape, they must physically reach an appropriate region.

Mission flags may record consequences. They may not fabricate them.

## 17. PHYSICAL SEQUENCE BREAKING IS VALID PLAY

If the player legitimately reaches, traverses, manipulates, or exploits something through the authoritative mechanics, the game accepts that result.

An intended progression path may not be protected with invisible blockers or fake mission gates merely because the player discovered another physically valid solution.

Actual structural, social, infrastructure, access, or equipment constraints remain legitimate.

## 18. HISTORY IS GAMEPLAY

Consequential state persists in the committed timeline.

This includes, where strategically relevant, deformation, topology, structural connections, rigging, machinery, network state, useful debris, inhabitants, traversal changes, mission predicates, and persistent aftermath.

Repair creates new state rather than silently restoring a pristine canonical world.

## 19. PERSISTENCE DOES NOT REQUIRE CONTINUOUS COMPUTATION

Strategically relevant state may sleep, stream, aggregate, reduce update frequency, or use lower-cost inactive representations.

Optimization may reduce computation.

It may not erase strategically meaningful truth or prevent later reactivation from reconstructing equivalent consequences.

## 20. SAVE/LOAD PRESERVES CONSEQUENTIAL TRUTH

Restoring committed state must reconstruct everything required for equivalent continuation.

That includes all strategically relevant topology, deformation, connections, rigging, machinery, networks, motion/history where necessary, inhabitants, traversal consequences, and mission predicates.

Loading may not secretly repair or reset inconvenient physical state.

## 21. ENTRAPMENT IS WORLD STATE

The player or inhabitants may become genuinely isolated because of physical consequences.

This is not automatically a navigation bug.

Recovery must come from legitimate intervention, rescue, alternate traversal, machinery, or checkpoint restoration.

No hidden teleportation. No fake route flags. No automatic geometry correction merely because the result is inconvenient.

## 22. ONE AUTHORITY OWNS EACH CONSEQUENTIAL FACT

Every consequential state has one authoritative owner.

There may not be competing truths for breakage, structural state, machinery state, rigging, topology, route validity, process state, persistence, or mission outcomes.

Presentation may request actions and display authoritative results. It may not independently resolve consequential outcomes.

## 23. AUTHORITY BOUNDARIES PRECEDE INTEGRATION

Before any physics engine, external package, specialized solver, or engine subsystem participates in consequential behavior, its owned state and synchronization boundary must be explicitly defined.

Two systems may cooperate. They may not independently resolve the same consequential fact.

## 24. MACHINERY CANNOT CREATE CAPABILITY FROM NOTHING

Critical machinery respects relevant finite limits including mass, force, torque, power, stored energy, braking, travel, attachment, pressure, alignment, and structural capacity.

A controller cannot create unlimited force.

A simplified model may reduce computation. It may not produce impossible free work or unlimited actuation.

## 25. REDUCED-ORDER PHYSICS CAN BE REAL PHYSICS

Maximum numerical complexity is not inherently superior.

A reduced model is legitimate when: (1) it is the authoritative representation of the phenomenon, (2) it preserves the quantities and predicates required by gameplay, and (3) it is validated for the behaviors claimed.

Fake state and cosmetic proxies presented as consequential physics remain forbidden.

## 26. NO PHYSICS THEATER

The following may not substitute for real consequential state: scripted collapse presented as emergent, cosmetic deformation over unchanged relevant collision, health scalars replacing required mechanical state, teleporting loads, unlimited-force actuators, animation-owned machine success, mission flags replacing physical predicates, fake telemetry, useful-debris deletion to hide simulation cost, solver failure interpreted as structural collapse, or presentation becoming a hidden second simulation authority.

The game may not claim a phenomenon that its actual representation cannot express.

## 27. CONTEXTUAL ACTION IS A GATEWAY, NOT A SOLVE BUTTON

The default interaction scheme should remain sparse and mobile-friendly.

A contextual Action input may select or engage nearby valid interactions.

It may not collapse meaningful machinery operation into a magical universal command.

Once operating equipment, the interface exposes the smallest sufficient set of understandable physical verbs needed for that mechanism.

## 28. GALAXY FOLD 6-CLASS ANDROID IS THE SHIPPING TRUTH

Controls, rendering, simulation budgets, memory, streaming, thermals, build architecture, and interface design target sustained real-device play.

The current performance target is **45 FPS sustained under representative gameplay**.

Default landscape interaction favors **left thumb → movement**, **right thumb → look**, **contextual Action → interaction**, with compact machine-specific controls only when needed.

## 29. DELIVERY CLAIMS MUST REMAIN DISTINCT

These are separate facts: **implemented**, **built**, **APK produced**, **APK installed**, **executed**, **observed working**, **observed working correctly on Fold 6**.

One may not be substituted for another.

Proxy evidence cannot prove actual device behavior.

## 30. EXTERNAL TECHNOLOGY MUST EARN ITS PLACE

`ScraperX` should aggressively use mature engines, packages, extensions, libraries, solvers, middleware, build systems, asset tooling, profiling tools, and other external technology when evidence shows material value.

Every dependency must earn its existence through improved capability, correctness, robustness, performance, tooling, interoperability, development leverage, or maintainability.

No technology is adopted merely because it is fashionable, advanced-sounding, or available.

Jolt or any alternative is a candidate, not dogma.

## 31. DEPENDENCIES MUST SURVIVE THE ACTUAL SHIPPING PATH

A dependency is not proven merely because it works on a desktop.

Critical technologies must survive the real Android pipeline and be evaluated for compilation, integration, runtime behavior, memory, performance, thermal impact, stability, maintenance risk, licensing, and authority compatibility.

Desktop brilliance that prevents the Android game from shipping is rejected.

## 32. THE GAME MAY NOT DRIFT INTO A LESSER GENRE

The project must not quietly mutate into a shooter, a sequential puzzle game, a simulation dashboard, a destruction sandbox, a disconnected tech demo, or a collection of isolated mechanism minigames.

Combat, puzzles, instrumentation, destruction, and experimentation may exist.

They remain subordinate to the governing game.

## 33. COMPLEXITY MUST PURCHASE PLAYER VALUE

Advanced mathematics, physics, AI, procedural systems, simulation, tools, and architecture must materially purchase one or more of a new player verb, a new causal relationship, more truthful consequences, better reliability, greater control, better performance, reusable capability, or richer world behavior.

Given equal outcomes, prefer fewer authorities, synchronization boundaries, mutable states, and moving parts.

## 34. THE GOVERNING GAME OUTRANKS INDIVIDUAL INSTRUCTIONS

No later instruction—including one from Cory—automatically overrides these laws.

If a requested change would materially weaken skyscraper identity, ascent, athletic traversal, physical causality, persistent consequence, authority integrity, inhabited-world behavior, mobile viability, or the core systemic game, the damaging mechanism must be challenged.

Preserve the legitimate objective where possible. Reject the part that harms the game.

## 35. CHANGE GATE

A consequential proposed change must answer:

**What player capability does this create?**

**Which authoritative state does it read or modify?**

**How does it couple to existing systems?**

**What player-visible consequence proves it exists?**

**How will the actual Android/Fold path verify it?**

If those questions cannot be answered materially, the change is not ready.

## 36. NORTH STAR

> **Climb an enormous industrial skyscraper by mastering athletic movement and learning how its interconnected machinery, structure, and infrastructure work; physically change what the tower can do, survive the consequences, and use those changes to reach higher.**

When two directions conflict, prefer the one that makes that statement more physically true.
