# SCRAPERX — AI BUILD PACKAGE v1.0

**Purpose:** Give an implementation AI enough authority and execution structure to build ScraperX correctly without treating the entire package as one giant prompt.

## Read first

Use this authority order:

1. `01_PRODUCT_AUTHORITY/00_GOVERNING_LAWS.md`
2. `01_PRODUCT_AUTHORITY/01_SCRAPERX_GDD.md`
3. `02_ENGINEERING_AUTHORITY/00_EXECUTION_PROTOCOL.md`
4. `02_ENGINEERING_AUTHORITY/01_TECHNICAL_ARCHITECTURE_TDD.md`
5. Current repository source / tests / build / runtime evidence
6. The current file under `03_WORK_ORDERS/`

Higher authority wins when documents conflict.

## Context-loading rule

Do **not** load every support file into every coding task.

Always load:
- Governing Laws;
- Execution Protocol;
- current Work Order;
- relevant source/tests/config;
- only the GDD/TDD sections that constrain the current task.

Use `01_PRODUCT_AUTHORITY/support/` only to trace or resolve a product decision. It is provenance, not routine coding context.

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

## Claim discipline

Never collapse:
`implemented → built → APK produced → installed → executed → observed → verified on Fold`

into one word such as “done.”

Evidence must match the claim.

## Current starting point

The TDD defines the initial sequence. The first implementation task is **Work Order 000 — Delivery Spine**:

`Godot 4.7 app → native GDExtension loads → authoritative fixed-step sim advances → Android arm64 APK produced`

Do not begin gameplay systems before that delivery spine is real.

## Deliberately excluded

This package does not contain:
- questionnaire executables;
- questionnaire JSON state;
- rejected drafts;
- screenshots;
- GraveSpire history;
- old branches/builds;
- discarded mathematics material.

Those are not ScraperX implementation authority.
