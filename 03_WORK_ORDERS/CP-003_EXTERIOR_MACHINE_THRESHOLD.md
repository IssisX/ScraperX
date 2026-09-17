# CP-003 — EXTERIOR MACHINE THRESHOLD

**Status:** implementation checkpoint  
**Branch:** `ScraperX-2`  
**Authority:** subordinate to Governing Laws, frozen GDD, and Technical Architecture/TDD.

## Objective

Land one playable checkpoint that proves three contiguous claims without creating a second physics owner:

`Fold-open composition → exterior grade approach → native industrial interaction that moves real matter`

This is deliberately broader than a single thin work order. It does **not** claim that TDD Work Orders 003, 004, or 005 are complete.

## Preserved authority

- `scraperx_sim` remains authoritative for player pose/velocity/support, moving supports, machine state, gate motion, load motion, and interaction eligibility.
- Jolt remains the only consequential rigid/contact world.
- Godot owns camera, HUD, rendering, authored presentation, touch interpretation, lighting, fog, and VFX mirrors.
- Existing WO-001 and WO-002 fixed-step, collision/support, support-point velocity, and inherited-momentum behavior must remain green.

## Slice A — Fold-open composition

Target the Galaxy Z Fold6 unfolded inner-screen aspect (`2160×1856`, 20.9:18) as the primary composition.

Required:
- near-square design surface rather than fixed 16:9 phone-strip composition;
- `expand` stretch behavior so available bounds are used;
- HUD anchors relative to viewport edges/center, not hard-coded 1600×900 coordinates;
- materially wider first-person FOV on near-square viewports;
- camera far plane sufficient to read tower mass;
- Android sensor-landscape orientation;
- no claim of actual Fold acceptance until installed and observed on Fold hardware.

## Slice B — exterior grade approach

Default native spawn moves to outdoor grade a measured distance from the tower. The player must be able to move toward a physically present lower tower threshold.

Presentation must communicate:
- wet asphalt / poured concrete / oxidized and mill-scale steel;
- sourced amber loading-bay illumination rather than neon edge emission;
- a large exterior tower mass and structural exoskeleton;
- high cloud / stack plume that interrupts the visible upper tower so the crown cannot be cleanly read;
- exterior loading-bay machinery integrated into the approach.

The atmospheric plume/cloud is presentation only in this checkpoint. It is forbidden to describe it as authoritative process-fluid simulation.

## Slice C — first matter-moving industrial interaction

Implement a loading-bay hopper release in `scraperx_sim`.

Native elements:
- stable hopper gate entity;
- stable dynamic load entity;
- stable static receiving chute;
- stable native control point;
- native proximity/eligibility predicate;
- one-shot release command;
- finite gate travel at finite speed;
- Jolt gravity/contact moves the released load into/down the chute.

Godot may only mirror gate/load state and expose an Action control when native eligibility says the interaction is valid.

### Rejection tests

Reject the checkpoint if:
- the load moves because a Godot node is animated;
- the control works from arbitrary distance;
- the load position is scripted after release;
- the viewport still assumes 1600×900 fixed HUD coordinates;
- neon/emissive edge language remains the dominant palette;
- WO-005 is marked complete despite lacking its full finite power/force/travel/brake contract;
- existing moving-support momentum tests regress.

## Proof

Host/native:
- existing fixed-step and invalid-input invariants;
- static support locomotion;
- translating support point velocity + inherited jump momentum;
- rotating support `omega × r` point velocity;
- exterior default spawn on grade;
- remote hopper release rejected;
- in-range hopper release accepted once;
- gate moves materially to finite endpoint;
- dynamic load departs its initial support by >1 m through Jolt state.

Godot runtime:
- extension loads;
- near-square Fold-layout predicate is true;
- CI avatar traverses >12 m of the exterior approach;
- native in-range interaction is accepted;
- gate-open and load-moved predicates are observed;
- screenshot is captured from the real current-source runtime.

Delivery:
- Android arm64 native library cross-compiles;
- APK contains the arm64 GDExtension;
- checkpoint SHA, APK name, and checksum are published.

## Explicitly not closed here

- WO-003 mantle/vault/ledge/hang;
- WO-004 parachute/checkpoint rollback;
- complete WO-005 freight mechanism with finite power/force/brake model;
- structural coupling;
- process/isolation solver;
- authoritative steam/fluid dynamics;
- actual Fold6 install/runtime/performance/thermal acceptance.
