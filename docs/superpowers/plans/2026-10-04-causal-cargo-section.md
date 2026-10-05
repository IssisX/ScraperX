# Causal cargo-net section implementation plan

> **For agentic workers:** REQUIRED MODULE: Open the installed Superpowers package root SKILL.md, then read skills/executing-plans/MODULE.md. Root owns implementation; children gather bounded evidence only.

**Goal:** Convert the complete grade → cargo-net → receiver → first tower-ring section to physical consequences, retaining the static megastructure.

**Architecture:** Native Jolt remains authoritative. Extend the finite hand owner with a reciprocal material-coordinate soft-net adapter; retain gravity and actual momentum. Replace the receiver transfer with force-driven contact, then replace fixed gameplay-frame support with compliant anchored load paths. Godot reads the same native parts, mesh and hands.

**Tech Stack:** C++17, pinned Jolt e77f175595e64cb44218cc9d9d56fc365ad0e36a, Godot 4.7.

**Spec:** This document records the user-selected causal conversion contract and section boundary.

## Global constraints

- Static exceptions: grade foundation, concrete ground anchor pads and tower ring/columns (entity11). The gantry, fascia, receiving deck and guardrails are gameplay structure, not an exception.
- Entry: ordinary grade spawn/walk. Exit: ordinary supported walking on the first ring, entity11 at11m, zero deaths and no slingshot work.
- Every interaction uses actual native positions, velocities, mass, contact and material coordinates. No gravity-off climb, root-velocity trajectory, timed landing success, fake load or hidden force source.
- Hand muscles supply bounded work; passive attachments supply reaction and dissipate energy. Render reads never change physics.
- Preserve existing launcher, accurate hand rendering, retired water-lift fixtures and concurrent work.
- A passing build is not traversal proof; minimum checks are finite-force/momentum falsifiers, the three real cargo routes and affected rigid-hand regressions, followed by required exact-source CI/APK delivery when the full section is ready.

## Task1: reciprocal flexible hand load

**Files:** src/sim/physical_hand_climb.hpp/.cpp, src/sim/cargo_net.hpp/.cpp, src/sim/simulation.cpp, tests/cargo_net_tests.cpp.
**Interfaces:** retain rigid-hand APIs; add optional CargoNet material adapter and pre_step(dt). Soft-point impulse is distributed with the same interpolation weights as point position/velocity; effective inverse mass is sum(w²/m).

- [x] Extend the existing cargo-route acceptance to reject disabled player gravity or missing real finite hands.
- [x] Observe the baseline rejection, using only the cargo target.
- [x] Bind actual material grips and current player offsets without changing instantaneous velocities. Resolve damped elastic hand forces at each native collision substep; apply equal opposite impulses, bounded1500N per hand. Move rest targets within the existing shared3000W conservative command budget. Preserve extensions on regrip.
- [x] Remove the soft-climb velocity driver and surrogate weight load from this path; test cargo routes and retained rigid hands.

## Task2: physical receiving transfer and departures

**Files:** src/sim/simulation.cpp, tests/cargo_net_tests.cpp.
**Interfaces:** actual finite hand targets, receiver ledge/contact queries and native support sample.

- [x] Reject gravity-off/velocity-prescribed top-out and momentum-reset departure in the section acceptance path.
- [x] Transfer real grips to the reachable receiver lip; bounded muscles raise/advance the rider until actual foot contact supports them. Release only on physical contact or explicit player departure. No timer determines success.
- [x] Jump applies a bounded reciprocal push; Drop only detaches. Run the same end-to-end routes and departure falsifiers.

## Task3: anchored gameplay structure

**Files:** src/sim/cargo_net_route.hpp/.cpp, src/sim/cargo_net.hpp/.cpp, src/sim/mechanism_kit.hpp/.cpp, src/sim/simulation.cpp, tests/cargo_net_tests.cpp.
**Interfaces:** kit body identity/parts remain native collision/drawing authority; material attachments connect the net to actual frame points and transfer reaction.

- [x] Demonstrate that load/impulse reaches the frame and receiver and cannot remain on fixed world anchors.
- [x] Give the gantry/receiver finite mass and passive compliant structural attachments to the concrete foundation/backbone. Retain net tension and safe supported travel; route success follows solver contact. Specify chosen stiffness/damping/mass and derive static deflections before implementing.
- [x] Account for net-anchor reactions through actual reciprocal attachment forces, not positional pin updates. Run affected route/structure falsifiers.

## Task4: integrate and deliver

**Files:** 03_EXECUTION/ASCENT/AS-024_CARGO_NET_ENTRY.md, current continuation documents, scoped evidence receipts and relevant generated native solids tables.

- [x] Update authoritative section inventory, force laws, chosen/derived numbers and actual proof boundaries alongside the code.
- [ ] Run the existing ordinary shipping-touch route on the rebuilt native library and affected display/solids checks only; update geometry expectations only for intentional native changes.
- [ ] Publish the complete section onChatGPT, finish exact-source green Actions and verify/copy the resulting APK. Keep the goal active until all gameplay-object categories and complete traversal are proven.

**2026-10-04 native checkpoint:** all three ordinary lanes X=19/20/21 reach supported Tower11 at+11m, zero deaths/no launcher work, with gravity1 for every climb/receiver-transfer tick. The physical-hand target passes, including actual soft recoil/momentum and extension-preserving regrip. Gantry/anchor/departure conversion and rebuilt shipping runtime/delivery remain unfinished. The requested net stance is0.55m (CHOSEN) versus0.35m capsule radius; finite hands acquire it without a pose write.

## Structural implementation parameters (2026-10-04 candidate)

| Quantity | Type | Definition |
|---|---|---|
| Steel density | CHOSEN |7850kg/m³ |
| Gantry hollow-member wall | CHOSEN |8mm; thin guardrails4mm |
| Stiffened deck equivalent thickness | CHOSEN |16mm |
| Fascia equivalent thickness | CHOSEN |8mm |
| Entry tread equivalent thickness | CHOSEN |10mm; finite separate body2954 |
| Frame mass/COM/inertia | DERIVED |sum of explicit native part masses and shape mass properties; body2952, welded rigid approximation |
| Foot mounts | CHOSEN |each3MN/m,180kNs/m,5MNm/rad,300kNms/rad; two constant-neutral passive SixDOF spring mounts to concrete body1952 |
| Net knot-corner mass | CHOSEN |0.06kg for ALL828 vertices (no world-pinned end rows) |
| Net clips | CHOSEN |each5000N/m,25Ns/m Kelvin-Voigt;72 end vertices attach to actual gantry material points |
| Clip reactions | INTEGRATED |equal/opposite impulses at the same world point; damping uses actual frame point velocity, preserving reciprocal linear/angular reaction |
| Rider hand muscles | CHOSEN |5000N/m,60Ns/m soft adapter;1500N vector bound per hand and shared3000W conservative command debit |
| Native integration | DEFAULT/INTEGRATED |90Hz outer tick, four Jolt collision substeps in ordinary Slingshot world; one serial listener resolves clips, hands, then unchanged launcher constitutive callback |

Concrete footings and grade are fixed foundation; the entry plate is not exempted. Clips have visible steel backs at both net ends and a visible lower tie rail. The fixed-neutral mount targets never move and supply no drive. Stored elastic extension, damping, gravity and actual contact determine frame response. Jolt soft-state and Kit body checkpoints preserve the same load path on restore.

## Departure and traction design (2026-10-04)

Selected-section Jump uses the existing reciprocal hand motors for a CHOSEN0.20s muscle stroke. Its target direction is away/up in the ratio2.5:4; the shared3000W conservative command debit bounds the entire stroke at600J, with1500N/hand and gravity retained. The duration defines the voluntary muscle command, not traversal success or a prescribed velocity. Drop removes the grips immediately and preserves momentum. Receiver ownership rebases hold coordinates; actual grip velocities supply the release reference.

Ordinary walking on bodies2952/2954 uses the existing native contact-point recovery force owner continuously: actual slip, finite traction and positive-power bound, with equal/opposite support force. The contact solver carries weight and resolves the low tread step; selected supports never use the positional step-up controller. Ground Jump uses a reciprocal contact impulse with a CHOSEN finite positive-work bound of0.5*85*5.5²=1285.625J, including actual support translation and rotation. Selected-body vault/mantle entry and lip hang handoffs must retain finite hands rather than legacy velocity trajectories.

Latest evidence: the finite-frame three-lane route passed; adding Action during the actual receiver transfer then rejected the old mantle's disabled gravity. The passive-release setup now waits (bounded) for real upward motion rather than assuming it after exactly90ticks; all three original release surfaces pass this setup with their original momentum tolerance.

**Integrated native checkpoint:** frame mass6562.78kg, clip/frame momentum residual0.00302Ns, top anchor displacement61.46mm following actual frame impulse. All three routes retain gravity1 and survive repeated receiving Action; command-bounded Jump and force-driven tread walking pass. Passive walk-off and final native launcher seam regression are being checked after the release guards were closed. Shipping renderer resource cache required importing the newly recorded footsteps; no audio source change was made.

**Owner pace correction:** deliver this section, then reuse the force/contact/attachment owners in subsequent complete sections. Do not turn a section into an expanding verification or polish project. The original1,000+m ambition remains; this selected first section is one systematic conversion, not a redefinition of the overall structure.
