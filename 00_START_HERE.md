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
| **Ascent frontier** | AS-001–003 implemented; water screw/lift and dock→+12 m connection implemented. Tower stair reaches 154 m. AS-006 A/B are implemented to a ridden +198.25 m cage; C, independent climb and the +220 m handoff are unbuilt. AS-004/005/007 are audited BLOCKED plans; AS-008–015 unauthored. |
| **Integrated evidence** | **GREEN gameplay candidate `7e66eb6581f0f7053503b46633761a79bf4b230a`**, run [36243395759](https://github.com/IssisX/ScraperX/actions/runs/36243395759), job `108408022217`: native suite, Godot/Fold-aspect runtime and input proofs, audio, world-solids drift, Android arm64 compile and APK export/publication succeeded. Artifact `10907096807`, `ScraperX-build-7e66eb6581f0f7053503b46633761a79bf4b230a` (30,949,342 bytes), contains `ScraperX-7e66eb6581f0-arm64.apk`. APK SHA-256 `4f71bb476e7185814314340691944deb20616896d06bb18c99766bb4cb9f36de`. Known-green `2849550` is an ancestor, preserved. This is build/headless-runtime evidence, not Android device execution. Documentation-only descendants do not create a new gameplay proof. |
| **World collision** | Static-world export checks authored solid coverage, not perfect agreement for every moving assembly or apparent opening: `godot/presentation/solid_export.gd` classifies each built part (solid, moving, non-solid, native mirror; unclassified fails) and generates `src/sim/world_solids.inc`, which the native world compiles in (1 808 bodies; drawn mirrors of owned bodies skipped by bounds). Native tests walk that same solid world; CI regenerates the table and fails on drift. Stairs are walked: a native step-up climbs any edge up to `0.35 m` (vault/mantle take over above it), and every stack flight rises through a stairwell cut in the deck it serves. The SKIN walkway at +32 m is 4 m deep, not 2: under the upper flight a standing player had one 0.4 m lane (reported as an opening too small to fit through); the lane is now 2.4 m (`AS-002` deviation). The plant's access landing no longer overhangs its second step (it left a 1.0 m slot). A crawl beam by the dock, underside 1.45 m, is the crouch fixture The yard sits in a closed alpine basin: a valley floor 0.3 m below grade out to a solid rim of 44 steep ridges (~1.1 km), so every direction ends against rock, not a void or an invisible wall |
| **Sky and light** | `godot/presentation/sky_cycle.gd` + `sky.gdshader`: a moving sun (24 real minutes per day, noon elevation 58 deg) that becomes the moon at night on the one shadowed light, sky dome, drifting lit cloud deck, stars, dusk glow; ambient from the sky, fog and exposure follow the sun. Shadows: 4096 atlas on every platform (Android's default was 2048), PCF soft filter 3 (2 on mobile), four blended cascades over 220 m. Presentation only; native has no time of day |
| **Audio** | `godot/presentation/audio/`: a bank synthesised at startup from seeded noise and partials (23 cues, ~0.35 s on a worker thread; no audio files ship), voiced for a phone speaker (identity above 300 Hz, low thump only for weight), and a director that plays what the native reports: footsteps on the head-bob stride (concrete / steel / meadow; softer crouched), jump, landings scaled by impact, ledge grab, vault/mantle scrape, crouch rustle, canopy, lethal impact; altitude wind, a distant yard bed and a falling-air rush; at the plant, positional motor hum, steam hiss from the native orifice flow, the hoist chain at the scoop's speed, the lift drive, clangs where the ballast loses velocity, the tipper's creak; birds near the ground by day. Master ends in a hard limiter (+6 dB in, -0.8 dB ceiling). The first bank measured -27.6 dBFS (footsteps) / -48.6 dBFS (ambience) above 300 Hz — next to silent on a phone; `audio_mix` now measures the mix leaving Master under Movie Maker (headless runs' Dummy driver never mixes): ambience -23.3, footsteps -14.5 dBFS above 300 Hz, peak -0.8, CI step "Measure the mix a player hears" (the same figures in run `35912759289`). Measured, **never listened to** — no output device here. **Not done: Governing Law 8's human fear voice on large falls — it needs recorded performances** |
| **Interface** | GDD §22 control surface in `godot/presentation/ui/`: touch (floating stick, drag-look, Jump, contextual Action, Drop/Chute on state, pendant controls only while operating), gamepad and keyboard over one verb vocabulary; sparse HUD (reticle cues, prompts, fall gauge against the native lethal speed, altimeter, station panel), pause with SETTINGS, GRAPHICS (quality preset LOW-ULTRA, render scale, shadows OFF-ULTRA, MSAA, bloom, frame-rate cap, FPS readout) and DISPLAY (FOV, brightness, head bob, speed-FOV kick, time of day, day length) pages; first-person arms (`godot/presentation/first_person_arms.gd`) whose hands grip, plant and reach at the ledge points the native reports; a standing mantle steps in to the hang standoff before it climbs (swept, supported, skipped on moving ground) so the palms land on the lip; opt-in gyro aim (off by default; screen-axis mapping taken from Godot 4.7-stable's Android sensor code, not yet observed on a device); double-tap Jump vaults when the takeoff could have (native window 0.30 s, the ground vault's own probes); PICK UP / SET DOWN on the contextual Action for the native carry (Drop and Back also set down), both hands drawn on the load; crouch (native 1.2 m capsule, feet fixed; stands only where the full 1.8 m capsule clears, so a low gap keeps you down; Jump and Action stand first) on a touch CROUCH/STAND toggle, C or right-stick click, or held Ctrl, with the eye gliding 1.52 to 0.95 m. 18 headless `--uitest` scenarios drive real input events into native state changes (`touch_carry` added with `AS-003`); all 18, and the Movie Maker `audio_mix`, passed in CI run `36010028676` |
| **Next code job** | None automatically opened. The authorized dock→+12 m slice is GREEN and stopped. Ground observations/model gaps remain below; upper construction stays paused until a bounded next task is authorized. |
| **Owner decision, 2026-09-26** | Restore the fuller tower implementation and all audited plans through a forward commit. Preserve the replacement experiment in Git history. Restore native vault/mantle/hang, crouch, carry, parachute and moving-support contracts; retain legal machinery and parkour routes. The source restoration targets `7e66eb6`; its known physical/proof debts below remain open. |
| **Planning audit** | AS-001–007 and their shared planning owners reconciled at `7e66eb6`: backward receiver→support→transmission→energy→control/recovery contracts; blocked interfaces remain explicit. No code, test or workflow changes in the audit. |
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
| **Lifecycle** | does source exist? | `IMPLEMENTED` · `IN PROGRESS` · `PLANNED` · `UNAUTHORED` | someone writes code |
| **Provenance** | were its numbers derived against *this* tree? | re-derived here · ⚠ imported, NOT re-derived | someone re-authors it |
| **Evidence** | what has actually been observed? | a named CI run and a named proof line | **every CI run** |

Every ticket header carries lifecycle, provenance and a READY/BLOCKED implementation gate.
The latest integrated evidence lives in this ledger. Tickets may preserve explicitly dated
historical observations and causal diagnoses; those are not parallel live status authorities.
A source audit or a plausible budget must not be promoted to runtime proof.

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
| `AS-004` | optional needle seating at 96 m | B01 | PLANNED / BLOCKED | audited; rigging, structure, approach and guide law unresolved | no campaign runtime |
| `AS-005` | optional TP-120 landing/exterior connection | B01 exit | PLANNED / BLOCKED | imported false geometry withdrawn; receiver/recovery unresolved | no campaign runtime |
| `AS-006` | A skip + B translating lattice lifts to 198.25 m; C to 220 unbuilt | B02 | IN PROGRESS | A/B source reconciled; C/rearm/no-lift route BLOCKED | native `AS-006 A no link`, `A ride`, `B no link`, `B ride` in the green §2 run; same simulation transfers A→B; UI rigging scenarios cover their exercised seam |
| `AS-007` | water isolation/drainage and TP-340 access | B03 | PLANNED / BLOCKED | imported geometry withdrawn; entry, pressure/flow and support closure unresolved | no campaign runtime |
| `AS-008` | seat `MOD-GIRDER-T` | B04 | UNAUTHORED | — | none |
| `AS-009` | hang rail / traveler stroke | B05 | UNAUTHORED | — | none |
| `AS-010` | service lift or SKIN | B06 | UNAUTHORED | — | none |
| `AS-011` | wind-frame structure / SKIN | B07 | UNAUTHORED | — | none |
| `AS-012` | isolate high riser / drum clutch | B08 | UNAUTHORED | — | none |
| `AS-013` | jack + seat crown beam | B09 | UNAUTHORED | — | none |
| `AS-014` | isolate + lock fans; bell as support | B10 | UNAUTHORED | — | none |
| `AS-015` | stand on 1600.00 m | B11 | UNAUTHORED | — | none |

AS-001–003 original result records remain explicitly historical within their tickets.
Superseded planning text remains recoverable in git at `7e66eb6`; it is no longer an
active implementation instruction. The audit changes documents, not shipped mechanics.

### Cross-ticket receiving contracts and open boundaries

| Producer → receiver | Established output / mismatch | Owning closure |
|---|---|---|
| Screw → water lift | Same conserved 2 m³ tank state; separate failure-isolation proofs plus an unseeded native player run from screw control through the +8 m fixed dock | `GROUND_WATER_ASCENT.md`: uninterrupted +12 extension and the full Godot-input route remain unproven |
| Water lift → AS-001 stair | Caught cage → fixed +8.25 dock → braced grating → +8 landing → +12.1872 landing | Native `ground water lift`, `dock_to_stair12=1`, green §2 run |
| AS-001 → AS-002 | Stable +24.1872 handoff; AS-002 freight configuration must be prepared separately | Existing source/runtime; gate is not universal tower access |
| AS-003 → AS-004 | Physical 36 kg block acquired at grade; receiver requires delivered rigging | No campaign needle interface; delivery/carry/attachment closure BLOCKED |
| AS-004 → AS-005 | Proposed seated structure/cage, no completed receiver | Both optional and BLOCKED; no invisible stub, remote rig or sideways slider parking |
| Tower stair → AS-006 A→B | 154 m entry; native A→B transfer reaches ridden cage at +198.25 | Band not closed: C, independent climb, full rearm and upper exit/recovery unproven |
| AS-006 → AS-007 | Required +220.25 stable ring vs actual +198.25 cage output | BLOCKED; no assumed 22 m link |
| AS-007 → AS-008 | TP-340 is proposed; shop ticket unauthored | BLOCKED geometry/process/recovery, not a runtime route |

Material source/proof debts: intake checkpoint omits pack/hook/dog/crane bodies;
AS-002 flight/handoff collision exclusion; AS-003 apparent barred-wall apertures;
Kit catch seating work/strength is not a tracked finite spring model; the water bucket's former 1.409 m³ capacity mismatch is closed in the geometry at 2.229 m³ to the rim;
water fill/drain uses bounded rates and pose/catch predicates rather than head-derived flow; B's speed
test tolerates 3.1 m/s despite its 3.0 m/s design wording. These are scoped findings,
not an assertion that each currently breaks the proven route. The reported belt
carry failure still needs reproduction on the reporting path.

---

## 8. What is next

The dock-to-+12 m candidate is GREEN; stop after that authorized slice. This
planning audit reconciles the AS tickets/Atlas/shared plans and identifies exact
unknowns; it does not start another mechanism or prove those unknowns away.
When another task is authorized, choose one bounded ground evidence/causal seam
before resuming upward construction. Current source, normal runtime observation
and the receiving contract decide the next repair, not a stale queue in a ticket.

Preserve `2849550` and all newer work. Publish one code candidate at a time, follow
its exact workflow to terminal GREEN/RED, and confirm the APK before advancing.
Documentation-only publication is explicitly separate from a gameplay candidate.
No Fold-device install/execution or sustained-device performance is proven.

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

**Historical identifiers.** Earlier branch layouts used `KI-000`–`KI-008` for a
different kernel decomposition. This current `ChatGPT` tree uses the closed
`WO-000`–`WO-013` kernel set and `AS-*` campaign tickets. Do not treat an old
branch-relative description of “ChatGPT” as the present namespace.

**Not implementation authority, and not to be reintroduced:** questionnaire executables or JSON
state; rejected drafts; GraveSpire history; old branches and builds; discarded mathematics; any
second "content package" outside this tree.

`MANIFEST.txt` and `SHA256SUMS.txt` are a **frozen snapshot of the v1.1 delivery package**. Their
paths predate this layout and are not maintained. They are kept as a delivery record, not as an
index of the tree.
