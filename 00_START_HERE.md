# SCRAPERX — START HERE

**Write branch: `ScraperX-Claude`.** All implementation and all document writes land there.
Other branches may be read to recover provenance. None of them is a write target, and none of
them is an authority. `ScraperX-Grok` in particular is a sibling experiment whose planning
documents were adopted here; where this tree and that one disagree, **this tree wins**.

---

## 1. What are the project authorities?

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

## 2. Vocabulary — one concept, one name, one home

This is the part that used to be ambiguous. Kernel engineering and ascent construction are
different classes of work and no longer share an identifier.

| Class | ID form | Home | What it is |
|---|---|---|---|
| **Kernel Work Order** | `WO-000`–`WO-013` | `03_KERNEL/` | foundational engineering. Proves one *tool type* on the `KX-*` substrate at source x ≈ 200. **Closed set — no new `WO-*` is ever created.** |
| **Ascent Slice** | `ASC-01`–`ASC-15` | `04_ASCENT/` | one causal slice of the 1.6 km climb. The live work. |
| **Atlas band** | `B00`–`B11` | Atlas §6 | a floor range of the tower. **Never a filename, never a ticket.** |
| **Atlas chain** | `K0`–`K8` | Atlas §7 | a causal chain spanning bands |
| **Campaign module** | `MOD-*` | Atlas §6 | a world object in the real tower |
| **Kernel substrate** | `KX-*` | Atlas §9 | a kernel fixture. Regression substrate; never retitled into a `MOD-*` |
| **Capability** | `CAP-*` | Atlas §4 | something the player holds or has acquired |
| **Commit** | — | `commit_checkpoint()` in source | the in-sim automatic checkpoint. A *gameplay* concept. There is no `CP-*` document class on this branch and none may be introduced |
| **Process document** | named, never numbered | `02_ENGINEERING_AUTHORITY/` | the protocol and the templates |

Status words, used identically everywhere:

| Word | Means |
|---|---|
| `COMPLETE` | in source, and its falsifier is green in CI |
| `AUTHORED` | plan written against **this branch's** real state. Not coded |
| `PORTED` | plan inherited from `ScraperX-Grok` and **not** re-derived. Its inherited constants describe a world that does not exist here. Must be re-authored before it can be coded |
| `UNAUTHORED` | nothing written yet |

Old names map to new ones in `02_ENGINEERING_AUTHORITY/02_ASCENT_SLICE_PROTOCOL.md` §5.1.

---

## 3. What has already been proven?

Observed on `ScraperX-Claude`, CI run [`35651140478`](https://github.com/IssisX/ScraperX/actions/runs/35651140478), all 11 steps green:

- **10 native falsifiers, exit 0** — `./build/host/scraperx_sim_tests`
- **Godot 4.7 at Fold aspect** under `gl_compatibility`: `width=2160 height=1856 aspect=1.164`,
  `tower_height=1600`, `shut_flow=0.00000`
- **Android arm64 APK** exported, carrying `libscraperx_native.so`

Unproven, and not to be claimed: Fold-device install, on-device execution, touch ergonomics,
sustained frame rate. No device access exists.

---

## 4. Kernel — foundational engineering

`03_KERNEL/`. All fourteen are in source. The kernel is **closed**: it exists to be regression
substrate, not to grow.

| Ticket | Slice | Status |
|---|---|---|
| `WO-000` | Godot 4.7 → GDExtension → fixed-step native sim → arm64 APK | COMPLETE — proven by the CI build/export steps |
| `WO-001` | native player on static `KX-DECK` | COMPLETE in source — **record gap**, see below |
| `WO-002` | translating / rotating support-point velocity | COMPLETE — falsifier |
| `WO-003` | vault / mantle / ledge / hang on real geometry | COMPLETE — falsifier |
| `WO-004` | Fold-aspect stage, industrial identity | COMPLETE — `SCRAPERX_WO004_VIEWPORT_PROOF` |
| `WO-005` | exterior grade and approach | COMPLETE — `SCRAPERX_WO005_APPROACH_PROOF` |
| `WO-006` | vessel → orifice → cylinder → lift | COMPLETE — falsifier + `SCRAPERX_WO006_MACHINE_PROOF` |
| `WO-007` | Kellerworks identity, alpine backdrop | COMPLETE — screenshot evidence only |
| `WO-008` | fall, chute, commit, survived lower landing | COMPLETE — falsifier |
| `WO-009` | kernel chain + persist / reload | COMPLETE — falsifier |
| `WO-010` | player mass drives the treadle | COMPLETE — falsifier |
| `WO-011` | `KX-JIB` moves `KX-CRATE` | COMPLETE — falsifier |
| `WO-012` | seated `KX-NEEDLE` changes traversal | COMPLETE — falsifier |
| `WO-013` | `KX-SUMP` changes `KX-GRATE` | COMPLETE — falsifier |

A completed work order's Existing-truth and Result-record sections quote the evidence and CI
names **as they stood when it was executed** — including the retired "ScraperX-2 checkpoint CI".
Those are historical records. They are deliberately not rewritten; retconning a proof record
would be worse than an out-of-date name.

> **Record gap, `WO-001`.** Its Result record reads `pending` on every line, yet the player body,
> its capsule and its support identity are in source and are exercised by every falsifier that
> follows. The code is real; the paperwork was never filled. Recorded here rather than silently
> promoted to COMPLETE. Closing it is bookkeeping, not engineering.

---

## 5. Ascent — the actual climb

`04_ASCENT/`. Fifteen slices, each bound to one Atlas band. This is where all new work happens.

| Ticket | Slice | Band | Status |
|---|---|---|---|
| `ASC-01` | apron → +24 m, intake rise | B00 | **COMPLETE** — falsifier green |
| `ASC-02` | first legal stand at +40 m | B00 | **AUTHORED** |
| `ASC-03` | `CAP-HOOK5` acquire | B00 | **AUTHORED** |
| `ASC-04` | seat needles at 96 m | B01 | **AUTHORED** |
| `ASC-05` | cage land at 120 m, or east climb | B01 exit | **PORTED** |
| `ASC-06` | counterweight ride / pin dump | B02 | **PORTED** |
| `ASC-07` | blind + drain, or north climb to 340 m | B03 | **PORTED** |
| `ASC-08` | seat `MOD-GIRDER-T` | B04 | UNAUTHORED |
| `ASC-09` | hang rail / traveler stroke | B05 | UNAUTHORED |
| `ASC-10` | service lift or SKIN | B06 | UNAUTHORED |
| `ASC-11` | wind-frame structure / SKIN | B07 | UNAUTHORED |
| `ASC-12` | isolate high riser / drum clutch | B08 | UNAUTHORED |
| `ASC-13` | jack + seat crown beam | B09 | UNAUTHORED |
| `ASC-14` | isolate + lock fans; bell as support | B10 | UNAUTHORED |
| `ASC-15` | stand on 1600.00 m | B11 | UNAUTHORED |

`ASC-05`–`ASC-07` are `PORTED`, not `AUTHORED`. They still quote `ScraperX-Grok` source constants
— a handoff at `(10.40, 24.00, 38.40)`, 68 treads at `x = 10.40`, entity id `319`, a well at
`(-1.76, —, 25.30)`. None of that exists here. Re-author each against real exit state before coding it.

---

## 6. What is next?

> **Next code job: `ASC-02` — first legal stand at +40 m.**
> Plan: `04_ASCENT/ASC-02_LEGAL_FORTY.md`, status AUTHORED, ready to implement.
>
> **Next planning job: re-author `ASC-05`** against this branch, per §5 above.

Do not start from the 1.6 km crown. Do not open a second ticket while one is open.

---

## 7. Context-loading rule

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
- `ASC-*`: Atlas §6 for **its own band only**, plus §§4, 5, 7, 12, 13 as the slice cites them.
  Loading bands the slice does not own is how scope creeps.

Use `01_PRODUCT_AUTHORITY/support/` only to trace a closed product decision.

---

## 8. Claim discipline

Never collapse

`implemented → built → APK produced → installed → executed → observed → verified on Fold`

into one word such as "done". Evidence must match the claim, and the claim must name its evidence.

A plan is not an implementation. `AUTHORED` is not `COMPLETE`. A green aggregate is not a green
falsifier — name the line.

---

## 9. Deliberately excluded

Not implementation authority, and not to be reintroduced: questionnaire executables or JSON
state; rejected drafts; GraveSpire history; old branches and builds; discarded mathematics;
any second "content package" outside this tree.

`MANIFEST.txt` and `SHA256SUMS.txt` are a **frozen snapshot of the v1.1 delivery package**. Their
paths predate this layout and are not maintained. They are kept as a delivery record, not as an
index of the tree.
