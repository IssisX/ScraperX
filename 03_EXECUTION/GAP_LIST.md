# Gap list: what the owner asked for that the game does not do yet

Built from the recovered owner calls (`00_START_HERE_ARCHIVE.md`, "Owner calls recovered from the
archived log and handoff"), the rules in force (`00_START_HERE.md`) and the original static audit at `2ac7fa1` (2026-10-29). Corrections below inspect trunk
`dea9cfcba0923242fc6046d88aada9726a1b97e1`; no game, device, visual or audio run was made
for this revision. Source and test definitions establish implementation or coverage, not a pass.
Older line numbers can drift; search the cited symbol. Issue #3 and open PRs own current assignments.

**Audit limit:** entries 0–21 have targeted
current-source corrections below. Some retained detailed findings and suggested next work are still the earlier static audit,
not fully reconciled at `dea9cfc`; those are marked historical or unverified below. Do not use
them alone to assign work.

**Ranking** is by how much the gap hurts the player's belief in the game, worst first
(impact 1 = invisible, 5 = breaks the game), ties broken by how many players meet it. **Size** is
small (hours), medium (a day), large (several days), as work for one agent including its test.
A change to a test's checks, world geometry or her force limits is a protected change (PR only).
The ordering is historical, not an assignment or current priority; check the issue board and open PRs.

## 0. Coverage hole

**Partial coverage; runtime status requires a revision-specific run.** The scenario loop in
`.github/workflows/wo000-delivery-spine.yml` runs `touch_matron_act` and other Matron scenarios,
but does not list `touch_matron_pipe`. It runs `matron_figure_test.gd` and separately captures
`touch_summit_ending`; it does not list `gore_shots_test.gd`, `matron_presentation_test.gd` or
`script_load_test.gd`. Check the workflow itself for the full current list.

Historical: the earlier pipe staging failed at drum pickup; it is not evidence that the scenario
has never passed anywhere. [PR #29](https://github.com/IssisX/ScraperX/pull/29) proposes a fix,
but is unmerged. Do not infer visual approval from a headless run or screenshots merely existing.

## Ranked gaps

### 1. Pulled starts and ladders still carry the route above 221 m (rules 52, 53)
- Asked: no new route ladder or pull-started lift; each stage a new physical action, with a few
  ladders and easy backup paths allowed under rule 52.
- **Partial implementation; policy fit and gameplay impact unverified:**
  `src/sim/band_plate_shop.cpp` `build_climbing_route` builds repeated board-and-ladder
  segments from 340.25 m to 484 m. `src/sim/band_facade_crane.cpp` `build_climbing_route`
  continues the pattern from 484 m and adds a ladder to TP-640. `add_handle` in that file
  constructs lanyard handles. `src/sim/vertical/vertical_route.cpp` handles Action commands
  for machine deck/entry/exit roles, except its separately handled machines.
- The earlier audit counted 20 handle/lanyard entities, 14 above 221 m, and named wet D/F/header,
  plate G/H/I, crane J/K/L and midstack M/N/O. It also counted three Action-started motor lifts
  and lower-route ladders. These counts and whether each is required on the live route are
  **historical, not reverified**. Do not use them as current coverage or claim that every player
  meets them. Existing machines are not automatically authorized for replacement; rule 52 and
  the current assignment board govern any proposed work.

### 2. Rejected mouth cues and generated human sounds (rule 41)
- Asked: no cluck or croak. **Implemented in source, runtime and listening unverified:**
  `godot/presentation/audio/matron_sound.gd` no longer calls either family, and
  `sound_bank.gd` no longer registers them. `audio_director.gd` still lists the old names in
  `HER_VOICE_CLIPS`; a name in that list alone does not prove a sound plays.
- **Remaining:** `sound_bank.gd` still generates `matron_wake` and the climber's breath in
  `_matron_wake` and `_breath`. The sniff bank is recorded; see
  `godot/presentation/audio/matron/CREDITS.md`. Check the actual sound and the owner's ear
  approval before calling the sound work complete.

### 3. Pulling him off a ladder (rule 35)
- Asked: in reach she pulls him off by the ankle. **Implemented; runtime unverified for this
  inspection:** `src/sim/physics_world.cpp` calls `Matron::caught` while climbing;
  `src/sim/physics_world_grab.cpp` applies the ladder pull, and `Matron::step_hold` in
  `src/sim/matron_hold.cpp` drives it. `tests/sim_tests_matron_ladder_pull.cpp` defines finite
  grip, force budget and release checks. The former claim that climbing is excluded is obsolete.
- Run the relevant native and actual touch path at the revision being evaluated; inspect the
  open assignments before taking any follow-up.

### 4. Perception and hiding (rule 29)
- Asked: perception is evidence with an age; rage is fed by sound reaching her ear.
- **Implemented in part; overall behavior unverified here.** The old five-leak list is not a
  current finding: `Matron::pre_step` in `src/sim/matron.cpp` now derives `d` from `ev.fix`
  or the heard lead, feeds crumbs from `ev.seen_at` / `ev.heard_at`, uses `ev.seen_holding`,
  and passes the resulting distance and heard loudness to `hush_step` and `choose_pace` in
  `src/sim/matron_moves.cpp`.
- The original audit also identified true player position in journey yielding, player-on-deck
  and braced state, and the true ankle position passed during a hold. `Matron::step_travel` in
  `src/sim/matron_journey.cpp` still has `player_on` checks; inspect
  their actual inputs and purpose before classifying them as a perception leak. Contact/hold
  state (`src/sim/matron_hold.cpp`) likewise requires a separate causal check. Do not claim
  that hiding works, or fails, across the game without a relevant runtime test. Rage deviations
  remain in gap 17.

### 5. Her voice lines (rule 43)
- Asked: all lines in `03_EXECUTION/MATRON_REDESIGN/HER_LINES_DRAFT.md` with varied delivery,
  including “Waste not.” The text exists; the old claim that no line text exists is wrong.
- **Incomplete on trunk:** `Matron::fatal_next` in `src/sim/matron_fatality.cpp` increments
  `waste_not_`; the spoken Matron-line system and approved takes remain to be established.
  `godot/presentation/audio/voice_lines.gd` is the climber's voice, not proof of hers.
- Check [issue #3](https://github.com/IssisX/ScraperX/issues/3) and open PRs before starting;
  unmerged voice work is not trunk. Approval by ear is separate from source and test coverage.

### 6. Seat the crew's last beam (C4)
- Asked: physically seat the last beam, hear the foreman's admission, return by parachute.
- **Implemented on trunk; runtime not reverified in this documentation pass:**
  `src/sim/summit.cpp` `Summit::build_ending`, `work` and `read_ending` build the beam,
  require contact and settling, free the sling and track the canopy return. The foreman's
  `last_beam` caption is cued by `godot/presentation/life/foreman_radio.gd` `_watch`, called by `update`.
  `godot/presentation/main_feel.gd` reads the ending state.
- `tests/sim_tests_summit.cpp` and the workflow's `touch_summit_ending` and rendered capture
  define proof paths; their existence does not prove they pass at this revision. See the
  [historical handoff](handoffs/SUMMIT_ENDING.md) for candidate-era evidence.

### 7. Her death and checkpoint restore
- **Implemented in source; runtime unverified here:** `Matron::restore` in `src/sim/matron.cpp`
  clears `dead_` and restores the saved state; `tests/sim_tests_matron.cpp` `run_matron_restore` defines checks for the
  restore. The earlier claim that death is always permanent across checkpoint restore is obsolete.
- The broader “comes back worse” design remains a separate owner decision under rule 64;
  restoring a checkpoint is not proof of that design.

### 8. The hold is a camera, not yet a cinematic (rule 33)
- Asked: third person from grab to release or death; PUNCH works; *"a whole cinematic, dramatic
  experience"* (10-29).
- Status: **partly built.** Third person, orbit, slow motion at the let-go (0.5 s at 0.3x), a
  pull-out, a fade, a 6x sim while black (`godot/presentation/life/matron_hold_camera.gd:26,56-67,
  135,222,269-390`; applied at `main.gd:447-453`); gore always on (`gore_view.gd:9`); her sounds cued
  per beat (`matron_sound.gd:360-440`). PUNCH works only before the rip and in the hang window
  (`sim_tests_strike.cpp:204-236` requires it refused after the rip).
- A rip shot is implemented in `matron_hold_camera.gd` (`rip_active`, `_note_rip`); its current runtime and visual quality are unverified here. [PR #59](https://github.com/IssisX/ScraperX/pull/59) has unmerged haptics, implementation complete with Godot verification pending.
- Missing: a shot per beat (`beat_cuts` is a counter, `:98,412-418`; `fov_offset()` and
  `vignette()` return 0.0, `:121-127`), haptics for the grab, the rip and the hang
  (`main_feel.gd` covers only the pipe knocks and a "her" pulse), a hold-specific mix, her voice
  (gap 5). No test asserts third person from grab to release; the camera is only logged
  (`ui_test_act.gd:278-329,750`).
- Hurts: 3. Size: large and visual; use the available production-path visual tooling, and report any tool or environment blocker. The missing
  assertion is small.

### 9. She does not wreck his work (A3, A4 B and C)
- Asked: carry and throw stock, shove placed stock off edges, reset or jam machines, send a lift
  away or board one he called, wait at a choke point, and learn repeated tricks within a run;
  meddling must be undoable.
- **Partial implementation; runtime unverified here:** `src/sim/matron.cpp` `notice_moved_stock`,
  `fetch_stock`, `take_stock` and `throw_stock` implement the stock path; fetching is limited to
  Pest and `matron.hpp` `kTakeMaxKg` is 25 kg. The same file's `State::Unrigging` and
  `State::Guarding` cover undoing rigging and guarding an exit. `fooled_` tracks noises leading
  to nobody; that is not evidence of learning a repeated player action.
- The earlier audit identified missing edge shoves, jamming/resetting, sending a cage away and
  boarding a player-called lift; `matron.hpp` documents clinging to a floor already moving and
  says she does not send a cage away. Those broader absence findings and the exact eligible-stock
  count remain **unverified** without a complete interaction-path search. Check current open PRs
  and the assignment board before taking a follow-up.

### 10. Loud machines mostly do not draw her (A7)
- Asked: collisions heard by material and by her, and loud machines draw her.
- **Partial; runtime unverified here:** `Simulation::PhysicsWorld::step` in
  `src/sim/physics_world.cpp` sends energy-scaled `ImpactLedger` events and yard-crane drive
  noises to `Matron::hear`. Its constructor tracks loose stock, forklift frames and carriages,
  and crane hook, magnet and spreader. `src/sim/physics_world_movement.cpp` includes forklift
  driving in player loudness; `Matron::carry_m` in `src/sim/matron.cpp` accounts for the path.
- The earlier audit identified cages, counterweights, plates, doors, lifts and wall hits as
  missing sources. The cited tracker does not register those machine bodies directly, but the
  complete absence of indirect sound paths and gameplay effects is **unverified**. The separate
  [PR #48](https://github.com/IssisX/ScraperX/pull/48) proposes Stage A hearing and is unmerged;
  it is not trunk. A general measured-motion/force sound path remains a possible follow-up.

### 11. Night lighting and the climb (B5)
- Asked: a readable morning-to-night climb with floodlights, a helmet lamp, rings lit behind the
  climber, moon and city glow.
- **Partial; visual quality unverified:** `godot/presentation/sky_cycle.gd` implements the
  sun/moon cycle, sky and night lamp gain; its default `day_minutes` is 24. The light rig in
  `godot/presentation/main_world.gd` builds site lights, including stack-level lamps.
- The earlier audit found no helmet lamp, lit rings behind the climber or city glow, and said
  the cycle did not follow climbing. Those absence and design conclusions are **unverified**
  at this revision; inspect their production wiring and the rendered night before assigning a
  change. The default alone does not prove what settings a player actually uses.

### 12. Foreman radio and story cues (B3)
- Asked: the foreman on the radio: furious, then the lifeline, then admitting he covered up her
  accident; her stage hints (*"She's quiet. Keep it that way."*) (A1).
- Status: **partly built.** `godot/presentation/life/foreman_radio.gd:20-26` has place/event captions, no voice, cued by place or event once per launch (`:69-84`); wired at `main.gd:185-188`.
  The earlier four-line/220.5 m ceiling is historical. The summit admission is now the `last_beam` caption in `foreman_radio.gd`. The recorded
  set is instantiated by `main.gd` as `StoryRadioPlayer`, and its
  `VOICE_RECORDINGS_APPROVED` gate is false in `story_radio_player.gd`. Instantiation does
  not establish that gameplay triggers these cues; inspect the active call path and voice
  approvals. The cast and recording choices still need reconciliation with the owner's story. `.github/workflows/wo000-delivery-spine.yml` invokes `ci/verify_story_radio_package.py`
  on the APK and imports `verify_recordings` for a source check; actual results require a run.
- Hurts: 3. It is the only story channel. Size: medium for lines, triggers and wiring; the voices
  wait on his picks by ear. Which cast he means is his call (listed in the owner decisions).

### 13. The story's premise and its crew are missing (B1, B2)
- Asked: a worker sneaks onto his own site on a weekend (badging in), wakes a coworker exposed by
  a lost X-ray source, the only way off is up; she kills the overtime crew, bodies higher up.
- Status: **B1 partial:** the gate, badge-in and X-ray source remain unverified. The topping-out beam now exists:
  `src/sim/summit.cpp` `Summit::build_ending` and `read_ending` (gap 6). **B2 historical finding; current status requires review**:
  workers cannot die (`src/sim/crew.hpp:37`), `matron.cpp` never touches `Crew`, about ten live
  workers stand from 22 to 418 m (`crew.cpp:61-79`), corpses are the climber's alone
  (`corpse.hpp:1-15`).
- Hurts: 3 each. Rule 50 wants every feature to have its reason in the story.
- Size: B1 large (a gate scene, a source prop, the crew story, and the cast
  conflict settled first). B2 medium (a dead-worker state or placed props, plus triggers).

### 14. Lived-in spot finder stops at 396 m (D1)
- Asked: little lived-in spots throughout the structure.
- **Partial implementation:** `LooseStock::dress_tower` in `src/sim/loose_stock.cpp` builds
  candidate rings at 11–143 m and 176.25–396.25 m; it does not generate higher rings. This
  does not establish that every other authored object above 396 m is absent.
- **Test definition on current trunk `65896fbd11ed2048bbd0a102b252008a8cc3db98`:**
  `tests/sim_tests_stock.cpp` `run_loose_stock` now checks every `dress_tower` ring for at
  least one piece and checks that tower pieces map to a ring. The old statement that the test
  checks only the total is obsolete. This test does not cover dressing above 396.25 m; no run
  at that revision was performed for this documentation change. `src/sim/physics_world.cpp`
  constructs upper ziplines through the same 176.25 + 22k ring range.
- Extending stock can change world body order. Apply the conditional whole-world test rule in
  `CLAUDE.md`, and check current assignments before starting.

### 15. A hard blow and escape (rule 34)
- Asked: a hard enough blow takes her balance or puts her down for seconds, alive.
- **Partially implemented; runtime unverified here:** `Matron::take_blow` in
  `src/sim/matron.cpp` evaluates impulses, stance, strike height and stage before staggering or
  knockdown. Its `his_blow` path handles a player's tagged blow during a hold. The player strike
  in `src/sim/physics_world_strike.cpp` uses three contact ticks, capped by the force and power
  in `src/sim/player_strike_limits.hpp`; `tests/sim_tests_matron.cpp` and `touch_matron_heave`
  define stock/machine checks.
- The prior audit estimated a punch at about 50 N·s and concluded it cannot stagger any stage.
  That arithmetic is **not a runtime proof** and does not by itself cover stance, repeated
  contacts or hold escape. The availability of useful stock along the whole route also remains
  unverified. Any threshold or force-limit change is protected under rule 4.

### 16. Movement continuity (rule 37)
- Asked: continuous drawn motion, no snapped poses, body joints at most 20 rad/s and hands
  reaching each hold.
- **Implemented in part; complete runtime coverage unverified:**
  `godot/presentation/life/matron_rig.gd` `govern` limits selected bone rotation to
  `GOVERN_RAD_S` 11.5 or `GOVERN_BURST_RAD_S` 19, excluding hair, eyes and fingers;
  `matron_view.gd` `_update_figure` drives the figure. `godot/tests/matron_figure_test.gd`
  exercises poses, and `matron_perception_climb_test.gd` checks a wrist anchor. Test definitions
  do not establish all drawn transitions, hand contacts or visual smoothness. A production-path
  time series of actual bone speeds and contact positions, plus visual review, would address
  the remaining proof gap; inspect existing tests before adding a new protected check.

### 17. Her mood hints, the stage-up cue and a missing test (A1 leftovers)
- Status: **core built.** `step_rage` `matron.cpp:1863-1896`, constants `matron.hpp:581-594`, no
  floor, no time-based gain; the look reads exposure (`matron_view.gd:2162`, `matron_look.gd:229-246`);
  stage drives pose and sound (`matron_view.gd:768-771,1701,1872`, `audio_director.gd:1358-1453`);
  no meter anywhere (`hud.gd`). Test `sim_tests_matron.cpp:2037-2106`. **Deviations**: pain also
  feeds her rage (`kStaggerRage`, `kKnockdownRage`, `matron.hpp:694-695`; `matron.cpp:4108,4140`),
  against "fed only by sound"; the former croak claim is obsolete (gap 2); no test covers Changed back
  to Dangerous (`grep kRageChangedDown tests godot` is empty); no radio hint (gap 12).
- Hurts: 2. Size: small each.

### 18. A wrecked machine leaves a way on (rule 51)
- Asked: wreckage must leave a physical way onward.
- **Partial test coverage; results not run here:** `tests/sim_tests_well.cpp` defines B and C
  wreck paths, `tests/sim_tests_plate.cpp` `run_plate_wreckage` covers G/H/I, and
  `tests/sim_tests_crane.cpp` `run_crane_wreckage` covers J/K/L. The earlier audit found no
  corresponding S1–S3, D–F, M–O or vertical-machine proof; that absence and each actual route
  remain **unverified**. The historical plan calls for a 0–221 m backup path; its current
  implementation needs a separate source and gameplay check. Run the applicable existing
  scenarios at the candidate revision; test existence alone is not a pass.

### 19. Her body: fingers, cloth and the 4 s warning are thin (A6)
- **Current source check:** `godot/presentation/life/matron_view.gd` creates finger pivots
  and updates curl; `godot/presentation/life/matron_rig.gd` maps the skirt bone. Contact and
  actual appearance require rendered inspection. `touch_matron_pipe` is not listed in the
  inspected CI scenario loop; no assertion about all possible warning tests follows from that.
- Fingers: four fingers of three joints and a thumb of two, each on a spring (`matron_view.gd:676-692,2012-2032`), but
  the curl is one value per hand per state (`:2000-2009`) and stays 0.35 in every hold state, so
  by the code the fingers do not wrap the ankle (UNVERIFIED by eye). Size small for variation,
  medium to wrap an ankle or rung.
- Cloth: one skirt bone on a canned sway (`:2041-2046`, `matron_rig.gd:194`), no cloth motion.
  Size medium.
- The 4 s warning: asserted in the sim only (`sim_tests_matron.cpp:723,740` `warned_s >= 4.0`);
  the touch-level check is in `touch_matron_pipe`, which the inspected workflow does not list (gap 0). Size small.
- Body proportions ("not too thin") are checked by no test (`matron_stats.json` is read by no code).
  Size small, by eye with the runtime.
- Hurts: 2.

### 20. Smaller owner calls not met
- **Current source check:** `godot/presentation/main_feel.gd` `_announce_summit` reports
  time and deaths. The remaining absence claims below are historical and require a fresh
  repository-wide implementation search before use as an assignment.
- **D4 diegetic machine controls** (grab the forklift's real lever): not built; the forklift is
  "Action takes the controls" (`forklift.hpp:10-14`), the crane a remote and buttons
  (`yard_crane.hpp:14-19`); only the hanging handles are physical grabs. Hurts 2. Size medium for
  the forklift, large for every machine.
- **C2 movement as a reason to play**: no time trial, ghost or best time (the summit toast reports elapsed time and deaths (`main_feel.gd` `_announce_summit`)). Hurts 2. Size medium; the ideas were the agent's.
- **D3 plate clamp**: not built (zero hits in `src`, `godot`, `tests`, `03_EXECUTION`); the magnet
  and the spreader are. Hurts 1. Size medium, on the magnet's pattern.
- **A4 (A)** frail first then the Turn: built, but a dangerous or changed Matron is no longer
  frail (`matron_found_climb.cpp:127,324,365`).

### 21. Background and process gaps (low for the player)
- **Current source check:** `src/sim/vertical/vertical_machine.cpp` registers the two
  vertical machine IDs and `src/sim/matron_pipe.cpp` uses `kStackPitch`. Registration
  is not proof of placement or playability. Other broad absence/coverage claims below remain
  UNVERIFIED until their production wiring and actual runs are checked.
- **Rule 39**: `godot/presentation/ui/pause_menu.gd` `_build_work_id_page` instantiates
  `WorkIdPage`, and `godot/presentation/main.gd` instantiates `StoryRadioPlayer`.
  `story_radio_player.gd` keeps voice approval false. **Partial:** gameplay cue triggering,
  approved recordings and consistency with the owner's story remain to be verified; call sites
  alone are not playback proof. The timing of this existing hookup against rule 39 is for review.
- **Rule 57**: the chart is built (`godot/presentation/uncanny/`, five CI scenarios) but not
  linked to the Matron sim (`grep chart src/sim/matron*` is empty), has no speech to text, and the
  export asks for no INTERNET or CAMERA permission (`export_presets.cfg:30-31` has vibrate and
  record_audio). Hurts 2. Size large (server models, rule 58).
- **Rule 56 and 27**: the climb above 340 m is the non-canonical AS-007 to AS-010
  (`touch_stack` climbs them to 750 m); `sx.gravity_balance` and `sx.rack_climber` are defined
  (`vertical_machine.cpp:112,114`) but unplaced and untested; no scenario plays the canonical route
  from the start. Hurts 1 to 2. Size large (about 400 m of ported machines) and medium for one
  chained scenario that would be slow in CI.
- **Rules 30, 31**: per-place constants remain (`matron.hpp:437-438` post, `:514-533` ladders and
  riser, `:720-726` pipe; the hand-drawn levels at `matron.cpp:75-141`; `matron_pipe.cpp` is a
  per-route climb of 730 lines). Deck-to-deck up and down has test coverage among 132, 143, 154 and 176.25
  (`sim_tests_matron_pursuit.cpp:125,213,297`); the islands above 176.25 and below 132 are
  UNVERIFIED. `kStackPitch` 11 m is baked into the pipe logic. Hurts 1 to 2. Size large.
- **Housekeeping**: the music box mesh is still in `matron.glb` (freed at load,
  `matron_look.gd:177`; a dead `box_cord` pivot remains); `matron.hpp:13` still says the catwalk is
  "the one thing that kills her". Size small.

## Earlier source and test inventory (not a current verification verdict)

| Call | Evidence | Test |
|---|---|---|
| A5 locker wakes her, hunts by sound and sight, the Turn, movable crates | `matron.cpp:2491-2503,2526-2629,1831-1856,1711-1757` | `sim_tests_matron.cpp:1011-1060,1231-1300,1422-1520`, hearing, sight and stalk slices |
| A6 hair, eyes, drum knocks her off, a hard blow downs her, throw with a faint arc | `matron_rig.gd:27-41,705`; `matron.hpp:687-689`; `throw_arc_view.gd` | `matron_figure_test` (CI), `sim_tests_matron.cpp:749,450,481`, `touch_matron_heave` |
| Rule 32 fatality and arm tears, full intensity, no toggle, short | `gore_view.gd:9`; 5.0-7.0 s hang, 3 s floor | `sim_tests_matron_grab.cpp:1237,1271,1355`, `touch_matron_act` |
| Rule 35 30 s hands-off after a tear or his blow (a timer, not a retreat) | `matron.hpp:849`, `matron_hold.cpp:1026,1187,1391-1409,1469-1477` | `sim_tests_matron_grab.cpp:661-748` |
| Rule 42 no sound twice running in a hold | `her_takes.gd:27-43` | `her_takes_test` (CI), `ui_test_act.gd:816-846` |
| Rule 44 machines make sounds, only people have voices | `sound_bank.gd:1101-1104,335-344` | none needed |
| C1 parkour set; the hook at 220.25 m (27% up) | `PARKOUR_MOVE_SET.md:50-67`; `physics_world_probes.cpp:815-841` | native `parkour`, `wall run`, `slide`, `brachiation`, `hook`; `touch_vault`, `touch_hook` |
| C3 zipline: rolling trolley, hand brake, sprung stop, death | `zipline.hpp:1-40`, `zipline.cpp:360-370` | native `zipline`; `touch_zipline_slam` |
| C5 deaths with consequences | `corpse.hpp:1-15`, `physics_world_movement.cpp:549,728-747`, `hud.gd:195` | native `corpse`, `arm_break`; `touch_lethal_feedback` |
| C6 thud footsteps | `sound_bank.gd:184-198`, `gait.gd:1-22` | cadence only (`footstep_contact_test`); timbre by ear |
| C7 launch shot (full-speed push, slow time at the let-go; "360" UNVERIFIED) | `launch_shot.gd:1-53` | native `slingshot`, `touch_slingshot` |
| C8 gestures; some buttons stay | `touch_controls.gd:1-20,116` | `touch_gyro_aim`, every `touch_*` scenario |
| D2 crane and forklifts up the tower, stock on pallets | `yard_crane.hpp`, `forklift.hpp:60-62` | `sim_tests_yard_crane.cpp:420-460`, `sim_tests_stock.cpp:89,103` |
| D3 magnet and spreader | `yard_crane.hpp:125-166` | `magnet`, `spreader`; `touch_crane_magnet`, `touch_crane_spreader` |
| Rule 51 a causal world; rule 55 no test pieces | `loose_stock.hpp:65-92`; `simulation.hpp:117-124` | native `stock`, `touch_stock` |
| Rule 36 her look (static only) | `matron_look.gd:7-9,229-246` | only a contrast check; the face and thinness are by eye |

## Corrections to the recovered list and to other documents

- The older B3 dead-code finding is historical: `main.gd` now instantiates `StoryRadioPlayer`;
  its approval gate remains false. See gap 12 for the remaining trigger and story questions.
- The recovered B4 row says built: the crane, forklifts and hook exist and are tested, but no band
  file references the crane or a forklift and no route machine needs the hook
  (`has_hook_` appears only in `physics_world_probes.cpp` and `physics_world_state.cpp`); its only
  job is the twin-tower bridge. That settles C1's tension in favor of "toy".
- The recovered A4 row says "tricks learned within a run" built: the only memory is `fooled_`
  (noises that led to nobody) and a trap counter capped at 3; nothing records a repeated player
  action.
- `PARKED_FEATURES_2026-10-29.md` is stale in three places: the stalk is built (`matron_stalk.cpp`,
  slice `matron-stalk`), so "all of it open" for dread is wrong; death exists (gap 7), so "nothing
  in code" for her death is wrong; her jump-catch, hang-drop, gap-jump and mantle moves exist
  (`matron_traversal.hpp:136-146`, `sim_tests_matron_climb.cpp:372-698`), so "ledge jump-hang-mantle
  open" is wrong. Its "haptics in about 8 scripts" is 3 production scripts. Its header ("build
  nothing unasked") also conflicts with rule 43, which is a rule in force.
- Rule 56 and the archive row call AS-007 to AS-010 "not canonical", yet `touch_stack` climbs them
  and no canonical machines stand between 340 and 750 m (gap 21).
- `matron/CREDITS.md` and `PARKED_FEATURES` both say her chart is "largely built"; it is implemented to
  its steps 1 and 3 and her hearing the room (`HER_CHART.md:100-115`).

## Waiting on him (rule 64): status only, not to be built unasked

- Dread before she is seen: **partly built.** The stalk (`matron_stalk.cpp:1-8`, slice
  `matron-stalk`), dread audio at first sight within 40 m (`matron_sound.gd:165-167`), locker
  knocks (`matron_view.gd:503,515`), a haptic pulse for pipe knocks within 14 m
  (`main_feel.gd:1359-1368`) and a "HER n M" marker (`main.gd:503-510`). No haptic, light,
  edge-of-view or HUD signal before she is seen.
- Her limbs torn off: **not built.** Her sim body is one capsule (`matron.cpp:78`); her limbs are
  drawn only; no harpoon, laser welder or nail gun exists. Size large.
- Her death and checkpoint restore are implemented in source (gap 7); the broader return design remains parked. No kill-shot weapon was found in the earlier audit.
- More decks: **partly built**, see gap 21.
- Tower two: the broken bridge is built (`twin_tower.*`, native `twin`, `touch_twin`); the property
  damage climb was not checked.
- The chiller drop and glass tube: the chiller drop is built (`chiller_drop_spec.md`); the glass
  tube is a parked design.
- Lift governors: not checked.
