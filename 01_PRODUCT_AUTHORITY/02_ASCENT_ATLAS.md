# SCRAPERX — ASCENT ATLAS v1.0

**Status:** PRODUCT CONTENT AUTHORITY  
**Product:** ScraperX  
**Package:** same tree as the Governing Laws and GDD  
**Depends on:** `00_GOVERNING_LAWS.md`, `01_SCRAPERX_GDD.md`  
**Purpose:** Specify the physical world-spine the GDD left to a later content document: elevation datum, bands, braids, named mechanisms, load paths, capability objects, reconnect nodes, fall geography, and the first implementable causal kernel.

This is not a second design bible. It is the GDD’s spatial form. It does not choose engines, solvers, or shipping architecture. If it conflicts with Laws or GDD, those win.

---

## 0. Authority and evidence class

### 0.1 Stack insertion

For implementation and content work, resolve conflicts in this order:

1. `00_GOVERNING_LAWS.md`
2. `01_SCRAPERX_GDD.md`
3. **This file**
4. `02_ENGINEERING_AUTHORITY/00_EXECUTION_PROTOCOL.md`
5. `02_ENGINEERING_AUTHORITY/01_TECHNICAL_ARCHITECTURE_TDD.md`
6. Current source / tests / runtime evidence
7. Current Work Order

Authored layout is for **development-time assembly**. It is not runtime procedural generation. It is not a secret solution script. If the player reaches a later band by a physically valid route this file did not highlight, the game accepts that result.

If this file conflicts with Laws or GDD, stop and surface the conflict. Do not “fix” the tower by weakening parkour, faking coupling, or scripting a consequence the sim cannot own.

### 0.2 What this file is allowed to freeze

Allowed:

- vertical datum and band cuts;
- plan envelope and primary grid;
- braid identities and reconnect rules;
- named modules and their physical jobs;
- which macro systems are live in which band;
- physical entry/exit conditions;
- capability objects that are real world items or machine-access states;
- fall/parachute landing geography;
- the first causal kernel used by WO-005–008;
- content rejection tests.

Not allowed to freeze (still GDD §30 / TDD §23):

- narrative reason the summit matters;
- player biography;
- exact numerical solver formulations;
- engine/package choices already owned by the TDD;
- final art/audio stacks;
- exact Fold frame-time budgets;
- bit-identical desktop↔Android replay.

### 0.3 Evidence class

This atlas is **authored product content**, not runtime proof.

Masses, strokes, pressures, and safe working loads below are **design targets**. They become simulation numbers only when the owning native subsystem exists and a benchmark scene can falsify them. Until then they are content intent, not measured physics.

---

## 1. North-star translation into space

The player starts on the apron of one inhabited industrial skyscraper and finishes only by standing on the summit walking surface.

Progress is not “complete puzzle N.” Progress is:

- reach a higher support that did not exist or was not usable;
- or gain a physical capability that makes an existing higher support usable;
- or change machine / structure / process state so a higher route becomes real.

The tower is one place. Bands are authorship and streaming cuts, not level loads.

---

## 2. Datum, envelope, grid

### 2.1 Datum

| Quantity | Value | Meaning |
|---|---|---|
| Origin | `(0, 0, 0)` | Top of apron slab at tower geometric center |
| +Z | up | Native SI meters |
| Summit walking surface | `z = 1600.00 m` | Campaign completion plane |
| Crown machine volume | `1600–1635 m` | Legal summit machinery above the walking surface; standing on 1600 is enough |

Plan axes: +X east, +Y north. Content import converts once at the descriptor boundary.

### 2.2 Plan envelope

| Zone | Plan | Role |
|---|---|---|
| Primary frame | 90 m × 90 m | Load-bearing core and heavy floors |
| Bay grid | 18 m × 18 m | 5 bays each way; columns at grid |
| Service risers | 12 m × 18 m inset on north face of core | Process/isolation braid |
| Hoist well | 18 m × 18 m at center-south | Freight braid vertical void |
| Facade envelope | 126 m × 126 m | Exterior cranes, travelers, timber decks, wind walkways |
| Apron | 180 m × 180 m | Yard, wreckage catch, parachute landing field |

The frame is **heavy I-beam + thick timber decking + plate**, densely braced. No spaghetti scaffold as the primary architecture. Light lattice may exist only as local catwalk infill that is obviously secondary.

### 2.3 Member language (content, not solver)

Reachable primary members are consequential candidates:

- rolled steel columns and girders on the 18 m grid;
- built-up plate girders at transfer plates;
- timber deck panels on steel joists (walkable, removable, load-bearing at declared ratings);
- chevron / K braces that can be pinned, unpinned, or replaced by needle beams;
- crane rails and travel beams outside the facade.

Decorative cladding that cannot take load must be visually distinct and collision-honest. If the player can stand on it, it is not decoration.

---

## 3. Braids

Three persistent route families share the same tower. They diverge and reconnect. None is “the real game” with the others as optional DLC.

| Braid ID | Name | Physical character | Typical verbs |
|---|---|---|---|
| `SHAFT` | Freight well | Cages, drums, dogs, counterweights, transfer cars, jump lifts | ride, load, pin, brake, dog, transfer |
| `SKIN` | Facade and exterior machines | Traveling cranes, facade trolleys, timber outriggers, exposed climbs | ride boom, traverse rail, mantle, balance |
| `FLOW` | Process / isolation risers | Headers, blinds, sumps, steam/hydraulic isolation, access after make-safe | isolate, vent, drain, lock out, reroute |

**Reconnect rule.** At every **Transfer Plate** (see §5), at least two braids offer a physically valid handoff: a platform, lock, or machine that can deliver the player onto another braid without a hidden teleport.

**Soft-lock rule.** A braid may become locally impassable because of committed state. That is legal. A Transfer Plate must still retain one recovery path that does not require the lost capability: usually a lower SKIN climb, a SHAFT ride if the cage still works, or checkpoint restoration after unrecoverable failure.

**No prescribed single chain.** The atlas marks *intended capability gates* and *example couplings*. It does not mark an invisible wall behind the “wrong” machine.

---

## 4. Capability objects

Progress blockers are missing physical capability or access, as the GDD requires. Capabilities are **world facts**, not XP.

| ID | Object / access | What it actually is | Typical acquire band | What it unlocks |
|---|---|---|---|---|
| `CAP-PENDANT` | Yard / local hoist pendant | Handheld or station-tethered control that issues Drive/Raise/Lower/Brake commands to one named machine | B00 | First freight actuation |
| `CAP-HOOK5` | 5 t hook block + rated sling | Portable attach hardware compatible with declared padeyes | B00–B01 | Legal attachment to loads ≤ design target 5 t |
| `CAP-NEEDLE` | Needle-beam pair (steel, ~12–18 m) | Two members that can be seated in pockets and become walkable/load-bearing | B01 | Span a bay; brace a missing K-brace; make a hoist land |
| `CAP-BLIND` | Isolation blind + handle | Carryable spectacle blind that can isolate one declared line | B02–B03 | Make-safe a header; drain a sump; kill a live machine feed |
| `CAP-DOGKEY` | Cage-dog / landing key | Physical key or removable pin that enables landing interlocks | B01–B02 | SHAFT landings that otherwise stay dogs-out |
| `CAP-TROLLEY` | Facade trolley clutch handle | Station or portable handle that engages traveler drive | B04–B05 | SKIN vertical travel on facade rails |
| `CAP-DRUM` | High-drum clutch access | Station-bound access after a structural/process make-safe | B07–B08 | Upper SHAFT beyond midstack |
| `CAP-CROWN` | Crown lockout set | Isolation + structural pin set required to enter summit machinery without dying to process/wind | B10–B11 | Legal summit approach |

Inventory is sparse. Most “power” stays in the tower: you find or free the machine, you do not become a crane.

Unaided manual work remains slightly heroic but grounded: shove a 200–400 kg crate on level deck; lift a 25–40 kg pin, handle, or timber block; drag a needle beam only with rollers, come-along, or a hoist. Industrial mass moves by machine.

---

## 5. Transfer plates and band table

Transfer Plates are thick structural floors that collect loads, park machines, and reconnect braids. They are the natural checkpoint-dense bands.

| Band | Name | Z range (m) | Dominant systems | Transfer Plate | Live braids |
|---|---|---|---|---|---|
| B00 | Apron and Intake | 0–40 | Freight, traversal | Apron slab | SHAFT start, SKIN start |
| B01 | Transfer Hall | 40–120 | Freight, structure | TP-120 at 120 m | SHAFT, SKIN |
| B02 | Counterweight Well | 120–220 | Freight, structure | — | SHAFT, SKIN |
| B03 | Wet Isolation | 220–340 | Process, freight | TP-340 at 340 m | FLOW, SHAFT |
| B04 | Plate Shop | 340–480 | Structure, freight | — | SHAFT, SKIN |
| B05 | Facade Crane Stack | 480–640 | Freight, traversal | TP-640 at 640 m | SKIN, SHAFT |
| B06 | Midstack Service | 640–780 | Inhabitants, process | — | FLOW, SHAFT |
| B07 | Wind Frame | 780–940 | Structure, traversal | TP-940 at 940 m | SKIN, SHAFT |
| B08 | High Process Column | 940–1100 | Process, freight | — | FLOW, SHAFT |
| B09 | Crown Approaches | 1100–1280 | All three | TP-1280 at 1280 m | all |
| B10 | Mast and Cooling | 1280–1480 | Process, structure | — | SKIN, FLOW |
| B11 | Summit Machine | 1480–1600 | Freight + structure | Summit at 1600 m | all |

Population intensity (GDD: varies by layer):

- B00, B04, B06: workers present when regions are active and access exists.
- B02, B05, B07, B10: sparse operators, mostly machines.
- B03, B08: process techs only if headers are not in hazard state.
- B11: empty of ordinary traffic unless the player brought someone by real access.

---

## 6. Band specifications

Each band lists **physical job**, **named modules**, **example couplings**, **fall geography**, and **exit states**. Example couplings are legal intended uses, not exclusive scripts.

Module IDs are stable authored IDs for descriptors. They survive save/load.

---

### B00 — Apron and Intake (0–40 m)

**Physical job.** Teach embodiment and state by a real jammed intake, not a cutscene. Give the player the first hoist and the first reason to go up.

**Opening (first 10–15 minutes).** Player begins standing on the south apron at `z ≈ 1.2 m`, looking north-up the tower. A 4 t crate pack sits crooked on `MOD-INTAKE-BELT`, pinning gate `MOD-DOG-A`. The first SHAFT cage at 8 m is visible but dogs-out. A yard jib `MOD-YARD-JIB` is cold until the pendant on the belt catwalk is reachable. Wind, height, and the full 1.6 km shaft are readable immediately.

**Modules**

| ID | What it is | Owner domain |
|---|---|---|
| `MOD-APRON` | 180 m slab, stair islands, timber cribbing | static + parkour |
| `MOD-INTAKE-BELT` | Translating slat deck, 18 m stroke, finite drive | freight |
| `MOD-DOG-A` | Mechanical landing dog / gate pinned by the crate | freight + structure |
| `MOD-YARD-JIB` | 12 m jib, 5 t SWL design target, finite torque/brake | freight |
| `MOD-HOOK5-RACK` | Hook block + slings in a locked cage opened by moving the crate or circling the belt | capability |
| `MOD-STAIR-A` | Open stair to +40 m that is blocked by `MOD-DOG-A` until the gate can travel | traversal |
| `MOD-SKIN-LADDER-S` | South facade ladder/ledge line to +40 m, always physically climbable, exposed | SKIN |

**Example couplings**

1. Shove/drag the crate off the dog **or** hook it with `MOD-YARD-JIB` and lift. Dog can close. Stair-A becomes a real route.  
2. Belt still runs if not isolated; riding the belt is valid moving-support traversal.  
3. Ignore the jib. Climb `MOD-SKIN-LADDER-S` to Transfer Hall catwalk and drop a chain to the crate from above (harder, fall risk).  

**Fall geography.** Apron is the primary chute landing. Falling from B00 is usually survivable without a chute. Falling *onto* the belt while it moves is a moving-support problem, not a kill plane.

**Exit states that make B01 reachable**

- `STAIR-A` open, or  
- south SKIN climb complete, or  
- yard jib used to place the player on the +40 m timber soffit.

Checkpoint intent: automatic commit when the player first stands on `z ≥ 40 m` with stable support.

---

### B01 — Transfer Hall (40–120 m)

**Physical job.** First structural seating problem. A missing needle beam means the SHAFT cage cannot take a landing at 120 m without racking the guides.

**Modules**

| ID | What it is |
|---|---|
| `MOD-HALL-DECK` | Timber-on-steel hall, open to facade on east |
| `MOD-NEEDLE-POCKETS` | Two seat pockets 18 m apart at `z = 96 m` |
| `MOD-NEEDLE-A/B` | The actual beams, stored on racks at 48 m |
| `MOD-CAGE-1` | Personnel/material cage in the well, finite drum below |
| `MOD-GUIDE-RACK` | Cage guides that stay out of tolerance until needles are seated |
| `MOD-EAST-OUTRIGGER` | Timber outrigger to SKIN walkway |

**Example couplings**

1. Hoist needles with `MOD-YARD-JIB` or `MOD-CAGE-1` as a winch (if player already reached the drum room). Seat both pockets. Guides align. Cage can land at TP-120.  
2. Seat only one needle: hall becomes walkable as a springy span but cage interlock still rejects landing. Player can use the seated needle as a parkour beam to SKIN.  
3. Leave needles. Climb east outrigger and SKIN all the way to 120 m. Cage remains unused.

**Fall geography.** Open well to apron. Chute from 120 m can land on apron or catch on `MOD-INTAKE-BELT` / yard jib boom if those objects are where the player steers.

**Exit.** Stand on TP-120, or reach the 120 m SKIN ring.

---

### B02 — Counterweight Well (120–220 m)

**Physical job.** Teach inherited momentum on a serious moving mass. The counterweight stack is a ride, a hazard, and a load.

**Modules**

| ID | What it is |
|---|---|
| `MOD-CW-STACK` | Multi-block counterweight on rails, stroke 120–220 m |
| `MOD-DRUM-LOW` | Hoist drum room at 128 m |
| `MOD-CW-PIN` | Removable pin that changes stack mass / available travel |
| `MOD-WELL-LEDGES` | Maintenance ledges every 8–10 m |

**Example couplings**

- Ride the stack as a moving support to 220 m. Jump preserves stack velocity.  
- Pull `MOD-CW-PIN` to dump one block onto a timber catch. Cage payload rating changes; travel limit changes; a new ledge appears on the dumped block.  
- SKIN climb bypasses the well entirely on the west facade.

**Fall geography.** The well is a 220 m shaft. Unrecoverable if you miss every ledge and do not deploy a chute in time. Chute through the well is tight; clearance may make deployment illegal until the player reaches a wider bay. That is physical, not a UI deny.

---

### B03 — Wet Isolation (220–340 m)

**Physical job.** First process braid that is not flavor. A live header makes a stair a hazard and keeps a fire door’s counterweight wet/locked.

**Modules**

| ID | What it is |
|---|---|
| `MOD-HEADER-W` | Lumped process line: inventory + isolation + pressure design target |
| `MOD-SUMP-3` | Volume that floods the 240 m stair when the header blows or is dumped wrong |
| `MOD-BLIND-STATION` | Spectacle blinds and `CAP-BLIND` |
| `MOD-ISO-STAIR` | Stair that is a route only when sump is drained and blinds are in |
| `MOD-WET-LOCK` | Door/counterweight that will not travel while the line is live |

**Example couplings**

1. Insert blind, vent, drain sump. Stair becomes ordinary parkour. FLOW continues to TP-340.  
2. Dump the header into the sump on purpose to drown a fire below (if that fire exists later). Stair dies; SKIN around the north riser remains.  
3. Ignore process. Climb SKIN around the wet core. Workers on this floor, if active, will not cross the flooded stair because access is physically gone.

**Exit.** TP-340 dry route, or north SKIN to 340 m.

---

### B04 — Plate Shop (340–480 m)

**Physical job.** Constructive structure as the main verb. A transfer girder is unseated. Nearly every reachable shop member should matter if manipulated.

**Modules**

| ID | What it is |
|---|---|
| `MOD-SHOP-CRANE` | Overhead cab crane, 20 t SWL design target, richer station controls |
| `MOD-GIRDER-T` | Transfer girder that should sit on two column capitals at 456 m |
| `MOD-BRACE-K` | K-braces that can be unpinned to make room for the girder, changing sway/clearance |
| `MOD-PLATE-TABLES` | Movable plate loads that alter local deck load and can prop a failed brace |

**Example couplings**

- Cab-operate the shop crane, pick girder, seat capitals. Upper SHAFT guides for the next cage come into tolerance.  
- Unpin a K-brace to make crane clearance. Shop deck gains a soft axis; a shortcut opens; a previously safe walkway now sways enough to dump the player.  
- Use plates as cribbing instead of seating the girder. Ugly, persistent, valid if the reduced structural model says it holds.

**Fall geography.** East wall is open to facade. Long exterior drops begin to feel sincere here.

---

### B05 — Facade Crane Stack (480–640 m)

**Physical job.** Exterior as ordinary play. A climbing tower crane / facade traveler is the SKIN elevator.

**Modules**

| ID | What it is |
|---|---|
| `MOD-TRAVELER` | Facade traveler on vertical rails, finite drive, wind-hold brake |
| `MOD-BOOM-5` | Luffing boom the player can walk |
| `MOD-RAIL-JOINT` | Rail segment missing until a shop plate from B04 is hung |
| `MOD-TIE-IN` | Structural ties back to primary frame; cutting/unpinning them changes traveler stability |

**Example couplings**

- Seat `MOD-RAIL-JOINT` with SHAFT-delivered plate. Traveler can climb to TP-640.  
- Ride the boom as moving support to transfer onto Wind Frame steel later.  
- Leave traveler broken. Pure parkour on facade flanges to 640 m. Legal, slower, more fall.

**Fall geography.** Full exterior. Chute can steer to apron, to B04 shop outriggers, to traveler boom, or into the well. Powered ascent by chute is illegal.

Checkpoint intent: commit on first stable support at TP-640.

---

### B06 — Midstack Service (640–780 m)

**Physical job.** Inhabited band. Workers use real access. A locked service lift and a canteen/transfer deck exist so the tower feels occupied without becoming a quest hub.

**Modules**

| ID | What it is |
|---|---|
| `MOD-SERVICE-LIFT` | Second cage, depends on B04 girder + B03 isolation if its hydraulic pack is on HEADER-W’s sister line |
| `MOD-OPS-FLOOR` | Occupied deck, sparse HUD, environmental story only |
| `MOD-LOCKOUT-BOARD` | Physical lockout points that duplicate `CAP-BLIND` logic for this band |

Workers will ride `MOD-SERVICE-LIFT` if it actually runs. If the player wrecked the girder seating, they walk SKIN or stay put. No mission teleport.

**Exit.** Lift to 780 m, stairs if isolation is safe, or SKIN.

---

### B07 — Wind Frame (780–940 m)

**Physical job.** Open structural parkour at height. Braces are the routes. Moving the wrong brace is a committed problem.

**Modules**

| ID | What it is |
|---|---|
| `MOD-WIND-X` | Primary X-bracing bays, walkable flanges |
| `MOD-DAMPER-MASS` | Tuned mass the player can ride a short stroke after unlocking its clutch |
| `MOD-JUMP-LIFT` | Temporary jump-lift frames that can be winched one story if `CAP-NEEDLE` logic is reused |

**Example couplings**

- Traverse X-brace flanges to TP-940.  
- Clutch the damper mass and ride it; inherited velocity launches a gap that is otherwise too long.  
- Steal a brace as a needle for a later bay. Wind Frame sway increases. Valid and persistent.

---

### B08 — High Process Column (940–1100 m)

**Physical job.** Process changes machine availability at height. A live steam/hydraulic riser keeps the high drum clutch in lockout.

**Modules**

| ID | What it is |
|---|---|
| `MOD-HEADER-H` | High riser network |
| `MOD-DRUM-HIGH` | Upper SHAFT drum |
| `MOD-VENT-STACK` | Vent path; opening it drops pressure and may create a scald/visibility hazard on SKIN |

**Example couplings**

- Isolate + vent HEADER-H. `CAP-DRUM` station becomes operable. Cage can continue.  
- Open vent without isolation: SKIN becomes a hazard; SHAFT still locked.  
- Ignore FLOW. Climb Wind Frame remnants on SKIN to 1100 m.

---

### B09 — Crown Approaches (1100–1280 m)

**Physical job.** First band that should often demand two macros at once, still not all three everywhere.

**Modules**

| ID | What it is |
|---|---|
| `MOD-TRANSFER-CROWN` | Last wide plate before the mast slims |
| `MOD-STRAND-JACK` | Strand-jack pair for lifting a crown beam section |
| `MOD-BEAM-CROWN` | Beam that, seated, becomes both a walkway and the mast’s missing chord |

**Example couplings**

- Freight lifts the beam; structure accepts the seat; SKIN walkway to TP-1280 appears.  
- Process must be isolated first if jack hydraulics share HEADER-H.  
- Without the beam, a desperate SKIN climb on remaining chords is possible and ugly.

---

### B10 — Mast and Cooling (1280–1480 m)

**Physical job.** Slim structure, process air/heat, high consequence. Cooling fans are machines and moving supports, not set dressing.

**Modules**

| ID | What it is |
|---|---|
| `MOD-MAST` | Stepped mast, timber platforms at landings |
| `MOD-FAN-A/B` | Large axial fans; blades are lethal and also periodic supports if locked out |
| `MOD-COIL-ISO` | Isolation that stops fans and drops heat hazard |

**Example couplings**

- Isolate coils, lock fans, climb through the bell.  
- Time a locked-out slow roll as a rotating support (only if the machine model can do this honestly; otherwise fans are binary lockout).  
- SKIN climb the mast lattice. Exposed, legal.

---

### B11 — Summit Machine (1480–1600 m)

**Physical job.** Final physical access. Campaign completes when the player’s support state is stable on `z ≥ 1600` at the summit deck.

**Modules**

| ID | What it is |
|---|---|
| `MOD-SUMMIT-DECK` | Walking surface at 1600 m |
| `MOD-CROWN-CRANE` | Last jib; optional, not a gate if the player already has a route |
| `MOD-CROWN-LOCK` | Isolation + pin set `CAP-CROWN` for people who come up through the machine room rather than the mast skin |

**Completion predicate (authoritative, physical):**

```text
player.alive
AND player.support_valid
AND player.z >= 1600.00
AND player.inside(MOD-SUMMIT-DECK or legal summit volume)
```

No flag may set “summit reached” without that predicate. Story presentation may react. It may not complete the campaign.

---

## 7. Causal grammar (how chains are authored)

Every important coupling must be writable as:

```text
ACT[player verb + object]
  → STATE[owned field changes]
  → WORLD[geometry / support / machine limit / process / inhabitant access]
  → PLAY[new traversal or capability]
```

Illegal authorship:

- `ACT → PLAY` with no STATE;
- `MISSION_FLAG → WORLD`;
- animation success without actuator limits;
- deleting aftermath so the next band can load clean.

Representative legal chains already placed in the tower:

| Chain | ACT | STATE | WORLD | PLAY |
|---|---|---|---|---|
| K0 Intake | move crate / lift crate | dog unpinned | gate can travel | stair to +40 m |
| K1 Needles | seat both needles | guide tolerance in range | cage interlock true | ride to 120 m |
| K2 Pin | pull CW pin | stack mass/travel changes | new body on catch | ride or climb new ledge |
| K3 Blind | insert blind + drain | header isolated, sump empty | stair not a hazard | FLOW to 340 m |
| K4 Girder | crane-seat girder | transfer load path exists | upper guides true | SHAFT continues |
| K5 Rail | hang missing rail | traveler track continuous | traveler stroke extends | SKIN elevator |
| K6 Header-H | isolate high riser | clutch lockout false | high drum actuates | SHAFT to approaches |
| K7 Crown beam | jack + seat beam | mast chord complete | walkway exists | walk to TP-1280 |
| K8 Fans | isolate + lock | fan kinematic frozen | bell is support | mast interior climb |

The player may skip a chain by another legal braid. Skipped machines stay in their last committed state.

---

## 8. Checkpoints, death, and chute landings

### 8.1 Automatic commits (content intent)

Commit when all are true:

- player support is stable for a short dwell;
- player is not in an unrecoverable hazard volume;
- player has entered a new Transfer Plate **or** a marked refuge ledger (drum rooms, shop floor, ops floor, summit).

Exact dwell seconds are tuning. Gaps should stay short enough that experiment is rational.

A commit stores player pose and all consequential world state required for equivalent continuation.

### 8.2 Hard-fail missions

None are designated in this atlas v1.0. Default law stands: live with committed mistakes unless physically recovered, or until a later document explicitly marks a hard-fail mission.

### 8.3 Parachute landing fields

Legal chute destinations if clearance/velocity allow:

- apron 180 m field;
- Transfer Plate roofs and outriggers;
- yard jib / traveler / shop-crane booms as moving or static decks;
- wide timber halls (B01, B04, B06);
- not: inside tight wells unless a bay opening exists;
- not: powered return to the last intended route.

After a survived landing, the player is where they are. Re-ascent uses real routes or a later unrecoverable-failure rollback to the last commit.

---

## 9. Kernel slice (what implementation builds first)

Do **not** author B00–B11 as content before WO-008 is real. The atlas’s implementable kernel is a compressed physical sentence at apron scale:

**Kernel space:** `z = 0–24 m`, plan cut 36 m × 36 m of apron + one bay of frame + a 8 m well stub.

**Kernel modules (stable IDs):**

| ID | Role in WO-005–008 |
|---|---|
| `KX-DECK` | Static walkable apron + one stair island |
| `KX-BELT` | Translating support (WO-002/005 substrate) |
| `KX-JIB` | First freight machine, 5 t class design target |
| `KX-CRATE` | Movable load |
| `KX-DOG` | Gate/dog that changes traversal when unpinned |
| `KX-NEEDLE` | One seatable beam that changes support/geometry (WO-006) |
| `KX-POCKETS` | Two structural pockets |
| `KX-SUMP` | Process volume + valve/blind (WO-007) |
| `KX-GRATE` | Walkway that is a hazard while the sump is wet |
| `KX-REFUGE` | Checkpoint volume on the +8 m island |

**Kernel chain (WO-008 proof):**

```text
player uses KX-JIB (or grounded shove if mass allows)
  → KX-CRATE pose changes
  → KX-DOG unpinned
  → player seats KX-NEEDLE with the jib
  → seated needle is walkable support
  → player isolates/drains KX-SUMP
  → KX-GRATE becomes ordinary support instead of a fall/hazard
  → player walks needle + grate to KX-REFUGE
  → checkpoint commit
  → reload restores crate/dog/needle/sump/player
```

Any physically valid subset that still crosses **freight + structure + process + changed traversal + persist** satisfies the architecture proof. The written chain is the acceptance fixture, not the only legal player solution.

After WO-008, content assembly proceeds band-by-band from B00 upward, reusing kernel primitives: moving support, finite machine, seatable member, isolation graph, checkpoint.

---

## 10. Streaming and sleep cuts

Runtime regions should follow band cuts and Transfer Plates.

A region may sleep only when TDD §15 allows it. Cross-band machines (`MOD-CAGE-1`, `MOD-TRAVELER`, `MOD-CW-STACK`, headers that span plates) keep both ends active or keep a reduced model that still owns travel, energy, and interlocks.

Sleeping B04 cannot delete a girder the traveler in B05 still needs.

---

## 11. Inhabitants (content rules, not a cast list)

No named story cast is introduced here.

When a band is marked occupied:

- NPCs path only on current traversal predicates;
- they may operate a machine only if that machine’s native state allows it;
- they evacuate through real exits;
- they do not walk through a live header or a missing deck because a beat would be nicer.

---

## 12. Content rejection tests

A module, scene, or “shortcut” fails this atlas if it:

1. loads a band as a disposable arena and unloads the tower below as if it never existed;
2. paints a parkour stripe instead of making the member itself the route;
3. scripts the crate/dog/needle/header result without native state;
4. uses a mission flag as the summit or plate-completion condition;
5. blocks a valid SKIN climb to protect a SHAFT puzzle;
6. gives the chute powered lift or auto-route return;
7. puts invisible walls on exterior bays “because 640 m is scary”;
8. makes every band require all three macros;
9. adds process state that does not change a verb, hazard, route, load, machine, inhabitant, or aftermath;
10. resets seated beams / dumped counterweights on band transition to keep authorship tidy;
11. treats Godot animation as machine success;
12. invents a combat loop to fill quiet vertical distance.

---

## 13. Acceptance tests for the atlas itself

This atlas is doing its job when later work can answer, without inventing a new tower:

- Where does the player start, in meters, and what is broken?
- What three braids exist, and where must they be able to hand off?
- What physical object is the usual gate at each Transfer Plate?
- What is the first chain WO-008 must prove?
- What predicate ends the campaign?
- What happens if the player ignores the “intended” machine and climbs the skin?

If a work order or scene cannot point at a module ID in this file, it is not yet using content authority.

---

## 14. Deliberate open content

Still open, on purpose:

- exact safe working loads after benchmarks;
- exact header fluid identity (water vs hydraulic vs steam) per line — B03 is wet and drainable; B08 is a lockout/heat/pressure line; species may be chosen at process-implementation time if gameplay predicates stay the same;
- final machine roster beyond the named modules;
- portable tool roster beyond the capability table;
- VO line lists for falls (GDD requires human panic/profanity; writing is downstream);
- art pass, signage language, branding on machines;
- any hard-fail mission list.

---

## 15. North star (content form)

> Start on the apron of one 1.6 km industrial tower. Unjam, seat, isolate, ride, and climb a braided machine that is the building. Ignore a mechanism if you can physically afford to. Live with what you moved. Land where the chute and the steel allow. Stop only when you are standing on 1600 m.
