# Work Order 013 — First Process Coupling (KX-SUMP / KX-GRATE)

**Work Order:** `WO-013`
**Status:** COMPLETE. Landed in `32b2f13`; record written retroactively on 2026-09-21.
**Depends on:** `WO-012_FIRST_STRUCTURAL_COUPLING.md`

> **Record note.** The code and the falsifier for this work order shipped in `32b2f13` on
> 2026-09-18, but the work-order file was never written. That was a real gap: `00_START_HERE.md`
> listed a `WO-013` slice with no document behind it. This file closes the record from the
> commit, the source, and observed test output. Nothing here is reconstructed from memory.

## Objective

Prove the third kernel tool type: a **process** state — not a structure and not a freight load —
that decides whether a piece of geometry is walkable support.

Ascent Atlas §9 names `KX-SUMP` (a lumped volume with one isolation edge and one drain sink) and
`KX-GRATE` (the walkway over it). The coupling to prove: while the sump holds inventory the grate
is a hazard; drained, it is ordinary support; refilled, it is a hazard again. The state must be
carried by the physics body, not by a render flag.

## Existing proven truth

`WO-011` (`KX-JIB` moves `KX-CRATE`) and `WO-012` (a seated `KX-NEEDLE` changes traversal) were
green before this slice started. Station gating — commands accepted anywhere, effective only
inside an XZ station radius — was already proven by `WO-011`'s pendant.

## Governing authority

- Governing Laws 2–7, 11–18, 21–27, 29, 32
- GDD process/FLOW sections
- Ascent Atlas §9 (kernel module IDs), §13
- TDD §§6, 8–11, 14: native 90 Hz Jolt owns contact; Godot mirrors

## Owner

Native 90 Hz C++/Jolt simulation. Godot presents authoritative state. HUD observes predicates only.

## Allowed seam

Reuse the kernel primitives already proven: kinematic support bodies, contact-ranked support
identity, the station-gating pattern, checkpoint capture/restore. New: a scalar inventory
integrated per tick, and a body whose *collision response* is driven by that inventory.

## Design decisions (stated, not left implicit)

1. **The grate is a Jolt sensor when wet.** `BodyInterface::SetIsSensor(id, !grate_safe)`. Jolt's
   own contract: "A sensor will receive collision callbacks, but will not cause any collision
   responses." So a wet grate is geometrically present and reports contacts, but the player falls
   through it. This is a physical property of the body, not a mesh swap and not a flag.
2. **The sensor flag is written every tick, not on edges.** `update_sump` recomputes
   `grate_safe = sump_volume_kg_ <= 0.0F` and sets the flag unconditionally. Consequence: restore
   from a checkpoint needs no topology reconciliation for the grate — the next tick re-derives it
   from the restored inventory. This is why `restore_from_checkpoint` has no grate-specific path.
3. **The valve is station-gated like the jib pendant.** `request_valve_toggle()` is accepted
   anywhere; it is a no-op unless the player is inside `kSumpStationRadius = 2.5 m` of
   (`kSumpStationX`, `kSumpStationZ`). Walking away mid-process does not un-toggle the valve, but
   it does stop the player from operating it.
4. **Inventory is a lumped scalar, not a fluid solver.** `kSumpCapacityKg = 1000`,
   `kSumpInflowKgPerSec = 150` while the valve is open, `kSumpDrainKgPerSec = 100` while isolated.
   Design target per Atlas §0.3, not a measured specification.

## Forbidden shortcuts (checked against)

- a `sump_drained` mission flag deciding traversal
- swapping the grate mesh without changing collision
- deleting or re-adding the grate body to fake state
- an invisible wall in place of the wet hazard
- letting the valve work from anywhere on the map

## Proof path

One deterministic native falsifier, `tests/simulation_tests.cpp`, asserting each link separately:

1. the valve toggle is a no-op off-station
2. wet, the grate is not crossable
3. isolating drains the volume to safe
4. drained, the grate is the reported support entity while the player is over it
5. drained, the player reaches the far deck
6. reopening the valve makes it a hazard again

Item 6 runs on a dedicated `Simulation` instance, because by the time the player has crossed they
are off-station and the toggle is correctly gated to a no-op — the first attempt at this check
failed for exactly that reason.

## Completion

- wet grate is impassable by geometry, not by a flag
- drained grate reports as `kSumpGrateEntityId` support under the player over open volume
- the far deck is reachable only after draining
- reopening restores the hazard
- kernel falsifiers `WO-000`–`WO-012` remain green

## Result record

- **Changed:** `src/sim/simulation.{hpp,cpp}` (`kSumpGrateEntityId` = 34; `InitialSpawn::KernelSumpStation` = 17; sump geometry/rate constants; `build_kernel_sump`; `update_sump`; `Simulation::request_valve_toggle`; sump fields in `MachineCheckpoint` / `commit_machine_checkpoint` / `restore_from_checkpoint` / `read_machine_state`; **`observe_support` now rejects any sensor body**); `src/bridge/scraperx_simulation.{hpp,cpp}` (`request_valve_toggle`, `is_sump_station_active`, `is_sump_isolated`, `get_sump_volume_kg`, `is_grate_safe`; spawn count 17→18); `tests/simulation_tests.cpp` (the WO-013 falsifier); `godot/presentation/main.gd` (`_build_kernel_sump`, render mirror, `Sump` HUD line); `godot/main.tscn` (new `Sump` HUD label).
- **Built:** host and bridge configurations, Release, GCC 13.3, `-Wall -Wextra -Wpedantic -Werror`, clean.
- **Executed:** `./build/host/scraperx_sim_tests`.
- **Observed:** `PASS scraperx_sim first process coupling: gated_off_station=1 wet_crossable=0 drained_safe=1 support_on_grate=1 reached_far_deck=1 dump_unsafe_again=1`. Re-confirmed 2026-09-21 on this branch's current HEAD: exit 0, 9 of 9 `PASS`.
- **Defect found and fixed by this slice:** `observe_support` accepted a sensor body as valid support. A Jolt sensor still produces a full contact manifold, so a wet grate would have read as solid ground from geometry alone — the coupling would have appeared to work while being physically false. Sensors are now never valid support (`src/sim/simulation.cpp`, `observe_support`). This was a real correctness bug in the shared support path, not specific to the sump.
- **Unverified boundary:** interactive desktop/Fold operation of the valve control; Android install/execution and Fold 6 observation remain unverified (no device access). The 1000 kg capacity and the 150/100 kg·s⁻¹ rates are design targets per Atlas §0.3, chosen for coherent cycle times, not derived from a real specification. An end-to-end **restore** falsifier is not exercised for the same reason recorded in `WO-012` — no lethal-height drop is reachable by walking from the kernel geometry — but unlike the needle, the grate needs none: its collision response is re-derived from restored inventory on the very next tick (design decision 2 above), so there is no grate topology to reconcile.
- **Regressions:** none observed.
