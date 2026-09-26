# SCRAPERX — AS-003 HOOK5 RACK

**Ascent Slice:** `AS-003`
**Lifecycle:** IMPLEMENTED
**Provenance:** reconciled against `ChatGPT` source at `7e66eb6` on 2026-09-26.
**Implementation gate:** maintenance after a reproduced failure; optional downstream receivers remain blocked.
**Evidence:** `00_START_HERE.md` §§2, 7; historical diagnosis below remains dated evidence.
**Write branch:** `ChatGPT`.

## Objective, authority and owner

Obtain the physical 36 kg hook block, open a real exit from its cage and leave with
that body. This is an acquisition route at grade, not an unlock flag at +40 m.
Laws 4–5, 9, 17, 22, 24–27; GDD §§7, 9, 14–17; Atlas B00 and CAP-HOOK5.
`src/sim/simulation.cpp` owns hook-cage bodies, door hinge, carry attachment and
traversal rules. Godot input/HUD may request and display actions only.

## Mechanical close — carried block back to access

| Required result | Physical prerequisite | Source contract |
|---|---|---|
| Usable hook delivered to a receiving padeye | Actual block exists and reaches that interface with compatible rigging | Being absent from the rack is not possession, attachment or lifting capacity |
| Player exits with hook | Door physically clears the doorway and the block is carried | Block 36 kg; both hands occupied; vault/mantle/hang unavailable while carrying |
| Door can swing inward | 38 kg removable bar is moved out of the sweep | 80 kg door; 600 N·m drive; hinge limit 1.45 rad; obstruction can still stall it |
| Player reaches the bar and rack inside | Enter through the roof hatch, then drop onto the real floor | Hatch 1.2 × 1.6 m; doorway 1.4 m wide under a 2.2 m header |
| Player reaches roof | Running jump and ledge grab from the moving belt | Roof top +4.45 m; cage x [8,12], z [-88,-84]; belt top +1.38 m |
| Belt supplies a reachable launch surface | Normal boarding and moving-support carry | Existing belt translation and native support-point velocity, no route flag |

The 2.90 m roof, cage near z=-104 and 6000 N·m door in the old plan are superseded.
Historical probes measured a 3.75 m running-jump grab from grade, hence a 4.45 m
roof cannot be reached by that grade jump. This does **not** make the belt the
only legal input: the kernel lift permitted an 11 m machine-assisted leap in the
recorded audit. Preserve that sequence break. Do not infer universal impossibility
from one probe family or the standing-mantle height alone.

### Support, collision and control

Route: apron → translating belt → roof jump/hang/mantle → hatch drop → floor →
move bar → door opening → pick up hook → apron. These actions use the existing
controller and force-limited carry constraint, not a parallel climb implementation.
Door/bar/block contact must remain consequential; “bar removed” cannot switch
collision or guarantee the door stays open despite another obstruction.

Source represents some cage walls as solid slabs while the presentation includes
barred detail. Apparent openings require a visual/collision audit before claiming
aperture fidelity. The roof hatch and exit doorway are explicit openings; a green
route test does not certify every visually open bar gap.

### Receiver, failures and recovery

Acquisition does not depend on AS-002's flight being deployed. Carrying the block
to the hall is a separate transfer: hands-free ledge mantles cannot be assumed
while holding it. The deployed freight stair provides an intended carrying route;
any alternate needs actual carry-clearance proof. AS-004 must accept a physical
hook/slings assembly at its interface, not `!hook_in_rack`.

A dropped block remains a body where it lands. Retrieve it using reachable geometry
and the existing carry controls; do not auto-respawn it. A moved bar may still
obstruct the door. A survived fall need not return the player to grade. An
unrecoverable state uses the last valid committed state, subject to the shared
checkpoint limitations; a checkpoint made after loss cannot magically restore
an earlier block location.

### Persistence

`MachineCheckpoint` captures the hook door, bar, block and carrying entity and
reconstructs carry topology. This is in-memory rollback, not serialized save
version 3. Checkpoint tests cover their stated scenario; full B00 restoration
still has the intake-body gap recorded in AS-001/002.

## Proof path and completion

Existing `tests/simulation_tests.cpp` groups `AS-003 apron`, `AS-003 cage` and
`Hook5 Rack` test the reach/entry/exit/carry route; `touch_carry` exercises real
input integration. Preserve those groups, belt support, door stalls and other
parkour regressions when implementation changes. The player's reported belt
carry failure remains an unresolved observation in the ground-water plan;
headless success is not a dismissal of that report.

Completion is physical acquisition and exit. Optional delivery to an unbuilt
AS-004 machine is not complete merely because acquisition passed. Stop here.

## Historical implementation record

The following is preserved from the pre-audit ticket. Its measurements describe
that execution, not current constants, lifecycle, exclusive routes or future work.
The active contract above supersedes its planning assignments and broad completion claims.

<details>
<summary>Original implementation observations and causal diagnoses</summary>

The nine §8.11 falsifiers are three native groups (`tests/simulation_tests.cpp`,
the AS-003 section before `EXIT_SUCCESS`) and one UI scenario:

- **1** is `AS-003 apron`: from the `Hook5Apron` spawn, eleven running leaps at
  every face but the belt's, each asserted to carry the body against the wall,
  with jump and traversal mashed; then four leaps at the cage off the 9 t
  pack's top, mantled from grade. No cage body is ever stood on, grabbed or
  offered as a ledge; grade never rises past a plain jump (`2.41 m`).
- **3, 4, 5, 6, 7** are `AS-003 cage`, one instance from the `Hook5Cage`
  spawn: 30 s of every command mashed leaves the door at `0.035 rad` and the
  bar in its brackets; lifted, carried north and set down, the bar frees the
  door, which travels past `1.20 rad` with no door command (none exists). The
  block comes off its rack, is carried out, toured for 10 s within `0.85 m`,
  set down to rest, and picked up again off the ground. Held at the 9 t
  pack's face, 5 s of traversal requests begin nothing and no ledge is
  offered; set down, the same face mantles at once. Carried to
  `MOD-SKIN-LADDER-S`'s foot, the first rung is neither offered nor climbed
  while held; set down on the spot, the same rung is offered at once and the
  same instance climbs to `+24 m` in 15 mantles. Falsifier 7 is asserted by
  that behaviour -- the refusal and the offer bracket a set-down with nothing
  else changing -- not by reading rung bodies, which no API exposes; the carry
  code has no path that touches them.
- **2, 8** and persist are `AS-003 Hook5 Rack`, one instance from the pendant:
  `AS-002`'s freight and cradle sequence, then the belt ride onto the roof
  (`10.6 s`, within two strokes), the hatch (`9.35 m/s`, non-lethal), the bar,
  the block, out through the door, round the belt's north end, through the
  throat, every flight of `MOD-STAIR-A`, the deployed flight and the upper
  flight to `MOD-HALL-DECK` with the block held (`y = 41.08`). Walked off the
  hall deck's north edge holding it, the 40 m fall is lethal and the restore
  returns the body to the deck with the block in hand, and holds there.
- **9**: every kernel, `AS-001` and `AS-002` group green in the same run; the
  9 t proof load is still on the ground at the end.
- `touch_carry` drives the same sequence inside the cage through the touch
  pipeline: Action reads PICK UP at the bar and at the block and SET DOWN
  while holding; the door swings open by itself; both rendered hands are on
  the block (`POSE_CARRY`); it is carried out and set down on the apron.

Measured, as §8.4.3 asked: the 36 kg load slows the launch -- `4.91 m/s` at
`0.25 s` against `5.5` empty -- and leaves top speed unchanged.

Deviations from the plan's numbers, each measured in the running solver and
each explained at its own constant in `simulation.cpp`:

- **Cage top `4.45 m`, not `2.90`.** The plan measured the lock against the
  mantle ceiling alone. A running jump into a ledge grab reaches `3.75 m` above
  the floor it leaves (measured against walls of every height), so `2.90` was
  grabbable from grade. At `4.45` grade is `0.70 m` short and the belt's deck
  (`5.13 m` reach) is `0.68 m` inside: the belt is still the key, jumped from
  and grabbed rather than mantled.
- **Sited at `z ∈ [-88, -84]`, not `[-106, -102]`.** A height lock holds only
  if nothing but the key is within a jump, and running jumps carry far: a
  `4.45 m` ledge is still grabbed across `6.3 m` of air from `1.38 m`, `6.8 m`
  from `1.8 m`, and a `4.45 m` roof is landed on across `10.8 m` from `8 m` up.
  At the plan's site the pendant catwalk, the 9 t pack's top, the bay's wall
  top (off `MOD-STAIR-A`'s first landing) and the WO-006 lift all reached the
  roof (7 of 48 probe leaps off the 9 t pack alone). At the new site a
  1 006-leap audit -- grade round every face, the 9 t pack, the catwalk, its
  ramp, the WO-006 catwalk, the WO-006 lift at its 9 m top -- reached a cage
  body only from the lift, 3 times in 162, by an `11 m` leap: a machine-made
  route, legal under Governing Law 17, recorded here and not blocked. The deck
  lies alongside for `43 %` of its stroke (the plan's site: `57 %`), lingering
  at the north end where the jump is made. §8.4.4's gap report stands and
  grows: the crate coupling is not sited, and the cradle is now 20 m away.
- **The door opens `1.45 rad` on `600 N·m`, not `1.35` on `6 000`.** The
  rigid bar holds against any torque; at `6 000` the leaf pinned the bar in
  its brackets with about `7 kN`, which lifting would have to fight. The
  hinge is `0.20 m` behind the west jamb as §8.3 asks.
- **Brackets, not keepers, and outside the leaf's sweep.** The plan's keepers
  sat inside the `1.146 m` sweep and would have stopped the door with the bar
  gone. Each bracket is a ledge under a bar end and a stop on its north face.
- **Bar seated at `1.00 m`, rack top `0.60 m` in the north-east corner** (plan:
  `1.30`, `0.90` mid-wall). The carry hands must be above every handle they
  lift, or the constraint pulls the load into its support and hoists the
  player instead (observed at the plan's `+0.30` with the bar at `1.30`); the
  loads are seated low rather than the hands raised, because at chest height
  the carried block's top sat at the eye and filled the view (seen in the
  Fold capture). In a `2.3 m` room the `2.05 m` bar needs a clear place to be
  set down; with the rack mid-wall every such place put one end on it.
- **The carry point: `0.60 m` ahead, `0.35 m` above the body's centre**
  (`1.25 m` over the soles), not `+0.30`; it swings round to the facing at
  `3 rad/s` rather than with it -- an instant half-turn threw the hand point
  `1.2 m` in one tick and the block out of it. A held body slips only when it
  is more than `0.90 m` from the hands **and** still moving apart, so a wedged
  bar lets go while a block lifted off the floor at the edge of reach does not.
- **Persist is the existing checkpoint, extended.** This branch has no
  serialized save and no "persist v2" to version; `MachineCheckpoint` gains
  the door, bar and block bodies and the carried entity, and
  `restore_carry_topology()` reconciles the constraint after a restore. A held
  load is restored at rest with the body it is held by (restored with its
  committed walking speed, it swung out of the still hands and dragged the
  body back off the edge it had just been restored onto).
- **Action puts PICK UP ahead of CLIMB.** The rack is itself a mantle ledge,
  and whoever faces the block means the block. Drop and Back set down.
- **The cage walls are drawn barred inside the native `0.30 m` slabs**, so the
  block, the bar across the door and the hatch are seen from outside while
  everything visible stays inside what is solid.

Defects found on the way and fixed at their source, not routed around:

- `DEFECT src/sim/simulation.cpp commit_checkpoint -> committed on any grounded
  tick, including a capsule held by its rim with its centre 0.3 m past an
  edge.` A lethal fall off that edge restored into the same slide, every time.
  Commits now need firm footing: a ray straight down from the body's centre
  meets walkable ground within its half-height plus `0.15 m`, which still
  commits on the deployed 30-degree flight.
- `DEFECT tests/simulation_tests.cpp AS-002 ascent -> its (-9, -108) waypoint,
  0.3 m inside the handoff deck's north edge, dithered the body off the deck
  -- a lethal 24 m fall the test never checked for.` It passed only because
  the rim checkpoint happened to restore onto a working trajectory. The walk
  now arrives and stops 1.3 m inside the edge, and the test asserts the route
  kills no one.
- `DEFECT src/bridge/scraperx_simulation.cpp kInitialSpawnCount -> the literal
  21 had fallen behind the enum and refused IntakeHandoffDeck.` It is now
  derived from the last spawn.

Not done here: a Godot capture of the deck alongside the cage mid-window
(the local captures are the cage interior, the carry and the approach);
Fold-device execution remains unverified.

---

</details>
