# SCRAPERX — START HERE

**Write branch: `ScraperX-Claude`.** All implementation and all document writes land there.
Other branches may be read to recover provenance. None of them is a write target, and none of
them is an authority. `ScraperX-Grok`, `ChatGPT` and `Gemini` are sibling experiments. On
2026-09-28 the owner had `Gemini`'s world merged into this branch (its route from grade to the
+221 m ring); planning material has also been adopted from `ChatGPT` and `ScraperX-Grok`. Where
this tree and another branch disagree, **this tree wins**.

**Owner directives**, dated, newest first. The owner builds several versions of this game at once;
every request is checked against this list, and a repeat or a contradiction is pointed out.

| Date | Directive | Status here |
|---|---|---|
| 2026-09-28 | Every APK from this branch is ScraperX-Claude, `com.cory.scraperx.claude`. Permanent | done; CI reads it back from every APK |
| 2026-09-28 | Stairs, ladders and easy paths are only the backup route; the route is the machines and the parkour climbs | in the plan (§2.6 rule 8); no backup path built yet |
| 2026-09-28 | Falls sound human: panicked yelling and real profanity, never filtered ("Motherfucker" ships) | done: 47 lines, every line of the script |
| 2026-09-28 | Take the `Gemini` branch's progress into this branch and carry on here | done: `Gemini` merged (to its `553cd7e`); its world replaces this branch's S2 swinging stair; the Claude identity, audio and fall voice kept. Grade to the +221 m ring is played through touch, pad and keyboard input; keyboard failed on `Gemini` and is fixed here |
| 2026-09-26 | No build is green unless the ascent is played through the game's own input from the game's start | done; CI builds the APK only after it |
| 2026-09-26 | A mechanism does not need five steps | in the plan (§2.2: one or two player actions a stage) |
| 2026-09-26 | The audio must not be "ting-ting-ping-ping" | done: the ringing cues re-voiced |
| 2026-09-25 | Almost every mechanism was incomplete and did not lift the player: start over from grade, no test pieces in the player's world, every machine works | done: rebuilt from grade (S1, C1 built here; S2, C2, S3, C3 and the well's link to +221 m from `Gemini`); movement test blocks only at test spawns; the old bands above 221 m kept for the owner's call |

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
| **Write branch** | `ScraperX-Claude` — the only one |
| **APK identity** | Every APK from this branch is **ScraperX-Claude**, application ID **`com.cory.scraperx.claude`**, set in `godot/export_presets.cfg` (`package/name`, `package/unique_name`) — permanent for this branch, never reverted. It installs beside the other AI branches' builds (`main`, `ChatGPT`, `ScraperX-Grok`: `com.cory.scraperx`; `Gemini`: `com.cory.scraperx.gemini`). `.github/scripts/check_apk_identity.sh` reads it back out of every exported APK in CI (checkpoint and playtest): the label in every locale, the package, every provider authority inside the package, no declared permission outside it, no shared user id |
| **Kernel** | `WO-000`–`WO-013`, all fourteen in source. **Closed set** |
| **Ascent frontier** | From the game's own start at grade, `touch_stack`, `pad_stack` and `keyboard_stack` climb S1, C1, S2, C2, S3, C3, the well's A, B and C, AS-007's D, E and F, AS-008's G, H and I and AS-009's J, K and L, and stand on TP-640 (641.15 m), each on its own device's input events (~585 s of play, no deaths); natively, `Mega-Ascent Grade to TP-340` and `ascent 154 to TP-640` run the same machines on scripted inputs. Each band above 220 m alone: `*_wet` from the 220 ring, `*_shop` from TP-340, `*_crane` from the 484 ring. Above TP-640 no route is built: only the tower's solid mass rises on to 1,600 m. |
| **Integrated evidence** | **Local, this tree** (2026-09-29): native suite passes (`scraperx_sim_tests`, 66 checks, including `Mega-Ascent Grade to TP-340` and the handling falsifiers; `scraperx_parkour_flow_tests`); all 40 viewport-input scenarios pass, the three stack runs from grade to TP-640 among them; `audio_mix` and `audio_fall` pass under Movie Maker; the world solids table regenerates with no drift. **CI on this tree: green**, `9a8b5a4` (the code of `ca2f14a`), [run 36506484467](https://github.com/IssisX/ScraperX/actions/runs/36506484467): all 40 scenarios pass, and `touch_stack`, `pad_stack` and `keyboard_stack` each end at `tp640_y=641.15`; the native `Mega-Ascent Grade to TP-340` and `ascent 154 to TP-640` pass; the playtest APK reads back as `com.cory.scraperx.claude` / ScraperX-Claude. No Fold device run or sustained performance is established. |
| **World collision** | Every static body the presentation draws is native collision: `godot/presentation/solid_export.gd` classifies each built part (solid, moving, non-solid, native mirror; unclassified fails) and generates `src/sim/world_solids.inc`, which the native world compiles in (1 808 bodies; drawn mirrors of owned bodies skipped by bounds). Native tests walk that same solid world; CI regenerates the table and fails on drift. Stairs are walked: a native step-up climbs any edge up to `0.35 m` (vault/mantle take over above it), and every stack flight rises through a stairwell cut in the deck it serves. The SKIN walkway at +32 m is 4 m deep, not 2: under the upper flight a standing player had one 0.4 m lane (reported as an opening too small to fit through); the lane is now 2.4 m (`AS-002` deviation). The plant's access landing no longer overhangs its second step (it left a 1.0 m slot). A crawl beam by the dock, underside 1.45 m, is the crouch fixture The yard sits in a closed alpine basin: a valley floor 0.3 m below grade out to a solid rim of 44 steep ridges (~1.1 km), so every direction ends against rock, not a void or an invisible wall |
| **Sky and light** | `godot/presentation/sky_cycle.gd` + `sky.gdshader`: a moving sun (24 real minutes per day, noon elevation 58 deg) that becomes the moon at night on the one shadowed light, sky dome, drifting lit cloud deck, stars, dusk glow; ambient from the sky, fog and exposure follow the sun. Shadows: 4096 atlas on every platform (Android's default was 2048), PCF soft filter 3 (2 on mobile), four blended cascades over 220 m. Presentation only; native has no time of day |
| **Audio** | `godot/presentation/audio/`: a bank synthesised at startup from seeded noise and filters (~0.35 s on a worker thread), voiced for a phone speaker (identity above 300 Hz, low thump only for weight), and a director that plays what the native reports: footsteps on the head-bob stride (concrete / steel / meadow; softer crouched), jump, landings scaled by impact, ledge grab, vault/mantle scrape, crouch rustle, canopy, lethal impact; altitude wind, a distant yard bed and a falling-air rush; kit machines heard from their own motion (a groan while they move, a creak at a hinge, a clunk where they stop hard); birds near the ground by day. Struck metal (steel steps, grabs, clangs, UI ticks) is noise through broad, fast-damped resonances: the first bank rang pure sine partials for 0.12–0.84 s (peak 58–60 dB over its neighbourhood), which the owner heard as "ting-ting-ping-ping"; now ~10 dB, the same as a concrete step. **Fear voice (GDD 8.4 / Governing Law 8):** 47 spoken lines in `audio/voice/` (Ogg, shipped as-is; the profanity of real terror included, never filtered), tiers yelp → panic → terror by the fall's speed and drop, cut dead by a lethal impact, pain on a hard landing lived through, relief when the canopy opens or a ledge is caught; no line repeats until its tier is spent. Generated offline by `src/voice/generate_fall_voice.py` (Chatterbox TTS, MIT; voice from Piper en_US-john, LibriVox public domain), each take transcribed by Whisper and rejected unless it says its line; a line no take passes fails the generator rather than being left out. Master ends in a hard limiter (+6 dB in, -0.8 dB ceiling). `audio_mix` and `audio_fall` measure the mix under Movie Maker (headless runs' Dummy driver never mixes): ambience -22.9, footsteps -14.8 dBFS above 300 Hz, peak -0.8; in a lethal fall the voice speaks at -14.5 dBFS over the ambience bed, which ducks 8 dB under a spoken line, at -27.9 (`audio_fall`, lines seeded so every run hears the same). Measured, **never listened to** — no output device here |
| **Interface** | GDD §22 control surface in `godot/presentation/ui/`: touch (floating stick, drag-look, Jump, contextual Action, Drop/Chute on state, pendant controls only while operating), gamepad and keyboard over one verb vocabulary; sparse HUD (reticle cues, prompts, fall gauge against the native lethal speed, altimeter, station panel), pause with SETTINGS, GRAPHICS (quality preset LOW-ULTRA, render scale, shadows OFF-ULTRA, MSAA, bloom, frame-rate cap, FPS readout) and DISPLAY (FOV, brightness, head bob, speed-FOV kick, time of day, day length) pages; first-person arms (`godot/presentation/first_person_arms.gd`) whose hands grip, plant and reach at the ledge points the native reports; a standing mantle steps in to the hang standoff before it climbs (swept, supported, skipped on moving ground) so the palms land on the lip; opt-in gyro aim (off by default; screen-axis mapping taken from Godot 4.7-stable's Android sensor code, not yet observed on a device); double-tap Jump vaults when the takeoff could have (native window 0.30 s, the ground vault's own probes); PICK UP / SET DOWN on the contextual Action for the native carry (Drop and Back also set down), both hands drawn on the load; crouch (native 1.2 m capsule, feet fixed; stands only where the full 1.8 m capsule clears, so a low gap keeps you down; Jump and Action stand first) on a touch CROUCH/STAND toggle, C or right-stick click, or held Ctrl, with the eye gliding 1.52 to 0.95 m; a step up (up to 0.36 m in one native tick) glides into the view the same way, never shown in one frame. 40 headless `--uitest` scenarios drive real input events into native state changes (including full-route `touch_stack`, `pad_stack`, `keyboard_stack` from grade to TP-640, AS-007 alone from the 220 ring (`*_wet`), AS-008 alone from TP-340 (`*_shop`), AS-009 alone from the 484 ring (`*_crane`), Stage B `boom`, Deck 4 checkpoint continuation, `stack_upper`, every START option loading (`touch_pause`), and the fall voice); every one blocks the APK, keyboard included, and so do the Movie Maker `audio_mix` and `audio_fall`. Handling: a taken handle is drawn in to the hands at 3 m/s and a ledge caught at arm's length pulls the body in at 3 m/s, never in one tick; a heavy free load slows its carrier to within 80% of the grip (D's 50 kg spool: 1.76 m/s) and is held from spinning in the hands |
| **Next code job** | Above TP-640: derive AS-010 (Atlas band B06) under the mechanism ascent plan, large visible machines first, played through the game's input from the TP-640 start. Open before it: the owner's call on AS-007's fluid drives (float, air, hydraulic ram) under `MACRO-TRAVERSAL-STRICT`, and the backup path (rule 8), not built. |
| **Next authoring job** | `AS-007` (220→340 m) remains a separate planned band; new height does not substitute for the missing device proof of the current route. |
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
| `AS-006` | counterweight well, 154 → 220 m: skip lift, derrick boom, debris chute | B02 | IN PROGRESS | re-derived here under the mechanism ascent plan; Stages A, B and C built (rigging kit with dogs, slip hooks and the declared rubble model; UNHOOK / HOOK / GRAB / LET GO on touch, gamepad and keyboard) and ridden in one run to the 220 ring; the climbing route waits on Step 2 | `PASS scraperx_sim AS-006 A no link`, `A ride`, `B no link`, `B ride`, `C no link`, `C ride`, `band`; uitests `touch_rig`, `pad_rig`, `keyboard_rig`, `touch_debris`, `pad_debris`, `keyboard_debris` (local; CI pending) |
| `AS-007` | wet isolation, 220 → 340 m: float ram, chiller drop, accumulator ram | B03 | **BUILT** | re-derived here under the mechanism ascent plan (replaces the imported plan) | native band, route and wreckage tests; played through the game's input: `touch_wet`, `pad_wet`, `keyboard_wet` and the stack runs |
| `AS-008` | plate shop, 340 → 484 m: scaffold slump, girder tip, domino monolith | B04 | **BUILT** | derived here under the mechanism ascent plan | native band, route and wreckage tests; played through the game's input: `touch_shop`, `pad_shop`, `keyboard_shop` and the stack runs |
| `AS-009` | facade crane stack, 484 → 640 m: traveler and runaway wagon, jib pendulum, kinetic winch | B05 | **BUILT** | derived here under the mechanism ascent plan | native band, route and wreckage tests; played through the game's input: `touch_crane`, `pad_crane`, `keyboard_crane` and the stack runs |
| `AS-010` | service lift or SKIN | B06 | UNAUTHORED | — | none |
| `AS-011` | wind-frame structure / SKIN | B07 | UNAUTHORED | — | none |
| `AS-012` | isolate high riser / drum clutch | B08 | UNAUTHORED | — | none |
| `AS-013` | jack + seat crown beam | B09 | UNAUTHORED | — | none |
| `AS-014` | isolate + lock fans; bell as support | B10 | UNAUTHORED | — | none |
| `AS-015` | stand on 1600.00 m | B11 | UNAUTHORED | — | none |

`AS-005` still quotes `ScraperX-Grok` constants — a handoff at `(10.40, 24.00, 38.40)`,
68 treads at `x = 10.40`, entity id `319`, a well at `(-1.76, —, 25.30)`. None of that exists here.

---

## 8. What is next

> **Next job: Step 2's movement**, then `AS-006`'s climbing route.
> `03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md`, locked with the owner on 2026-09-24. The
> climb above the stair's 154 m top deck is built from linked lift stages that the player
> completes and rides. Step 1 is built: the rigging kit (ropes with a carryable loose end,
> anchors, brake-only governed guides, catches with trip lines, a 900 N grip) and Stage A, a
> counter-mass skip lift off the 154 m deck that the player rigs (UNHOOK, HOOK, GRAB) and rides
> 22 m. Its falsifiers: no link, no lift; the payload never gains more energy than the skip
> released; the rider rides the cage as a moving support; a death restores the stage as
> committed.
>
> Stages B and C are built too: the derrick boom (a cleat-found line hooked onto B's cage, a slip
> hook that lets the line run at the top, the boom left hanging as a lattice) and the debris chute
> (the rebar thrown, 900 kg of rubble into the dumpster, the keeper latch freed), whose spent
> dumpster empties into A's parked cage and re-arms A. One native run rides all three from the
> 154 m deck to the 220 ring, then comes down by parachute, tips A's rubble out and rides A again.
> Next: Step 2's moves for the band's climbing route, then `AS-007`.
> `AS-004` stays `PLANNED`: the stair stays (owner's direction), so its needles are no longer the
> way to 96 m.

Do not start from the 1.6 km crown. Do not open a second ticket while one is open.

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

**Sibling-branch identifiers.** Other branches may assign the same number to different
content, or use different numbers for similar work. Compare actual source, geometry and
authority before reusing anything; a matching `AS-*` label is not proof of equivalence.

**Not implementation authority, and not to be reintroduced:** questionnaire executables or JSON
state; rejected drafts; GraveSpire history; old branches and builds; discarded mathematics; any
second "content package" outside this tree.

`MANIFEST.txt` and `SHA256SUMS.txt` are a **frozen snapshot of the v1.1 delivery package**. Their
paths predate this layout and are not maintained. They are kept as a delivery record, not as an
index of the tree.
