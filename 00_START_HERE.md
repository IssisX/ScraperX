# SCRAPERX — START HERE

**Write branch: `ScraperX-Claude`.** All implementation and all document writes land there.
Other branches may be read to recover provenance. None of them is a write target, and none of
them is an authority. `ScraperX-Grok` and `ChatGPT` are sibling experiments; planning material
has been adopted from both, but where this tree and either of those disagree, **this tree wins**.

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
| **Kernel** | `WO-000`–`WO-013`, all fourteen in source. **Closed set** |
| **Ascent frontier** | `AS-001`–`AS-002` implemented; `AS-003`–`AS-004` planned; `AS-005`–`AS-007` imported, not re-derived; `AS-008`–`AS-015` unwritten |
| **Integrated evidence** | **RED** at `7e831a2` — runs [`35818695969`](https://github.com/IssisX/ScraperX/actions/runs/35818695969) (`c8b5eed`) and [`35820568808`](https://github.com/IssisX/ScraperX/actions/runs/35820568808) (`7e831a2`) fail at the Android arm64 cross-compile only: a file-local alias of `kLethalImpactSpeedMps` shadowed at its one use by the class constant, which NDK clang rejects under `-Werror` (`-Wunused-const-variable`; GCC does not warn). Every earlier step of `35820568808` passed, the 14 `--uitest` scenarios included. Alias removed in the next commit; its run is the next evidence. Last **GREEN**: `25335a2`, run [`35799920271`](https://github.com/IssisX/ScraperX/actions/runs/35799920271), all steps; `AS-002`'s falsifier group CI-integrated since run [`35798864772`](https://github.com/IssisX/ScraperX/actions/runs/35798864772) |
| **Interface** | GDD §22 control surface in `godot/presentation/ui/`: touch (floating stick, drag-look, Jump, contextual Action, Drop/Chute on state, pendant controls only while operating), gamepad and keyboard over one verb vocabulary; sparse HUD (reticle cues, prompts, fall gauge against the native lethal speed, altimeter, station panel), pause/settings; first-person arms (`godot/presentation/first_person_arms.gd`) whose hands grip, plant and reach at the ledge points the native reports; a standing mantle steps in to the hang standoff before it climbs (swept, supported, skipped on moving ground) so the palms land on the lip. 14 `--uitest` scenarios drive real input events into native state changes — all 14 passed in CI run `35820568808`; the standing-mantle plant assertion added since is green locally only |
| **Next code job** | **`AS-003`** — `CAP-HOOK5` acquire. Gate satisfied (`AS-002` implemented, its exit revalidated in source) |
| **Next authoring job** | re-author **`AS-005`** against this branch |
| **Authoring contract** | `03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md` |
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
    PLANNING/                 the ascent authoring contract
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
| `AS-003` | `CAP-HOOK5` acquire | B00 | PLANNED | re-derived here | none — **next code job** |
| `AS-004` | seat needles at 96 m | B01 | PLANNED | re-derived here | none |
| `AS-005` | cage land at 120 m, or east climb | B01 exit | PLANNED | ⚠ imported | none |
| `AS-006` | counterweight ride / pin dump | B02 | PLANNED | ⚠ imported | none |
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

> **Next code job: `AS-003` — `CAP-HOOK5` acquire.**
> `03_EXECUTION/ASCENT/AS-003_HOOK5_RACK.md`. Lifecycle `PLANNED`, provenance re-derived here,
> implementation gate satisfied (`AS-002` implemented, its exit revalidated in source).
>
> **Next authoring job: re-author `AS-005`** against this branch, per §7.

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
