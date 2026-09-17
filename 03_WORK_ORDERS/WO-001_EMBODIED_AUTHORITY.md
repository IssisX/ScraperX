# SCRAPERX — WORK ORDER 001 — EMBODIED AUTHORITY

**Status:** READY

## Objective
Put the player inside a native-authoritative physical body on one static industrial tower deck.

This work order proves locomotion and support truth only. It is the first embodied slice of the skyscraper, not a substitute for its traversal or large-scale machinery.

## Existing truth
- The current source builds a portable native simulation and Godot 4.7 GDExtension.
- One native clock owns authoritative simulation at 90 Hz.
- Godot can read native snapshots and present them.
- The current-source graph builds on Linux and Android arm64, and exports an APK in CI.
- No player body, world collision, or support state exists yet.

## Authority
- Governing Laws: one native authority; causal truth over presentation; no duplicate physics authority.
- GDD: one persistent 1.6 km skyscraper; first-person athletic industrial parkour; large-scale Rube-Goldberg mechanisms; mobile-first controls.
- Execution Protocol: implement the smallest complete causal slice and stop at its completion boundary.
- TDD sections 4, 6, 7, 8, 10, 20, 21, and 24, including the prescribed WO-001 sequence.

## Owner
`scraperx_sim` owns player position, velocity, collision response, grounded truth, and support identity.

## Allowed seam
A command/snapshot extension on the existing `ScraperXSimulation` GDExtension bridge.

## Required causal path
`touch/desktop move intent → native player command → native 90 Hz Jolt body integration → static-world contact/support → immutable snapshot → Godot first-person camera and telemetry`

## Forbidden shortcuts
- No `CharacterBody3D`, Godot collision body, or second gameplay physics world.
- No Godot-authored position, velocity, grounded flag, or support identity.
- No transform animation presented as physical movement.
- No render-frame-dependent acceleration or movement gain.
- No climbing, vaulting, parachute, checkpoint, moving supports, freight, structure, or process simulation in this work order.
- No claim of Android installation, Android execution, Fold 6 observation, touch ergonomics, or sustained performance without direct evidence.

## Implementation scope
- Native Jolt world with one static support deck and one translation-only player capsule.
- Bounded desired-velocity locomotion integrated on the native fixed step.
- Native contact-derived grounded/support state.
- Player command and immutable snapshot fields through the thin GDExtension bridge.
- First-person Godot presentation of a recognizably vertical industrial tower-deck slice.
- Desktop and touch move input routed to the same native command.
- Host tests, Linux runtime capture, Android arm64 build, APK export, and bounded CI evidence.

## Out of scope
- Athletic traversal state machine and climbing probes.
- Moving-platform inheritance.
- Fall/parachute/checkpoint loop.
- Freight, load-bearing structure, and process/isolation systems.
- Destruction, persistence, networking, missions, NPCs, and final art.

## Proof path
1. Source inspection proves Godot is presentation/input only and the player body lives in `scraperx_sim`.
2. Native tests prove falling onto a static Jolt support, grounded/support truth, locomotion, frame-partition invariance, and invalid-input rejection.
3. The Linux GDExtension build runs in Godot and captures a first-person tower-deck frame after native movement and support are observed.
4. The same native graph cross-compiles for Android arm64.
5. Godot exports a current-source Android arm64 APK containing the native library.
6. Installation, device execution, Fold 6 behavior, and touch ergonomics remain unverified unless separately observed.

## Completion
This work order is complete when:
- a native Jolt capsule falls under gravity and is supported by the native static deck;
- desktop and touch paths both submit desired movement through the bridge;
- native tests prove support and locomotion remain fixed-step-owned;
- Godot mirrors native pose/support in a first-person industrial tower scene without gameplay physics nodes;
- CI produces host-test, runtime, screenshot, Android native-library, and APK evidence;
- the result record preserves every unverified boundary.

Stop. Do not begin climbing or moving-support work inside WO-001.

## Result record
- Changed: pending.
- Built: pending.
- Executed: pending.
- Observed: pending.
- Unverified boundary: Android install/runtime, Fold 6 observation, touch ergonomics, sustained frame rate, and every WO-002+ system.
- Regressions: pending.
