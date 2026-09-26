# SCRAPERX — AS-007 WET ISOLATION

**Ascent Slice:** `AS-007`
**Lifecycle:** PLANNED; no campaign process route in source.
**Provenance:** audited against `ChatGPT` at `7e66eb6`, 2026-09-26; imported pseudo-implementation removed.
**Implementation gate:** BLOCKED on AS-006's physical 220 m exit and the dimensional closure below.
**Evidence:** `00_START_HERE.md` §7; calculations are design rejection evidence, not runtime proof.
**Write branch:** `ChatGPT`.

## Objective, authority and owner

Reach TP-340 by making a flooded service route physically usable, or by a legal
exterior route. Native process state must couple real inventory/pressure to
hazards and machinery. Laws 4–5, 9, 17, 22, 24–26; GDD §§7, 9, 13–17;
Atlas B03/K3, `MOD-HEADER-W`, `MOD-SUMP-3`, `MOD-BLIND-STATION`,
`MOD-ISO-STAIR`, `MOD-WET-LOCK`, CAP-BLIND and TP-340.

The native process owner must own mass, connectivity, pressure/flow, and any force
it applies. Rigid-body/structural owners resolve motion/contact; mission/UI state
only observes. The kernel's reduced process fixture does not prove this 120 m
campaign route. No Godot-owned isolation, collision mask or door-success state.

## Mechanical close — dry exit back to accessible controls

| Required downstream state | Required physical producer | Closure gate |
|---|---|---|
| Stable feet on TP-340, with onward service/recovery access | Supported stair or exterior connection ending at the same actual surface | BLOCKED: derive footprint/load ties to the real tower; a 340 m label is not geometry |
| Player traverses stair and open wet lock | Stair remains solid, water hazard reduces, door/counterweight can travel | BLOCKED: actual hazard law, door load path and finite release mechanism |
| Sump falls below the traversable hazard level | Net outflow exceeds inflow; drainage has a lower receiving sink | BLOCKED: dimensioned basin, outlets, head and capacity |
| Header cannot refill sump | Physical upstream isolation, depressurization, then fitted blind | BLOCKED: reachable isolation/bleed topology and local differential-pressure criterion |
| Player safely installs blind | Correct flange, compatible plate, low differential pressure, access/carry clearance | BLOCKED: route to both control and flange before the flood is cleared |
| Player reaches the service approach | AS-006 actually delivers stable +220.25 m support | BLOCKED: current A/B stop at +198.25 m; C/alternate not built |

### Process choice and equations

Use **water** for this proposed wet/drainable system unless the Atlas is explicitly
amended at process design time. This selects a design model, not a source fact.
Steam, hydraulic oil and water cannot share an unspecified pressure/density/phase
law merely because all are called fluid.

Carry the following dimensional inventory through the design and later tests:
`V_header`, `V_sump`, `V_sink` in m³; water mass `ρV`; pressure in Pa; pipe area
in m²; flow in m³/s; tank surface area/level relation; finite source inventory or
an explicitly powered replenishing network. At each node,
`dV/dt = sum(Qin) - sum(Qout)` with transfers capped by available inventory and
receiving capacity. No double counting the same water at producer and receiver.

For an open, gravity-drained sump, a declared orifice model may use
`Q = Cd A sqrt(2 g max(h_up-h_down,0))`, bounded by wet area, valves and capacity.
For pressure-driven flow use the actual differential pressure and validated loss
model. A live header may still permit drainage while continuing to refill; do
not force `Qout=0` just because isolation is false. A vent matters if there is a
real sealed gas volume/airlock; an open sump cannot require a fictional vent flag.
Include receiver backpressure, full sink, blocked drain and loss of head.

The old “6 bar live line; hand-insert a 32 kg blind” is incomplete. At 600000 Pa
across a 0.5×0.7 m plate, pressure thrust is **210000 N**, not a hand operation.
Specify upstream isolation → reachable bleed into a sized safe sink → verified
low local differential pressure → fit/retain the blind → safe vent/drain. Derive
the permitted insertion force from the actual exposed area and mechanism; provide
reverse/removal sequencing. Isolation must change flow connectivity, not teleport
pressure to zero in a trapped volume. Account for pressure work and discharge hazard.

### Geometry rejected by the backward check

The imported layout was internally incompatible even before comparison with source:

- 340 steps from z=38 at 0.32 m run end near z=146.8, while the proposed TP-340
  covered only z=9.3..41.3. Its elevation alone did not make a landing.
- The proposed 260 m lock at z=44 did not intersect the stair near z=74;
  the 236 m sump near z=40 likewise missed the stair near z=52.
- Sump centre y=236 with half-height 4 spans y=232..240, not 232..248.
  Half extents (4,4,3) imply 384 m³ gross volume. A rate of 0.28 whole-volumes/s
  would mean 107.52 m³/s, with no pipe or energy budget to supply it.
- Blind rack near 221 m and flange near 241 m had no pre-isolation carrying
  approach. Controls beyond the hazard they must remove make a circular dependency.
- An exterior 0.4 m rung pitch at a fixed XZ is not a route for the current
  vault/mantle/hang controller. No ladder/climb implementation may be presumed.

Withdraw these coordinates/rates rather than silently translate them into a new
unsupported tower. Re-author one supported service bay and its immediate receiving
support first, then close the full 220→340 route as bounded slices if needed.
Every relevant geometry table must include source pose, extents, actual walking
surface, structural ties, collision opening, clearance and controller action.

### Wet lock and traversal law

A wet stair remains a solid stair. Flood/flow can create a real hazard or block
access through a physically actuated door; it cannot change a solid to
`support_rank=0`. Choose a dimensioned mechanism such as water loading or buoyancy
operating a lock, only if that model buys the required gameplay. Specify wetted
volume, density, lever arms, moving mass, force/torque, travel, friction/brake and
actual latch engagement. Do not freeze a door angle or change collision when
`isolated && drained` becomes true. A seated blind is a physical network boundary;
`blind_seated` may be derived telemetry, never an unexplained success switch.

### Recovery, persistence and receivers

Before READY, prove a dry approach to controls, a legal exterior bypass unaffected
by the skipped process, and recovery for dropped blind, incomplete isolation,
blocked/full drain, reflood, stalled door and missed transitions. Preserve actual
momentum and hazard state. “Last checkpoint” is valid only if that committed state
contains a recoverable player location and all necessary process/body/topology
state; it cannot restore a lost plate from before the checkpoint.

Persist water inventories, pressure-defining state, valves/connectivity, blind
pose/retention, machinery bodies/motion, and player support through the actual
checkpoint owner. No serialized version 6 exists. Equivalent process continuation
must be tested, not inferred from saved booleans.

AS-008 is UNAUTHORED. Its intended input is physical access to the B04 shop at
TP-340; do not link to an absent file or promise shop machinery is present.
The frame's existing rings do not automatically provide a 340 m floor between
its 330/352 m levels: a real supported receiver is required.

## Proof path, completion and stop

Design validation must first close dimensional geometry, reachability, mass/work
budgets and pressure/flow boundaries. Implementation then exercises normal
approach/isolate/bleed/fit/drain/traverse, the independent bypass, each failure and
checkpoint continuation; kernel process tests are regression, not substitute
campaign proof. No runtime tests were run for this unbuilt mechanism in this audit.

This is a causally constrained BLOCKED contract, not a claim of end-to-end
feasibility. Stop at the unresolved receiving interface; no coding above it.
