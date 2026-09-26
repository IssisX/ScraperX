# SCRAPERX — AS-002 LEGAL FORTY

**Ascent Slice:** `AS-002`
**Lifecycle:** IMPLEMENTED
**Provenance:** reconciled against `ChatGPT` source at `7e66eb6` on 2026-09-26.
**Implementation gate:** preserve proven routes; reproduce a local defect before repair.
**Evidence:** `00_START_HERE.md` §§2, 7; historical diagnosis below is not an active build specification.
**Write branch:** `ChatGPT`.

## Objective, authority and owner

Connect AS-001's +24.1872 m handoff to `MOD-HALL-DECK` at +40.1872 m through
real freight-driven structure and the outside-climb alternative. Laws 4–5, 9, 17,
22, 24–26; GDD §§7, 9, 11–17; Atlas B00/K0. `src/sim/simulation.cpp` owns
`build_legal_forty`, the `kLegalForty*` / `kIntakeSwing*` / `kIntakeCw*` constants,
contacts and attachment topology.

The hall is reachable by other legal structure; deploying this flight is not a
universal progression gate. AS-003 acquires a hook at grade and is not chronologically
locked behind this ticket. AS-004/005 are optional, unbuilt receivers.

## Mechanical close — exit back to energy source

Coordinates are source Y-up; actual constants/builder are the geometry owner.

| Receiving state | Required upstream state | Owner / quantitative contract |
|---|---|---|
| Feet on hall at +40.1872 m | Existing upper static flight, from +32.1872 m, clears the hall opening | Upper flight centre x 2.422, z -117; well x [-4.80,1.50], z [-119.20,-114.80] |
| Stable mid-landing at +32.1872 m | Walk from deployed dynamic flight, or the outside walkway | Landing centre x 9.35, z -115.8, half extents x 1.05, z 5.20 |
| Swinging flight held deployed | Tension through pulley overcomes flight gravity; hinge limits stop travel | Flight length 16 m, width 1.8 m, mass **400 kg**, hinge x 7.856, z -112.5 at mid-landing datum |
| Sustained counterweight tension | Pack rests on free guided cradle and is detached from crane | Cradle **500 kg**, loaded **4500 kg**, half extents (1.5,0.9,1.2); initial top +5.20 m |
| Pack seated without crane carrying it | Operate AS-001's crane from its reachable grade station | Cradle on crane's 12 m working circle at bearing 0.80 rad; actual release/attach predicates below |

Flight angle from downward vertical: stowed 8°, deployed 60°; physical travel
52° = 0.907571 rad. Jolt's reported hinge angle uses the opposite sign; do not
confuse displayed positive deployment with raw hinge rotation. The rope attaches
15 m from the hinge; sheave height above hinge is 4 m. Cradle top travel limits
are +3.00..+5.80 m, with signed displacement measured from its build pose.

For an ideal point attachment, `l(θ) = sqrt(15² + 4² + 2×15×4 cos θ)` and
`Tcrit(θ) = 400 g × 8 l(θ)/(15×4)` follows torque balance away from sin θ = 0.
This gives equivalent suspended mass about 1011.69 kg at 8° and 925.30 kg at 60°.
500 kg tare is below both; 4500 kg loaded mass is above both. These are a static
screen, not a rated dynamic payload margin. Real attachment offset, support loading,
rope slack, acceleration, hinge-stop contact and arrest require the runtime path.
Do not restore obsolete 1900 kg flight / 1800 kg cradle values from earlier drafts.

Release requires pack/cradle XZ error ≤0.55 m, Y error ≤0.35 m and speed ≤0.15 m/s;
reattach requires hook/pack separation ≤0.40 m and speed ≤0.20 m/s. Native creates
or removes the actual attachment. A release request does not teleport the pack onto
its seat. Source `update_intake` remains authoritative for the exact measured points.

### Outside route and support transfer

The first 15 ledges come from AS-001. Five continuation ledges (16–20, 1.6 m rise)
reach +32.1872 m, **not** a straight ladder to +40. A fixed walkway at z [-121,-117]
meets the mid-landing; its 4 m depth preserves the usable headroom corridor beneath
the upper flight. Walk the shared upper flight to the hall. No freight mutation is
required. The main tower stair to 154 m is also retained.

For the freight route: handoff → dynamic flight → fixed mid-landing → fixed upper
flight → hall. Native support state owns identity/contact and `v + ω×r`; preserve
motion on departure. A static force calculation does not prove rider contacts.

### Collision and recovery boundaries

Source excludes flight/hinge-anchor contact to avoid self-binding at the joint.
It also excludes flight/handoff contact where solids overlap. The latter is an
explicit collision-fidelity debt: green traversal does not establish that visible
solids agree through the entire sweep. Audit the actual overlap before deciding
whether joint clearance or geometry needs repair; do not bless nonphysical
interpenetration by repeating the historical explanation.

Unloading removes counterweight torque and retracts the flight; the outside route
remains legal. Recovery is physical traversal/rigging or a coherent checkpoint,
not restoring the freight by flag. The yard crane's 11.5 m boom cannot service
+40 m directly. A station-range predicate alone is not proof that all possible
crane-riding/control combinations are impossible.

### Persistence and receiving interface

`MachineCheckpoint` records flight/cradle bodies and `intake_pack_slung`, but not
the intake pack/hook/dog/crane bodies. Restoring those partial fields is not proof
of equivalent loaded-system continuation. No invented save version 2.

AS-004 may consume the +40.1872 m hall and an actual delivered hook block. It may
not assume the block was acquired, both hands are free, freight is solved, a 48 m
rack exists, or a service lift is built. AS-006 instead enters from the existing
154 m tower stair; AS-004/005 are not prerequisites for that route.

## Proof path and completion

Preserve the existing `tests/simulation_tests.cpp` Legal Forty groups: crane
placement/release, stable loaded deployment, walk to hall, unloaded retraction,
outside route with freight untouched, and their checkpoint segment. Keep historical
claims scoped to those segments; checkpoint coverage above remains incomplete.
The current evidence ledger identifies the successful full workflow, not a new
runtime execution during this documentation audit.

Stop at the hall interface; no new ascent implementation in this ticket.

## Historical implementation record

The following is preserved from the pre-audit ticket. Its measurements describe
that execution, not current constants, lifecycle, exclusive routes or future work.
The active contract above supersedes its planning assignments and broad completion claims.

<details>
<summary>Original implementation observations and causal diagnoses</summary>

All ten §8.11 falsifiers pass (`tests/simulation_tests.cpp`, the AS-002 section
appended before `EXIT_SUCCESS`), evidence line above. Both braids close;
retraction is real and reversible; the jib gap is confirmed, not engineered
around. Kernel and `AS-001` falsifiers remain green (`PASS scraperx_sim B00
intake rise` unchanged in the same run).

Deviations from the plan's own literal numbers, each verified against the
running solver rather than assumed, each with a code comment at its own
constant explaining why:

- **`kLegalFortyFlightMassKg = 400`, not the plan's `1900`.** §8.4.1's own
  `T_crit(theta)` derivation is sound and re-checked independently — the
  plan's own worked margins (loaded `1.21x`, empty `0.37x`) are arithmetically
  correct at `1900`/`1800` kg. They are not sufficient in the actual solver: a
  hinge built exactly at its own stowed hard limit needs real headroom to
  depart it, not a bare `>1x` quasi-static margin. At `1900` kg the flight sat
  inert under the real `4000` kg pack for `80+ s` of simulated time. `400` kg
  is the value that deploys reliably, verified, not calculated to a target
  ratio.
- **`kIntakeCwCradleTareMassKg = 500`, not the plan's `1800`.** Same finding,
  the loaded side of it: the ratio between "loaded" and "tare-only" demand on
  the flight's own `T_crit` is fixed by tare mass alone once the (frozen,
  `AS-001`-owned) `4000` kg pack sets the loaded side.
- **`MOD-CW-CRADLE` car half-height `0.90`, not the plan's `1.20`; top face
  unchanged at `5.200 m`.** §8.4's clearance list never checks the `B00`
  belt. At the plan's deployed centre (`2.380`) the car's underside is at
  `1.18 m`, under the belt's `1.38 m` top, and the belt's `18 m` stroke
  carries its south end to `z = -111`, across the car's `x ∈ [7.108, 8.0]`.
  Once per stroke (`15.7 s`) the belt rammed the loaded car, knocked the
  flight `0.045 rad` off its stop, and walked the seated pack `0.3–0.6 m`
  per blow until it fell off (observed within `~40 s`, nobody near it).
  Keeping the top face — the seat and the rope's body point — where the plan
  puts it leaves the rope, the `1.620 m` stroke and every statics number
  unchanged; only the underside rises, to `1.78 m` deployed. The slider limits
  are restated on the top face (`[3.00, 5.80]`), the same range. Falsifier:
  the loaded flight holds `>= 0.90 rad` and the pack drifts `< 0.05 m` over two
  full belt strokes (`INFO AS-002 deployed hold`).
- **`MOD-SKIN-LADDER-S` rungs 17–20 step `0.8 m` west of the column.**
  Directly over the deployed flight's foot, rung 17 on the column line left
  `1.4 m` of headroom over the flight's first metre. The walkway's west end
  follows rung 20 to `x = -5.8`.
- **The retract falsifier's own failure was not a mass problem at all**, though
  it looked exactly like one and cost the most iteration: unloaded and
  re-slung, the flight sat frozen at its exact deployed limit for `20+ s`
  with a real, non-zero pulley tension favouring departure the whole time.
  Direct diagnostic tracing (logging the real world-space contact point of
  every contact touching the flight, not just entity IDs) found the flight in
  continuous, ordinary rigid-body contact with its **own hinge anchor** —
  coincident with it at every sweep angle by construction, since the anchor
  sits at the hinge pivot the flight's own cross-section always occupies.
  This is the same class of defect the plan's own `MOD-DOG-A` retrospective at
  §8.3 already names (a hinge fixture sized bigger than its sweep clearance),
  now recurring against a fixture too small to trip that same check by
  inspection alone. Fixed by excluding that one body pair from collision
  (`kIntakeSwingAnchorEntityId`, `OnContactValidate` in `simulation.cpp`),
  the same mechanism already used for the flight-vs-handoff-deck exclusion
  §8.3 itself calls for ("the stop is the hinge limit, not the deck"). Once
  fixed, retraction works cleanly at the already-verified deploy masses above
  — no further mass or geometry retuning was needed for it.
- **`MOD-SKIN-LADDER-S` continues only to rung 20 (5 rungs), not rung 25 (10),
  and does not turn to climb the hall's south fascia in `+X`.** The plan's own
  mantle-clearance law, correctly re-derived, is a floor on the GAP between
  rungs, not on each rung's own depth: `step - kLandingInset >
  kTraversalReach + kPlayerRadius`, i.e. `step > 1.77 m`, independent of rung
  depth. A climb of nine rungs at that spacing needs `>= 14.2 m` of clear run;
  `MOD-HALL-DECK`'s own well is `4.4 m` deep and the run north of rung 16 is
  capped at `12.5 m` by `build_stack()`'s own southern columns. No single
  straight column threads both. SKIN instead climbs only to the mid-landing's
  own height (`32.1872 m`, clear of the well problem entirely), and a short
  static walkway (`kLegalFortySkinWalkway*`) carries the remaining horizontal
  distance to the mid-landing, from which the upper flight is the rest of the
  route — the same one the SHAFT braid uses past that point.
- **The mid-landing's own south edge moved from the plan's `z = -111.6`
  (`half_z = 3.20`, its literal geometry-table value) to `z = -119.0`
  (`half_z = 4.20`), and later to `z = -121.0` (`center -115.80`,
  `half_z = 5.20`) with the walkway below.** The walkway above and the landing touch at `x = 8.30`
  with zero x-overlap; the only safe crossing is whatever z-band both cover
  at once. The upper flight's own underside, directly overhead near that
  seam, leaves under `2.1 m` of capsule headroom (the same figure the hall
  well's own east-edge widening below required) for `x` past about `5.7`,
  ruling out its own z-band as a crossing corridor; south of it, at the
  plan's original `-111.6` edge, was not covered by the landing at all.
  Widening the landing's own south edge to meet the walkway's is the fix that
  needed no change to the walkway or the flight.
- **The SKIN walkway is `4.0 m` deep, `z` in `[-121, -117]`, not rung 20's own
  `2.0 m` band.** That band runs under the upper flight (`z` in
  `[-117.9, -116.1]`), whose underside falls to `0.6 m` over the walkway at
  `x = 8`: a standing capsule had one `0.4 m` lane (`z` in
  `[-118.65, -118.25]`), and a player reported the top of the climb as an
  opening too small to fit through. South of the flight the air is clear to
  the hall deck's underside (`39.79 m`), so the walkway grows `2 m` south and
  the mid-landing's south edge follows it; the lane is `2.4 m`. Falsifier: after
  the 20-rung climb, a walk that zigzags between `z = -120.3` and `-118.6`
  through the flight's low stretch reaches every point on foot and never drops
  (`INFO AS-002 walkway lane: lowest_y=33.086`); on the old footprint it falls
  off the walkway's edge (`lowest_y=20.63`).
- **The hall well moved from the plan's `center x = -4.51`, `half_x = 3.00`
  to `center x = -1.65`, `half_x = 3.15`.** East edge: a capsule needs
  `~2.1 m` of headroom over an inclined surface, not the `~0.9 m` flat floor
  needs, so the upper flight's own climb does not clear the original east
  strip's underside until past where that edge sat. West edge: the flight's
  own top (local `-X` end, `x = -4.506`) left a `3.0 m` open gap to the
  nearest deck strip at the plan's original edge; the new edge overlaps the
  flight's own last `0.3 m` instead, harmless since Jolt does not solve
  contact response between two static bodies.
- **The swing flight vs. the handoff deck needed an explicit collision
  exclusion**, not just the "the stop is the hinge limit, not the deck"
  design intent §8.3 already states in words: the final third of the sweep
  (`theta` in `[48.8 deg, 60 deg]`) drags the flight's own solid body through
  the deck's near corner by construction (up to `0.36 m`, `0.156 m` still
  present at the rest pose itself) — no `AS-002`-owned knob clears it without
  unpicking the geometry §8.3's own numbers derive from. `OnContactValidate`
  rejects contact between `kIntakeSwingFlightEntityId` and
  `kIntakeHandoffEntityId` specifically; every other pair, flight vs. player
  included, keeps ordinary collision.

Godot presentation twins exist for every new body (the swing flight and its
rope, the cradle and its guide mast, the mid-landing, upper flight, hall deck
and well, the SKIN continuation and its walkway) and R/G pendant bindings for
Release/Attach, with a `LEGAL 40` HUD line. Verified headless (`godot
--headless --path godot --quit-after 300`, and the existing `--ci` WO-004/005/006
proof sequence through `MACHINE_PROVEN`): the extension loads, the scene
builds without a script error, and the full existing proof chain is
unregressed. Not verified: rendered visual correctness on an actual display,
and Fold-device execution, both out of reach of this environment.

Persist: `MachineCheckpoint` captures `intake_swing_flight` and
`intake_cw_cradle` as full `BodyCheckpoint`s (pose and velocity, the same
mechanism every other machine body already uses) plus `intake_pack_slung`,
and `restore_legal_forty_topology()` reconciles the sling constraint against
the restored flag before the next tick reads contacts — §8.10's own
requirement, met with the codebase's existing generic body-checkpoint
mechanism rather than the plan's bespoke `flight_hinge_angle`/`cradle_y`
scalar fields. Neither this ticket nor `AS-001` before it tracks a literal
persist-version number anywhere in source; "version 2" in §8.10 is this
file's own description of the blob's shape, not a field to assert against.

---

</details>
