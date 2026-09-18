# SCRAPERX — WORK ORDER 008 — FIRST FULL CAUSAL CHAIN

**Work Order:** `WO-008`  
**Status:** READY after WO-007 completion  
**Depends on:** WO-005, WO-006, WO-007 primitives exist

## Objective

Prove ScraperX’s defining architecture on the atlas kernel, not on a 1.6 km content set.

Required chain (`01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md` §9):

```text
player intervention
  → freight/load change (KX-JIB / KX-CRATE / KX-DOG if dog is wired)
  → structural consequence (KX-NEEDLE seated)
  → process consequence (KX-SUMP isolated/drained)
  → changed traversal (needle + dry grate to KX-REFUGE)
  → checkpoint commit
  → reload restores equivalent crate/dog/needle/sump/player state
```

If `KX-DOG` was not implemented in WO-005, either wire it here as the freight-side gate or use crate-on-needle as the freight consequence. Do not skip an entire domain.

## Existing truth

Separate slices exist. Coupling + persistence of the combined kernel is unproven until this WO.

## Authority

- Laws 2, 3, 13, 16–18, 20, 22, 26, 32, 33
- GDD §§6, 16, 23, 28–29
- `01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md` §§7, 9, 12
- TDD §§14, 15, 19.1 items 6–7, 24 WO-008
- Execution Protocol §§4–7, 12

## Owner

Native simulation as a whole under the existing tick order. Persistence owner writes one commit. Mission/UI may observe predicates only.

## Allowed seam

Integrate already-built kernel modules into one deterministic test scene. Add `KX-DOG` only if required to make freight change access. Extend the WO-004 commit blob to the new owners.

## Required causal path

Exactly the `01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md` §9 kernel chain, plus at least one documented alternate legal path (example: shove crate if mass allows; or climb around wet grate if a SKIN ledge exists) to prove sequence-break acceptance.

## Forbidden shortcuts

- scripting the chain with animation events
- one Action button “solve kernel”
- save/load that restores a prettier default kernel
- loading B01–B11 modules
- claiming the 1.6 km campaign exists
- mission flag `chain_complete` as traversal
- desktop web build as Android proof

## Implementation scope

- kernel scene descriptor with stable IDs
- dog/crate/jib/needle/sump/grate/refuge together
- persistence round-trip
- deterministic command-stream test
- Godot playable kernel (desktop and/or Android; claims split)

## Out of scope

Bands B01–B11, NPCs, traveler, crown, VO library expansion, art production, Fold 45 FPS certification of representative tower play.

## Proof path

1. Native scenario test replays a command stream; predicates match: crate moved, needle seated, sump safe, refuge reachable.
2. Commit at refuge; mutate world after commit; death restore returns committed kernel, not the mutated failed timeline.
3. Reload from disk reconstructs equivalent continuation.
4. Alternate legal path test: at least one intended step skipped by another physical route still allowed.
5. Godot: a human can perform the chain with sparse controls.
6. APK / install / Fold remain separate claims if performed.

## Completion

This work order is complete when all of the following are true and recorded at the correct evidence class:

- three macro domains participated in one kernel situation;
- traversal at refuge is a consequence of those states;
- persist/reload preserves aftermath;
- no presentation or mission system double-resolved the outcome;
- B01–B11 were not smuggled in.

After this, ScraperX has proven its architecture. Content assembly may begin at B00 using these primitives.

Stop. Do not start a content-production campaign inside this work order.
