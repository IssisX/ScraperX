# SCRAPERX — START HERE

**Write branch: `ScraperX-Claude`.** All implementation and all document writes land there.
Other branches may be read to recover provenance. None of them is a write target, and none of
them is an authority. `ScraperX-Grok`, `ChatGPT` and `Gemini` are sibling experiments. On
2026-09-28 the owner had `Gemini`'s world merged into this branch (its route from grade to the
+221 m ring); planning material has also been adopted from `ChatGPT` and `ScraperX-Grok`. Where
this tree and another branch disagree, **this tree wins**.

**Owner directives**, dated, newest first. The owner builds several versions of this game at once;
every request is checked against this list, and a repeat or a contradiction is pointed out.

| Date | Directive | Status here |
|---|---|---|
| 2026-10-20 | No harpoon; a laser welder and a nail gun instead: *"A hook on and a harpoon - maybe we could be a little more varietal."* then *"Laser welder, and nail gun too"* | in force: the hook gun stays; the planned line-throwing harpoon is dropped for a handheld laser welder and a powder-actuated nail gun, both to design with the owner. The welder welds and cuts metal too (*"Laser welder can cut metal too."*) |
| 2026-10-20 | Her limbs can be torn off: *"Would be nice to have real limbs and stuff rip off if hit by harpoon (hook gun) or put through any other damage that would do so. This Could be a very cool fixation point of the game that one would wanna constantly play it for."* | to design with the owner: the hook gun (built, found on the 220 ring), the laser welder, the nail gun, and any other blow strong enough to tear a real limb from her body; what she loses she comes back without (her return, the row below) |
| 2026-10-20 | The Matron can die: *"the harpoon can kill her, if it's a kill-shot. She can die in many ways as shes flesh and blood even if gross and scary! Have to figure out storyline that fits, that lets her maybe revive or respawn at some point"* | to design with the owner (the Matron's look stays parked): a kill-shot from the hook gun, the laser welder or the nail gun (the plan's harpoon is dropped for those two, the row above), and other deaths a flesh-and-blood body can die. Her return, the owner's pick (*"Option 3, she's done this before"*): she already survived something that should have killed her (the accident the foreman covered up), so death only sets her back; after a while she crawls back up the tower, visibly damaged and angrier |
| 2026-10-20 | Workflows, but efficient: *"workflows take immense resources and time ... So - yes use them, but be efficient without losing quality"* | in force: a workflow only where independent attempts or an adversarial check change the result (designs the owner picks from, risky shared-physics fixes); small clear jobs done directly; fewer agents, lighter effort on mechanical steps; no judge-and-spec step when the owner picks from the options, the full spec written only for the one chosen; no two design workflows at once unless truly independent |
| 2026-10-20 | Show every option a workflow finds: *"Sometimes there are good things that you don't choose in your workflows, so make sure you list the potentials in your workflow. Maybe I'll like one that you didn't choose"* | in force: a design workflow's results reach the owner with every design and every good idea in them, chosen or not, with the judge's reasons; the owner picks. The chiller drop's unchosen ideas are listed in `03_EXECUTION/CLIMB_REDESIGN/README.md` |
| 2026-10-20 | Replace ladders, never working machines: *"Yes I much much much much rather you replace stupid things like ladders rather than working machines dude - DUH"*, and *"I didn't want to replace any crane functionality."* | in force: a new experience takes the place of ladders and the like; a working machine stays whole (at most how it is started changes, as at the chiller drop). The crane band's machines stand as built; its save point is the branch `claude/crane-band-machines-2026-10-20` (this session pushes `claude/` branches; tags are refused) |
| 2026-10-20 | Keep replaced machines for reuse: *"If you can save these already made machines somewhere so we can use them modularly in the future that would be nice."* | in force: a machine taken off the route becomes a placeable module, as the owner's vertical machines are (`src/sim/vertical/`, placed by `vertical_route.cpp`), with its own native test in a test world so it stays proven. Nothing is being taken off the route now: the crane band's machines stay (the owner, 2026-10-20: *"I didn't want to replace and crane functionality"*) |
| 2026-10-20 | Two new experiences, from the design panel's three for the chiller drop: *"so #1 and #3"*. #1 at the chiller drop (256 → 298 m): walk through its door onto no floor, drop to the deck, and the landing sends the chiller down; its air drives the deck up and out of the shaft's mouth. #3 grown into the owner's own idea: a glass air tube *"like a bank"*, run by an air pump (*"dont have to show any internal vacuum parts"*), that curves; *"I want this to stretch many levels, maybe starts on one side of structure and ends on another, a 90° turn"*, round a corner. Its reason: a tube built for heavy spares that the crew rode anyway. It is an addition: *"I didn't want to replace and crane functionality and such"*; the crane band's runaway wagon, crane jib and kinetic winch stay as they are. It runs *"Instead of a ladder route"*: in place of the crane band's backup ladders, 484 m to the 640 m plate | in force: both designed with the owner before they are built |
| 2026-10-20 | The machines do not all have to be man powered: *"all I was saying was that the machines do not have to be just man powered. That's it."* (*"This one seemingly small change should actually open up a whole other world of possible experiences"*) | in force: plan §2.6 rule 1 |
| 2026-10-20 | CI is never skipped for work: *"I dont want instructions to 'skip CI' completely ... you were running full native suites over and over when we didn't have to"*, and *"always assume I could be wrong and dont take what i say at face value EVER"*. Every push that changes code, assets, tests or the CI file runs CI, work in progress included (from 7bb1e47 to 00bbc5e, nine such commits went up with `[skip ci]` and the build went untested for a day). Only a commit that changes nothing but documents carries `[skip ci]`: its run would rebuild the same game and, CI never cancelling a run in progress, hold the next real build behind it for an hour. The waste the owner meant is local: run the checks a change touches, the full native suite only when the change reaches the whole world and CI is not about to run it | in force; the owner may still want every push run, and is asked |
| 2026-10-20 | Next, the climb itself, not the Matron: *"I think we've done enough work on the matron. Let's not spend all of our usage on one part of the game."* The pulled lifts and ladders are replaced by physical experiences, but a few may stay: *"you can have a few of them in the game as theres many many levels/bands and a brand new experience for each one isnt REALLY feasible"*. Each new experience is talked through and refined with the owner before it is built | in force: the Matron's design is parked until the owner asks; the first replacement's design is with the owner |
| 2026-10-20 | Some on-screen controls stay: *"I still need SOME on-screen controls"* | done: the on-screen buttons are back in the gesture scheme (Jump, Crouch, Action and Drop drawn as on the buttons layout, Jump reading THROW with a load in the hands); the swipes are shortcuts over them, and only Turn is left to the stick |
| 2026-10-19 night | From now on, extraordinary experiences with true wow-factor: the game is deep enough for the stunning stuff, and the person playing should feel the climb and be engulfed in it. Help develop the game; add stock and dressing freely; iterate and prototype quickly with anything available; never shortcut to a weaker result | in force: the plan of action (progress log, the newest entry) puts it first |
| 2026-10-19 night | The Matron moves from deck to deck, up and down the tower: no "just her deck" | in force; begun: her corner pipe, the shaft's north-east riser from 132 m to her deck at 154 m. Her line on it is the riser's north-west corner, and she goes over the shaft's rail where her pipe comes up on 143 and 154. Heard at every hold, she comes down it after you and back up it. She listens for 2.5 s after stepping off it, and the riser up to her deck is a climb she knows. Her first act is staged round her pipe (`touch_matron_act`). Blows count only by contact, and her mischief takes only light stock (25 kg or less) |
| 2026-10-19 night | The player is never helpless against her: a fun way to keep her at bay that does not kill her | in force; to build |
| 2026-10-19 night | Her face as realistic as possible with the scary cosmetics: an uncanny, adult-scary face, not Halloween | in force; widened the next row |
| 2026-10-19 late | The Matron gets a serious graphical upgrade: *"She's too thin, and I don't like the red skin, and shes too i guess fake halloween-scary and not real true adult horror scary. I want to REALLY freak players out in unique ways."* Her whole figure, not only the face: a real old woman's body, real skin, unique adult-horror behavior | in force; a design panel prototyping it (progress log) |
| 2026-10-19 late | Graphical depth by importance: she gets *"the really elite tier stuff - the rare, truly complex & sophisticated graphical stuff"*; ordinary things such as the yard's stock do not get nearly the same focus | in force |
| 2026-10-19 late | Real voices for the characters, people and the Matron, made the way the climber's fear yells were (the method in `src/voice/generate_fall_voice.py`) | in force; replaces waiting on a recorded foreman. The climber's fear yells themselves are finished: not reworked, and their generator not edited, for the new voices (they get their own generator) |
| 2026-10-19 late | No replaying CI locally: *"There's no reason for you to be messing with the fall voice ... More wasteful behavior"*. After a fix, run locally only the scenarios and native groups that change touches; never CI's remaining list wholesale (the crane fix replayed it, `fall_voice` and all). CI plays every scenario on every push | in force |
| 2026-10-19 late | Touch only, for good: *"gamepad and keyboard and mouse and shit we are not doing now or ever. Stop wasting your time on that."* | in force: the 22 pad and keyboard playthroughs are out of CI; their input code stays in the game dormant, neither tested nor worked on; none is written again |
| 2026-10-19 late | Run workflows automatically and autonomously: *"I allow all. You have permissions."* The repository's `.claude/settings.json` allow list names every tool the cloud session uses, so routine work does not prompt | in force |
| 2026-10-19 night | Ultracode: Claude can and should run complex multi-agent workflows for substantive work. Supersedes the 2026-10-18 row limiting the workflow to once per big feature | in force |
| 2026-10-19 night | One GitHub branch: `ScraperX-Claude` (a side branch or clone only when needed) | in force |
| 2026-10-19 night | No redundant test runs: *"each change does not require a full native suite run - dont be wasteful. We want progress not redundancy"*. Locally, run the native groups and touch scenarios a change touches (`SCRAPERX_ONLY=<group>`). Run the full native suite only when a change reaches widely (the kit's body order, the world's solids, shared physics or tuning, the contact listener, save layout) and CI is not about to run it anyway. CI runs the full suite on every push | in force; restores the 2026-10-18 "no waste" row |
| 2026-10-19 night | No label on the screen for every object: the Action button's verb (PICK UP, LET GO) is enough in most cases | in force: the toolbox's name was taken off again |
| 2026-10-19 | How Claude works now: *"you CAN start workflows"*, push to the branch at will (*"You can push and should push at will"*), run local checks and CI at will, and optimize its own way of working rather than follow earlier process rules that get in the way. Supersedes the same day's "never start a session or workflow unasked, ask before anything large, keep work local until told to push" and the 2026-10-18 rows on local checks and on workflow prompts | in force |
| 2026-10-19 | A causal physics world: most things can be used, moved, bumped and knocked about; a thing that moves when hit is the physics working. Rule 5 "nothing strands" is dropped: the parachute is the way back from anywhere; a spent or wrecked machine must still leave some way to fix it or go on | in force (plan §2.6 rule 5) |
| 2026-10-19 | Talk in plain words: name things by what they are (the float lift at 220 m), not by internal labels (D, touch_wet) | in force |
| 2026-10-19 | This device only: the game is made for the phone it is played on, by touch. No time on mouse, keyboard or gamepad work: *"We're not particularly supporting PC, that's wasteful. Just target this device"* | in force; made absolute 2026-10-19 late (no gamepad, keyboard or mouse, now or ever) |
| 2026-10-18 | American spelling everywhere Claude writes (chat, commits, comments, docs): meter, center, color, tire, curb | in force; older British spellings are converted as each file is touched |
| 2026-10-18 | Claude has every permission: never ask the owner for permission to act; decide, act and report. Machines make sounds; only people have voices | in force; the project settings allow every tool the sessions use |
| 2026-10-18 | No waste: run locally only the checks for what a change touches; CI plays every scenario | superseded 2026-10-19 evening (checks at will), back in force 2026-10-19 night (no redundant test runs) |
| 2026-10-18 | The owner is on US Central time: give every time in Central (e.g. "2:33 pm Central"), not UTC | in force |
| 2026-10-18 | A prompt the app itself raises is acceptable now and then, never over and over: the multi-agent workflow (which always asks) at most once per big feature, for its closing review; everything routine on tools that do not ask | superseded 2026-10-19 night: run workflows freely |
| 2026-10-06 | Not "3D chutes and ladders": *"I didn't ask you to make me a game of 3D chutes and ladders … There's lifts that need a pull of something, and fucking ladders throughout the whole damn game. So boring"* | in the plan (§2.6 rule 11): no new ladder on the route and no lift started by pulling something; each stage is a different physical experience (launch, swing, ride what falls or tips, momentum). Built: the ground slingshot (`AS-011`), from `ChatGPT`'s working bands; and up from the +221 m ring, the swing (`AS-012`: a ram strikes the rider's seat, which swings up to the 242 ring; the rider kicks the trip), then C6, a climb with no ladder to the 264 ring (a running leap caught by the hands; girders walked on the balance out over the void and up at 24°). Replaced so far below 221 m: S1, the first lift, now goes on the rider's weight on a scale plate in its cage (2026-10-07); S2 on the rider shoving a shoot bolt out with the body (2026-10-17); S3 on an iron pig the rider carries into its cage and lets go of into the pan its brake's trip line ends in (2026-10-19); and above 220 m, D on its own spool, carried through the fill line's gap onto the platform and set in the cradle behind the rider, no valve (2026-10-19). Next: E to O (all but N), still started by a pull, and the climbs' ladders, one at a time; above the 264 ring, the next machine. Dropped: importing `ChatGPT`'s teeter and swinging ladder above 221 m (both end in ladder climbs), and a counterweighted boom from the 220 ring (S2's walking beam again, rule 9) |
| 2026-10-06 | The canonical climb ends at the **+221 m ring** (the route rebuilt from grade); it does not run to the 750 deck. Import the `ChatGPT` branch's machines modularly and insert them into the canonical climb from there, to help the player ascend | in progress: the bands above 221 m (AS-007 to AS-010, built on the old bands) are not canonical, whatever their tests reach; each imported machine is placed from the +221 m ring up, played through touch. The climb now goes on to the 242 ring by `AS-012`, the swing (`touch_swing`), and to the 264 ring by C6 (`touch_c6`) |
| 2026-10-06 | Bring the good parts of the `ChatGPT` branch here, and keep this branch's own identity on purpose ("keep the Claude vibe going") | in progress: parts are ported one at a time and each is proven on this branch's suite and touch route. Not taken: their APK identity, and their documents and evidence, which would become a second authority. Their machines below 143 m are judged one by one, and one that repeats a machine this route already has stays out (plan §2.6 rule 9) |
| 2026-09-30 | Law: *"The initial state should determine the final state."* | in the plan (§2.6 rule 10); the native `determinism` check runs the same start and inputs twice and requires the same end state, bit for bit. Not established across machines (ARM APK against x86 CI) |
| 2026-09-30 | Nothing repetitive: *"there are way too many machines or mechanisms that are the same … I don't want repetitive anything"* | in the plan (§2.6 rule 9): each new stage brings a new action, form and look; the hanging-handle pull, on about twenty machines from S1 to O, is spent. Open for the owner: reworking those |
| 2026-09-30 | A working slingshot on the ground, like Angry Birds, that launches the player far up the structure, with large rubber bands that visibly stretch | done (`AS-011`, ported from `ChatGPT`'s AS-023): in the yard south of the tower; ENTER POUCH, the stick pulled back draws the bands (10 kN/m each, drawn up to 12 m), drag to aim, RELEASE; a 10.2 m draw throws the rider to 254 m, and the chute and stick bring them onto the 220 ring. Played through touch from the game's start (`touch_slingshot`). The landing is the chute's: no net |
| 2026-09-28 | Every APK from this branch is ScraperX-Claude, `com.cory.scraperx.claude`. Permanent | done; CI reads it back from every APK |
| 2026-09-28 | Stairs, ladders and easy paths are only the backup route; the route is the machines and the parkour climbs | in the plan (§2.6 rule 8); no backup path built yet |
| 2026-09-28 | Falls sound human: panicked yelling and real profanity, never filtered ("Motherfucker" ships) | done: 47 lines, every line of the script |
| 2026-09-28 | Take the `Gemini` branch's progress into this branch and carry on here | done: `Gemini` merged (to its `553cd7e`); its world replaces this branch's S2 swinging stair; the Claude identity, audio and fall voice kept. Grade to the +221 m ring is played through touch, pad and keyboard input; keyboard failed on `Gemini` and is fixed here |
| 2026-09-26 | No build is green unless the ascent is played through the game's own input from the game's start | done; CI builds the APK only after it |
| 2026-09-26 | A mechanism does not need five steps | in the plan (§2.2: one or two player actions a stage) |
| 2026-09-26 | The audio must not be "ting-ting-ping-ping" | done: the ringing cues re-voiced |
| 2026-09-25 | Almost every mechanism was incomplete and did not lift the player: start over from grade, no test pieces in the player's world, every machine works | done: rebuilt from grade (S1, C1 built here; S2, C2, S3, C3 and the well's link to +221 m from `Gemini`); movement test blocks only at test spawns; the old bands above 221 m kept for the owner's call. **Watch:** every lift below TP-640 stops on the kit's governor motor, not its own drive (plan §11, open for the owner); AS-010's stage M is the first without one |

---

## 1. What ScraperX is

A first-person physical ascent of a 1 600 m industrial tower. Every consequential fact — support
validity, traversal, machine state, structural coupling, process state, progression — is owned by
a native C++17 / Jolt simulation stepping at a fixed 90 Hz. Godot 4.7 owns presentation, input,
camera, HUD and Android delivery, and decides nothing.

The climb is won by operating real machines under finite limits. A route opens because a load
moved, not because a predicate flipped. Nothing teleports, nothing has unlimited force, nothing
bypasses collision. Most things can be used, moved and knocked about: it is a causal physics world
(the owner, 2026-10-19). The parachute is the player's way back from anywhere on the structure, so
the old "no mechanism may strand the player" is not a design rule here (plan §2.6 rule 5).

---

## 2. Current position

The one block to read before doing anything. Everything below is detail behind it.

| | |
|---|---|
| **Write branch** | `ScraperX-Claude` — the only one |
| **APK identity** | Every APK from this branch is **ScraperX-Claude**, application ID **`com.cory.scraperx.claude`**, set in `godot/export_presets.cfg` (`package/name`, `package/unique_name`) — permanent for this branch, never reverted. It installs beside the other AI branches' builds (`main`, `ChatGPT`, `ScraperX-Grok`: `com.cory.scraperx`; `Gemini`: `com.cory.scraperx.gemini`). `.github/scripts/check_apk_identity.sh` reads it back out of every exported APK in CI (checkpoint and playtest): the label in every locale, the package, every provider authority inside the package, no declared permission outside it, no shared user id |
| **Kernel** | `WO-000`–`WO-013`, all fourteen in source. **Closed set** |
| **Ascent frontier** | From the game's own start at grade, `touch_stack` climbs S1, C1, S2, C2, S3, C3, the well's A, B and C, AS-007's D, E and F, AS-008's G, H and I, AS-009's J, K and L, AS-010's service lift M, **C4, the service gantry climb**, **N, the granular discharge hoist**, **C5, the cooling plant climb**, and **O, the gravel wheel**, then over the 750 deck's east parapet onto **the crown gondola** and **the luffing derrick**, and stands on the route's top (803.5 m) on touch input events (796 s of play, no deaths, no slingshot); `pad_stack` and `keyboard_stack` climb the same to the 662 deck (touch is the device the game is played on; the owner, 2026-09-24: no keyboard or gamepad needed). Natively, `Mega-Ascent Grade to TP-340` and `ascent 154 to TP-640` run the machines to TP-640 on scripted inputs, and `AS-010 band` M, C4, N, C5 and O to the 750 deck. Each band above 220 m alone: `*_wet` from the 220 ring, `*_shop` from TP-340, `*_crane` from the 484 ring, `*_service` from TP-640, `touch_c4` from the 662 deck, `touch_n` from the 684 deck, `touch_c5` from the 706 deck, `touch_o` from the 728 deck. Above the 750 deck no route is built: only the neighbouring shaft's mass, 140 m north, stands on to 1,600 m. |
| **Integrated evidence** | **CI, `09af0c9`** (2026-10-15): green, [run 38808136099](https://github.com/IssisX/ScraperX/actions/runs/38808136099), every step, and the playtest APK (`ScraperX-APK-09af0c9…`, [artifact](https://github.com/IssisX/ScraperX/actions/runs/38808136099/artifacts/12009948468)). Since `fd3a88d` it adds the grappling hook's real rope: a line that bends round the steel it meets and comes off again, run on a 3 m/s, 1.2 kN winch (native `hook`; `touch_hook_rope`, new: off the 220 ring's edge the line catches over the lip and holds the body); the parkour pass (momentum through a mantle, the rising cat-leap catch, the aimed tic-tac, the sighted precision jump: native `parkour`, `touch_precision`); and the four largest source files split into 37 of 270-960 lines with output byte-identical (`d73a89d`, `48717ae`, `069a4a9`). Since `886b4ff` it adds the slingshot's launch shot (the push at full speed; slow time only once the rider leaves the bands, 0.3× for half a second, back to normal by 1.2 s, the eyes turned back to the bands and the yard then forward to the tower; `touch_slingshot`) and a ram-air canopy seen overhead on its lines when the chute is pulled. Since `167eb1a` it adds the basin's mountains carved down from their cones (crests, gullies, rock bands, snow only where it lies, no double haze); clouds that hold up on phones (no sin() hash, clear past each puff's edge, squared to the screen); smaller, softer oil stains; a jump pressed up to 0.1 s before landing kept for the landing (native `jump buffer`); and captions of the climber's voice and a VOICE volume (`touch_lethal_feedback`). Since `5016e4d` it adds the kit's ropes taking up their load in the step they come taut, with no tick of free fall (native `AS-010 O`: lowest rope tension 11,528 N; all 100 native tests); two welders at work in the Plate Shop, and workers who no longer step off edges (native `workers`, `touch_shop`); saves refused when the world's bodies stand in another order (native `save`); the yard's aged asphalt and a layered skyline; and containers and steel stock in the yard south of the start. Since `96e7b2c` it adds each band heard as itself (drips, far clanks, cable creak, a low boom above 640 m; `touch_wet`, `touch_shop`, `touch_crane`, `touch_c4`); Wet Isolation drawn wet; machine beacons moved off their riders' feet; and the wheel's skip-line grip. Since `829e029` it adds the wheel's hopper reloaded at the top by the rider's own work (native `wheel`, `touch_wheel`); wind off the west face pushing C7's climber (native `c7`); a cloud layer at 455–505 m and wind that grows with height (`touch_clouds`); the title's view sweeping up the tower into the clouds (`touch_title`); and a stumble when something moving strikes the player (native `workers`). Since `65614ae` it adds Phase 5 haptics scaled from the native's own events and loads (`touch_roll`, `touch_teeter`, `touch_pipe_gallery`, `touch_machines` fail without their pulse); slips and strain seen and heard, and REDUCED MOTION (`touch_roll_reduced`); and three workers on the 220 and 264 rings (native `workers`, `touch_workers`). Since `ae12505` it adds C7, the exposed climb from the 264 ring to the 291 m walkway without the cascade mast (native `c7`, `touch_c7`); the pitman's call line reeved 2:1, with both calls tested (native `pitman`); and the basin's mountains carved and hazed, with collision unchanged. Since `7f88465` it adds the stone wheel's stone falling under gravity with dust and a pour sound (`touch_wheel` fails if a pour is not heard) and the hook's gun in the right fist (`touch_hook`). Since `d4a4fca` it adds the crew heard and faced (footsteps, tool strikes, eyes and brows), three workers on decks 2 and 4 (native `workers`, `touch_workers`), and the pitman lift hauled by hand on a counterweighted 2:1 line with a dead-man brake (native `pitman`, `touch_machines`). Since `dac2846` it adds life: pigeons that flush from the player and gulls at height (`touch_birds`), the yard crew of six real-bodied workers who step aside, flinch and stumble (native `workers`, `touch_workers`), and beacons on moving machines with steam from the vent stacks (`touch_works`). Since `abbcbc4` it adds the speed vault (`touch_speed_vault`), the wall-run (`touch_wall_run`) and the grappling hook, found on the 220 ring, with the right fist closed on its line (`touch_hook`). That tree also carries the physical ledge hang with a finite grip; brachiation and the pipe gallery in the yard (`touch_monkey_bars`, `touch_pipe_gallery`); whole arms overhead with the camera near plane at 0.05 m; the slide (`touch_slide`); the draw-call batching (measured locally, not in CI: `perf_top` and `perf_grade` worst 398 and 607 draw calls at LOW, from 9,716 and 8,701), as well as the one-run climb from grade to 803.5 m on touch (`touch_stack`, no slingshot), save and continue (`touch_save`), the title (`touch_title`), the licences page, phase 4's landing recovery, roll and edge teeter (`touch_roll`, `touch_teeter`), the summit latch (`touch_high`), the yard's trapeze, caught in the air and pumped (`touch_trapeze`), and the slab incline, traction tram and luffing derrick worked with the body (`touch_incline`, `touch_tram`, `touch_high`). Not re-run on this tree: `audio_mix` and `audio_fall` (Movie Maker), valgrind, and the six side-by-side suites (all on `b8cd98b`, 2026-09-29). No Fold device run or sustained performance is established. |
| **World collision** | Every static body the presentation draws is native collision: `godot/presentation/solid_export.gd` classifies each built part (solid, moving, non-solid, native mirror; unclassified fails) and generates `src/sim/world_solids.inc`, which the native world compiles in (1 808 bodies; drawn mirrors of owned bodies skipped by bounds). Native tests walk that same solid world; CI regenerates the table and fails on drift. The solver's contact order is keyed on entities, not body ids, so a body added in one band leaves every simulation elsewhere unchanged (`03_EXECUTION/ENGINEERING/BODY_ORDER_STABILITY.md`, native `body order`). Stairs are walked: a native step-up climbs any edge up to `0.35 m` (vault/mantle take over above it), and every stack flight rises through a stairwell cut in the deck it serves. The SKIN walkway at +32 m is 4 m deep, not 2: under the upper flight a standing player had one 0.4 m lane (reported as an opening too small to fit through); the lane is now 2.4 m (`AS-002` deviation). The plant's access landing no longer overhangs its second step (it left a 1.0 m slot). A crawl beam by the dock, underside 1.45 m, is the crouch fixture The yard sits in a closed alpine basin: a valley floor 0.3 m below grade out to a solid rim of 44 steep ridges (~1.1 km), so every direction ends against rock, not a void or an invisible wall |
| **Sky and light** | `godot/presentation/sky_cycle.gd` + `sky.gdshader`: a moving sun (24 real minutes per day, noon elevation 58 deg) that becomes the moon at night on the one shadowed light, sky dome, drifting lit cloud deck, stars, dusk glow; ambient from the sky, fog and exposure follow the sun. Shadows: 4096 atlas on every platform (Android's default was 2048), PCF soft filter 3 (2 on mobile), four blended cascades over 220 m. Presentation only; native has no time of day |
| **Audio** | `godot/presentation/audio/`: a bank synthesised at startup from seeded noise and filters (~0.35 s on a worker thread), voiced for a phone speaker (identity above 300 Hz, low thump only for weight), and a director that plays what the native reports: footsteps on the head-bob stride (concrete / steel / meadow; softer crouched), jump, landings scaled by impact, ledge grab, vault/mantle scrape, crouch rustle, canopy, lethal impact; altitude wind, a distant yard bed and a falling-air rush; kit machines heard from their own motion (a groan while they move, a creak at a hinge, a clunk where they stop hard); birds near the ground by day. Struck metal (steel steps, grabs, clangs, UI ticks) is noise through broad, fast-damped resonances: the first bank rang pure sine partials for 0.12–0.84 s (peak 58–60 dB over its neighbourhood), which the owner heard as "ting-ting-ping-ping"; now ~10 dB, the same as a concrete step. **Fear voice (GDD 8.4 / Governing Law 8):** 47 spoken lines in `audio/voice/` (Ogg, shipped as-is; the profanity of real terror included, never filtered), tiers yelp → panic → terror by the fall's speed and drop, cut dead by a lethal impact, pain on a hard landing lived through, relief when the canopy opens or a ledge is caught; no line repeats until its tier is spent. Generated offline by `src/voice/generate_fall_voice.py` (Chatterbox TTS, MIT; voice from Piper en_US-john, LibriVox public domain), each take transcribed by Whisper and rejected unless it says its line; a line no take passes fails the generator rather than being left out. Master ends in a hard limiter (+6 dB in, -0.8 dB ceiling). `audio_mix` and `audio_fall` measure the mix under Movie Maker (headless runs' Dummy driver never mixes): ambience -22.9, footsteps -14.8 dBFS above 300 Hz, peak -0.8; in a lethal fall the voice speaks at -14.5 dBFS over the ambience bed, which ducks 8 dB under a spoken line, at -27.9 (`audio_fall`, lines seeded so every run hears the same). Measured, **never listened to** — no output device here |
| **Interface** | GDD §22 control surface in `godot/presentation/ui/`: touch (floating stick, drag-look, Jump, contextual Action, Drop/Chute on state, pendant controls only while operating), touch only (the gamepad and keyboard paths are dormant: not tested, not worked on); sparse HUD (reticle cues, prompts, fall gauge against the native lethal speed, altimeter, station panel), pause with SETTINGS, GRAPHICS (quality preset LOW-ULTRA, render scale, shadows OFF-ULTRA, MSAA, bloom, frame-rate cap, FPS readout) and DISPLAY (FOV, brightness, head bob, speed-FOV kick, time of day, day length) pages; first-person arms (`godot/presentation/first_person_arms.gd`) whose hands grip, plant and reach at the ledge points the native reports; a standing mantle steps in to the hang standoff before it climbs (swept, supported, skipped on moving ground) so the palms land on the lip; opt-in gyro aim (off by default; screen-axis mapping taken from Godot 4.7-stable's Android sensor code, not yet observed on a device); double-tap Jump vaults when the takeoff could have (native window 0.30 s, the ground vault's own probes); PICK UP / SET DOWN on the contextual Action for the native carry (Drop and Back also set down), both hands drawn on the load; crouch (native 1.2 m capsule, feet fixed; stands only where the full 1.8 m capsule clears, so a low gap keeps you down; Jump and Action stand first) on a touch CROUCH/STAND toggle, C or right-stick click, or held Ctrl, with the eye gliding 1.52 to 0.95 m; a step up (up to 0.36 m in one native tick) glides into the view the same way, never shown in one frame. 47 headless `--uitest` scenarios drive real input events into native state changes (including full-route `touch_stack` from grade to the 750 deck, AS-007 alone from the 220 ring (`*_wet`), AS-008 alone from TP-340 (`*_shop`), AS-009 alone from the 484 ring (`*_crane`), AS-010's service lift from TP-640 (`*_service`), its C4 climb from the 662 deck (`touch_c4`: the beam, DROP into a hang, the shimmy), its stage N from the 684 deck (`touch_n`: GRAB the flap chain), its C5 climb from the 706 deck (`touch_c5`: the vault, the crawl, a sprint latched by the stick pushed past its ring), its stage O from the 728 deck (`touch_o`: GRAB the chock's lanyard and step back with it), Stage B `boom`, Deck 4 checkpoint continuation, `stack_upper`, every START option loading (`touch_pause`), and the fall voice); every one blocks the APK, and so do the Movie Maker `audio_mix` and `audio_fall`. Handling: a taken handle is drawn in to the hands at 3 m/s and a ledge caught at arm's length pulls the body in at 3 m/s, never in one tick; a heavy free load slows its carrier to within 80% of the grip (D's 50 kg spool: 1.76 m/s) and is held from spinning in the hands |
| **Next code job** | AS-010's next stage from the 750 deck, derived as M and N were (the next stage only, a machine's numbers INTEGRATED by the evaluator, played through touch): after M, C4, N, C5 and O, a climb again, as the owner laid the route out (2026-09-25: *"a couple of mechanisms can be stacked, and then a climb, and then another mechanism"*). The 750 deck is railed all round and round the well and the cab's hole but where the cab lands, on the east side of the hole; the next stage opens the way off it. The ground slingshot is built (`AS-011`). Open for the owner: the governors on every lift below TP-640 (plan §11), AS-007's fluid drives (float, air, hydraulic ram) under `MACRO-TRAVERSAL-STRICT`, and the backup path below TP-640 (rule 8), not built. |
| **Next authoring job** | `AS-007` (220→340 m) remains a separate planned band; new height does not substitute for the missing device proof of the current route. |
| **Authoring contract** | `03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md`; for everything above the stair, `03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md` |
| **Unproven** | Fold-device install, on-device execution, touch ergonomics, sustained frame rate. **No device access exists** |

---

## 3. Authorities

Read in this order. Higher wins on conflict.

| # | Document | Owns |
|---|---|---|
| 1 | `01_PRODUCT_AUTHORITY/00_GOVERNING_LAWS.md` | what the game may never do |
| 2 | `01_PRODUCT_AUTHORITY/01_SCRAPERX_GDD.md` | what the game is |
| 3 | `01_PRODUCT_AUTHORITY/02_ASCENT_ATLAS.md` | the tower in metres, modules and braids |
| 4 | `02_ENGINEERING_AUTHORITY/00_EXECUTION_PROTOCOL.md` | how work is executed and claimed |
| 5 | `02_ENGINEERING_AUTHORITY/01_TECHNICAL_ARCHITECTURE_TDD.md` | how the engine is built |
| 6 | repository source / tests / CI evidence | what is actually true |
| 7 | the one open ticket | the current job |

The atlas is the GDD written in metres. It is not a sidecar spec.
`01_PRODUCT_AUTHORITY/support/` is provenance for how the GDD was closed — never a second product.

`Mechanism-Ideas-and-archetypes.md` (repository root) is the owner's catalogue of 20 lift
archetypes: templates the ascent is built from, to be mutated and recombined freely. It is input
to the mechanism ascent plan, not an authority, and it is not edited.

---

## 4. Vocabulary — one concept, one name, one home

Kernel engineering and ascent construction are different classes of work and never share an
identifier.

| Class | ID form | Home | What it is |
|---|---|---|---|
| **Kernel Work Order** | `WO-000`–`WO-013` | `03_EXECUTION/KERNEL/` | foundational engineering. Proves one *tool type* on the `KX-*` substrate at source x ≈ 200. **Closed set — no new `WO-*` is ever created** |
| **Ascent Slice** | `AS-001`–`AS-015` | `03_EXECUTION/ASCENT/` | one causal slice of the 1.6 km climb. The live work |
| **Atlas band** | `B00`–`B11` | Atlas §6 | a floor range of the tower. **Never a filename, never a ticket** |
| **Atlas chain** | `K0`–`K8` | Atlas §7 | a causal chain spanning bands |
| **Campaign module** | `MOD-*` | Atlas §6 | a world object in the real tower |
| **Kernel substrate** | `KX-*` | Atlas §9 | a kernel fixture. Regression substrate; never retitled into a `MOD-*` |
| **Capability** | `CAP-*` | Atlas §4 | something the player holds or has acquired |
| **Transfer plate** | `TP-###` | Atlas §5 | a stable handoff support at a named elevation. `TP-120` is the plate at 120 m |
| **Commit** | — | `commit_checkpoint()` in source | the in-sim automatic checkpoint. A *gameplay* concept. There is no `CP-*` document class and none may be introduced |
| **Process document** | named, never numbered | `02_ENGINEERING_AUTHORITY/`, `03_EXECUTION/PLANNING/` | the protocol and the authoring contract |
| **Template** | named, never numbered | `03_EXECUTION/TEMPLATES/` | the form a ticket takes |

### Where things live

```
00_START_HERE.md              this file — position, vocabulary, ledgers
01_PRODUCT_AUTHORITY/         what the game is        (laws, GDD, atlas, + support/ provenance)
02_ENGINEERING_AUTHORITY/     how work is executed    (protocol, TDD, evidence snapshot)
03_EXECUTION/
    KERNEL/                   WO-000 .. WO-013        closed, historical, regression substrate
    ASCENT/                   AS-001 .. AS-015        the live queue
    PLANNING/                 the ascent authoring contract and the mechanism ascent plan
    TEMPLATES/                ticket forms
src/ tests/ godot/            implementation truth
```

A directory holds exactly one kind of artifact. If something does not fit one of these, it is
probably not supposed to exist.

---

## 5. Lifecycle, provenance and evidence are three different things

This is the distinction the whole control plane rests on. Collapsing them is how a stale plan
gets coded, or a plan gets reported as a working game.

| Field | Answers | Values | Changes when |
|---|---|---|---|
| **Lifecycle** | does source exist? | `IMPLEMENTED` · `PLANNED` · `UNAUTHORED` | someone writes code |
| **Provenance** | were its numbers derived against *this* tree? | re-derived here · ⚠ imported, NOT re-derived | someone re-authors it |
| **Evidence** | what has actually been observed? | a named CI run and a named proof line | **every CI run** |

Every ticket header carries lifecycle, provenance and an implementation gate. **No ticket carries
its own evidence** — it points at the ledgers in §6 and §7 below. Evidence changes every run while
lifecycle changes only when code is written, so evidence copied into a ticket goes stale silently.
It lives in exactly one place.

`IMPLEMENTED` is a claim about source and nothing else. Proof is a separate, named thing.

⚠ **imported, NOT re-derived** is a hard gate. Such a ticket's geometry, entity ids and persist
versions describe another branch's world. Re-author it against this tree before coding any of it.

---

## 6. Kernel ledger

`03_EXECUTION/KERNEL/`. All fourteen are in source. The kernel exists to be regression substrate,
not to grow.

| Ticket | Slice | Lifecycle | Evidence |
|---|---|---|---|
| `WO-000` | Godot 4.7 → GDExtension → fixed-step native sim → arm64 APK | IMPLEMENTED | the CI build + export steps |
| `WO-001` | native player on static `KX-DECK` | IMPLEMENTED | **record gap** — see below |
| `WO-002` | translating / rotating support-point velocity | IMPLEMENTED | falsifier |
| `WO-003` | vault / mantle / ledge / hang on real geometry | IMPLEMENTED | falsifier |
| `WO-004` | Fold-aspect stage, industrial identity | IMPLEMENTED | `SCRAPERX_WO004_VIEWPORT_PROOF` |
| `WO-005` | exterior grade and approach | IMPLEMENTED | `SCRAPERX_WO005_APPROACH_PROOF` |
| `WO-006` | vessel → orifice → cylinder → lift | IMPLEMENTED | falsifier + `SCRAPERX_WO006_MACHINE_PROOF` |
| `WO-007` | Kellerworks identity, alpine backdrop | IMPLEMENTED | screenshot only |
| `WO-008` | fall, chute, commit, survived lower landing | IMPLEMENTED | falsifier |
| `WO-009` | kernel chain + persist / reload | IMPLEMENTED | falsifier |
| `WO-010` | player mass drives the treadle | IMPLEMENTED | falsifier |
| `WO-011` | `KX-JIB` moves `KX-CRATE` | IMPLEMENTED | falsifier |
| `WO-012` | seated `KX-NEEDLE` changes traversal | IMPLEMENTED | falsifier |
| `WO-013` | `KX-SUMP` changes `KX-GRATE` | IMPLEMENTED | falsifier |

The fourteen kernel files predate the §5 header and keep their original `**Status:**` line. Their
Existing-truth and Result-record sections quote evidence and CI names **as they stood when the work
was executed** — including the retired "ScraperX-2 checkpoint CI". Those are historical execution
records, deliberately not rewritten. Retconning a proof record is worse than an out-of-date name.

> **Record gap, `WO-001`.** Its Result record reads `pending` on every line, yet the player body,
> its capsule and its support identity are in source and are exercised by every falsifier that
> follows. The code is real; the paperwork was never filled. Recorded here rather than silently
> promoted. Closing it is bookkeeping, not engineering.

---

## 7. Ascent ledger

`03_EXECUTION/ASCENT/`. Fifteen slices, each first bound to one Atlas band. All new work happens here.
`AS-011` and `AS-012` now name the slingshot and the swing; the Atlas bands they first held (B07
wind-frame structure / SKIN, B08 isolate high riser / drum clutch) were never authored.

| Ticket | Slice | Band | Lifecycle | Provenance | Evidence |
|---|---|---|---|---|---|
| `AS-001` | apron → +24 m, intake rise | B00 | **IMPLEMENTED** | re-derived here | falsifier, green in run `35727055407` |
| `AS-002` | first legal stand at +40 m | B00 | **IMPLEMENTED** | re-derived here | falsifier, green in run `35798864772` |
| `AS-003` | `CAP-HOOK5` acquire | B00 | **IMPLEMENTED** | re-derived here | falsifiers, green in run `36010028676` (`AS-003 apron`, `cage`, `Hook5 Rack`; `touch_carry`) |
| `AS-004` | seat needles at 96 m | B01 | PLANNED | re-derived here; revalidated; the stair stays, so not the next job | none |
| `AS-005` | cage land at 120 m, or east climb | B01 exit | PLANNED | ⚠ imported | none |
| `AS-006` | counterweight well, 154 → 220 m: skip lift, derrick boom, debris chute | B02 | IN PROGRESS | re-derived here under the mechanism ascent plan; Stages A, B and C built (rigging kit with dogs, slip hooks and the declared rubble model; UNHOOK / HOOK / GRAB / LET GO on touch, gamepad and keyboard) and ridden in one run to the 220 ring; the climbing route waits on Step 2 | `PASS scraperx_sim AS-006 A no link`, `A ride`, `B no link`, `B ride`, `C no link`, `C ride`, `band`; uitests `touch_rig`, `pad_rig`, `keyboard_rig`, `touch_debris`, `pad_debris`, `keyboard_debris` (local; CI pending) |
| `AS-007` | wet isolation, 220 → 340 m: float ram, chiller drop, accumulator ram | B03 | **BUILT** | re-derived here under the mechanism ascent plan (replaces the imported plan) | native band, route and wreckage tests; played through the game's input: `touch_wet`, `pad_wet`, `keyboard_wet` and the stack runs |
| `AS-008` | plate shop, 340 → 484 m: scaffold slump, girder tip, domino monolith | B04 | **BUILT** | derived here under the mechanism ascent plan | native band, route and wreckage tests; played through the game's input: `touch_shop`, `pad_shop`, `keyboard_shop` and the stack runs |
| `AS-009` | facade crane stack, 484 → 640 m: traveler and runaway wagon, jib pendulum, kinetic winch | B05 | **BUILT** | derived here under the mechanism ascent plan | native band, route and wreckage tests; played through the game's input: `touch_crane`, `pad_crane`, `keyboard_crane` and the stack runs |
| `AS-010` | midstack service, 640 → 780 m: stage M, the service lift on a cable reel (no governor), TP-640 to the 662 deck; C4, the service gantry climb, the 662 deck to the 684 deck; stage N, the granular discharge hoist (no governor, no catch), the 684 deck to the 706 deck; C5, the cooling plant climb, the 706 deck to the 728 deck; stage O, the gravel wheel (no governor, no catch), the 728 deck to the 750 deck | B06 | **IN PROGRESS** | derived here under the mechanism ascent plan | native `AS-010 M`, `C4`, `N`, `C5`, `O`, `band`, `route`; played through the game's input: `touch_service`, `pad_service`, `keyboard_service`, `touch_c4`, `touch_n`, `touch_c5`, `touch_o` and the stack runs |
| `AS-011` | the ground slingshot: the yard to the 220 ring | yard | **BUILT** | ported from `ChatGPT`'s AS-023 | native `slingshot`; `touch_slingshot`, green in run `37616086844` |
| `AS-012` | the swing: the 220 ring to the 242 ring (then C6, a climb to the 264 ring) | 220 → 264 m | **BUILT** | derived here | native `swing`, `C6`; `touch_swing`, green in run `37616086844`; `touch_c6` |
| `AS-013` | jack + seat crown beam | B09 | UNAUTHORED | — | none |
| `AS-014` | isolate + lock fans; bell as support | B10 | UNAUTHORED | — | none |
| `AS-015` | stand on 800.00 m (was 1600.00 m) | B11 | UNAUTHORED | — | none |

`AS-005` still quotes `ScraperX-Grok` constants — a handoff at `(10.40, 24.00, 38.40)`,
68 treads at `x = 10.40`, entity id `319`, a well at `(-1.76, —, 25.30)`. None of that exists here.

---

## 8. What is next

> **Next job: Step 2's movement**, then `AS-006`'s climbing route.
> `03_EXECUTION/PLANNING/MECHANISM_ASCENT_PLAN.md`, locked with the owner on 2026-09-24. The
> climb above the stair's 154 m top deck is built from linked lift stages that the player
> completes and rides. Step 1 is built: the rigging kit (ropes with a carryable loose end,
> anchors, brake-only governed guides, catches with trip lines, a 900 N grip) and Stage A, a
> counter-mass skip lift off the 154 m deck that the player rigs (UNHOOK, HOOK, GRAB) and rides
> 22 m. Its falsifiers: no link, no lift; the payload never gains more energy than the skip
> released; the rider rides the cage as a moving support; a death restores the stage as
> committed.
>
> Stages B and C are built too: the derrick boom (a cleat-found line hooked onto B's cage, a slip
> hook that lets the line run at the top, the boom left hanging as a lattice) and the debris chute
> (the rebar thrown, 900 kg of rubble into the dumpster, the keeper latch freed), whose spent
> dumpster empties into A's parked cage and re-arms A. One native run rides all three from the
> 154 m deck to the 220 ring, then comes down by parachute, tips A's rubble out and rides A again.
> Next: Step 2's moves for the band's climbing route, then `AS-007`.
> `AS-004` stays `PLANNED`: the stair stays (owner's direction), so its needles are no longer the
> way to 96 m.

Do not start from the 1.6 km crown. Do not open a second ticket while one is open.

---

## 9. Context-loading rule

Do **not** load every file into every task. Load:

- Governing Laws;
- Execution Protocol;
- the one open ticket;
- the relevant source / tests / config;
- only the GDD / atlas / TDD sections that ticket cites.

Atlas loading by class:

- `WO-000`: no atlas.
- `WO-001`–`WO-004`: Atlas §§2, 9 (datum + kernel deck).
- `WO-005`–`WO-013`: Atlas §§4, 7, 8, 9, 12.
- `AS-*`: Atlas §6 for **its own band only**, plus §§4, 5, 7, 12, 13 as the slice cites them.
  Loading bands the slice does not own is how scope creeps.

Use `01_PRODUCT_AUTHORITY/support/` only to trace a closed product decision.

---

## 10. Claim discipline

Never collapse

`implemented → built → APK produced → installed → executed → observed → verified on Fold`

into one word such as "done". Evidence must match the claim, and the claim must name its evidence.

A plan is not an implementation. `PLANNED` is not `IMPLEMENTED`, and `IMPLEMENTED` is not proven.
A green aggregate is not a green falsifier — name the line.

---

## 11. Retired and excluded

**Retired identifiers.** `ASC-*` (campaign prefix, lived one day, now `AS-*`); `03_WORK_ORDERS/`
(one directory holding four kinds of artifact); `WO-014`–`WO-028` as campaign tickets. The full
old-name → current-name map is `03_EXECUTION/PLANNING/ASCENT_PRE_RESOLUTION.md` §5.1. Do not
reintroduce any of them; resolve them through that table.

**Sibling-branch identifiers.** Other branches may assign the same number to different
content, or use different numbers for similar work. Compare actual source, geometry and
authority before reusing anything; a matching `AS-*` label is not proof of equivalence.

**Not implementation authority, and not to be reintroduced:** questionnaire executables or JSON
state; rejected drafts; GraveSpire history; old branches and builds; discarded mathematics; any
second "content package" outside this tree.

`MANIFEST.txt` and `SHA256SUMS.txt` are a **frozen snapshot of the v1.1 delivery package**. Their
paths predate this layout and are not maintained. They are kept as a delivery record, not as an
index of the tree.
