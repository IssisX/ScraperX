# Finish line and reasons to play

An audit for Cory, 2026-11-12, against trunk `8fb771f6`. It changes no code and no test.

## Decisions (11-12; Cory: "I want you to answer that stuff for me", so Claude answered C3)

1. The route the game plays is the route: yes. Rule 56's "not canonical above 221 m" is ended.
2. The fight is done at today's contact: yes. J-044c to k and n, J-060, J-036 and J-040 are parked.
3. The hold with real arms (J-001, then J-093): no cut date (Cory: "we're not gonna wait another week").
   Each beat lands as soon as it is green; a fix that fails goes up CLAUDE.md's ladder at once.
4. She must hunt to the summit for "done": no. Done is proven to 340 m (F5 measures above 220 m first).
5. Park the open set pieces and bring them back as missions: yes, except J-108 (the bait lift, built, in proof)
   and J-043 (the swing, cause found, rail move in build), which finish. "Land what is green" holds for the rest.
6. The story frame: the punch list.

Not taken: F2's "the peak check replaced by a law bound". A measured peak is tighter than the law, so the swap
weakens a test (CLAUDE.md). The re-pin habit is fixed another way when it comes up, never by loosening.
Already done when decided: F2's cause (J-054 inserts stock mid-table) is found; F1's "merge #343" waits on the
switchboard fix now being built.

**How to read the marks.** Every claim about the game carries one:
- **[measured]** a CI result or a count that was run. The trunk proof used is run 2457, green on `7c077336`
  (the full path: native, 94 touch scenarios, Android). Counts were made by grep on this checkout.
- **[read]** read in the source, a test or an issue thread, not run.
- **[estimated]** a judgment or a size.

No native test and no game session was run for this audit: the checkout has no built test binary, and the
trunk proof already covers what a local run would. Where one check would settle a doubt, it is named.

## The short version

1. **The climb is finished enough to ship and nobody has said so.** One touch run plays from the yard to
   803 m with no deaths, in CI [measured]. The ending runs [measured, from the summit spawn]. What is missing
   is small: the two joined in one run, and a ruling that the route the game plays is the route.
2. **She is rich but stuck behind two rebuilds.** Her hold with real arms (J-001) and her jointed upper body
   (J-044c) are each a rebuild of something that works on trunk. Eight other jobs wait on them [read].
3. **Above about 220 m there is no hunter, no story and no goal.** Her pursuit is proven from her locker to
   132 m, and at 154 to 220 m [read]. The radio's last line is at the 220 ring [read]. Three quarters of the
   tower is rides.
4. **Finished work is not landing.** Five green branches have no PR, four finished jobs are still open, two
   fixes sat unpushed, and four goals were restated as new jobs [read]. In ten days 114 labeled jobs were
   opened and 49 closed [measured].
5. **The game already has a story and the bones of missions.** Jack Miller, Walt Brenner, the Saturday, her
   accident, the last beam, the searchlight that stays lit [read]. It needs a spine, not a new idea.

---

# PART A: THE FINISH LINE

## A1. What exists today

### The climb [read unless marked]

- Start in the yard at grade; the title reads KELLERWORKS TOWER (`ui/title_screen.gd`).
- **0 to 154 m, the Stack** (`band_stack.cpp`): three hoists (S1 water balance, S2 walking beam, S3 brake
  override) and three climbs (C1 to C3) between them.
- **154 to 220 m, the Counterweight Well** (`band_counterweight_well.cpp`): a skip lift, a derrick boom, a
  debris chute. He rigs, pulls and rides.
- **220 to 340 m, Wet Isolation** (`band_wet_isolation.cpp`): the float ram, the chiller drop, the accumulator.
- **340 to 484 m, the Plate Shop** (`band_plate_shop.cpp`): scaffold slump, girder tip, the monolith; then a
  ladder to the 484 ring.
- **484 to 640 m, the Facade Crane Stack** (`band_facade_crane.cpp`): the runaway wagon, the jib pendulum, the
  kinetic winch.
- **640 to 750 m, Midstack Service** (`band_midstack_service.cpp`): the reel lift, a climb (C4), the gravel
  hoist, a climb (C5), the gravel wheel.
- **750 to 802.64 m** (`vertical/vertical_route.cpp`): the crown gondola and the luffing derrick.
- **One run plays all of it.** `touch_stack` goes from grade to the derrick's upper receiver, above 803 m,
  with zero deaths, on touch input, in CI [measured].
- **A second branch** leaves the 220 ring: swing, C6, cascade mast or C7, pitman lift, barrel helix, TP-340.
  It is proven only in pieces (`touch_swing`, `touch_c6`, `touch_c7`, `touch_machines`).
- **Side ways:** the slingshot (grade to 220 m), the slab incline, the stone wheel, the traction tram, the
  hook bridge to the second shaft.
- **A no-lift route** runs beside the machines: native `ascent` plays 154 m to TP-640 with no lift.
- He has about thirty moves, each with its own touch scenario (vault, slide, roll, wall run, wall kick,
  hang, monkey bars, trapeze, zipline and so on) [measured: the scenarios pass].

### Her [read unless marked]

- She sleeps in a locker on deck 1 (11 m). Noise wakes her: she strains, bursts out and hunts.
- **Senses:** hearing by how loud he is and how sound carries through the frame; sight at close range. Her
  code can only read evidence with an age, never his true position (`matron_perception.hpp`).
- **States:** dormant, straining, emerging, hunting, searching, shuffling, guarding, peering, unrigging his
  rope, staggering, down, and the hold's seven states (`matron.hpp`).
- **Stages:** pest, dangerous, changed, by how much noise has reached her.
- **Travel:** one general skill. She scans real steel, plans, and climbs by capped force: ladders, poles,
  risers, mantles, drops (`matron_traversal.*`). One budget: 8 kN, 44 kW.
- **Stalk:** she freezes under his gaze and closes when he looks away (`matron_stalk.cpp`).
- **Hold:** ankle grab, carry to the rail, dangle, arm tear, set down; the second catch is the fatality. His
  blow frees him before the rip (`matron_hold.cpp`, `matron_fatality.cpp`).
- **Fight:** his strikes read on her, she has tells, she takes damage, staggers, goes down, and can die.
- **Injuries persist:** a checkpoint and a relaunch both restore her damage and her death
  (`tests/sim_tests_matron_damage.cpp` lines 98 and 118).
- **She uses the world:** throws loose stock, shoves his placed things, takes his rope off its eye.
- **Proof:** about sixty native `matron-*` groups and eleven touch scenarios, green on trunk [measured].
- **Where she is proven:** locker to 132 m (J-085), and 154 to 220 m (`touch_matron_act`). Above that, nothing.
  The whole-climb run with her switched on reports "she never chases him" (J-084's thread).

### The ending, saves and the rest [read unless marked]

- **Summit, 802.64 m** (`summit.hpp`): a dead searchlight, a runway arm, and the crew's last beam on chain
  blocks.
- **Ending:** hold LOWER LAST BEAM until it sits on two bearings; Brenner's last caption; FREE SLING; run off
  the runway, open the canopy, land alive at grade; an end card. No cutscene, no credits roll. Proven from the
  summit spawn by native `summit-ending` and `touch_summit_ending` [measured].
- **The searchlight, once lit, stays lit on every later launch** (`user://summit.lit`).
- **Checkpoints** are automatic on firm footing. They hold him, every machine, loose stock, ropes, water and
  her state, stage, damage and death.
- **Death** leaves his body where it fell and returns him to the checkpoint.
- **Relaunch:** `user://climb.save`, CONTINUE and NEW CLIMB on the title, bad saves set aside.
- **Things to use:** the forklift (yard and 154 deck), the yard crane with magnet and spreader, 24 kinds of
  loose stock, the hook (220 ring), the parachute (always carried), throwables. The yard crane is needed by
  nothing.
- **People:** up to fourteen workers who yield, flinch and stumble. They cannot die.
- **Story on screen:** five radio captions from Walt Brenner; a WORK ID page for JACK MILLER, ACCESS & RIGGING;
  the end card. There is no opening text, no mission, no objective and no goal list anywhere [measured by
  grep].

## A2. Definition of done

"Done" is shippable and fun in the hand. Each line is a fact a player would see, with its proof.

| Part | Done means | Proof | Today |
|---|---|---|---|
| **The climb** | One run from the yard to the last beam and home by canopy, no deaths, on touch | `touch_stack` joined to `touch_summit_ending` | Two runs, both green; not joined |
| | The route the game plays is the named route | A ruling on rule 56 | Open: C3 question 1 |
| | A death at any stage returns him to a playable spot | The `-wreck` and restore tests per band | Read as covered; not audited band by band |
| | The canopy brings him down alive from every deck edge | J-124 | Queued |
| **Each way up** | From every deck there is a way on with no lift | Native `ascent`, extended to 750 m | 154 m to TP-640 proven |
| | A machine spent with nobody aboard never ends the climb | One test per machine: the no-lift way still reaches the next deck | J-015: partly |
| | No ladder left on the machine route above 221 m but the few he keeps | The leg spiral (J-105 step 1), by his eye | Built on a branch |
| **Her chase and hunt** | Awake, she reaches him or his last sound at four heights | Pursuit scenarios under 132 m, 200s, 300s, high | Under 132 m only |
| | Hiding in silence loses her | J-049's tests | A third landed |
| | She can never silently stop chasing again | J-102's regression test on trunk | Green, no PR |
| **Her grab and hold** | Caught, he sees the whole hold from grab to release or death | `matron-grab`, `touch_matron` | Green on trunk |
| | His blow frees him before the rip | `strike` | Green |
| | Her hand is on his ankle through the five beats; the fatality is 5 to 7 s | J-001's checks | Branch red in four groups |
| **The fight** | Every landed blow reads; she has tells | J-002, J-004 | Landed |
| | A hard blow from a real object staggers or drops her | J-077 | Done, not pushed |
| | Where he hits her shows: the black eye appears in play | J-059, PR #343 | Clean, not merged |
| **Injuries and death** | Her damage and death survive a checkpoint and a relaunch | `matron-damage` | Landed |
| **The ending** | The last beam, the caption, the canopy, the end card | `summit-ending` | Green |
| **Save and restore** | A relaunch equals a death restore; a bad save never breaks a load | J-116 | Queued |
| | Leaving the app mid-ride, mid-hold or mid-fall returns to that moment | J-118 | Queued |
| **The phone** | Ten climbs on the Fold with no force-close; frame time inside the bars | J-045, on his phone | Unproven |

**Not needed for done:** a jointed upper body, hits scored on drawn limbs, a new action on every stage above
340 m, dents, bending boards, finer hair, more scenery.

## A3. The gap to done, ranked by player impact over effort

| # | What is missing | Evidence | Size |
|---|---|---|---|
| 1 | **Finished work that has not landed.** Green branches with no PR: J-084 (whole climb), J-102 (the chase test), J-021 (waterfall), J-090 pass 1, J-096 slice 1. #343 is clean and unmerged. J-077 and J-078 are fixed and unpushed. J-069, J-042, J-016 and J-064 are finished and open. | [read] job threads, 11-12 | S |
| 2 | **The phone.** The force-close on full-height views has a fix and no proof. A crash outranks everything else on this list. | [read] J-045 | S, needs his phone |
| 3 | **One shared red.** `matron-crash` and the budget's "measured peak" check go red on six or more branches after unrelated changes. It blocks five lanes. | [read] J-107 | M |
| 4 | **The climb and the ending are two runs.** Nothing plays the receiver, the summit deck and the beam in one go. | [read] `ui_test_stack.gd`, `ui_test_act.gd` | S |
| 5 | **She does not hunt above 220 m.** Three quarters of the height. Her ladders do not fit her (#421), she cannot ride a cage (her scan skips anything that can move), and no proof exists above 220 m. | [read] J-084, J-085, `matron_traversal.cpp` | L; first a measurement (S) |
| 6 | **The hold rebuild.** 77 commits, 32 files, five PRs closed unmerged, 102 comments, still red. J-093, J-086, J-090 pass 2 and J-018 wait on it. | [read] J-001 | L, or S to cut |
| 7 | **A spent machine can end the climb.** S2, S3, F, M, N and O have no proven way on. | [read] J-015 | M |
| 8 | **Save holes.** Relaunch against death restore, a checkpoint on a moving floor, leaving mid-ride. Her stage may be lost on relaunch: save at Dangerous, load fresh, read her state. | [read] J-116 to J-118, `main.gd` | M each |
| 9 | **The one ladder on the machine route** (462 to 484 m). | [read] `band_plate_shop.cpp` line 405 | M, his eye |
| 10 | **Two synthesized human sounds** (her wake, his breath) against rule 41. | [read] J-098 | S, his ear |
| 11 | **Hiding leaks** in the hold and fatality code. | [read] J-049 | S to M |
| 12 | **The ladder pull** on every ladder kind (rule 35). | [read] J-086 | M, after 6 |

## A4. The stop list

Each of these adds to a part that already works, or rebuilds one. None moves a part to done. Stop means
park, with the branch kept.

| Stop | Why |
|---|---|
| **J-044c to k and n: the jointed upper body.** With it J-060, J-036, J-040. | It changes her mass split, so every hold and budget test moves. Nothing a player sees lands until all of it does. Only Cory can call this: C3 question 2. |
| **J-038 after PR #372, and J-099 (dents).** | 86 comments, seven FIX heads, three red heads. Land crates that break; park dents. J-099 restates work already inside #372. |
| **J-067, her hair.** | No written finish; each look made the next fix. The 11-10 ruling said it closes on the scorer; the scorer merged. Close it. |
| **J-059, after #343 merges.** | The look landed days ago; only the wire-up is left. No more bruise work. |
| **J-021 after the waterfall fix; J-022's clouds and dust.** | Scenery on a part that reads fine. |
| **J-082.** | Stopped by Cory 11-11. Close it; J-105 holds the goal. |
| **J-105 beyond step 1.** | He OK'd one step. The other ten ways up are new content. They matter only if question 4 says she must hunt to the top. (This audit's author wrote that design; it still belongs here.) |
| **New set pieces: J-043, J-054, J-055, J-091, J-094, J-095, J-100, J-108, J-090 pass 2, J-096 beyond slice 1.** | Each is a new thing to do, not a finished part. J-043 has 29 comments and has not tried one acceptance item. Land what is green, then hold them for Part B, where each comes back as a mission with a goal. |
| **The "prove everything" batch: J-111, J-113, J-114, J-120 to J-123.** | Proof and polish of working parts. Keep J-116 to J-118 (saves), J-119 (controls on the Fold), J-124 (the canopy), J-125 (licences). |
| **J-078, J-079, J-081, J-088** once their open PRs land. | Motion, camera, night and fingers: all polish. |

**Three habits to stop:**
1. **A new job for an old goal.** J-017 became J-082 became J-105. J-015 became J-109. J-016 became J-112.
   J-038 became J-099. Reopen the old issue instead.
2. **Re-pinning the budget's measured peak, one protected PR per branch.** Fix the check once so it tests the
   law (8 kN, 44 kW), not a recorded number.
3. **A long branch with no PR.** J-001 is 77 commits ahead of trunk. Land it in slices that pass.

## A5. The finish plan

Routing follows the evidence in the threads [read]. Muse lands well-specified work (J-077: five scenarios,
first try; J-069; J-078). It stalls when the cause is unknown or crosses systems (J-043: three tweaks, 4 to
9 cm short, until the cause was found for it; J-054: red on `matron-crash`, never bisected; J-038: her hunt
reacting to break noise). `work/tasks/job-routing.md` does not exist yet.

| # | Job | Owner | Done-test |
|---|---|---|---|
| F1 | Land the green: PRs for J-084, J-102, J-021, J-090 pass 1, J-096 slice 1; merge #343; push J-077 and J-078; close J-069, J-042, J-016, J-064 | Integrator | No green branch without a PR; those issues closed |
| F2 | The shared red (J-107) | Claude helper | The cause named; `matron-crash` passes three times on trunk and on the five branches; the peak check replaced by a law bound (protected) |
| F3 | The phone (J-045) | Cory's phone, Muse | Ten climbs with no force-close; walk p95 at or under 11.1 ms; locker burst no frame over 50 ms |
| F4 | The climb and the ending in one run | Claude helper | One touch run from grade to the Home phase, zero deaths, in CI |
| F5 | Measure her above 220 m | Claude helper | A table per band from the whole-climb run with her on: did she get within 10 m, and what stopped her |
| F6 | The hold: land or cut (J-001, then J-093) | Claude helper | By the date Cory sets, the branch is green and merged, or the beats that pass are merged and the rest parked |
| F7 | A way on after every spent machine (J-015 and J-109 as one) | Muse | For S2, S3, F, M, N, O spent empty: the no-lift way reaches the next deck |
| F8 | Saves (J-116, J-117, J-118; her stage on relaunch) | Muse | Each job's own test; one save version bump for all three |
| F9 | The canopy from every deck edge (J-124) | Muse | He lands alive from each edge |
| F10 | The leg spiral (J-105 step 1) | Muse built it; Cory's eye | She climbs it without tipping; he walks it; the ladder at 462 m is gone |
| F11 | Hiding leaks (J-049 parts 2 and 3) | Muse | Silent and hidden through a hold: she does not know where he is on release |
| F12 | The ladder pull (J-086) | Muse, after F6 | Every ladder kind passes |
| F13 | Real recordings for two sounds (J-098) | Muse, Cory's ear | No synthesized human sound in the APK |
| F14 | Crates break; dents parked (#372 cut down) | Claude helper for the red | #372 merged without the dent system, or with it green |
| F15 | Controls on the Fold, folded and open (J-119); licences (J-125) | Muse | Every control reachable on both screens; the Licences page true |

After F15 the current parts are done. F1 to F4 are a few days. F5 decides how big the rest is.

---

# PART B: REASONS TO PLAY

## B1. Who plays this beyond the sandbox

| Player | What they need | What the game gives them now |
|---|---|---|
| **The one who wants to know what happened** | A premise in the first minute, a question that pays off, an ending that means something | Five captions, the last 580 m in silence, a strong final line |
| **The one who wants a job to finish** | Clear goals, a list that fills, something that changes when a goal is met | Nothing. Time and deaths on the end card |
| **The one who wants to get good** | Choices with a cost, a route to perfect, a foe who answers back | The systems exist (noise, her injuries, three routes, the slingshot); nothing asks him to use them |

The sandbox player is already served: the forklift, the crane, and her.

## B2. Story frames

All three keep what is in the game: Kellerworks Tower, Jack Miller the rigger, Walt Brenner on the radio, a
Saturday, her accident that Brenner blamed on her, the last beam, the canopy home. Her identity and the story
are Cory's. These are options.

### Frame 1: Saturday (the cover-up, finished as written)

- **Him:** a rigger who came in on a Saturday. **Why he climbs:** by the 220 ring she is below him and the
  only way off is the top.
- **Her:** the site nurse the crew called the Matron, changed by a lost X-ray source; Brenner wrote it up as
  her fault. **The tower:** a job one beam short. **The summit:** the beam seated, Brenner's admission, the
  canopy.
- **Strongest scene:** the 220 ring. "Don't come down. She's below you. The only way off is the top." A
  trespass turns into an escape.
- **Risk:** it is told to him, not done by him. It never says why Jack came. It lives in five captions and
  then stops for 580 m.
- **Cost:** smallest. More lines, an opening card, the overtime crew's bodies.

### Frame 2: The punch list (recommended)

- **Him:** the same Jack. **Why he climbs:** to finish the tower. Kellerworks tops out Monday. The overtime
  crew went up Friday night to finish and never came down. Jack goes up to finish their work, and Brenner,
  who needs it finished more than anyone, feeds him the punch list.
- **Her:** the same nurse, and the reason nobody finished. **The tower:** every machine is a crew's rig for a
  job left half done, which is rule 50 made literal: every machine has its reason. **The summit:** topping
  out. The beam is home, the searchlight is lit, and the tower is finished because of him.
- **Strongest scene:** the first overtime man, at his station with the job half done and the radio still in
  his hand. Jack finishes the man's job while she comes.
- **Risk:** chores, if a job does not change the tower. Guard: every finished job lights that deck for good.
- **Cost:** the mission layer in B5. It is the frame that makes missions part of the story instead of a menu.
- Frame 1's facts all stay true inside it. Frame 1 is what happened; frame 2 is what he does about it.

### Frame 3: Her ward

- **Him:** her next patient. **Why he climbs:** she keeps a chart on him, and it is right (the design in
  `03_EXECUTION/UNCANNY/HER_CHART.md`).
- **Her:** the centre of the game. She has done this before. **The tower:** hers. **The summit:** discharge.
- **Strongest scene:** a page in her hand that describes what this player did five minutes ago.
- **Risk:** it rests on work that is parked or unbuilt (the chart from live play, the mic, her return), it
  uses the machines least, and it changes her the most.
- **Cost:** largest. Best kept as a layer on frame 2 later: her chart pages as things to find.

## B3. Missions

### The structure

**A mission is a line on the punch list.** Each line is a fact about the world that the native code can
check every tick, the way `Summit` already checks the beam (seated on two bearings, settled, sling free). No
script decides an outcome. The world stays causal.

**Kinds of line, all built from what exists:**
1. **Seat it:** put a load where it belongs (the beam; a crane load on a deck).
2. **Start it:** bring a machine or a plant up (S1; the three wet-isolation machines).
3. **Bring it:** carry or drive stock from one place to another (the forklift, the hoists).
4. **Find him:** reach one of the overtime crew.
5. **Use her:** put her where the tower can hurt or drop her (the bay, the catwalk, the slab).
6. **Get past her:** reach a place with her still asleep, or lose her and stay lost.

**Given by** Brenner on the radio. His lines are already cued by a place or an event, once each
(`foreman_radio.gd`). A job line is the same thing with one more cue.

**Tracked** on the pause card, which already shows altitude, checkpoint, deaths and the work ID: add PUNCH
LIST, three lines at most. The route hint, which exists, points at the current line.

**Failure and retry.** A line cannot fail. It is true or not yet true. A dropped load stays dropped; he
re-rigs it or takes the canopy down and comes back. Death returns the checkpoint, and the list with it. Her
injuries persist, so a retry meets her as he left her: hurt her early and every later job is easier; enrage
her and every later job is louder. If she is dead she stays dead, and the jobs go on without her.

**Progress and unlocks.**
- A finished job lights its deck's work lamps for good, on every later launch, the way the searchlight
  already stays lit. The tower fills with light from the ground up. That is the record, and it can be seen
  from the yard.
- A finished band gives a start on the title for that band (the start list exists today for debug builds).
- Jobs hand over tools where the tools already are: the hook at the 220 ring.

**Through save and load:** which lines are done, which lamps are lit, which starts are open. The world
itself already saves.

### The first eight

| # | Mission | Where | Goal | Systems | How she complicates it | He learns or gains | New code | Proof |
|---|---|---|---|---|---|---|---|---|
| 1 | Clock in | Yard to 22 m | Get S1 running and ride it | S1, Brenner's first line | Asleep | The list | S | `touch_stack` first leg; line 1 done |
| 2 | Feed the tower | Yard | Set a crane load down on the deck 3 landing | Yard crane, magnet, stock | The crane is loud; she wakes sooner | Stock up top to fight with | S to M | `touch_crane_magnet` extended: the load rests in the zone |
| 3 | Wake nothing | Deck 1 to 44 m | Reach deck 2 with her still asleep | Her hearing, creep and sprint | She is the goal | Noise is a cost | S | Native: creep past, dormant; sprint, straining |
| 4 | Danny's rig | 88 to 132 m | Find the first overtime man; load the pig; send S3 | S3, a body (his own corpse system) | The hoist calls her | The story's turn | M | New scenario: body found, S3 at the top |
| 5 | Turn the bay | 154 m | Put her down a deck with the tower's own trap | The bay, the catwalk, the forklift | She is the load | Time to rig the well | S | `touch_bay` plus the line |
| 6 | Bridge the shafts | 220 ring | Hook across to the second shaft, fetch the striker, come back | Hook, twin tower | She takes his rope off its eye | The hook | S to M | `touch_twin` with a carried part |
| 7 | Bring the plant up | 220 to 340 m | Float ram, chiller, accumulator, in order | Wet isolation as built | Nothing new | The upper tower opens | S | `touch_wet` plus three lines |
| 8 | Top out | Summit | Seat the last beam, light the searchlight, go home | The ending as built | Whatever is left of her | The end | S | `touch_summit_ending` |

Missions 1, 7 and 8 wrap stages that already pass. They are playable the day the list exists.

## B4. Gameplay depth, ranked by depth gained over effort

| # | Where the depth is | What exists | What it needs | New mechanic? |
|---|---|---|---|---|
| 1 | **Noise is the price of speed.** The sprint that outruns her calls her; machines call her. | Her hearing, machine hearing | Jobs with a quiet slow way and a loud fast way | No |
| 2 | **Her injuries are a plan.** A leg hurt on deck 2 is a slower hunter at 400 m. | Persistent local damage | Show it: her state on the pause card | No |
| 3 | **The tower as a weapon.** | Bay, catwalk, slab, forklift ram, thrown stock | "Use her" jobs | No |
| 4 | **Three ways up every band.** Machine, no-lift way, shortcut. | All three | Jobs that pay for each | No |
| 5 | **She answers back.** She unrigs his rope, shoves his stock, guards the way out. | Built | Jobs where that matters (mission 6) | No |
| 6 | **Her weight is a tool.** | J-094 and J-108 on branches | Land one as a mission | Yes |

**New mechanics, each Cory's to OK:** the punch list itself; lamps that stay lit; earned starts; a crew body;
her weight as a key or a lift; a best time (decided "no" on 11-02; it would serve the third player type).

## B5. The least the missions need

| Order | System | Size | Note |
|---|---|---|---|
| 1 | Objective core in the native: a table of checks, a done flag each, saved with the checkpoint | M | New files; it only reads the world |
| 2 | Job lines and cues in `foreman_radio.gd` | S | Text first; his voice is Cory's pick by ear |
| 3 | The punch list on the pause card and one HUD line | M | Must fit the Fold folded |
| 4 | Save fields and one version bump | S | Do it with F8's bump, not after |
| 5 | The route hint follows the current line | S | |
| 6 | Lamps lit for good per finished job | S to M | The searchlight's pattern |
| 7 | Earned starts on the title | S to M | |
| 8 | A native `objectives` group and one touch scenario per mission | S each | |

---

# PART C: SEQUENCE

## C1. From now to done to first missions

1. **This week: F1 to F4.** Land the green, kill the shared red, prove the phone, join the climb to the
   ending. This closes about a dozen jobs and puts the whole game in one CI run.
2. **Then F5,** the measurement, and Cory's answers to questions 1 to 4. They set the size of everything left.
3. **Then F6 to F15,** Muse on the specified ones, helpers on the hold and the reds.
4. **In parallel from day one,** touching nothing being finished: B5 items 1 to 3 and missions 1, 7 and 8.
   They read the world and add a card. The one shared edge is the save format: one bump, done with F8.
5. **After F15:** missions 2 to 6, and the parked set pieces come back as missions.

## C2. What drags the finish out again, and the guard

| Risk | Guard |
|---|---|
| New jobs faster than closed ones (114 against 49) | Until F15: a new job opens only when one closes, and only if it is on this plan |
| A goal restated as a new job | One issue per goal; reopen it |
| "Done" written as an adjective ("console quality", "reads as real") | A job without a checkable finish fact does not start |
| Cory's eye, ear or phone as the last gate on many jobs | One sitting a week: stills and clips ready, each a yes or no |
| Shared brittle checks | F2 fixes the check, not the branches |
| A long branch that never lands | No branch older than three days without a PR; slices |
| New ideas mid-finish, his included (bending, the hook, set pieces on 11-11) | A visible "after done" list; say no plainly, as rule 9 asks |
| The set pieces return as loose jobs | Each comes back only as a mission with a goal |

## C3. Questions for Cory

1. **Is the route the game plays the route?** Recommended: yes. End rule 56's "not canonical above 221 m".
   Otherwise the climb is not done until the second branch is joined and 340 to 750 m is rebuilt.
2. **Is the fight done at today's contact (hits on her body within 0.36 m, bruises where he hits)?**
   Recommended: yes, and park the jointed upper body. Otherwise the fight and four jobs wait on a body
   rebuild of unknown length.
3. **A cut date for her hold with real arms?** Recommended: one week, then ship what passes. Otherwise four
   jobs keep waiting on it.
4. **Must she hunt him to the summit for "done"?** Recommended: done is proven to 340 m; the upper tower gets
   her after the missions. Otherwise ten more ways up become finish work.
5. **Park the open set pieces and bring them back as missions?** Recommended: yes. Otherwise they keep
   pulling Muse off the finish jobs.
6. **Which story frame?** Recommended: the punch list, on the Saturday facts. Otherwise: frame 1 needs the
   least code and gives the least to do; frame 3 waits on her chart.
