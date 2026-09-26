# SCRAPERX — AS-004 NEEDLE SEAT

**Ascent Slice:** `AS-004`
**Lifecycle:** PLANNED; no campaign needle/cage implementation.
**Provenance:** source audit at `7e66eb6`, 2026-09-26; obsolete executable geometry withdrawn.
**Implementation gate:** BLOCKED on the mechanical closure below; optional, not the next ascent requirement.
**Evidence:** `00_START_HERE.md` §7. Historical `8f9e954` revalidation established the free tower stair and missing approach/support geometry; it did not run this machine.
**Write branch:** `ChatGPT`.

## Objective and authority

Use real lifting/rigging to seat `MOD-NEEDLE-A/B` in the 96 m pockets, restoring a
load path for an optional cage landing toward TP-120. Laws 4–5, 9, 17, 22, 24–26;
GDD §§7, 9, 11–17; Atlas B01/K1. Preserve the existing tower stair to 154 m, as
chosen by the owner on 2026-09-24. These machines must earn useful transport,
structural access or carrying capability; their necessity cannot be manufactured
by removing the stair.

## Existing truth, owner and allowed seam

AS-002 produces a hall surface at +40.1872 m. AS-003 produces a physical hook block
at grade, not automatic delivery to this hall. Native `simulation.cpp` owns the
existing carry, finite drives, attachment and support systems. Kernel seatable
members are precedent, not this campaign machine already implemented. Godot must
render the owning native geometry/state; no duplicate success state.

## Mechanical close — receiver first

| Backward requirement | Required physical predecessor | Resolution / gate |
|---|---|---|
| AS-005 accepts a stable loaded cage and a safe exit toward TP-120 | Real aligned guides, a finite brake/catch and a continuous receiving landing | BLOCKED: AS-005 must close this interface jointly before either ticket promises a completed ride |
| Guide reactions reach the tower | Seated needles, pockets, columns/bracing and guide attachments carry actual loads | BLOCKED: define a reduced structural law and reaction/load limits; endpoint counts alone are not guide alignment |
| Needles remain seated under cage/rider load | Compatible seat faces, retention and bounded seating error | BLOCKED: old 0.18 m capture tolerance cannot establish a 0.02 m guide tolerance without finite corrective work |
| Needle reaches both pockets at 96 m | Stable suspended pose plus controllable horizontal alignment | BLOCKED: one vertical hoist cannot independently correct lateral pickup/pocket alignment |
| Load hangs from the hoist | Real hook, sling lengths, attachment coordinates and tension limits | BLOCKED: a 0.6 m block cannot bridge the old 2.3 m offset, nor a ≥5 m hook-to-padeye separation with ≤0.7/0.8 m attachment reaches |
| Player can rig and operate | Supported service approach from +40.1872 to the rack at 48 m and reachable controls | BLOCKED: hall alone did not reach the racks; no free-floating service platform |
| Stored loads and drum are supported before manipulation | Rack at 48 m, pockets at 96 m, drum/head near 121 m tied into the real frame | BLOCKED: old table supplied heights without their load-bearing connections |

### Design quantities retained, with their actual meaning

Atlas freezes pocket spacing 18 m and elevation 96 m. Proposed needle mass 1600 kg,
cage tare 900 kg, hoist force 32000 N are **design targets**, not source constants.
The known hook is 36 kg; rider is 85 kg. With additional sling/rigging mass `m_r`:

- One-needle lift mass = `900 + 1600 + 85 + 36 + m_r = 2621 + m_r kg`.
- Static demand = `25712.01 + 9.81 m_r N`; nominal force reserve at `m_r=0`
  is 6287.99 N, before acceleration, friction or guide reactions.
- A physically rigged two-needle load is `4221 + m_r kg`, requiring at least
  `41408.01 + 9.81 m_r N`; it exceeds the proposed drive. A one-hook arrangement
  does not acquire a second load merely because a test assigns twice the mass.
- Required input power is at least `F v / η` for a specified efficiency and speed;
  brake rating must hold the full load on release/off-station, not merely set motor
  command to zero. Motor/hoist energy source and thermal/duty limits remain to be sized.

The next design pass must freeze sling topology, actual loaded centre of mass,
clearance envelope, hoist travel and lateral control against real frame coordinates.
Do not insert remote PointConstraints that drag disconnected attachment points
across metres as if a sling existed. Do not lengthen the yard crane: its boom is
only 11.5 m high and cannot service the 48/96 m operation.

### Guide and seating law

Select the smallest native structural representation that can express the actual
failure: guide displacement/reaction under supported load, or a physical alignment
interlock operating a real latch. Declare stiffness/compliance, damping, admissible
load and measured guide error. Distance from a needle to its storage rack is not
a guide-racking measurement. Switching off the hoist above 118 m because a seating
predicate is false is a controller limit, not a physically jammed guide.

One seated needle may provide the Atlas's alternate structural path only if its
actual support and section carry the player and its downstream landing exists.
Do not describe a PointConstraint-pinned rigid member as “springy” without an
elastic law. No invisible stub enabled by `seat_count`.

### Support sequence, failure and recovery

Required route: hall → supported service approach → rigging stance → dynamic cage
or real operator station → seated structural support / stable cage stop → safe
landing. Coordinates, headroom, capsule passage and transfer velocity are unresolved;
this row is an implementation blocker, not permission to invent them in code.

Overload must stall within finite limits. Off-station/released control must produce
a defined safe brake state rather than retain a stale raising command. Missed seat,
lost hook, one beam seated, occupied sweep and parked cage each need a reachable
recovery route. The tower stair is available only where a measured connector joins
it; its existence alone does not rescue someone trapped in this proposed cage.
A ride ending in a void while waiting for AS-005 is not a complete slice.

### Checkpoint and proof contract

Extend the existing in-memory checkpoint with actual new bodies, velocities,
connections, controls and structural state after those owners exist; no fictitious
save-file version 4. Equivalent loaded continuation must include all upstream
rigging bodies. Durable serialization remains the separate TDD §14 requirement.

Before READY, provide a dimensioned supported layout and a complete handoff with
AS-005. Then exercise the normal approach/rig/load/seat/exit path and falsify
misalignment, under-force, brake release, obstruction, one-seat load and lost-tool
recovery. Check rider momentum and real geometry, preserve kernel/intake/tower
routes, run local checks and one full candidate proof path. No test was run on a
campaign needle machine during this audit because none exists.

## Completion and stop

This planning revision removes contradictory build instructions; it does not close
the critical geometry, rigging or guide-law unknowns. Keep implementation BLOCKED
until those owners have measurable contracts. AS-005 consumes the same interface;
AS-006 can be reached by the independent tower stair. Stop here.
