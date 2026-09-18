# SCRAPERX — WORK ORDER 005 — FIRST FREIGHT MECHANISM

**Work Order:** `WO-005`  
**Status:** READY after WO-004 completion  
**Depends on:** WO-004 fall/chute/checkpoint proven in kernel

## Objective

One real machine moves one real load under finite limits.

Atlas kernel: `KX-JIB` actuates; `KX-CRATE` is the load; `CAP-PENDANT` / local station issues commands.

Machine success is native mechanism state, not animation.

## Existing truth

Player can move, ride, climb short geometry, fall, chute, commit. No freight authority until this WO.

## Authority

- Laws 11, 12, 22, 24, 26, 27
- GDD §§11, 15
- `01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md` §§4, 9 (`KX-JIB`, `KX-CRATE`, `KX-BELT` may remain as support)
- TDD §§9, 19.1 item 2

## Owner

Native freight/machinery. Jolt (or the chosen native rigid substrate) owns contact of hook/load/deck. Actuator code requests force/torque/constraint targets; it does not write a successful final crate transform.

## Allowed seam

One mechanism definition: members, joint, actuator channels, SWL/torque/power/travel/brake design targets, hook attachment interface. Godot exposes Raise/Lower/Slew/Brake only while the player is at the pendant/station.

## Required causal path

`Action to enter station → Drive/Raise/Lower/Brake commands → finite actuator effort → load pose/contact changes → player can ride or step on resulting geometry if physically valid`

## Forbidden shortcuts

- AnimationPlayer moves the crate
- unlimited winch force
- teleport load to “solved” pose on button
- Godot RigidBody as the machine authority while native watches
- Action as one-button “solve hoist”
- skipping attachment compatibility (`CAP-HOOK5` may be pre-placed on the crate for this WO, but the hook must still be a real constraint)

## Implementation scope

- `KX-JIB` finite machine
- `KX-CRATE` consequential body
- station command mapping
- native tests: stall against overweight / brake holds / travel limit stops
- diagnostic presentation of load/hook in Godot

## Out of scope

Needle seating, process, full B00 intake belt as a puzzle, richer multi-panel cab (a compact pendant is enough), NPCs.

## Proof path

1. Native: command stream lifts crate within SWL; overweight or locked brake does not produce free work.
2. Godot: player at pendant sees the same motion as native snapshot.
3. Player may ride the crate if support is valid (WO-002 law applies).
4. Checkpoint from WO-004 still serializes crate/jib state or explicitly records that serialization waits for WO-008 — do not claim persist if not implemented. Prefer implementing machine state in the existing commit blob if the seam is already there.

## Completion

- The crate moves because `KX-JIB` did bounded work.
- Controls are local and non-magical.
- Limits are observable (stall, stop, hold).

Stop. Do not seat the needle in this WO unless required to prove the hook; seating as structural traversal belongs to WO-006.
