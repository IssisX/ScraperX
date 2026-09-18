# SCRAPERX — WORK ORDER 007 — FIRST PROCESS COUPLING

**Work Order:** `WO-007`  
**Status:** READY after WO-006 completion  
**Depends on:** WO-006 seated needle can be a route

## Objective

A process/isolation state change alters the same kernel situation: hazard, support, or machine availability.

Atlas kernel: `KX-SUMP` + valve/blind. Wet sump makes `KX-GRATE` a hazard or non-support. Isolated + drained grate is ordinary walkable support.

## Existing truth

Freight and a seatable needle exist. No process graph is proven.

## Authority

- Laws 11, 13, 14, 22, 25, 26
- GDD §§13, 16
- Atlas B03 grammar compressed into §9 `KX-SUMP` / `KX-GRATE`
- TDD §§12, 19.1 item 5

## Owner

Native process/isolation network. Structural/player owners *read* derived hazard/support. Process does not teleport the player. Player Action may insert a blind or close a valve only at the real station.

## Allowed seam

Lumped graph: one volume, one isolation edge, one drain sink, one derived “grate safe” predicate. No particle fluid. Update on the authoritative tick or an integer divisor.

## Required causal path

`Action at blind/valve → network isolation/inventory changes → grate predicate changes → player walk or fall outcome changes`

## Forbidden shortcuts

- wet decal over an always-solid grate
- timer that “dries” the grate without inventory
- mission flag `sump_clear`
- adding a second process engine in Godot
- process complexity that does not change a verb, hazard, or support

## Implementation scope

- `KX-SUMP` network
- `KX-GRATE` derived support/hazard
- compact local controls
- native tests: isolate+drain ⇒ safe; dump/fail ⇒ unsafe
- optional coupling: jib power is dead while a shared line is live — only if it can be done with the same graph without expanding scope. Default is grate coupling only.

## Out of scope

B08 steam identity, inhabitants fleeing, particle spray as authority, HEADER-H.

## Proof path

1. Native predicates flip only when network state flips.
2. Player crossing wet grate produces the declared hazard/fall; dry grate is support.
3. Godot gauges/VFX amplify real state and do not precede it.

## Completion

- Process state is the cause of the changed traversal/hazard.
- The model is reduced and authoritative.

Stop. Do not assemble the full WO-008 fixture beyond what this coupling needs.
