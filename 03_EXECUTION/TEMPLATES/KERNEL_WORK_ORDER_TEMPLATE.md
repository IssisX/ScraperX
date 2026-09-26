# SCRAPERX — KERNEL WORK ORDER TEMPLATE

> **The kernel is a closed set.** `WO-000`–`WO-013` are all in source and no new `WO-*` is ever
> created. This template is kept as the historical form those fourteen documents take. New work
> is an **Ascent Slice** — see `03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md`, whose §10
> template carries these fields **plus** the §8 mechanical close.

**Kernel Work Order:** `WO-___`
**Lifecycle:** `IMPLEMENTED` (source exists) or `PLANNED` (contract only)
**Provenance:** re-derived here, or imported and not yet re-derived
**Implementation gate:** what must be true before source work starts
**Evidence:** lives in the `00_START_HERE.md` ledger, never duplicated here — it changes
every CI run, while lifecycle changes only when someone writes code

## Objective
One concrete player-visible or system-visible capability.

## Existing truth
List only what current source, tests, build output, or runtime evidence already proves.

## Authority
Quote or reference the exact Governing Law, GDD, Execution Protocol, and TDD sections that constrain this task.

## Owner
Name the single subsystem that owns the consequential state being changed.

## Allowed seam
Name the smallest existing interface/state/solver boundary through which this capability should be added.

## Required causal path
`input/request → authoritative state → consequential behavior → presentation/result`

## Forbidden shortcuts
List the likely fake/duplicate/proxy implementations that would violate authority or product truth.

## Implementation scope
Files/subsystems that may be changed.

## Out of scope
Nearby systems that must not be opportunistically expanded.

## Proof path
State the actual path that must be exercised:
- source/config inspection;
- native tests;
- build;
- APK generation;
- install;
- runtime;
- Fold observation;
as applicable.

## Completion
Concrete falsifiable conditions that end the task.

## Result record
After execution, record only:
- changed;
- built;
- executed;
- observed;
- unverified boundary;
- regressions, if any.

Stop when Completion is proven.
