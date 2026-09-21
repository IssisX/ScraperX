# SCRAPERX — MECHANICAL PRE-RESOLVE JOB CHAIN

**Origin:** authored on `ScraperX-Grok` as `SECTION_PRE_RESOLVE_PROTOCOL.md`; adopted here
on 2026-09-21 with ticket numbers remapped to this branch's spine (see §5.1). The prose is
theirs. The status claims are re-derived from evidence on this branch.

**Use:** Paste this entire file as the standing instruction for every later section response.
**Repo:** `IssisX/ScraperX`
**Branch:** `ScraperX-Claude`
**Mode:** planning documents only. No C++, Godot scenes, tests, CI, or APK in this pass.
**Push rule:** review-then-push. Show the full file in chat. Wait. Write/commit/push that one file only after explicit approval of that file.

**Names.** One ticket list: `WO-000`–`WO-028` in `03_WORK_ORDERS/`. `B00`–`B11` exist only as atlas band IDs in `02_ASCENT_ATLAS.md` §6 (floors of the tower). Never name a work-order file `B##`. C++ tokens such as `kB00StairDY` are frozen source names; leave them.

---

## 0. Why this exists

Later coding must spend energy on implementation, not on inventing geometry, ratings, or causality.

Each section file is the job ticket a coder can follow without inventing a module, a gate, a recovery path, or a climbable/filler distinction.

The atlas is the map. The kernel work orders (`WO-000`–`WO-009`) proved the tool types. `WO-014_INTAKE_RISE.md` plus source proved the first 0–24 m campaign slice. This chain authors the remaining tickets, one causal slice at a time.

---

## 1. Chat register

Surrounding chat is short and plain. The markdown file is the engineering artifact.

Do:

- name the slice in one sentence
- say what machine or route this slice closes
- say where it stops
- paste the **full** file
- ask whether to push **this** file

Do not:

- dump the whole tower
- lecture about kernels vs campaign in every reply
- number the user
- summarize the file instead of pasting it
- start the next file in the same reply
- write C++ “to illustrate”

---

## 2. What one section is

One section = the smallest complete causal slice that a later coding pass can implement without looking at the next ticket.

A valid slice has all of:

1. A named **entry state** inherited from source and/or the previous section file.
2. One owned mechanism **or** one braid handoff that changes PLAY.
3. A falsifiable **exit state** the next file inherits (meters, support identity, machine poses, persist fields).
4. Atlas §13 answered for **this** slice with no new tower.

It is not:

- a whole atlas band unless that band is already one chain (atlas band B00 0–24 was one chain; `WO-015_LEGAL_FORTY` is the leftover PLAY of K0, not all of atlas band B01)
- a visual pass
- a kernel re-proof
- three macros forced into one floor (atlas §12.8, GDD §10 region-dependent coupling)

Gold standard for freeze quality: the “Pre-resolved mechanism” block in `03_WORK_ORDERS/WO-014_INTAKE_RISE.md`.

---

## 3. Authority load (minimum)

Always load:

- `00_START_HERE.md` (spine + current job only)
- `01_PRODUCT_AUTHORITY/00_GOVERNING_LAWS.md` — only laws cited by the slice
- `01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md` — §6 of the named band, §7 chain row, §12, §13; plus §4/§5/§8 if the slice uses a CAP, Transfer Plate, or commit
- `03_WORK_ORDERS/WORK_ORDER_TEMPLATE.md`
- the previous section file’s **Completion / exit state**
- current source constants for any machine this slice reuses (`src/sim/simulation.cpp` `kB00*` and player capsule)

Load GDD / TDD / Execution Protocol **only** for the sections that constrain this slice (typical: GDD §§7, 11–17, 23–24; TDD §§6, 8–11, 14; Protocol §§3–7, 11–12).

Do not load `ChatGPT` branch files as authority. Do not load `01_PRODUCT_AUTHORITY/support/` unless tracing a frozen product decision.

Conflict order: Laws > GDD > Atlas > Execution Protocol > TDD > current source > current work order.

---

## 4. Hard rules

1. **No invention.** Module IDs, CAP IDs, band cuts, braid names, and example couplings come from the atlas. If the slice needs an ID not in the atlas, stop (see §9).
2. **Do not retcon.** Do not rewrite WO-000–008, CP-003, CP-004, or `WO-014_INTAKE_RISE.md`. Do not treat CPs as TDD completion.
3. **Do not duplicate B00 0–24.** That slice is already specified and in source at HEAD. Inherit its numbers.
4. **Kernel stays substrate.** Do not rename `KX-*` objects and call them campaign modules.
5. **Native owns consequence.** Godot presents. Flags, animation, mission state, and mesh-swap-without-collision-change are not causality.
6. **Finite machines.** `τ = r × F`. SWL, stroke, brake, travel, occupancy. No teleporting load. No unlimited drive.
7. **Support identity is the route.** Climbable members are collision-honest and load-bearing at a declared rating. Filler cannot be stood on, or it is not filler.
8. **SKIN is a legal braid.** Do not block a valid SKIN climb to protect a SHAFT puzzle.
9. **Not every band needs all three macros.**
10. **Coordinate honesty.** Atlas is z-up, +X east, +Y north, origin = top of apron at tower center. This tree’s sim/Godot is Y-up. Every geometry table maps both. See §6.
11. **Design target vs frozen.** Atlas masses/strokes/SWL are design targets until a named native constant exists. Quote existing `kB00*` / kernel constants verbatim. Newly derived numbers are labeled `DESIGN TARGET` with the formula shown. Do not silently change a frozen source constant in a planning file.
12. **Claim discipline.** This pass authors documents. Result record stays `pending`. Do not claim implemented / built / APK / Fold.
13. **One file per response.** Full body, template-complete, no “snip.”
14. **Push only after explicit approval of that file.**

Forbidden shortcuts (always in force): painted parkour stripe; mission flags as gates; invisible walls vs SKIN; chute teleport / powered lift; animation-owned machines; mesh-swap without collision change; reset-on-band-load; loading atlas bands B01–B11 inside the B00 intake slice; duplicating `WO-014_INTAKE_RISE`; claiming CP-003 hopper as WO-011; second atlas; stranding with no recovery path.

---

## 5. Section queue (author in this order)

Do not skip SKIN as a legal braid. Do not start at atlas band B01 (Transfer Hall). Do not reopen WO-009 or re-author 0–24 m.

Queue number **is** the WO number.

| WO | File to author | Slice | Atlas band | Status on this branch |
|---|---|---|---|---|
| 014 | `WO-014_INTAKE_RISE.md` | 0–24 m | B00 | Plan adopted. **Code in progress.** |
| 015 | `WO-015_LEGAL_FORTY.md` | first legal stand at atlas `z ≥ 40 m` | B00 leftover + K0 PLAY | **Authored here.** Not coded. |
| 016 | `WO-016_HOOK5_RACK.md` | `MOD-HOOK5-RACK` / `CAP-HOOK5` acquire | B00 | **Authored here.** Not coded. |
| 017 | `WO-017_NEEDLE_SEAT.md` | seat needles at `MOD-NEEDLE-POCKETS` | B01 + K1 | **Authored here.** Not coded. |
| 018 | `WO-018_CAGE_OR_SKIN.md` | cage landing at TP-120 vs `MOD-EAST-OUTRIGGER` | B01 exit | Plan adopted. Not coded. |
| 019 | `WO-019_CW_PIN.md` | `MOD-CW-PIN` / stack as moving support | B02 + K2 | Plan adopted. Not coded. |
| 020 | `WO-020_WET_ISOLATION.md` | blind + drain vs SKIN around wet core | B03 + K3 | Plan adopted. Not coded. |
| 021 | `WO-021_SHOP_GIRDER.md` | seat `MOD-GIRDER-T` | B04 + K4 | Not authored. |
| 022 | `WO-022_FACADE_TRAVELER.md` | hang rail / traveler stroke | B05 + K5 | Not authored. |
| 023 | `WO-023_MIDSTACK.md` | service lift or SKIN; do not force three macros | B06 | Not authored. |
| 024 | `WO-024_WIND_FRAME.md` | wind-frame structure / SKIN | B07 | Not authored. |
| 025 | `WO-025_HIGH_HEADER.md` | isolate high riser / drum clutch | B08 + K6 | Not authored. |
| 026 | `WO-026_CROWN_BEAM.md` | jack + seat crown beam | B09 + K7 | Not authored. |
| 027 | `WO-027_FANS.md` | isolate + lock fans; bell as support | B10 + K8 | Not authored. |
| 028 | `WO-028_SUMMIT.md` | stand on `z = 1600.00 m`; no flag shortcut | B11 | Not authored. |

### 5.1 Ticket-number map

`ScraperX-Grok` authored these plans against its own spine, which numbered the shared kernel
work differently from this branch. The Atlas **band** is the stable key; the ticket number is
bookkeeping. Mapping applied when the plans were ported:

| Slice | Atlas band | `ScraperX-Grok` | Here |
|---|---|---|---|
| First freight | kernel | WO-005 | WO-011 |
| First structural coupling | kernel | WO-006 | WO-012 |
| First process coupling | kernel | WO-007 | WO-013 |
| First causal chain | kernel | WO-008 | WO-009 |
| Fall / parachute / checkpoint | kernel | WO-004 | WO-008 |
| Intake rise | B00 | WO-009 | WO-014 |
| …campaign rows | B00–B11 | WO-0NN | WO-0(NN+5) |

If a later coding pass splits a row, split only at a falsifiable handoff. Never author two rows in one response.

Queue may grow **only** if atlas §6 names a module this chain skipped (the first likely extra is none; `MOD-HOOK5-RACK` is already `WO-016` because `WO-014` left it out of scope).

---

## 6. Datum and unit law (every file)

SI only: m, kg, s, rad, N, N·m, Pa.

**Atlas frame**

- origin `(0, 0, 0)` = top of apron slab at tower geometric center
- +X east, +Y north, +Z up
- summit walking surface `z = 1600.00 m`

**Source / Godot / Jolt frame in this tree**

- +X east
- +Y up  (= atlas Z)
- +Z north (= atlas Y)

Mapping:

```
source.x = atlas.x
source.y = atlas.z
source.z = atlas.y
```

Every geometry row lists **atlas (x, y, z)** and **source (x, y, z)**.

Player capsule already frozen (do not change):

- `kPlayerCapsuleRadius = 0.35 m`
- `kPlayerCapsuleCylinderHalfHeight = 0.55 m`
- standing half-height = `0.90 m`
- max relative walk `kPlayerMaximumRelativeSpeed = 5.5 m/s`
- support-point velocity law: `v_support_point = v_linear + ω × r`

Gravity: `9.81 m/s²` (`kB00Gravity`).

---

## 7. B00 0–24 numbers

**Claim class on this branch: `DESIGN TARGET`.** On `ScraperX-Grok` these were written as frozen
source constants. They are not frozen here: that branch's source has not compiled since
2026-09-20, and at its last compiling commit every falsifier — kernel included — was red. So
these numbers are inherited as a *specification* for `WO-014`, not as quoted source. Once
`WO-014` lands in `src/sim/simulation.cpp` with a green falsifier, the constants that ship
become frozen and later B00 files must consume those.

Geometry is additionally re-sited here: this branch already has a tower at source
`(0, —, -150)`, so B00 is built against that tower's south face rather than against a bare
origin. Ratings, strokes, masses and moments carry over unchanged; positions are re-derived.

Yard jib `MOD-YARD-JIB`:

- boom `12.00 m`, boom height `11.50 m`
- mast source `(-6.50, —, 8.00)` with boom height as Y
- SWL `5000 kg`
- rated slew moment `τ = 12 × 5000 × 9.81 = 588600 N·m`
- rated winch force `5000 × 9.81 = 49050 N`
- 4 t pack `4000 kg` → `F = 39240 N`
- 9 t overweight must stall
- slew `[-0.90, 0.90] rad`, winch `[2.80, 10.20] m`, hoist `0.85 m/s`, slew `0.22 rad/s`

Intake belt: stroke `18.00 m`, `ω = 0.40 rad/s`, riding is legal.

Dog: hinge source `(11.90, 1.45, 13.50)`, retract `+1.45 rad` at `0.62 rad/s`, latch clear Y `3.08 m`. Stair open is dog collision pose, never a flag.

Stair-A (built through +24): `x = 10.40`, `y0 = 0.00`, `dy = 0.353 m/tread`, `z0 = 14.20`, `dz = 0.320 m/tread`.

Handoff already in source: source `(10.40, 24.00, 38.40)` = atlas `(10.40, 38.40, 24.00)`. That is the **entry** of section 1, not its exit.

SKIN-S start: source x `-17.00`, z `16.60`, always climbable.

Unaided shove range (atlas): `200–400 kg`. 4 t is machine work.

If section 1 (or any later slice) wants the yard jib to place the player on a +40 m soffit, **derive reach from the frozen 12 m boom / 11.50 m height / winch travel**. If that PLAY cannot close, report the gap. Do not lengthen the boom. Do not invent a second jib.

---

## 8. Required mechanical close (every file)

The file must contain a `## Mechanical close` section that fills every row below. If a row is N/A, say why in one line (example: no actuator in a pure stair continuation). Blank rows are a fail.

### 8.1 Identity

- Band, Z range this slice actually authors (not the whole band unless it is the whole band)
- Chain ID from atlas §7 if any (`K0`…`K8`)
- Braids live in this slice (`SHAFT` / `SKIN` / `FLOW`)
- Modules allowed (atlas IDs only)
- Modules forbidden (nearby IDs this slice must not build)

### 8.2 Entry state

Quote the previous file’s exit and the source constants consumed. Player support identity at entry. Machine poses that must already be true (or explicitly “unknown / either,” if both legal). Persist fields already committed.

### 8.3 Geometry

Table of every consequential member this slice adds or extends:

- atlas ID / local member name
- atlas (x, y, z) origin or hinge
- source (x, y, z)
- extents (half-x, half-y, half-z) or length × section
- climbable or filler
- declared rating if climbable (player + pack, or machine load)

Rise/run for stairs. Pocket spacing for beams (atlas: needle pockets 18 m at `z = 96 m`). Rail stroke for travelers.

Clearance vs capsule: standing width, hang/mantle reach, well openings, dog/gate throat. State the gap in meters, not “enough room.”

### 8.4 Mechanism (if any machine works)

For each actuator:

- bodies, joints, attachment interface
- command channels (`Drive` / `Raise` / `Lower` / `Brake` / …)
- mass, SWL, `τ = r × F` at declared working radius
- force/torque/power/travel/brake limits
- stall conditions (overload, out-of-travel, occupancy, isolation, alignment)
- what the actuator is **not** allowed to write (final transform, mission flag)

Show the arithmetic. Example quality: `τ = 12 × 5000 × 9.81 = 588600 N·m`.

Energy: report the work/limit the solver will enforce (winch force, moment, brake hold). Do not invent a player-calorie model. GDD fatigue is not a section-close quantity unless a later authority freezes it.

### 8.5 Occupancy and interlocks

What body in which envelope stalls which travel. Numbers in meters. No `crate_moved` / `gate_open` / `needles_seated` flags as causality. Predicates may be derived from pose/contact/graph for HUD and tests, but WORLD follows bodies.

### 8.6 Required causal path

Indented atlas grammar, one primary and every atlas-legal alternate for this slice:

```
ACT[player verb + named object]
  → STATE[owned field]
  → WORLD[geometry / support / limit / process / access]
  → PLAY[new traversal or capability]
```

Illegal: `ACT → PLAY` with no STATE; `MISSION_FLAG → WORLD`; animation success; deleting aftermath so the next band loads clean.

### 8.7 Support / traversal handoff

Ordered list of what the player stands on, from entry to exit. Each line: member ID, source Y (atlas Z), support type (static / kinematic / dynamic), inherited velocity yes/no.

Handoff is a change of support identity, not a teleport.

### 8.8 Failure states

For each: trigger, what the world does, what the player can still do, what must **not** happen.

Minimum set when relevant: overload, out-of-travel, brake-held, occupancy stall, unseated member, wet/live process, missed chute clearance, SKIN used while freight unsolved.

### 8.9 Recovery

Atlas §3 soft-lock rule. Name one recovery that does not require the lost capability:

- remaining braid (usually SKIN)
- chute landing field from atlas §8.3 (apron, plate roofs, booms if present, not powered return)
- last commit if unrecoverable

Do not strand. Do not auto-repair.

### 8.10 Persist

If this slice consumes WO-009 persist: list the native fields that must survive commit/reload. If it does not: say “no new persist fields; inherit B00 0–24 blob.”

First stable stand at a Transfer Plate or marked refuge follows atlas §8.1. B00 exit commit is `z ≥ 40 m` with stable support.

### 8.11 Falsifiers (deterministic proof)

Named native tests the later coding pass must add. Each is a sentence that can fail.

Always include:

- unsolved is not traversable as claimed
- solved is
- atlas alternate path works without mutating the skipped machine
- kernel / prior-slice regressions stay green
- moving only a presentation/mission variable cannot produce PLAY

### 8.12 Exit state

Meters. Support identity. Machine poses. Whether +next-elevation is visible and unfinished. What the next file is allowed to assume.

End the file with:

**Stop. Do not begin the next file inside this one.** Name the next file from the queue.

---

## 9. Hard stop (write nothing further)

If atlas §6 for this slice cannot answer atlas §13 without invention, **do not author a fake WO**. Report the gap in chat, naming:

- the missing module or CAP
- the PLAY that does not close given frozen ratings/geometry
- the braid handoff or recovery that is absent
- the question from §13 that has no atlas answer

Do not lengthen booms, add unnamed machines, or invent IDs to look complete.

Examples that must stop the file:

- “yard jib places the player on +40 m soffit” if the frozen 12 m / 11.50 m jib cannot reach that plane
- a needle pocket not at a declared elevation
- a process line with no drain/isolation predicate
- SKIN “blocked until the puzzle is solved”

---

## 10. File template

Path: `03_WORK_ORDERS/<FILE_FROM_QUEUE>.md`

Fill every `WORK_ORDER_TEMPLATE.md` field, then append `## Mechanical close` from §8.

Skeleton:

```
# SCRAPERX — <TITLE>

**Work Order:** `<ID>`
**Status:** READY after <previous> / BLOCKED if gap
**Depends on:** <previous file + kernel proof>

## Objective
## Existing truth
## Authority
## Owner
## Allowed seam
## Required causal path
## Forbidden shortcuts
## Implementation scope
## Out of scope
## Proof path
## Completion
## Result record
pending

## Mechanical close
(all of protocol §8)

Stop. Do not begin the next file inside this one.
Next file: `<name>`
```

Owner is almost always: native 90 Hz C++/Jolt. Godot presents. Mission/UI observe predicates only.

Allowed seam: reuse kernel primitives (moving support, finite machine, distance-constraint hook, kinematic occupancy, seatable member, isolation graph, checkpoint). New campaign `MOD-*` geometry. Do not retitle `KX-*`.

Implementation scope names modules and climbable vs filler. Out of scope is the next queue row, Fold 45 FPS, art/VO, hard-fail missions (atlas §8.2: none), reopening WO-009.

Proof path separates claim classes (Law 29). This planning pass does not execute.

---

## 11. Review-then-push (mandatory)

**Step A.** Show first: proposed path + complete markdown body. No summaries. No snip.

**Step B.** Stop. Approval is a clear go-ahead for **that file** (“approved”, “push it”, “it’s all good”). Silence, questions, and requested edits are not approval. If they request edits, paste the full revised file and wait again.

**Step C.** Only after approval: write that file to `03_WORK_ORDERS/` on `ScraperX-Claude`, commit **only that file**, push `IssisX/ScraperX` `ScraperX-Claude`. No C++, no atlas rewrites, no unrelated README churn in the same commit.

Until Step C is authorized: no GitHub writes.

If push credentials fail after approval, say so. Do not pretend it landed.

---

## 12. First action when this protocol is active

Nothing in the `WO-014`–`WO-028` queue is in source on this branch yet.

- `WO-014` is in source and green (CI run `35651140478`).
- Next **code** job: `WO-015_LEGAL_FORTY` (B00, 24–40 m).
- `WO-015`, `WO-016` and `WO-017` are **authored against this branch**, replacing
  the `ScraperX-Grok` ports whose inherited constants described a world that does
  not exist here. `WO-018`–`WO-020` are still the unmodified ports and carry the
  same defect; re-author each one before coding it.
- Next **plan** to author: `WO-018_CAGE_OR_SKIN.md`.

Do not implement `WO-015` until `WO-014` is in source with a passing falsifier. Do not treat a
plan's numbers as proven because they are written down.
