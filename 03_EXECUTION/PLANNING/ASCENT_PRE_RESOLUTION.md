# SCRAPERX — ASCENT SLICE PROTOCOL

**Scope:** authoring and auditing `AS-*` contracts in `IssisX/ScraperX`.
**Write branch:** `ChatGPT` only. Other branches are provenance, never active authority.
**Status owner:** `00_START_HERE.md` §§2, 7–8. Do not duplicate a live job queue here.
**Audit basis:** current source at `7e66eb6`, 2026-09-26.

## Authority and scope

Laws → GDD → Atlas → Execution Protocol → TDD define the required product and
engineering contracts. Current source/tests/runtime establish what exists and
what has actually been demonstrated; a specification cannot overrule an observed
failure into a pass. The current ticket names the smallest authorized seam.
Surface a requirement/implementation conflict and repair its owner. Do not silently
lower a law, retune global movement or remove a valid route to protect a puzzle.

`WO-000`–`WO-013` are closed historical kernel work orders; `AS-001`–`AS-015` are
ascent ticket identities; `B00`–`B11` are Atlas bands. `AS-008`–`AS-015` have no
files yet. Historical IDs in source/comments are provenance, not new tickets.

A usual authoring pass handles one complete slice. An explicitly requested
cross-document audit may update all contradictory owners together in one
planning-only commit. It must not add gameplay code, tests, CI or speculative
machinery. Preserve historical observations, but remove obsolete active build
instructions. Do not require another approval when the user has already authorized
the work; the old per-file paste/approval rules are retired.

## Start with the receiving support; then validate forward

Work backward until every required input has a producer the player can actually
reach and operate. Then exercise the chain forward. Backward reasoning locates
missing causes; it is not runtime proof.

| Backward question | Required contract |
|---|---|
| Where do the player's feet finish? | Support identity, source pose/extents, walking-surface height, stability/clearance, receiving next action |
| What allows the last transfer? | Contact geometry, relative speed, obstacle/headroom, controller verb, capture/landing and missed-transfer recovery |
| What puts a moving support there? | Real body, joint/guide, travel, anchor geometry, ratio, stop and swept collision |
| What transmits enough load/work? | Force/torque law, inertia, friction, slack, leverage over full stroke, finite drive and brake limits |
| What supplies the energy/mass? | Initial inventory/pose, work source, producer access, all losses, residual state and conservation |
| How can the player connect and trigger it? | Supported approach, reachable control, compatible physical attachment, carried-hand limits and observable mechanism state |
| How can it be tried again? | Retrieval, unloading/recharge, return support, aftermath and complete checkpoint continuation |

Check predecessor output against receiver input at the same instant: position,
orientation, supported mass, available inventory/energy, velocity, constraint
state, tool/hand state and player access. Matching altitude or a module name is
insufficient. Do not install a hidden bridge, completion flag, remote attachment
or compulsory teleport to make mismatched interfaces appear connected.

## Evidence classes and readiness

Every substantial claim identifies its basis: SOURCE, OBSERVED RUNTIME, DESIGN
TARGET, HISTORICAL OBSERVATION or UNKNOWN. Lifecycle is separate: IMPLEMENTED,
IN PROGRESS, PLANNED or UNAUTHORED. BLOCKED/READY is an implementation gate,
not proof. A critical unknown remains BLOCKED; do not choose a convenient number
and call the contract mechanically closed.

Source pointers identify the owning builder/constants; avoid multiple independent
geometry tables purporting to own the same implemented body. Historical result
records may stay in a ticket when explicitly isolated from active instructions.
The latest integrated-run/artifact status lives in the start-page ledger.

## Datum, coordinates and movement

Atlas §2 owns datum and frame conversion. Use SI. Distinguish capsule centre,
body centre, centre of mass, member top and walking surface. Report source Y-up
coordinates for current code, and map tower-local Atlas coordinates explicitly;
do not transplant another branch's world origin or old `kB00*` constants.

Existing controller: standing capsule radius 0.35 m, total height 1.8 m;
step-up 0.35 m; current walk/jump/vault/mantle/hang/crouch/carry/parachute support.
Inspect current constants and runtime probes for reach/clearance in the relevant
posture. A named future climb/shimmy/balance capability is not implemented by
writing closely spaced rungs. Do not infer running-jump reach from standing
mantle rise. Carrying can remove hand-dependent verbs.

Moving-support law: `v_contact = v_linear + ω × r`; detach preserves relevant
inherited motion. Do not compensate for local geometry with broad movement tuning.

## 8. Mechanical close — required content of each ticket

A READY ticket supplies these contracts; a BLOCKED ticket names the missing owner
and exact closure needed instead of pretending to fill the row.

- **Identity/authority:** band, modules, capability IDs, bounded objective, allowed
  owner/seam and out-of-scope neighbor. New modules belong in Atlas, not a second map.
- **Entry:** actual predecessor support and machine/tool state; explicit alternate
  entries; any setup performed by proof fixtures distinguished from player actions.
- **Geometry/load path:** every route-critical solid/opening, source pose/extents,
  supported ends/anchors and rating or unverified structural assumption. Filler
  cannot secretly provide useful support. Render and collision agree materially.
- **Transmission:** degrees of freedom, attachment points, ratios/signs and
  constraints; real actuation, occupancy and stop behavior across the whole sweep.
- **Budgets:** force and moment about the actual axes, energy and mass conservation,
  acceleration, speed, brake/arrest and capture. A static surplus proves neither
  dynamic stability nor structural SWL. Ratchets/catches cannot recharge for free.
- **Controls:** player access, command limits, release/off-station state, reset and
  observable causality. A control predicate may operate a modeled valve/brake;
  it cannot create an unsupported route or unearned mechanical work.
- **Traversal:** ordered support identities, posture/hand use, momentum transfer,
  exit to next support and recovery on a missed transition. Keep legal sequence breaks.
- **Aftermath/recovery:** overload, slack, lost connection/tool, obstruction,
  spent source and rearm. “Still exists” is not “player can retrieve or recharge it.”
- **Checkpoint:** authoritative body/control/process/topology state required for
  equivalent continuation, including upstream bodies that load this stage. The
  current in-memory checkpoint is not durable serialization; no invented version.
- **Proof:** normal intended input path, actual failure before repair, strongest
  rival causes and discriminating evidence; same-path completion and relevant
  neighboring regressions. Fixtures support only the segment they exercise.
- **Exit/stop:** stable support and consumable output for the next ticket, remaining
  unknowns and explicit finish line. A future ticket must not be needed to make
  the current slice safely usable.

For machinery the forward causal chain is player input → native connection/control
or body state → force/flow/contact → changed support/access → actual traversal.
For static parkour, real supported geometry plus the current controller may be the
smallest complete seam. Complexity is justified by gameplay/physical/structural value,
not the permission to build a machine.

## Verification and publication

Read the current branch state and preserve newer work. For a repair, first exercise
the intended runtime segment; distinguish geometry, controller, support, mechanism
state and route design. Do not manufacture an artificial failing scenario.
Run applicable local checks before publishing. Documentation-only checks establish
consistency and derivations, not new gameplay behavior; do not rebuild an APK merely
to label a planning audit green. Retain the latest gameplay proof at its actual commit;
a documentation publication does not create a new runtime result.

For code candidates, publish one at a time and follow that exact run to terminal
GREEN/RED. Never intentionally overlap candidate APK builds. Estimate duration
from comparable completed workflow/job paths, wait approximately estimate +2 min,
then inspect that run. A timer is not success. On RED retrieve its first causal
failure and repair that seam; on GREEN confirm required jobs and APK/artifact.
Stop at the authorized completion boundary; a green result does not authorize the
next ascent section. Report actual checks and unverified boundaries without assigning
routine file comparison or bookkeeping to the user.

## 10. Ticket scaffold

Use the Execution Protocol §3 header: ticket ID, lifecycle, provenance,
implementation gate and evidence pointer. Then state objective/authority,
existing truth, owner/allowed seam, Mechanical close (§8), proof path,
completion and stop. A BLOCKED contract must name the missing producer/receiver
and required discriminator; an empty “pending” row is not closure.

## Historical identifier migration

The following map is provenance only; branch names here are not write targets.

### 5.1 Migration map — old name → current name

Two renames have happened. This table is the whole record; it is frozen and needs no upkeep.

**1. Adoption from `ScraperX-Grok` (2026-09-21).** That branch numbered the shared kernel work
differently, so the same slice carried different numbers on each side.

| Slice | Class | `ScraperX-Grok` | here |
|---|---|---|---|
| Fall / parachute / checkpoint | kernel | `WO-004` | `WO-008` |
| First freight | kernel | `WO-005` | `WO-011` |
| First structural coupling | kernel | `WO-006` | `WO-012` |
| First process coupling | kernel | `WO-007` | `WO-013` |
| First causal chain | kernel | `WO-008` | `WO-009` |

**2. Taxonomy split (2026-09-22).** Kernel and campaign work had shared one ambiguous `WO-*`
namespace. Campaign tickets left it; kernel tickets kept `WO-*` and the set was closed.

The campaign prefix was briefly `ASC-NN` on 2026-09-22 before settling on `AS-NNN` the same day,
to match the sibling `ChatGPT` branch and to give the set a fixed three-digit width. **`ASC-*` is
retired.** It appears in no file and must not be reintroduced; it is recorded here only so a
reader who finds it in git history can resolve it (`ASC-NN` → `AS-0NN`).

| Slice | Atlas band | `ScraperX-Grok` | old here | **current** |
|---|---|---|---|---|
| Intake rise | B00 | `WO-009` | `WO-014` | **`AS-001`** |
| Legal forty | B00 | `WO-010` | `WO-015` | **`AS-002`** |
| Hook5 rack | B00 | `WO-011` | `WO-016` | **`AS-003`** |
| Needle seat | B01 | `WO-012` | `WO-017` | **`AS-004`** |
| Cage or skin | B01 exit | `WO-013` | `WO-018` | **`AS-005`** |
| CW pin | B02 | `WO-014` | `WO-019` | **`AS-006`** |
| Wet isolation | B03 | `WO-015` | `WO-020` | **`AS-007`** |
| Shop girder → Summit | B04–B11 | `WO-016`–`WO-023` | `WO-021`–`WO-028` | **`AS-008`–`AS-015`** |

Documents moved in the same pass:

| Old path | Current path |
|---|---|
| `03_WORK_ORDERS/WO-000..013_*.md` | `03_EXECUTION/KERNEL/WO-000..013_*.md` |
| `03_WORK_ORDERS/WO-014..028_*.md` | `03_EXECUTION/ASCENT/AS-001..AS-015_*.md` |
| `03_WORK_ORDERS/SECTION_PRE_RESOLVE_PROTOCOL.md` | `03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md` |
| `03_WORK_ORDERS/WORK_ORDER_TEMPLATE.md` | `03_EXECUTION/TEMPLATES/KERNEL_WORK_ORDER_TEMPLATE.md` |

The Atlas **band** is the stable key across every rename. The ticket number is bookkeeping.

If a later coding pass splits a row, split only at a falsifiable handoff. The queue may grow only
if Atlas §6 names a module this chain skipped.
