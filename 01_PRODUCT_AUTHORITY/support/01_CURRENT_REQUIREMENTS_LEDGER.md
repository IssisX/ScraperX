# SCRAPERX — CURRENT REQUIREMENTS LEDGER v1.2

**Status:** CANONICAL PRE-GDD PRODUCT REQUIREMENTS  
**Repository:** `ScraperX`  
**Game/app:** `ScraperX`  
**Governing authority:** `00_GOVERNING_LAWS.md`

This ledger contains **game, product, runtime, and delivery requirements only**. Assistant behavior, document-writing rules, elicitation procedure, and conversational guardrails are not product requirements and do not belong here.

## Identity and core premise

| ID | Requirement |
|---|---|
| REQ-001 | The repository and game/app are both named **ScraperX**. |
| REQ-002 | ScraperX takes place in one enormous inhabited industrial skyscraper / megastructure approximately **1.6 km tall**. |
| REQ-003 | The player character is athletic and parkour-capable, physically climbing the skyscraper rather than operating the game as a detached simulation or management interface. |
| REQ-004 | The skyscraper is composed of and traversed through **large-scale, life-size interconnected machines, mechanisms, structures, and physical systems** capable of forming long Rube-Goldberg-like causal chains. |
| REQ-005 | **Ascent is the campaign spine**: progression is principally upward through the same persistent skyscraper. |

## Movement, height, falling, and recovery

| ID | Requirement |
|---|---|
| REQ-010 | Athletic industrial parkour is core gameplay and must remain grounded in actual geometry, support, clearance, contact, and meaningful momentum. |
| REQ-011 | The player can genuinely fall wherever the physical world permits it; inconvenient falls are not removed by invisible safety barriers. |
| REQ-012 | Reached checkpoints provide recovery after death or unrecoverable failure according to the committed-state checkpoint law. |
| REQ-013 | During physically applicable falls the player can deploy a parachute / grounded descent system; it is not a teleport or guaranteed rescue. |
| REQ-014 | Significant falls produce varied, believable fear reactions including yelling, screaming, panic, and natural-sounding profanity. |
| REQ-015 | Falling can be darkly humorous because of credible human panic and relief, but the physical danger itself remains sincere. |

## Industrial simulation and systemic play

| ID | Requirement |
|---|---|
| REQ-020 | The three principal player-manipulable macro-system families are **freight/lifting**, **load-bearing architecture**, and **process/isolation**. |
| REQ-021 | Those macro systems must interconnect rather than behave as isolated minigames. |
| REQ-022 | Player intervention must be capable of propagating across systems—for example mass/load changes affecting structure, machinery, process state, traversal, inhabitants, or mission possibilities. |
| REQ-023 | The player must be able to physically change what the tower can do through legitimate manipulation of machinery, structure, rigging, process state, access, and related world systems. |
| REQ-024 | Physically valid alternate solutions and sequence breaks are accepted when the authoritative world actually permits them. |
| REQ-025 | Consequential deformation, topology, connections, rigging, machinery/network state, useful debris, inhabitants, traversal changes, and important aftermath persist in the committed playthrough where strategically relevant. |
| REQ-026 | Missions and progression may record outcomes, but cannot fabricate routes, machine success, structural outcomes, or other physical truths that the world state does not support. |

## Interaction and target device

| ID | Requirement |
|---|---|
| REQ-030 | Galaxy Fold 6-class Android hardware is the real shipping target. |
| REQ-031 | Default landscape interaction is sparse and touch-first: left-thumb movement, right-thumb look, contextual Action, and compact machine-specific controls only when physically meaningful operation requires them. |
| REQ-032 | The contextual Action control is an interaction gateway, not a universal solve button. |
| REQ-033 | The current sustained representative-gameplay performance target is **45 FPS** on the real Fold-class target. |

## Technology and physical truth

| ID | Requirement |
|---|---|
| REQ-040 | ScraperX should aggressively use mature external engines, libraries, packages, extensions, middleware, solvers, profiling/build tooling, and related technology **when measured value justifies the dependency**. |
| REQ-041 | Jolt Physics is a candidate, not a predetermined winner; another solution may replace or complement it only when evidence shows a better fit for the actual responsibility and Android shipping path. |
| REQ-042 | Consequential state has one authoritative owner per fact; external engines or specialized solvers may cooperate but may not independently double-resolve the same outcome. |
| REQ-043 | Reduced-order models are acceptable when they are the real authoritative model for the claimed phenomenon and preserve the gameplay-relevant invariants; fake physical theater is not acceptable. |
| REQ-044 | Critical machinery obeys relevant finite physical limits rather than gaining unlimited force, torque, power, braking, travel, pressure, or free work from control requests. |

## Delivery truth

| ID | Requirement |
|---|---|
| REQ-050 | Implemented, built, APK-produced, installed, executed, observed working, and observed working correctly on the Fold target are distinct proof states. |
| REQ-051 | Desktop or web execution cannot substitute for real Android/Fold evidence for claims about the shipping game. |

---

**Ledger boundary:** This file records established ScraperX requirements. It intentionally contains no assistant-conduct rules, elicitation instructions, legacy-history inventory, speculative lore, invented story, or questionnaire-generated assumptions.
