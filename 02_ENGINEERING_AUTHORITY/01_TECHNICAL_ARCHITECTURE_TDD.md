# SCRAPERX — TECHNICAL ARCHITECTURE / TDD v1.0

**Status:** Technical baseline  
**Product / repository:** `ScraperX`  
**Product authority:** `01_PRODUCT_AUTHORITY/00_GOVERNING_LAWS.md` + `01_SCRAPERX_GDD.md` + `02_ASCENT_ATLAS.md`  
**Engineering authority above this document:** `00_EXECUTION_PROTOCOL.md`  
**Target:** Galaxy Fold 6-class Android, sustained 45 FPS under representative play  
**Purpose:** Define ownership, runtime boundaries, data flow, persistence, build delivery, and verification strongly enough that bounded implementation work can begin without redefining the game.

---

## 0. Technical thesis

ScraperX uses **one authoritative native simulation** for every consequential physical fact and uses Godot as the embodiment/input/content/presentation host around that authority.

The baseline stack is:

- **Godot 4.7.x** for rendering, input, scene/content authoring, audio, UI, Android application/export integration, and presentation.
- **Godot Mobile renderer** as the primary renderer for the Android target.
- **Official `godot-cpp` GDExtension bindings** targeting the Godot 4.7 API for the native boundary.
- **Portable C++17 `scraperx_sim`** as the owner of consequential simulation state.
- **Jolt Physics linked directly into `scraperx_sim`** as the baseline commodity rigid-body / collision / contact substrate for consequential objects.
- ScraperX-owned reduced-order solvers layered beside Jolt where the GDD requires phenomena that commodity rigid-body dynamics does not truthfully represent: structural state, rigging/cable state, process/isolation networks, persistence predicates, and other explicitly justified domains.

This is deliberately **not** "Godot decides physics and C++ watches." Godot sends requests and renders authoritative snapshots.

It is also deliberately **not** "custom-sim everything." Jolt buys mature broadphase/narrowphase, rigid bodies, constraints, CCD, sleeping, multithreaded solving, and an Android-capable C++ substrate. ScraperX custom code exists only where the game requires semantics or physical state Jolt does not own.

---

## 1. Technology decisions

### 1.1 SELECTED — Godot 4.7.x

Godot 4.7.x is the application/runtime host.

Reasons:

- official Android export path;
- official C++ GDExtension bindings;
- Mobile Vulkan renderer appropriate to the shipping target;
- mature scene/content/UI/audio workflow;
- no requirement to maintain a custom engine fork for the initial architecture.

A custom Godot engine module is **rejected initially**. It increases build/rebase cost without yet buying a required capability that GDExtension cannot provide.

### 1.2 SELECTED — GDExtension boundary

The native simulator is loaded through a C++ GDExtension.

The Godot-facing extension must stay thin. It owns:

- lifecycle;
- input-command marshaling;
- content-descriptor registration;
- fixed-step invocation;
- authoritative snapshot access;
- event extraction;
- save/load requests;
- diagnostics/profiling surfaces.

It must not become a second gameplay simulation.

### 1.3 SELECTED — direct Jolt inside native authority

Critical rigid bodies and consequential collision/contact use **Jolt linked directly into the native simulator**, not Godot `RigidBody3D` nodes as their authority.

Why this boundary matters:

- Jolt supports Android ARM64 and is a standalone C++ library.
- Jolt exposes rigid-body/contact/constraint functionality directly to the owner that also holds ScraperX structural, machinery, rigging, and persistence state.
- This avoids a hidden authority split where Godot owns transforms/contact while the native simulator independently owns breakage, machine success, or persistent consequence.

Godot's built-in Jolt integration may still be used for **strictly nonconsequential presentation physics** such as cosmetic fragments. Those objects must live in isolated collision layers and must never apply gameplay-relevant impulses or state changes to native-authoritative objects.

**Promotion rule:** if a "cosmetic" object later needs to support, obstruct, damage, carry, actuate, or persist strategically, it stops being cosmetic and moves into native authority.

### 1.4 SELECTED — CMake for native build

Use one CMake graph for:

- `scraperx_sim`;
- direct Jolt dependency;
- `godot-cpp`;
- the ScraperX GDExtension shared library;
- native host-side tests/tools.

`godot-cpp` officially supports CMake as a secondary build system and can target API version 4.7. Jolt is natively CMake-friendly. One graph reduces duplicated build semantics.

Godot project export remains a separate Godot/Gradle delivery stage.

### 1.5 SELECTED — C++17 baseline

C++17 is the native language baseline because both Jolt and current `godot-cpp` support it directly.

Do not increase the language requirement merely for convenience unless a concrete capability justifies doing so.

---

## 2. Authority map

| Consequential state | Runtime owner | Godot role |
|---|---|---|
| Player physical pose/velocity/support state | `scraperx_sim` | input + visual/camera representation |
| Critical rigid bodies/contact/impulses | native Jolt in `scraperx_sim` | render mirrors |
| Freight/lifting mechanism state | `scraperx_sim` | controls + presentation |
| Structural members/connections/deformation/failure | `scraperx_sim` | mesh/collision presentation generated from snapshots |
| Rigging connections/tension/breakage | `scraperx_sim` | interaction UI + rendered cable/attachments |
| Process/isolation network state | `scraperx_sim` | gauges/audio/VFX/control surfaces |
| Route/traversal predicates | `scraperx_sim` | presentation/navigation hints only |
| Persistent NPC consequential state/access | `scraperx_sim` | animation/voice/rendering |
| Mission predicates/outcomes | `scraperx_sim` | mission presentation |
| Checkpoint/save state | `scraperx_sim` | request + user feedback |
| Cosmetic debris/VFX | Godot allowed | no consequential feedback |
| Audio, UI, camera polish | Godot | presentation only |

A state not listed here does not acquire authority by accident. Add it deliberately before implementing it.

---

## 3. Runtime topology

```text
Android / Godot 4.7.x
│
├── Input + touch
├── Renderer / audio / UI / animation
├── Authored scene/content descriptors
│
└── ScraperX GDExtension bridge
      │
      ├── CommandQueue  ─────────────►
      │
      ├── Content registration ──────►   scraperx_sim (portable C++)
      │                                │
      ◄──────── SnapshotBuffer ────────┤
      ◄──────── EventBuffer ───────────┤
                                       │
                                       ├── Jolt rigid/contact world
                                       ├── Player / parkour controller
                                       ├── Freight & machinery
                                       ├── Structural state
                                       ├── Rigging
                                       ├── Process/isolation
                                       ├── NPC consequential state
                                       ├── Mission predicates
                                       └── Persistence/checkpoints
```

The boundary is command/snapshot based.

Godot does not directly mutate authoritative simulation structs.

Native code does not directly manipulate arbitrary Godot scene nodes from the solver.

---

## 4. Time and stepping

### 4.1 Fixed authoritative timestep

Consequential simulation uses a fixed timestep.

**Initial implementation target: 90 Hz (11.111… ms).**

Why 90 Hz is the starting point:

- fast first-person parkour benefits from tighter support/contact updates than a low-frequency simulation;
- the shipping render target is 45 FPS, giving an exact 2:1 simulation-to-render target relationship;
- it provides a clear performance discriminator early.

This is a **benchmark-gated technical value**, not product law.

The 90 Hz rate may be revised only through an explicit TDD change after actual Fold profiling demonstrates that the target is unsustainable despite correcting identifiable implementation cost. Do not casually reduce physics frequency to hide unrelated performance defects.

### 4.2 Step ordering

At each authoritative tick:

1. consume timestamped player/NPC/world commands in stable order;
2. update machine actuator/control requests;
3. advance process/isolation state that is due on this tick;
4. update structural/rigging pre-coupling state;
5. prepare/apply forces, constraints, kinematic targets, and body changes to Jolt;
6. advance Jolt;
7. read canonical contacts/body state needed by higher-level owners;
8. solve/update post-contact structural/rigging consequences;
9. evaluate traversal/access predicates;
10. update consequential NPC/mission state;
11. emit authoritative events;
12. publish immutable render/query snapshot;
13. commit checkpoint only if a checkpoint request is valid at this tick boundary.

No gameplay writes occur between these phases from presentation code.

### 4.3 Threading

**Initial rule: one authoritative step coordinator.**

Jolt may use its own worker jobs internally, but ScraperX domain owners are advanced under one deterministic coordinator rather than independent asynchronous simulation loops.

Do not create separate free-running structure/process/rigging threads before profiling proves a need. Independent wall-clock solvers create synchronization boundaries and causal ambiguity.

If subsystems later parallelize, they must preserve the same tick dependency graph.

---

## 5. Determinism and reproducibility

ScraperX needs reproducible causes, debugging, checkpoint integrity, and reliable tests. It does **not** currently require multiplayer lockstep or bit-identical desktop↔Android replay.

Therefore:

- same-build, same-command-stream deterministic replay is required for test scenes;
- authoritative commands receive stable tick numbers and stable entity IDs;
- unordered callback/event sources are canonicalized before consequential consumption;
- contact/listener events that can arrive from worker threads are buffered and sorted by stable keys before higher-level logic uses them;
- simulation randomness, where deliberately allowed, comes from explicit seeded streams owned by the relevant system.

Jolt's optional cross-platform deterministic mode is **not enabled by default** because it has a documented performance cost and ScraperX has no current product requirement for cross-platform lockstep. Revisit only if a real requirement appears.

---

## 6. Spatial model and precision

### 6.1 Units

Native simulation uses SI units:

- meters;
- kilograms;
- seconds;
- radians;
- Newtons;
- Newton-meters;
- Pascals where relevant.

Content import must convert once at the boundary rather than scatter unit conversions through simulation code.

### 6.2 Precision

Use ordinary 32-bit simulation positions initially.

At 1600 m, IEEE-754 float32 spacing is approximately **0.000122 m (0.122 mm)**. That is far below the spatial precision ScraperX requires for human-scale traversal and industrial mechanisms.

Therefore:

- **no floating-origin system initially**;
- **no double-precision Godot build initially**;
- no coordinate rebasing complexity until runtime evidence proves precision is a problem.

If the world later extends far beyond the current tower scale, revisit with evidence.

---

## 7. Stable identity and state layout

Every persistent/consequential object receives a stable `EntityId`.

Requirements:

- stable across save/load;
- independent of Godot node instance IDs;
- deterministic content-created IDs for authored objects;
- generation-safe runtime IDs for spawned consequential objects;
- never use raw pointers as persistent references.

Authoring data is immutable after load wherever possible.

Runtime mutable state is owned by the relevant native subsystem.

Do not mandate ECS, SoA, or object-oriented layouts globally. Use data-oriented layouts only where profiling shows hot-path benefit. Architectural fashion is not a requirement.

---

## 8. Player / parkour architecture

### 8.1 Owner

Player physical state is native-authoritative.

Godot's `CharacterBody3D` is not the player authority.

### 8.2 Collision substrate

The ScraperX player controller uses Jolt collision/narrowphase support, but **ScraperX owns the movement controller logic**.

Jolt `CharacterVirtual` is useful reference/prototyping material, but it is not accepted as final authority merely because it exists. Current Jolt issue history includes moving-platform/dynamic-body instability cases, which intersect directly with a ScraperX constitutional requirement.

Before delegating major controller behavior to `CharacterVirtual`, it must pass ScraperX's own moving-support suite.

### 8.3 Required state

At minimum the native controller owns:

- position/orientation basis;
- linear velocity;
- grounded/support state;
- support body/subshape ID;
- support contact point;
- support point linear velocity;
- stance;
- climb/hang/mantle state;
- parachute state;
- fall state/severity state.

### 8.4 Moving support law

Support velocity is sampled at the actual contact point:

`v_support_point = v_linear + ω × r`

Grounded locomotion is relative to that support.

Detachment/jump preserves relevant inherited support-point velocity.

No Godot parenting trick is allowed to substitute for this.

### 8.5 Traversal assistance

Mantle/vault/ledge assistance may search for candidate geometry and execute a constrained trajectory only when:

- collision/support geometry exists;
- reach/clearance rules pass;
- the target state is physically valid;
- the operation does not teleport through blockers;
- relevant platform motion is included.

The assist is a controller, not a route flag.

---

## 9. Freight, lifting, and machinery

Machinery is represented as explicit mechanisms, not animation states.

A machine definition contains:

- rigid members/bodies;
- joints/constraints;
- actuator/control channels;
- finite force/torque/power/travel limits;
- braking/holding behavior where relevant;
- attachment interfaces;
- machine-local sensors/outputs;
- persistent failure/damage state.

Actuator logic requests forces/torques/constraint targets. It cannot directly write a successful final transform.

Machine controls in Godot generate commands such as `Drive`, `Brake`, `Raise`, `Lower`, `Traverse`, `Extend`, `Retract`, `Tension`, or `Release`. Native machinery decides the result.

---

## 10. Load-bearing structure

### 10.1 Representation

Do **not** turn every decorative beam into a finite element.

Consequential structural assemblies use a native reduced-order structural graph:

- nodes / connection frames;
- load-bearing members;
- connection state;
- section/material parameters;
- elastic state;
- persistent plastic/rest-state changes;
- failure/topology state.

The representation must support the GDD-required consequences: load redistribution, bending/torsion where gameplay depends on them, persistent deformation, connection change, and useful failed material.

### 10.2 Coupling to Jolt

Critical structural masses/contact shapes exist in Jolt where rigid-body/contact response matters.

The structural owner computes constitutive/internal state and determines when topology/rest state changes.

Jolt resolves rigid-body/contact motion.

Neither side independently decides the same breakage event.

### 10.3 Solver choice gate

The exact member formulation is **not frozen yet**.

Candidates such as corotational beam formulations are acceptable only after a benchmark scene demonstrates:

- correct qualitative load redistribution;
- stable large rotations;
- required bending/torsional response;
- persistent deformation;
- mobile cost compatible with active-region budgets.

Full general-purpose FEM across the tower is rejected unless later evidence demonstrates a requirement that reduced structural models cannot satisfy.

---

## 11. Rigging and cable systems

Rigging is native-authoritative.

Required concepts:

- typed attachment points;
- endpoint compatibility;
- cable/slack length;
- tension;
- winch payout/take-up;
- finite strength;
- breakage;
- persistent connections;
- endpoint force transfer to machinery/structure.

The initial cable solver is **benchmark-gated**.

XPBD/compliant constraint methods are a strong candidate because they can model compliant constraints with force estimates while controlling timestep/iteration sensitivity, but ScraperX does not adopt a solver family merely because it is mathematically attractive.

The first rigging benchmark must compare at least:

- Jolt constraint-chain approach;
- ScraperX-owned XPBD cable representation;

against tension fidelity, moving-anchor behavior, winching, breakage, CPU cost, and persistence complexity.

Arbitrary rope self-contact/wrapping is not automatically required by the GDD and must not be smuggled in as solver scope.

---

## 12. Process and isolation networks

Process/isolation uses a reduced graph/lumped-state model rather than particle fluids.

A network contains:

- volumes/nodes;
- connections/edges;
- valve/isolation state;
- pressure/inventory/temperature variables where gameplay requires them;
- source/sink/machine coupling;
- structural loads/hazards derived from network state.

Process updates occur on integer divisors of the authoritative tick according to required dynamics. Fast transients may update every tick; slow networks need not.

A process system earns state variables only when they affect actual machinery, force, hazard, traversal, mission predicates, or persistence.

---

## 13. NPCs, traversal, and missions

### 13.1 NPC consequential state

Native simulation owns persistent NPC identity and consequential state:

- location/region or authoritative physical body when active;
- goal/task;
- access eligibility;
- injury/death/rescue state if later established;
- machine/work dependencies;
- mission-relevant predicates.

Godot owns animation, voice, facial/presentation behavior.

### 13.2 Traversal truth

The native world exposes traversal predicates derived from current geometry/support/hazard state.

Godot navigation meshes may assist local pathfinding in stable regions, but nav data never overrides authoritative route truth.

If a physical route ceases to exist, presentation/navigation must adapt.

### 13.3 Missions

Mission evaluation is read-only with respect to physical truth.

Mission logic may:

- observe predicates;
- record objective state;
- request explicitly authored hard-fail rollback.

Mission logic may not create a route, machine success, structural break, or process condition merely by setting a flag.

---

## 14. Checkpoints and persistence

### 14.1 Commit boundary

Checkpoint commits occur only at authoritative tick boundaries.

A commit captures all state required for equivalent continuation, including:

- stable entity existence/topology;
- transforms/velocities of consequential bodies;
- machine/control state;
- structural rest/plastic/connection state;
- rigging state;
- process network state;
- NPC consequential state;
- mission predicates/outcomes;
- player state;
- content/chunk activation metadata needed for reconstruction.

Transient solver caches/contact manifolds need not be serialized if equivalent valid state can be reconstructed deterministically enough without them.

### 14.2 Format

Persistence uses a versioned schema with:

- magic/version;
- build/content compatibility metadata;
- chunked subsystem sections;
- explicit lengths/checksums;
- stable IDs, never raw pointers;
- unknown-section skip capability where practical.

The exact serialization library/encoding is **not selected yet**. Do not add a dependency until the first real state schema exists.

### 14.3 Atomicity

Checkpoint write is atomic from the game's perspective:

1. snapshot immutable authoritative state;
2. serialize off the critical gameplay path where safe;
3. write temporary file;
4. validate checksum/header;
5. replace previous commit atomically.

A partial save may never replace a valid checkpoint.

---

## 15. Streaming, sleep, and reactivation

The tower is partitioned into authored runtime regions/chunks with stable IDs.

Chunk size is not frozen until content/benchmark evidence exists.

Each chunk has:

- immutable content descriptor;
- compact persistent mutable state;
- optional active native bodies/solvers;
- presentation instances when visible/relevant.

### 15.1 Sleep eligibility

A region may reduce computation only when doing so preserves consequential truth.

It must remain active if, for example:

- a cross-region active constraint/load path depends on it;
- a moving machine spans the boundary;
- strategically relevant motion is still evolving;
- an unresolved process transient must continue;
- active NPC behavior requires the region.

Sleep cannot delete stored energy, topology, obstruction, support, or other state needed later.

### 15.2 Reactivation

Reactivation rebuilds runtime bodies/solvers from persistent state, then validates invariants before exposing the region as active.

If reconstruction would change a strategically relevant consequence, the reduced representation is insufficient and must be strengthened.

---

## 16. Godot presentation architecture

Godot owns:

- touch/input interpretation;
- camera presentation;
- mesh/scene instancing;
- animation;
- audio;
- particles/VFX;
- UI/HUD;
- authored interaction prompts;
- content editing.

Critical scene objects are **mirrors**, not authorities.

A critical Godot node stores its stable `EntityId` and renders from snapshots.

No script may write a critical node transform and thereby change simulation truth.

### 16.1 Snapshot model

Native publishes immutable double-buffered snapshots containing only presentation/query data needed by Godot:

- transforms;
- velocities needed for effects;
- deformation parameters or render proxies;
- machine indicators;
- attachment/rigging geometry;
- player support/fall/parachute state;
- events.

Do not copy entire native world state to Godot every frame.

---

## 17. Content authoring pipeline

Godot scenes are the content-authoring surface.

Authored modules contain:

- render geometry;
- stable authored IDs;
- simulation descriptors;
- collision/shape references;
- machinery definitions;
- structural definitions;
- process network definitions;
- attachment/control points;
- region/chunk metadata.

A validation/export step converts scene metadata into a runtime content descriptor consumed by `scraperx_sim`.

The same authored source must generate both presentation placement and simulation descriptors so they cannot drift independently.

Invalid modules fail validation before packaging.

Examples of validation:

- duplicate stable IDs;
- missing attachment references;
- impossible machine joint graph;
- unsupported collision shape;
- process edge references missing nodes;
- structural connection references missing members;
- critical render object lacking simulation descriptor.

---

## 18. Rendering and mobile constraints

Primary renderer: **Godot Mobile renderer / Vulkan**.

Forward+ is not the baseline shipping renderer.

Rendering priorities:

1. stable frame pacing;
2. readable geometry/support/machinery;
3. tower scale and exterior visibility;
4. character/machine motion readability;
5. secondary visual richness.

When over budget, reduce GPU presentation cost before weakening consequential simulation truth.

LOD, occlusion, shadow distance/count, reflection cost, particle density, transparency, and distant detail are presentation levers.

Do not convert critical collision/structure to fake low-detail state merely because the corresponding render mesh uses LOD.

---

## 19. Performance and profiling

Shipping target: **45 FPS sustained** on Fold 6-class hardware during representative play.

At 45 FPS the frame interval is 22.22 ms.

The TDD does not invent detailed CPU/GPU budgets before representative content exists. Instead it requires:

- per-authoritative-step native CPU timing;
- Jolt timing;
- structural/rigging/process timing;
- Godot main-thread timing;
- render-thread/GPU timing;
- active body/constraint/member counts;
- memory high-water marks;
- thermal-duration tests.

Performance regressions are measured against repeatable benchmark scenes, not empty maps.

### 19.1 Required benchmark scenes

Before broad content production, maintain:

1. **Parkour support benchmark** — static, translating, rotating, accelerating supports.
2. **Freight benchmark** — heavy load + hoist/winch/brake + player riding/transitioning.
3. **Structure benchmark** — load redistribution, deformation, connection failure, useful aftermath.
4. **Rigging benchmark** — tension, winch, moving anchors, breakage.
5. **Process coupling benchmark** — isolation/pressure state affecting a real mechanism or hazard.
6. **Persistence benchmark** — damage/motion/state → commit → reload → equivalent continuation.
7. **Integrated causal benchmark** — at least three macro domains coupled in one chain.

These are engineering fixtures, not disposable fake gameplay.

---

## 20. Build and Android delivery

### 20.1 Repository build graph

Native build:

```text
CMake
├── third_party/jolt          (pinned exact tag/commit)
├── third_party/godot-cpp     (pinned exact tag/commit; API target 4.7)
├── src/sim                   (portable authority)
├── src/bridge                (thin GDExtension)
└── tests                     (native)
```

Godot project:

```text
godot/
├── project.godot
├── addons/scraperx_native/
├── content/
├── presentation/
├── ui/
└── export_presets.cfg
```

The final repository layout may vary modestly, but authority boundaries must not.

### 20.2 Android ABI

Shipping baseline: **arm64-v8a**.

Do not build every ABI by default if the actual target does not require it.

### 20.3 Remote automation

CI must be capable of:

1. checking out the repo including pinned dependencies;
2. running native host tests;
3. cross-compiling native GDExtension for Android arm64;
4. assembling the Godot Android export;
5. producing a real installable APK artifact;
6. publishing build logs and checksums.

The user must be able to trigger/retrieve this without owning a desktop development machine.

An APK artifact proves only "built." Installation/execution/Fold behavior remain separate evidence states.

---

## 21. Verification strategy

### 21.1 Native tests

Host-side native tests cover:

- stable IDs;
- command ordering;
- save/load schemas;
- machine limit invariants;
- support-point velocity transfer;
- structural topology transitions;
- rigging conservation/strength rules;
- process graph invariants;
- mission predicate purity.

### 21.2 Scenario tests

Deterministic test scenes replay command streams and compare authoritative state predicates, not screenshots.

Where floating-point exact equality is inappropriate, compare physically meaningful tolerances/invariants.

### 21.3 Android smoke path

Every merge that changes native/runtime integration should eventually prove:

- APK builds;
- APK installs;
- app boots;
- native extension loads;
- authoritative sim advances;
- one known interaction produces expected state.

### 21.4 Fold acceptance

Claims involving controls, frame rate, thermals, motion comfort, touch ergonomics, or sustained simulation are not accepted without actual Fold-class observation.

---

## 22. Dependency policy

### Approved baseline dependencies

- Godot 4.7.x.
- `godot-cpp` targeting API 4.7.
- Jolt Physics direct native integration.

All are pinned by exact version/tag/commit in source control when implementation begins.

### Candidate / benchmark-gated

- XPBD cable implementation or library substrate.
- Structural math helper libraries if a demonstrated formulation benefits.
- Serialization library, only after schema needs are concrete.
- Android platform plugins only when a real platform API requires them.

### Rejected by default

- second consequential rigid-body world;
- runtime procedural tower generation merely because modules exist;
- full-tower general FEM;
- particle fluids for ordinary hydraulics/process state;
- floating origin for the current 1.6 km scale;
- broad plugin accumulation;
- engine fork without a proven GDExtension blocker.

---

## 23. Technical gates still intentionally open

These are **not forgotten decisions**. They require evidence before freezing:

1. exact Jolt release/commit;
2. Jolt worker-thread configuration on Fold;
3. whether 90 Hz survives representative device benchmarks;
4. exact structural member formulation;
5. cable/rigging solver selection;
6. persistence binary encoding/library;
7. region/chunk dimensions and active-radius policy;
8. detailed NPC planning representation;
9. detailed process solver equations/update rates;
10. any need for Android-specific plugin code beyond ordinary export.

A coding model may not pick one silently because it wants to proceed.

---

## 24. First implementation sequence

Do not begin with a giant tower, structural solver, or content production.

The playable geometry for WO-001–008 is the atlas **kernel slice** (`02_ASCENT_ATLAS.md` §9), not bands B01–B11.

### Work Order 000 — delivery spine
File: `03_WORK_ORDERS/WO-000_DELIVERY_SPINE.md`

Prove:

`Godot 4.7 app → native GDExtension loads → authoritative fixed-step sim runs → Android arm64 APK produced`

No fake game systems.

### Work Order 001 — embodied authority
File: `03_WORK_ORDERS/WO-001_EMBODIED_AUTHORITY.md`

`touch/desktop test input → native player state → static-world collision/support → Godot render mirror`

No climbing system yet. World: `KX-DECK`.

### Work Order 002 — moving-support truth
File: `03_WORK_ORDERS/WO-002_MOVING_SUPPORT_TRUTH.md`

Add:

- translating support (`KX-BELT`);
- rotating support;
- support-point velocity;
- inherited momentum on detach/jump.

This is a constitutional discriminator. Do not advance if it is fake or unstable.

### Work Order 003 — athletic traversal
File: `03_WORK_ORDERS/WO-003_ATHLETIC_TRAVERSAL.md`

Add bounded mantle/vault/ledge/hang primitives on real geometry, preserving moving-support behavior.

### Work Order 004 — fall / parachute / checkpoint
File: `03_WORK_ORDERS/WO-004_FALL_PARACHUTE_CHECKPOINT.md`

Prove real fall, survivable parachute dynamics, checkpoint commit, death rollback, and surviving lower-level fall continuation. Refuge: `KX-REFUGE`.

### Work Order 005 — first freight mechanism
File: `03_WORK_ORDERS/WO-005_FIRST_FREIGHT.md`

One real machine with finite power/force/travel/brake behavior and a real movable load: `KX-JIB` + `KX-CRATE`.

### Work Order 006 — first structural coupling
File: `03_WORK_ORDERS/WO-006_FIRST_STRUCTURAL_COUPLING.md`

The machine/load changes an actual structural state that changes geometry/support/traversal: `KX-NEEDLE` in `KX-POCKETS`.

### Work Order 007 — first process coupling
File: `03_WORK_ORDERS/WO-007_FIRST_PROCESS_COUPLING.md`

A process/isolation state changes the same physical situation or machine capability: `KX-SUMP` → `KX-GRATE`.

### Work Order 008 — first full causal chain
File: `03_WORK_ORDERS/WO-008_FIRST_CAUSAL_CHAIN.md`

Demonstrate one complete chain on the kernel:

`player intervention → freight/load change → structural/process consequence → changed traversal/world capability → persistent checkpoint/reload`

This is the first point at which ScraperX has proven its defining architecture.

Do not scale content before this slice is real. After it is real, assemble from atlas band B00 upward using the same primitives. Do not author a second tower.

---

## 25. Architecture rejection tests

Reject a technical proposal if it:

- makes a Godot node authoritative for consequential physics;
- duplicates rigid/contact authority between Godot and native Jolt;
- treats animation as machine state;
- allows mission logic to manufacture physical predicates;
- serializes only mission flags while losing world state;
- requires runtime tower procedural generation not demanded by the GDD;
- adopts a complex solver without a benchmarked gameplay need;
- forces a desktop-only build workflow;
- uses a web build as Android proof;
- lowers simulation quality before identifying actual performance cost;
- creates free-running asynchronous subsystem clocks;
- uses presentation physics to affect critical world state;
- makes save/load reconstruct a prettier but mechanically different world;
- authors a second tower, sidecar atlas, or parallel work-order pack instead of amending this tree.

---

## 26. Completion condition for this TDD

This TDD is sufficient to begin bounded implementation when:

- product authority remains unchanged;
- every consequential domain has one owner;
- the Godot/native boundary is explicit;
- the commodity physics role is explicit;
- persistence and streaming ownership are explicit;
- Android delivery has an actual architecture;
- unresolved numerical choices are named as gates rather than guessed;
- Work Order 000 can be written without inventing another architecture.

That condition is satisfied by this v1.0 baseline.
