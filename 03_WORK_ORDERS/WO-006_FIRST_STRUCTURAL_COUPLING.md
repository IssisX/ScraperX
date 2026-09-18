# SCRAPERX — WORK ORDER 006 — FIRST STRUCTURAL COUPLING

**Work Order:** `WO-006`  
**Status:** READY after WO-005 completion  
**Depends on:** WO-005 `KX-JIB` can place a load

## Objective

A seated structural member changes support and traversal.

Atlas kernel: `KX-NEEDLE` seated in `KX-POCKETS` becomes walkable/load-bearing. Unseated, the span is a gap or an untrusted rest.

Freight from WO-005 is the legal way to place the beam. Slightly heroic manual alignment at the pocket is allowed; industrial travel of the beam is the jib.

## Existing truth

Jib moves crate. Needle is not yet a structural owner object.

## Authority

- Laws 11–13, 18, 22, 25, 26
- GDD §§12, 16
- `01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md` §9 kernel chain steps involving `KX-NEEDLE`
- TDD §§10, 19.1 item 3

## Owner

Native structural state. Jolt owns the rigid body/contact of the needle as a mass. Structure owns pocket connection, whether the span is a valid support, and persistent seating.

Neither side independently invents “broken” vs “seated.”

## Allowed seam

Reduced-order connection: two pockets + one member + seated/unseated + rest pose. Full tower FEM is rejected. Exact beam formulation remains TDD-gated; this WO needs a model that can say **seated ⇒ support predicate true** and **unseated ⇒ gap**, with optional sag only if the chosen reduced model already exists. Do not invent FEM to finish this WO.

## Required causal path

`jib places needle in pockets → structural owner accepts seat → support/collision/traversal predicate changes → player walks the needle`

## Forbidden shortcuts

- swapping a “broken” mesh for a “fixed” mesh with unchanged collision
- mission flag `needle_ok` that enables an invisible walkbox
- deleting the gap without a seated body
- auto-snap seat from Action across the room
- resetting seat on play-mode restart without going through checkpoint rules

## Implementation scope

- `KX-NEEDLE` + `KX-POCKETS`
- seat/unseat topology
- traversal predicate read by player controller
- Godot mirror of seated vs free beam
- native test: unseated gap fails walk; seated gap succeeds

## Out of scope

Process sump, full Plate Shop girder, plastic failure library, B04 crane cab.

## Proof path

1. Native: player cannot cross the bay; after a valid seat, support exists along the member.
2. Unseat (if a legal unseat path exists) removes that support.
3. Godot collision/render match the snapshot.
4. If WO-004 commit already stores world state, seated/unseated survives reload; if not, record persist as unverified until WO-008.

## Completion

- Traversal changed because structural state changed.
- Presentation did not invent the span.

Stop. Do not add the sump in this WO.
