# SCRAPERX — START HERE

**Write branch: `ChatGPT`.** All implementation and all document writes land there.
Other branches may be read to recover provenance. None of them is a write target, and none of
them is an authority. `ScraperX-Claude` is the verified baseline this branch was reset from;
where another branch and this tree disagree, **this tree wins**.

---

## 1. What ScraperX is

A first-person physical ascent of a 1 600 m industrial tower. Every consequential fact — support
validity, traversal, machine state, structural coupling, process state, progression — is owned by
a native C++17 / Jolt simulation stepping at a fixed 90 Hz. Godot 4.7 owns presentation, input,
camera, HUD and Android delivery, and decides nothing.

The climb is won by operating real machines under finite limits. A route opens because a load
moved, not because a predicate flipped. Nothing teleports, nothing has unlimited force, nothing
bypasses collision, and no mechanism may strand the player without a legal recovery path.

---

## 2. Current position

The one block to read before doing anything. Everything below is detail behind it.

| | |
|---|---|
| **Write branch** | `ChatGPT` — the only one |
| **Kernel** | `WO-000`–`WO-013`, all fourteen in source. **Closed set** |
| **Ascent frontier** | `AS-001`–`AS-003` implemented; `AS-004` planned, not next; `AS-005`–`AS-007` imported, not re-derived; `AS-008`–`AS-015` unwritten. Reachable ceiling 154 m (the tower stair); the ascent above it follows `03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md` |
| **Integrated evidence** | **GREEN** at `8f9e954` — run [`36010028676`](https://github.com/IssisX/ScraperX/actions/runs/36010028676), all steps: native suite (`AS-003 apron`, `cage` and `Hook5 Rack` included, each held to its thresholds by the step's gates), Fold proof, all 18 `--uitest` scenarios (`touch_carry` included), the Movie Maker mix, world-solids drift check, Android arm64 cross-compile and APK export. Before it, green: [`35912759289`](https://github.com/IssisX/ScraperX/actions/runs/35912759289) (`0a77ec4`, 17 scenarios, `audio_mix bed_db=-23.3 steps_db=-14.5 peak_db=-0.8 steps=10`), [`35881134280`](https://github.com/IssisX/ScraperX/actions/runs/35881134280) (`0509496`, 16 scenarios), [`35880804993`](https://github.com/IssisX/ScraperX/actions/runs/35880804993) (`e413d8b`), [`35879820621`](https://github.com/IssisX/ScraperX/actions/runs/35879820621) (`45e610d`). Earlier: **GREEN** at `b4dc10b` — run [`35839443871`](https://github.com/IssisX/ScraperX/actions/runs/35839443871), all 15 scenarios then. Runs [`35818695969`](https://github.com/IssisX/ScraperX/actions/runs/35818695969) (`c8b5eed`) and [`35820568808`](https://github.com/IssisX/ScraperX/actions/runs/35820568808) (`7e831a2`) were red at the Android cross-compile only — a file-local alias of `kLethalImpactSpeedMps` that NDK clang rejects under `-Werror` (`-Wunused-const-variable`; GCC does not warn), removed in `a39974b`. `AS-002`'s falsifier group CI-integrated since run [`35798864772`](https://github.com/IssisX/ScraperX/actions/runs/35798864772) |
| **World collision** | Every static body the presentation draws is native collision: `godot/presentation/solid_export.gd` classifies each built part (solid, moving, non-solid, native mirror; unclassified fails) and generates `src/sim/world_solids.inc`, which the native world compiles in (1 808 bodies; drawn mirrors of owned bodies skipped by bounds). Native tests walk that same solid world; CI regenerates the table and fails on drift. Stairs are walked: a native step-up climbs any edge up to `0.35 m` (vault/mantle take over above it), and every stack flight rises through a stairwell cut in the deck it serves. The SKIN walkway at +32 m is 4 m deep, not 2: under the upper flight a standing player had one 0.4 m lane (reported as an opening too small to fit through); the lane is now 2.4 m (`AS-002` deviation). The plant's access landing no longer overhangs its second step (it left a 1.0 m slot). A crawl beam by the dock, underside 1.45 m, is the crouch fixture The yard sits in a closed alpine basin: a valley floor 0.3 m below grade out to a solid rim of 44 steep ridges (~1.1 km), so every direction ends against rock, not a void or an invisible wall |
| **Sky and light** | `godot/presentation/sky_cycle.gd` + `sky.gdshader`: a moving sun (24 real minutes per day, noon elevation 58 deg) that becomes the moon at night on the one shadowed light, sky dome, drifting lit cloud deck, stars, dusk glow; ambient from the sky, fog and exposure follow the sun. Shadows: 4096 atlas on every platform (Android's default was 2048), PCF soft filter 3 (2 on mobile), four blended cascades over 220 m. Presentation only; native has no time of day |
| **Audio** | `godot/presentation/audio/`: a bank synthesised at startup from seeded noise and partials (23 cues, ~0.35 s on a worker thread; no audio files ship), voiced for a phone speaker (identity above 300 Hz, low thump only for weight), and a director that plays what the native reports: footsteps on the head-bob stride (concrete / steel / meadow; softer crouched), jump, landings scaled by impact, ledge grab, vault/mantle scrape, crouch rustle, canopy, lethal impact; altitude wind, a distant yard bed and a falling-air rush; at the plant, positional motor hum, steam hiss from the native orifice flow, the hoist chain at the scoop's speed, the lift drive, clangs where the ballast loses velocity, the tipper's creak; birds near the ground by day. Master ends in a hard limiter (+6 dB in, -0.8 dB ceiling). The first bank measured -27.6 dBFS (footsteps) / -48.6 dBFS (ambience) above 300 Hz — next to silent on a phone; `audio_mix` now measures the mix leaving Master under Movie Maker (headless runs' Dummy driver never mixes): ambience -23.3, footsteps -14.5 dBFS above 300 Hz, peak -0.8, CI step "Measure the mix a player hears" (the same figures in run `35912759289`). Measured, **never listened to** — no output device here. **Not done: Governing Law 8's human fear voice on large falls — it needs recorded performances** |
| **Interface** | GDD §22 control surface in `godot/presentation/ui/`: touch (floating stick, drag-look, Jump, contextual Action, Drop/Chute on state, pendant controls only while operating), gamepad and keyboard over one verb vocabulary; sparse HUD (reticle cues, prompts, fall gauge against the native lethal speed, altimeter, station panel), pause with SETTINGS, GRAPHICS (quality preset LOW-ULTRA, render scale, shadows OFF-ULTRA, MSAA, bloom, frame-rate cap, FPS readout) and DISPLAY (FOV, brightness, head bob, speed-FOV kick, time of day, day length) pages; first-person arms (`godot/presentation/first_person_arms.gd`) whose hands grip, plant and reach at the ledge points the native reports; a standing mantle steps in to the hang standoff before it climbs (swept, supported, skipped on moving ground) so the palms land on the lip; opt-in gyro aim (off by default; screen-axis mapping taken from Godot 4.7-stable's Android sensor code, not yet observed on a device); double-tap Jump vaults when the takeoff could have (native window 0.30 s, the ground vault's own probes); PICK UP / SET DOWN on the contextual Action for the native carry (Drop and Back also set down), both hands drawn on the load; crouch (native 1.2 m capsule, feet fixed; stands only where the full 1.8 m capsule clears, so a low gap keeps you down; Jump and Action stand first) on a touch CROUCH/STAND toggle, C or right-stick click, or held Ctrl, with the eye gliding 1.52 to 0.95 m. 18 headless `--uitest` scenarios drive real input events into native state changes (`touch_carry` added with `AS-003`); all 18, and the Movie Maker `audio_mix`, passed in CI run `36010028676` |
| **Next code job** | Restore the current full proof path to GREEN for the implemented ground water bucket/cage lift from `03_EXECUTION/PLANNING/GROUND_WATER_ASCENT.md`. The native suite passes locally; the full Fold/Android proof is pending. Do not begin another mechanism while that gate is red. `AS-006` Stages B and C are paused, not complete. |
| **Next authoring job** | Reconcile Atlas B00 and the active ascent ticket with the proposed ground-to-+8 m water route and the existing crane/gate/stair alternatives before calling implementation complete. `AS-007` authoring remains queued after `AS-006`. |
| **Authoring contract** | `03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md`; for everything above the stair, `03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md` |
| **Unproven** | Fold-device install, on-device execution, touch ergonomics, sustained frame rate. **No device access exists** |

---

## 3. Authorities

Read in this order. Higher wins on conflict.

| # | Document | Owns |
|---|---|---|
| 1 | `01_PRODUCT_AUTHORITY/00_GOVERNING_LAWS.md` | what the game may never do |
| 2 | `01_PRODUCT_AUTHORITY/01_SCRAPERX_GDD.md` | what the game is |
| 3 | `01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md` | the tower in metres, modules and braids |
| 4 | `02_ENGINEERING_AUTHORITY/00_EXECUTION_PROTOCOL.md` | how work is executed and claimed |
| 5 | `02_ENGINEERING_AUTHORITY/01_TECHNICAL_ARCHITECTURE_TDD.md` | how the engine is built |
| 6 | repository source / tests / CI evidence | what is actually true |
| 7 | the one open ticket | the current job |

The atlas is the GDD written in metres. It is not a sidecar spec.
`01_PRODUCT_AUTHORITY/support/` is provenance for how the GDD was closed — never a second product.

`Mechanism-Ideas-and-archetypes.md` (repository root) is the owner's catalogue of 20 lift
archetypes: templates the ascent is built from, to be mutated and recombined freely. It is input
to the mechanism ascent plan, not an authority, and it is not edited.

---

## 4. Vocabulary — one concept, one name, one home

Kernel engineering and ascent construction are different classes of work and never share an
identifier.

| Class | ID form | Home | What it is |
|---|---|---|---|
| **Kernel Work Order** | `WO-000`–`WO-013` | `03_EXECUTION/KERNEL/` | foundational engineering. Proves one *tool type* on the `KX-*` substrate at source x ≈ 200. **Closed set — no new `WO-*` is ever created** |
| **Ascent Slice** | `AS-001`–`AS-015` | `03_EXECUTION/ASCENT/` | one causal slice of the 1.6 km climb. The live work |
| **Atlas band** | `B00`–`B11` | Atlas §6 | a floor range of the tower. **Never a filename, never a ticket** |
| **Atlas chain** | `K0`–`K8` | Atlas §7 | a causal chain spanning bands |
| **Campaign module** | `MOD-*` | Atlas §6 | a world object in the real tower |
| **Kernel substrate** | `KX-*` | Atlas §9 | a kernel fixture. Regression substrate; never retitled into a `MOD-*` |
| **Capability** | `CAP-*` | Atlas §4 | something the player holds or has acquired |
| **Transfer plate** | `TP-###` | Atlas §5 | a stable handoff support at a named elevation. `TP-120` is the plate at 120 m |
| **Commit** | — | `commit_checkpoint()` in source | the in-sim automatic checkpoint. A *gameplay* concept. There is no `CP-*` document class and none may be introduced |
| **Process document** | named, never numbered | `02_ENGINEERING_AUTHORITY/`, `03_EXECUTION/PLANNING/` | the protocol and the authoring contract |
| **Template** | named, never numbered | `03_EXECUTION/TEMPLATES/` | the form a ticket takes |

### Where things live

```
00_START_HERE.md              this file — position, vocabulary, ledgers
01_PRODUCT_AUTHORITY/         what the game is        (laws, GDD, atlas, + support/ provenance)
02_ENGINEERING_AUTHORITY/     how work is executed    (protocol, TDD, evidence snapshot)
03_EXECUTION/
    KERNEL/                   WO-000 .. WO-013        closed, historical, regression substrate
    ASCENT/                   AS-001 .. AS-015        the live queue
    PLANNING/                 the ascent authoring contract and the mechanism ascent plan
    TEMPLATES/                ticket forms
src/ tests/ godot/            implementation truth
```

A directory holds exactly one kind of artifact. If something does not fit one of these, it is
probably not supposed to exist.

---

## 5. Lifecycle, provenance and evidence are three different things

This is the distinction the whole control plane rests on. Collapsing them is how a stale plan
gets coded, or a plan gets reported as a working game.

| Field | Answers | Values | Changes when |
|---|---|---|---|
| **Lifecycle** | does source exist? | `IMPLEMENTED` · `PLANNED` · `UNAUTHORED` | someone writes code |
| **Provenance** | were its numbers derived against *this* tree? | re-derived here · ⚠ imported, NOT re-derived | someone re-authors it |
| **Evidence** | what has actually been observed? | a named CI run and a named proof line | **every CI run** |

Every ticket header carries lifecycle, provenance and an implementation gate. **No ticket carries
its own evidence** — it points at the ledgers in §6 and §7 below. Evidence changes every run while
lifecycle changes only when code is written, so evidence copied into a ticket goes stale silently.
It lives in exactly one place.

`IMPLEMENTED` is a claim about source and nothing else. Proof is a separate, named thing.

⚠ **imported, NOT re-derived** is a hard gate. Such a ticket's geometry, entity ids and persist
versions describe another branch's world. Re-author it against this tree before coding any of it.

---

## 6. Kernel ledger

`03_EXECUTION/KERNEL/`. All fourteen are in source. The kernel exists to be regression substrate,
not to grow.

| Ticket | Slice | Lifecycle | Evidence |
|---|---|---|---|
| `WO-000` | Godot 4.7 → GDExtension → fixed-step native sim → arm64 APK | IMPLEMENTED | the CI build + export steps |
| `WO-001` | native player on static `KX-DECK` | IMPLEMENTED | **record gap** — see below |
| `WO-002` | translating / rotating support-point velocity | IMPLEMENTED | falsifier |
| `WO-003` | vault / mantle / ledge / hang on real geometry | IMPLEMENTED | falsifier |
| `WO-004` | Fold-aspect stage, industrial identity | IMPLEMENTED | `SCRAPERX_WO004_VIEWPORT_PROOF` |
| `WO-005` | exterior grade and approach | IMPLEMENTED | `SCRAPERX_WO005_APPROACH_PROOF` |
| `WO-006` | vessel → orifice → cylinder → lift | IMPLEMENTED | falsifier + `SCRAPERX_WO006_MACHINE_PROOF` |
| `WO-007` | Kellerworks identity, alpine backdrop | IMPLEMENTED | screenshot only |
| `WO-008` | fall, chute, commit, survived lower landing | IMPLEMENTED | falsifier |
| `WO-009` | kernel chain + persist / reload | IMPLEMENTED | falsifier |
| `WO-010` | player mass drives the treadle | IMPLEMENTED | falsifier |
| `WO-011` | `KX-JIB` moves `KX-CRATE` | IMPLEMENTED | falsifier |
| `WO-012` | seated `KX-NEEDLE` changes traversal | IMPLEMENTED | falsifier |
| `WO-013` | `KX-SUMP` changes `KX-GRATE` | IMPLEMENTED | falsifier |

The fourteen kernel files predate the §5 header and keep their original `**Status:**` line. Their
Existing-truth and Result-record sections quote evidence and CI names **as they stood when the work
was executed** — including the retired "ScraperX-2 checkpoint CI". Those are historical execution
records, deliberately not rewritten. Retconning a proof record is worse than an out-of-date name.

> **Record gap, `WO-001`.** Its Result record reads `pending` on every line, yet the player body,
> its capsule and its support identity are in source and are exercised by every falsifier that
> follows. The code is real; the paperwork was never filled. Recorded here rather than silently
> promoted. Closing it is bookkeeping, not engineering.

---

## 7. Ascent ledger

`03_EXECUTION/ASCENT/`. Fifteen slices, each bound to one Atlas band. All new work happens here.

| Ticket | Slice | Band | Lifecycle | Provenance | Evidence |
|---|---|---|---|---|---|
| `AS-001` | apron → +24 m, intake rise | B00 | **IMPLEMENTED** | re-derived here | falsifier, green in run `35727055407` |
| `AS-002` | first legal stand at +40 m | B00 | **IMPLEMENTED** | re-derived here | falsifier, green in run `35798864772` |
| `AS-003` | `CAP-HOOK5` acquire | B00 | **IMPLEMENTED** | re-derived here | falsifiers, green in run `36010028676` (`AS-003 apron`, `cage`, `Hook5 Rack`; `touch_carry`) |
| `AS-004` | seat needles at 96 m | B01 | PLANNED | re-derived here; revalidated; the stair stays, so not the next job | none |
| `AS-005` | cage land at 120 m, or east climb | B01 exit | PLANNED | ⚠ imported | none |
| `AS-006` | counterweight well, 154 → 220 m: skip lift, derrick boom, debris chute | B02 | IN PROGRESS | re-derived here under the mechanism ascent plan; Stage A built (rigging kit, skip lift, UNHOOK / HOOK / GRAB / LET GO on touch, gamepad and keyboard) — Stages B and C **paused for ground rehabilitation** | `PASS scraperx_sim AS-006 A no link`, `AS-006 A ride`; uitests `touch_rig`, `pad_rig`, `keyboard_rig` (local; CI pending) |
| `AS-007` | blind + drain, or north climb to 340 m | B03 | PLANNED | ⚠ imported | none |
| `AS-008` | seat `MOD-GIRDER-T` | B04 | UNAUTHORED | — | none |
| `AS-009` | hang rail / traveler stroke | B05 | UNAUTHORED | — | none |
| `AS-010` | service lift or SKIN | B06 | UNAUTHORED | — | none |
| `AS-011` | wind-frame structure / SKIN | B07 | UNAUTHORED | — | none |
| `AS-012` | isolate high riser / drum clutch | B08 | UNAUTHORED | — | none |
| `AS-013` | jack + seat crown beam | B09 | UNAUTHORED | — | none |
| `AS-014` | isolate + lock fans; bell as support | B10 | UNAUTHORED | — | none |
| `AS-015` | stand on 1600.00 m | B11 | UNAUTHORED | — | none |

`AS-005`–`AS-007` still quote `ScraperX-Grok` constants — a handoff at `(10.40, 24.00, 38.40)`,
68 treads at `x = 10.40`, entity id `319`, a well at `(-1.76, —, 25.30)`. None of that exists here.

---

## 8. What is next

> **Next job: ground mechanism rehabilitation, one physical mechanism per work cycle.**
> `03_EXECUTION/PLANNING/GROUND_WATER_ASCENT.md` holds the full paired contract. First restore the
> current required proof path to GREEN. Then implement and prove only the Archimedes screw: finite
> motor work transfers conserved water from the apron basin to a supported +5..5.5 m tank. The
> receiving bucket/cage lift stays PLANNED / NOT BUILT in that same file, with its water input,
> rope ratio, energy budget, +8 m landing, and reset contract already resolved. Completing the
> pump does not complete the ascent stage. The player must actually stand at +8 m after a later
> lift cycle before that claim is allowed. Reconcile the new route with Atlas B00 and the existing
> crane/gate/stair alternatives. The reported belt support failure remains open.
>
> `AS-006` Stage A and its prior evidence are preserved; Stages B and C are paused, not passed.
> `AS-004` remains PLANNED, and `AS-007` authoring remains queued. Continue above the stair only
> after ground mechanisms are finished to the player's runtime standard.

Do not open two mechanism implementations in one work cycle.

---

## 9. Context-loading rule

Do **not** load every file into every task. Load:

- Governing Laws;
- Execution Protocol;
- the one open ticket;
- the relevant source / tests / config;
- only the GDD / atlas / TDD sections that ticket cites.

Atlas loading by class:

- `WO-000`: no atlas.
- `WO-001`–`WO-004`: Atlas §§2, 9 (datum + kernel deck).
- `WO-005`–`WO-013`: Atlas §§4, 7, 8, 9, 12.
- `AS-*`: Atlas §6 for **its own band only**, plus §§4, 5, 7, 12, 13 as the slice cites them.
  Loading bands the slice does not own is how scope creeps.

Use `01_PRODUCT_AUTHORITY/support/` only to trace a closed product decision.

---

## 10. Claim discipline

Never collapse

`implemented → built → APK produced → installed → executed → observed → verified on Fold`

into one word such as "done". Evidence must match the claim, and the claim must name its evidence.

A plan is not an implementation. `PLANNED` is not `IMPLEMENTED`, and `IMPLEMENTED` is not proven.
A green aggregate is not a green falsifier — name the line.

---

## 11. Retired and excluded

**Retired identifiers.** `ASC-*` (campaign prefix, lived one day, now `AS-*`); `03_WORK_ORDERS/`
(one directory holding four kinds of artifact); `WO-014`–`WO-028` as campaign tickets. The full
old-name → current-name map is `03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md` §5.1. Do not
reintroduce any of them; resolve them through that table.

**Sibling-branch identifiers.** `ChatGPT` numbers its kernel `KI-000`–`KI-008` over a different
decomposition of the same work; that prefix is deliberately **not** adopted, because the same
prefix over two different sets is a worse collision than two different prefixes. Its `AS-*`
campaign numbering is the same as ours and was adopted for exactly that reason.

**Not implementation authority, and not to be reintroduced:** questionnaire executables or JSON
state; rejected drafts; GraveSpire history; old branches and builds; discarded mathematics; any
second "content package" outside this tree.

`MANIFEST.txt` and `SHA256SUMS.txt` are a **frozen snapshot of the v1.1 delivery package**. Their
paths predate this layout and are not maintained. They are kept as a delivery record, not as an
index of the tree.
