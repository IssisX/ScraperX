# SCRAPERX — AI BUILD PACKAGE v1.1

**Purpose:** Give an implementation AI one authority tree: laws, game, spatial content, engineering, and bounded work orders. Do not treat this package as a pile of parallel briefs.

## Read first

Use this authority order:

1. `01_PRODUCT_AUTHORITY/00_GOVERNING_LAWS.md`
2. `01_PRODUCT_AUTHORITY/01_SCRAPERX_GDD.md`
3. `01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md`
4. `02_ENGINEERING_AUTHORITY/00_EXECUTION_PROTOCOL.md`
5. `02_ENGINEERING_AUTHORITY/01_TECHNICAL_ARCHITECTURE_TDD.md`
6. Current repository source / tests / build / runtime evidence
7. The current file under `03_WORK_ORDERS/`

Higher authority wins when documents conflict.

The atlas is the GDD written in meters, modules, and braids. It is not a sidecar spec. Support files under `01_PRODUCT_AUTHORITY/support/` remain provenance for how the GDD was closed. They are not a second product.

Requester rules live in Execution Protocol §15. Fold-only operation lives in §16. The human does not paste this package into the model. The model already has the tree. The legal human act is a short command.

## Fold-only operator

Shipping target and current workstation are the same device: Galaxy Fold 6, no desktop.

Human types one line. Model loads the WO, the listed sections, and the source.

| Thumb | Meaning |
|---|---|
| `000` | execute WO-000, then stop |
| `001` … `008` | execute that WO, then stop |
| `status` | current WO, what is proven, what is unverified |
| `fix:` + what broke | repair inside the current WO only |
| `amend:` + one fact | change one atlas/TDD/WO field, then stop |

Do not ask the Fold user to open, copy, or re-upload authority files on each turn.

If the current WO is unexecuted and the ask is not in that table, name the current WO and wait.

## Context-loading rule

Do **not** load every file into every coding task.

Always load:

- Governing Laws;
- Execution Protocol;
- current Work Order;
- relevant source/tests/config;
- only the GDD / atlas / TDD sections that constrain the current task.

Use `01_PRODUCT_AUTHORITY/support/` only to trace or resolve a product decision.

Atlas loading by work-order class:

- WO-000: no atlas required.
- WO-001–004: Atlas §§2 and 9 only (datum + kernel deck).
- WO-005–008: Atlas §§4, 7, 8, 9, 12.
- Later content work: the named band in Atlas §6.

## Implementation method

Work through bounded Work Orders. Each Work Order must identify:

- one concrete capability;
- existing proven truth;
- governing product/technical sections;
- authoritative owner;
- allowed implementation seam;
- forbidden shortcuts;
- actual proof path;
- explicit completion condition.

Implement the smallest **complete causal slice**, not the smallest textual diff.

Do not expand into adjacent features after the completion condition passes.

Do not assemble bands B01–B11 before WO-008 is proven on the kernel slice.

## Claim discipline

Never collapse:

`implemented → built → APK produced → installed → executed → observed → verified on Fold`

into one word such as “done.”

Evidence must match the claim.

## Work-order spine

| WO | File | Slice |
|---|---|---|
| 000 | `03_WORK_ORDERS/WO-000_DELIVERY_SPINE.md` | Godot 4.7 → GDExtension → fixed-step native sim → arm64 APK |
| 001 | `03_WORK_ORDERS/WO-001_EMBODIED_AUTHORITY.md` | native player on static `KX-DECK` |
| 002 | `03_WORK_ORDERS/WO-002_MOVING_SUPPORT_TRUTH.md` | translating/rotating support-point velocity |
| 003 | `03_WORK_ORDERS/WO-003_ATHLETIC_TRAVERSAL.md` | vault/mantle/ledge/hang on real geometry |
| 004 | `03_WORK_ORDERS/WO-004_FALL_PARACHUTE_CHECKPOINT.md` | fall, chute, commit, survived lower landing |
| 005 | `03_WORK_ORDERS/WO-005_FIRST_FREIGHT.md` | `KX-JIB` moves `KX-CRATE` |
| 006 | `03_WORK_ORDERS/WO-006_FIRST_STRUCTURAL_COUPLING.md` | seated `KX-NEEDLE` changes traversal |
| 007 | `03_WORK_ORDERS/WO-007_FIRST_PROCESS_COUPLING.md` | `KX-SUMP` changes `KX-GRATE` |
| 008 | `03_WORK_ORDERS/WO-008_FIRST_CAUSAL_CHAIN.md` | kernel chain + persist/reload |

## Current starting point

If the delivery spine is not proven, the current task is **WO-000**.

If it is proven, take the next incomplete work order in the table. Do not start from the 1.6 km crown.

Kernel module IDs live in the atlas, §9. Campaign completion lives in the atlas, B11, and remains the GDD summit rule: physically stand on 1600 m.

## Deliberately excluded

This package does not contain:

- questionnaire executables;
- questionnaire JSON state;
- rejected drafts;
- screenshots;
- GraveSpire history;
- old branches/builds;
- discarded mathematics material;
- a second “content package” outside this tree.

Those are not ScraperX implementation authority.
