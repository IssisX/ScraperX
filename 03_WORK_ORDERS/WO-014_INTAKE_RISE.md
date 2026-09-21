# SCRAPERX — WO-014 INTAKE RISE (~0 m TO +24 m)

**Work Order:** `WO-014`  
**Status:** READY after WO-009 completion  
**Depends on:** the kernel slice (WO-000–WO-003, WO-008–WO-009, WO-011–WO-013) proven on `ScraperX-Claude`

> **Provenance.** Adopted from `ScraperX-Grok`, where this slice was authored as `WO-009_INTAKE_RISE`.
> Ticket numbers remapped per `SECTION_PRE_RESOLVE_PROTOCOL.md` §5.1. Geometry, ratings and
> falsifiers carry over as **DESIGN TARGET**: they were never proven in source on that branch
> (it has not compiled since 2026-09-20). Positions are re-sited against this branch's tower
> at source `(0, —, -150)`.

## Objective

Author the first real campaign slice of `B00 - Apron and Intake`, not another kernel fixture and not the whole 0–40 m band.

Required modules as real world content:

```text
MOD-APRON
  → MOD-INTAKE-BELT
  → pendant access
  → MOD-YARD-JIB
  → 4 t crate pack
  → MOD-DOG-A
  → physically moving gate/landing mechanism
  → newly traversable MOD-STAIR-A
  → stable player support around +24 m
     with the unfinished +40 m continuation visibly above
```

## Existing truth

The kernel is in source on `ScraperX-Claude` and green. Observed 2026-09-21,
`build/host/scraperx_sim_tests`, exit 0, 9 of 9 `PASS`:

```
PASS scraperx_sim moving-support truth
PASS scraperx_sim athletic traversal
PASS scraperx_sim player is the plant's missing component
PASS scraperx_sim first full causal chain
PASS scraperx_sim coupled machine
PASS scraperx_sim fall/parachute/checkpoint
PASS scraperx_sim first freight
PASS scraperx_sim first structural coupling
PASS scraperx_sim first process coupling
```

Kernel `KX-JIB`, `KX-NEEDLE`, `KX-SUMP` sit at source x ≈ 200 and remain regression substrate;
this slice does not move or retitle them. The tower frame is at source `(0, —, -150)`,
half-extent 26 m, and is presentation-plus-collision only — no campaign machine is sited on it
yet. Fold-device execution is not proven. Android APK export is proven by CI on this branch.

The claim on `ScraperX-Grok` that this slice was already "DONE in source, proven by Actions run
`35454049354`" does not hold: that run is the kernel causal-chain run at `b90c5911`, and it
predates every campaign commit. See `SECTION_PRE_RESOLVE_PROTOCOL.md` §7.

## Authority

- Laws 2–6, 11–17, 22–27, 29, 32–36
- GDD freight / traversal / sequence-break sections
- Atlas §§4, 6 (B00), 7 (K0 Intake), 12
- TDD: native 90 Hz / Jolt owns contact; Godot mirrors
- Execution Protocol §§4–7, 12

## Owner

Native 90 Hz C++/Jolt simulation. Godot presents authoritative state. Mission/UI may observe predicates only.

## Allowed seam

New B00 spawn/world using kernel primitives (moving support, finite machine, distance-constraint hook, kinematic travel, contact-ranked support). Do not retitle `KX-JIB` as `MOD-YARD-JIB`.

## Required causal path

```text
ACT[ride/walk belt → enter CAP-PENDANT → Drive/Raise/Lower/Brake]
  → STATE[finite actuator torque/speed/brake/travel; crate pose from Jolt]
  → WORLD[crate leaves MOD-DOG-A pin envelope; dog body travels]
  → PLAY[MOD-STAIR-A throat is physically open; walk to +24 m]

alternate: ACT[climb MOD-SKIN-LADDER-S] → PLAY[+24 m] with freight state unchanged
```

Pre-resolved mechanism (authoritative numbers):

- `MOD-YARD-JIB`: 12 m boom, 5 t SWL. Rated slew moment `τ = r × F = 12 × 5000 × 9.81 = 588600 N·m`. Rated winch force `5000 × 9.81 = 49050 N`.
- 4 t pack: `F = 39240 N`. At working radius ~11.1 m, `τ ≈ 436000 N·m` — inside rating, slower than empty. 9 t pack exceeds both force and moment and must stall.
- Hook is a real distance constraint. No teleporting load. Brake holds. Travel limits stop the winch/slew.
- `MOD-DOG-A` is a kinematic hinged gate whose travel stalls while the crate body occupies the latch envelope. Stair accessibility is the dog body's collision pose, never `crate_moved` / `gate_open` / mission flags / animation / timers / collision toggles.
- `MOD-INTAKE-BELT`: 18 m stroke kinematic slat deck. Support-point velocity and inherited momentum are law. Riding is legal.
- 4 t cannot be shoved by the unaided player (atlas unaided range 200–400 kg).

## Forbidden shortcuts

- renaming `KX-*` objects and calling that B00
- `crate_moved` / `gate_open` / mission flags / animation events as causality
- unlimited-force or teleporting hoist
- blocking `MOD-SKIN-LADDER-S` to protect the freight sequence
- building the rest of B00, B01, or new process systems
- neon, filler scaffold forest, toy mechanism arena, decorative dead machinery
- claiming Fold-device execution

## Implementation scope

Native B00 world + falsifier tests; Godot presentation twins; CI two Fold-aspect screenshots; Android arm64 APK with `libscraperx_native.so`.

## Out of scope

B00 +24–40 m completion, B01–B11, NPCs, new process graphs, Fold 45 FPS certification.

## Proof path

1. Native deterministic causal/falsifier tests.
2. Fold-aspect 1080×928 grade-level screenshot: tower reads as a giant machine.
3. Second screenshot: intake mechanism and traversal consequence.
4. Godot runtime using authoritative native state.
5. Android arm64 APK contains `libscraperx_native.so`.
6. The kernel falsifiers (WO-000–WO-003, WO-008–WO-009, WO-011–WO-013) remain green.

## Completion

- before the crate is removed, the main stair route is physically impassable
- moving only a mission/presentation variable cannot open it
- an overloaded or out-of-travel jib cannot solve the mechanism
- after the crate physically clears the dog, the mechanism travels and the main route becomes physically passable
- the SKIN bypass works without falsely mutating the freight mechanism
- player has stable support around +24 m; +40 m continuation is visible and unfinished
- Fold install / on-device play remain unverified

Stop at the first stable ~+24 m handoff.
