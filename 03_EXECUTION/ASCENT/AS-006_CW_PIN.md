# SCRAPERX — AS-006 COUNTERWEIGHT WELL

**Ascent Slice:** `AS-006`
**Lifecycle:** IN PROGRESS — A and B implemented; C and the independent climbing route unbuilt.
**Provenance:** reconciled against `ChatGPT` source at `7e66eb6`, 2026-09-26.
**Implementation gate:** upward construction paused; do not implement C from the incomplete sketch below.
**Evidence:** `00_START_HERE.md` §§2, 7; native A→B path exists in `tests/simulation_tests.cpp`.
**Write branch:** `ChatGPT`.

## Objective, authority and owner

Earn access from the retained tower stair's 154 m top deck to the 220.25 m ring,
using linked physical mechanisms and a legal independent climb. Laws 4–5, 9, 17,
22, 24–26; GDD §§7, 9, 11–17; Atlas B02/K2; `MECHANISM_ASCENT_PLAN.md`.
This band's completion is **not** established by A/B reaching 198.25 m.

`src/sim/band_counterweight_well.cpp` owns authored parts/anchors/ratings;
`mechanism_kit.cpp` owns bodies, ropes, catches, guides, attachment topology and
checkpoint reconstruction. `simulation.cpp` owns player controls/support and kit
integration. Godot draws native parts/rope state. No second solver in presentation.

## Mechanical close — actual receiving supports back to inputs

Coordinates use source Y-up. +Z is toward the apron on this side of the tower;
use coordinates rather than inherited inconsistent facade names.

| Stage | Exit / payload | Source and transmission | Controls and support |
|---|---|---|---|
| A | Cage floor 154.25→176.25 m; tare 350 kg + 85 kg rider | 800 kg skip centre 188→166 m; tension-only 1:1 rope over sheaves at 191 m | Move slack rope end from bollard to cage eye; carry trip handle and pull 60 kg lever through 0.50 rad; real catch releases |
| B | Cage floor 176.25→198.25 m; tare 350 kg + rider | 2500 kg **translating lattice**, centre 209.25→187.25 m; tension-only 1:1 rope over sheaves at 223 m | Same physical rig/pull pattern; guided 22 m descent, not a swinging boom or snapped guy |
| C | Intended platform 198.25→220.25 m | Proposed rubble-filled dumpster; not implemented | BLOCKED below; no player route to 220 is proven |

A cage footprint: x [-12,-9], z [-132.8,-130]. B: x [-8.8,-5.9], same z span.
Their slab-edge gap is 0.20 m from current constants, not the sketch's 0.30 m.
The source test boards B from A's parked cage in the same simulation. Actual posts,
rails, hands and support contact still govern passage; footprint arithmetic alone
would not prove it. A's +176 ring inner edge is z=-128.91; B's +198 edge is
z=-129.82. Reaching cage height is not proof of every ring exit or return.

The descended B lattice spans +176.25..+198.25 m. It has two longitudinal members
and crossbars at 2 m spacing. It neither reaches 220 m nor proves a ladder-climb
route: no general climb/shimmy/balance verb is implemented. Do not call it a
one-shot wreckage bridge; it survives translation and requires external work to
be raised again. The original boom design is superseded, not another active spec.

### Force, energy and arrest

At 1:1 travel, ideal full-stroke gravity budgets (excluding the small hardware,
initial kinetic energy and dissipative/contact terms) are:

| Stage | Source potential released | Cage + rider potential gained | Difference available for kinetic energy/losses |
|---|---|---|---|
| A | `800×9.81×22 = 172656 J` | `435×9.81×22 = 93881.7 J` | 78774.3 J |
| B | `2500×9.81×22 = 539550 J` | 93881.7 J | 445668.3 J |

A guide: 2.50 m/s command, finite brake 9000 N, end-leveling acceleration parameter
2.0 m/s². B: 2.85 m/s command, 42000 N brake, 2.5 m/s² leveling parameter.
`Kit::govern` limits force to oppose the measured velocity; it is not a hidden
lifting motor. Source B comment calls 3.0 m/s the ceiling, but the current test
allows 3.1 m/s and the recorded run peaks near 3.005 m/s. Therefore a strict
≤3.000 m/s guarantee is **not proven**. Do not silently call a tolerance a rating.
A's test allows 2.6 m/s. Stops/catch reactions and normal-rider stability remain
separate from these speed thresholds.

The native ride energy assertions compare source loss against cage/rider height
gain with stance-jitter tolerance; they do not close the entire energy ledger.
A full ledger must include rope/shackle/handle/lever motion, kinetic energy,
brake/impact dissipation and catch seating work. No structural SWL or rope-break
rating follows from a successful lift.

### No-link state and catch limits

As found, rope ends are fast on real bollards. Pulling the release while unlinked
permits roughly 0.03 m source descent before rope arrest; the payload does not
rise. This is not “all energy remains stored.” Tests require ≤0.05 m displacement
and actual rope tension, then relatch and rerig. Large impulse peaks at slack
capture are not static working loads.

`Kit::latch` creates a FixedConstraint drawing a slow body onto a seat within
0.05 m. Its comment describes a tapered spring pin with work ≤`m g δ`: up to
392.4 J for A and 1226.25 J for B for a purely vertical 0.05 m correction. No
finite spring-energy store, pin strength or recharge is tracked by that code.
This is an explicit reduced-model debt; do not claim fully accounted spring work
or unbounded repeatable energy. A later repair must size the real seating/recharge
law or latch at a genuinely achieved compatible seat, preserving tested rigging.

### Stage C — required backward closure, not an executable design

| Receiver condition | What must produce it | Critical unresolved quantity |
|---|---|---|
| Player stands safely on ring +220.25 m | Platform arrives aligned, arrested and supported | Footprint, guides, frame ties, brake/catch loads and exit clearances |
| Platform rises 22 m | Dumpster descends 44 m through a 2:1 purchase | Rope anchors, ratio convention, slack/swept clearance and all sheave reactions |
| Dumpster contains intended 900 kg rubble before useful stroke | Finite hopper discharge reaches a held receiver | Fill catch or equivalent real restraint; stream capture geometry and release cause |
| Filled bucket drives payload | 250 kg tare + rubble overcomes 700 kg platform + 85 kg rider | Ideal threshold `m_dump > 785/2 = 392.5 kg`; only 142.5 kg rubble starts motion without restraint |
| C leftovers can reset A | Discharge reaches A's cage at +176.25 m, with no rider trapped there | Final outlet must sit above receiver; initial outlet is ≥44 m higher, plus stream clearance |
| A can lift again | Rubble first lowers A, then leaves its cage; skip re-latches | Actual unloading gate/receiver/control and conserved rubble destination; none is built |

At proposed 150 kg/s flow, an unrestrained dumpster begins descending after about
0.95 s, before the six seconds needed for 900 kg. A fixed mouth may then lose
capture. Do not grant the full 1150 kg mass by a timer while the receiver moves.
If all mass is loaded before release, `1150×9.81×44 = 496386 J` is an upper
full-stroke potential release; the proposed platform/rider need
`785×9.81×22 = 169418.7 J`. If filling during motion, integrate changing mass,
entry momentum, hopper depletion and spill; count hopper-to-dump work once.

A 44 m drop ending above A's +176.25 m cage requires the initial outlet above
+220.25 m **plus** clearance. The hopper mouth must be higher still to gravity
fill it. “Hopper on the 220 ring” alone does not close that vertical envelope.
Supported upper structure, bucket dimensions and rope headroom must be derived
before the cascade is feasible. B's current lattice does not provide a 198→220
link or trigger automatically. The inter-stage input must be physically earned.

A's cage with 900 kg rubble outweighs the 800 kg skip by 450 kg before hardware.
That establishes only a direction tendency. Arrest/braking, collision, discharge,
access and release topology must permit the complete reset. A surviving skip is
potentially rearmable, not proof that this player can rearm it. No automatic refill.

### Recovery and persistence

The promised independent 154→220 climbing route is unbuilt. A/B ride and partial
checkpoint tests do not establish recovery from every missed exit, spent state,
parted rope or lost shackle. Before a new upper checkpoint, show a reachable return,
alternate climb, rearm or valid rollback that restores equivalent state. Do not
call solver launch at an arrest acceptable unless it is intentional, bounded and
recoverable gameplay.

Kit checkpoints capture body state and rope/catch topology in memory. Current
A test commits, falls and restores before B's ride. There is no serialized
version for this band, no C inventory to restore yet, and no proof of a full
A/B/C aftermath restore. General hook use is limited to actual authored compatible
anchors, not arbitrary mesh points.

## Proof path and exit

Existing native groups: `AS-006 A no link`, `A ride`, `B no link`, `B ride`; actual
Action UI scenarios `touch_rig`, `pad_rig`, `keyboard_rig` exercise the earlier
rigging seam. The same native simulation transfers A→B; do not downgrade that
observed segment to two unrelated isolated tests. Its initial spawn and checkpoint
operations also mean it is not an uninterrupted apron-to-198 playthrough.

Required future proofs: real B exit/return, no-lift route, C and its energy/mass
transfer, A unloading/rearm, full 154→220 normal route and equivalent checkpoint
continuation. Source geometry and a plausible budget alone are not those proofs.

The implemented output is a ridden cage at +198.25 m. AS-007 requires **feet on a
stable +220.25 m support** with access to its service controls. That handoff is
BLOCKED. Stop; do not implement another ascent while ground rehabilitation or
its required proof is unresolved.
