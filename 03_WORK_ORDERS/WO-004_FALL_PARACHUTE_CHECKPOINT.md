# SCRAPERX — WORK ORDER 004 — FALL / PARACHUTE / CHECKPOINT

**Work Order:** `WO-004`  
**Status:** READY after WO-003 completion  
**Depends on:** WO-003 traversal on real geometry

## Objective

Height is real gameplay in the kernel volume:

- the player can fall off honest edges;
- significant falls emit severity-scaled human fear VO (including natural profanity), with enough variation to avoid a tiny loop;
- an always-carried reusable chute can deploy from actual pose/velocity/clearance;
- chute permits survival, steering, and descent — not teleport, not powered ascent, not auto-return;
- a refuge commit stores player + world state;
- death / unrecoverable failure restores that commit;
- a survived landing on a lower deck is the real position.

## Existing truth

Player can leave a ledge. No chute, VO, or commit system is proven until this WO.

## Authority

- Laws 6–10, 20
- GDD §§8–9, 23
- `01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md` §§8–9 (`KX-REFUGE`, apron as landing field)
- TDD §§8.3, 14, 21

## Owner

Native player (fall/chute state), native persistence (checkpoint). Godot plays VO and renders canopy. Godot does not decide survival.

## Allowed seam

Player airborne integration + chute aerodynamic/constraint model reduced but authoritative + tick-boundary commit of kernel state (player + any existing bodies).

## Required causal path

`loss of support → fall state/severity → optional chute request if clearance/speed allow → chute changes drag/steer from current state → landing support or death → commit restore only on death/unrecoverable`

## Forbidden shortcuts

- invisible perimeter walls
- chute as teleport to refuge
- chute as double-jump / powered climb
- auto-snap back to the intended ledge after a survived fall
- committing only player pose while leaving world objects on the failed timeline
- one scream file looped
- kill planes that replace landing on `KX-DECK`

## Implementation scope

- fall severity from kinematics (height/speed/time)
- VO event hook with multiple lines / intensity buckets
- chute deploy rules + steering within kernel apron
- `KX-REFUGE` commit volume
- death restore
- survived-lower-deck continuation test

## Out of scope

Full 1.6 km well clearance model, hard-fail missions, B05 traveler landings, content-complete VO library (minimum: several lines per intensity bucket).

## Proof path

1. Native: fall from a known ledge; land on lower deck alive; position is the landing, not the ledge.
2. Native: death restore returns commit pose and committed crate/body state if those bodies exist; post-commit moves are gone.
3. Native: chute deploy denied in a volume with insufficient clearance; allowed in open apron; post-deploy altitude does not increase except by existing upward velocity bleeding off.
4. Godot: fear VO fires; canopy presents from native chute state.
5. Fold claims remain separate.

## Completion

- Honest falls exist.
- Chute is a grounded descent system.
- Checkpoints restore committed player and world state.
- Survived lower landings continue in place.

Stop. Do not begin WO-005 here.
