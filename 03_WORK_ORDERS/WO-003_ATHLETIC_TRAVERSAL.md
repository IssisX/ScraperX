# SCRAPERX — WORK ORDER 003 — ATHLETIC TRAVERSAL

**Work Order:** `WO-003`  
**Status:** READY after WO-002 completion  
**Depends on:** WO-002 moving-support suite green

## Objective

Bounded athletic primitives exist on real geometry:

vault, mantle, ledge grab, hang, drop-to-hang, and climb-onto-support.

Assists may search candidate geometry and run a constrained trajectory only when reach, clearance, and support are valid. Moving-support inheritance from WO-002 remains in force during these moves.

## Existing truth

Native player walks and rides. No climb/mantle authority exists until this WO.

## Authority

- Law 4
- GDD §§7.2–7.5
- `01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md` §2.3 (if you can stand on it, it is not decoration), §9 kernel ledges on `KX-DECK` / needle height later
- TDD §8.5

## Owner

Native player controller. Geometry queries may use Jolt collision. Traversal predicates are native.

## Allowed seam

Controller assistance on existing collision/support queries. Add kernel ledge/rail colliders to `KX-DECK` (a crate-height block, a chest-height slab, a hang rail).

## Required causal path

`input + nearby valid geometry → native assist predicate true → constrained trajectory that never ignores blockers → end state with valid support (including moving support)`

## Forbidden shortcuts

- route flags / painted splines that ignore collision
- teleport mantle
- assists that work on moving supports by freezing the world
- stamina-as-identity (fatigue is situational later; do not add a universal tax here)
- a full parkour animation suite presented as proof without predicate tests

## Implementation scope

- vault over waist-height volume
- mantle onto chest-height deck
- ledge grab + hang + pull-up
- drop from hang to deck if clearance exists
- tests on static and translating ledges

## Out of scope

Chute, checkpoints, machines, structure/process, fatigue model, full tower facade kit, animation polish as authority.

## Proof path

1. Native tests: valid geometry succeeds; blocked geometry fails; moving ledge keeps inherited motion.
2. Godot observation of the same fixtures.
3. One negative test: assist refuses a mantle through a blocker.

## Completion

- Listed primitives work on real kernel geometry.
- Assists cannot fabricate support or pass through blockers.
- WO-002 inheritance still holds on translating climb fixtures.

Stop. Do not begin WO-004 here.
