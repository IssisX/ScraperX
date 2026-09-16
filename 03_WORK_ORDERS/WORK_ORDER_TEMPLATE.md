# SCRAPERX — WORK ORDER TEMPLATE

**Work Order:** `WO-___`  
**Status:** READY / BLOCKED / COMPLETE

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
