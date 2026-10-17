# Physical top-out into onward movement — 2026-10-17

Holding inward movement through Action now keeps the existing hand command
moving slightly onto the receiver instead of braking at the landing aim point.
The continuation is at most the existing0.35m player radius, rechecked each
native tick for clear standing geometry and the same current receiver body,
shape, entity and accepted walkable surface. Invalid continuation falls back
to the original command. The accepted receiver and real foot-acquisition
footprint never move. Lowering is tied to the original receiving plane.

Hand release retains fresh exact receiver contact, a completed shared solve,
actual firm footing and standing clearance. Final aim-distance/low-speed
requirements are removed. The finite foot row survives, its target stops
extending, and body momentum remains solver-owned. No force, stiffness, damping,
stroke, duration, work limit, global footing predicate, pose, velocity or gravity
law changed.

## Observed native execution

[Actual commands, source hashes and output](TOPOUT_FLOW_COMMANDS.json).
Host: ARM64 Ubuntu proot on Termux, GNU C++17 Release and the pinned Jolt archive.
Source: isolated published fafc416 plus these owned changes; all native files
outside the three listed owners match the accepted yard archive source. This
avoids the unrelated dirty grapple headers and ABI. Root wrote the implementation;
one Sol/high read-only physics specialist reviewed the bounded decision.

- Baseline `physical_topout_tests onward-run`: exit1, releases at native tick179,
  inward0.0404817m/s. The regression correctly rejects that forced stop.
- Removing only final settling gates produced the same result. The actual trace
  showed the hand target braking before first foot contact, rather than a
  completion gate delaying release. That failed hypothesis is retained locally.
- Final `physical_topout_tests onward-run`: exit0, release tick194,
  inward1.52015m/s, then0.551256m actual inward travel over20ticks while remaining
  on the receiving body. Fresh receiver contact, loaded shared solve and surviving
  finite foot are required by the case.
- Existing topout, single-step, jump-buffer, release, no-candidate, blocked,
  restart and frames modes: each exit0. Frame delivery remains equivalent under
  fixed90Hz,60/360Hz caller frames and grouped4ticks (0.0001m tolerance).
- `traversal_affordance_tests`: exit0,79 Release-safe checks. New real-Jolt
  continuation queries reject missing support, wrong body, wrong surface leaf,
  changed entity, sensor and replaced shape. A successful prospective query
  cannot create a solved foot sample, start a foot or record a shared solve.

## Limits and tuning

The public-input case stages once at an existing production ledge, then uses
ordinary Action, movement and sprint. This does not establish a connected ascent
from grade, new rotating-receiver behavior, or completion of movement phases2–8.
The initial blocked-receiver case and continuation query negatives cover distinct
paths; no claim of a new full public overhead-blocked continuation case is made.
Phone feel/rendered behavior and exact-source full release remain unverified
until the matching APK and required CI paths are observed.

No new tuning knobs: continuation uses the existing player radius, input strength,
hand target lead and2.2m/s finite advancement. Neutral input retains its existing
resting top-out. Other physical movement, material/cross-body transfer and palm
vault work remains in the existing backlog.
