# Gap list: what the owner asked for that the game does not do yet

Built from the recovered owner calls (`00_START_HERE_ARCHIVE.md`, "Owner calls recovered from the
archived log and handoff"), the rules in force (`00_START_HERE.md`) and the original static audit at `2ac7fa1` (2026-10-29). Corrections inspect `dea9cfcba0923242fc6046d88aada9726a1b97e1` and the
`tests/sim_tests_stock.cpp` delta on trunk `65896fbd11ed2048bbd0a102b252008a8cc3db98`; no game, device, visual or audio run was made
for this revision. Source and test definitions establish implementation or coverage, not a pass.
Older line numbers can drift; search the cited symbol. Issue #3 and open PRs own current assignments.

**Evidence boundary:** entries 0–21 distinguish inspected source and test definitions from
historical findings and unverified behavior. Named searches do not rule out code under other
names, and static inspection does not prove a gameplay or device result. Check current
assignments and evidence before starting work.

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
- `src/sim/simulation.hpp` defines 20 named handle entity IDs: five well, four wet,
  three plate-shop, four crane, three service and one stack. These are IDs, not 20 active
  route pulls: `src/sim/band_wet_isolation.cpp` passes `kWetDValveHandleEntityId` to
  `add_retired`, while D drain, F and header have other handles. The upper-band groups have
  14 IDs including that retired one. Requirement and encounter frequency are unverified.
- `src/sim/band_wet_isolation.cpp` `build_climbing_route` also places route ladders at
  242.25, 308.25 and the 340 m plate top. `src/sim/vertical/vertical_route.cpp` places
  cascade mast, barrel helix and crown gondola and routes Action through machine commands;
  this source evidence alone does not prove every machine is mandatory or played successfully.
  The earlier lower-route ladder count remains historical. Rule 52 governs proposed changes.

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
- A rip shot is implemented in `matron_hold_camera.gd` (`rip_active`, `_note_rip`); its current runtime and visual quality are unverified here. [PR #59](https://github.com/IssisX/ScraperX/pull/59) adds grab, tear and fatal-hang haptics; its focused Godot event-routing and script-load checks passed. Vibration on a physical device remains unverified.
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
- `LooseStock::lay_matron_throwables` in `src/sim/loose_stock.cpp` adds two toolboxes and
  a drum at deck 154; `src/sim/physics_world.cpp` supplies the last three pieces to
  `Matron::set_throwables` (`LooseStock::kMatronThrowables`). Other stock exists, but is not
  automatically in that particular list. `Matron::walk_toward` in `src/sim/matron.cpp` can
  push a blocking dynamic stock/fabricated body and checks nearby voids when selecting a
  direction; this does not establish deliberate shoving of player-placed stock off an edge.
  `PhysicsWorld::step` handles `take_unrig` by unhooking the selected A/B rope.
- `matron.hpp` documents clinging to a moving floor and says she does not send a cage away.
  General jamming/resetting, boarding a called lift, deliberate edge shoves and learning a
  repeated trick remain unverified beyond these observed paths. Test presence or code comments
  are not gameplay proof. Check current assignments before taking a follow-up.

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
- `godot/presentation/main.gd` creates the sky cycle, applies `SettingsStore.day_minutes`
  when time-of-day is automatic, or fixes the selected hour; `sky_cycle.gd` advances the hour
  from `delta`, not climb height. `godot/presentation/ui/settings_store.gd` defaults to 24
  minutes and permits 8–60; `pause_menu.gd` exposes both settings. Stack lamps in
  `main_world.gd` are placed on odd numbered stack levels, not every level.
- A source search for helmet, headlamp, flashlight, city glow and lit-ring identifiers in
  `src` and `godot` found no named player helmet lamp or climb-following light. This does not
  rule out an implementation under another name or establish whether night is readable. The
  requested helmet lamp, illuminated rings behind the climber and city glow remain open until
  production-path and rendered review establishes them.

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

### 13. The story's premise and its crew (B1, B2)
- Asked: a worker sneaks onto his own site on a weekend and badges in, wakes a coworker
  exposed to a lost X-ray source, and can escape only upward; she kills the overtime crew and
  their bodies are found higher up.
- **Partial implementation:** `src/sim/summit.cpp` `Summit::build_ending` creates the last beam,
  and `godot/presentation/life/foreman_radio.gd` `LINES` references Saturday and the accident.
  A search of `src` and `godot` code and scene/JSON files for X-ray, badge-in and overtime
  found the beam and related caption but no named X-ray-source or badge-in sequence. An
  implementation under different terminology and its runtime presentation are unverified.
- `src/sim/crew.cpp` `Crew::Crew` constructs six yard workers and eight on tower decks from
  22 to 418.25 m; `src/sim/crew.hpp` `Crew::State` contains walking, working, yielding,
  startled and stumbling, with no death state. `PhysicsWorld` steps the crew; searches of the
  Matron files found no `Crew` reference. This supports a missing explicit worker-death path,
  but does not prove all possible indirect interactions or rendered bodies absent. The request
  for her to kill the overtime crew and leave bodies remains open.

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
  against "fed only by sound"; the former croak claim is obsolete (gap 2); no test directly names `kRageChangedDown` in `tests` or `godot` (this search does not prove
  absence of indirect coverage); the requested radio hint remains to be checked (gap 12).
- Hurts: 2. Size: small each.

### 18. A wrecked machine leaves a way on (rule 51)
- Asked: wreckage must leave a physical way onward.
- **Partial test coverage; results not run here:** `tests/sim_tests_well.cpp` defines B and C
  wreck paths, `tests/sim_tests_plate.cpp` `run_plate_wreckage` covers G/H/I, and
  `tests/sim_tests_crane.cpp` `run_crane_wreckage` covers J/K/L. A search for wreckage and backup-path terms across `tests` found these named checks but
  did not identify equivalent checks for S1–S3, D–F, M–O or vertical machines; differently
  named tests and actual routes remain unverified. `03_EXECUTION/handoffs/PARKED_FEATURES_2026-10-29.md`
  item 9 historically calls the backup path open; that handoff is not proof of its current state. Run the applicable existing
  scenarios at the candidate revision; test existence alone is not a pass.

### 19. Her fingers, cloth and pipe warning (A6)
- Asked: a believable body, fingers and cloth, plus the pipe warning.
- **Partial implementation; visual result unverified:** `godot/presentation/life/matron_view.gd`
  builds finger pivots and updates their curls; `matron_rig.gd` maps a skirt bone. The earlier
  audit found common state-driven curls and a canned skirt sway, with no proven ankle/rung wrap;
  inspect the rendered production path before deciding what needs work.
- **Warning coverage:** `tests/sim_tests_matron.cpp` contains the four-second warning assertion;
  the touch check is `touch_matron_pipe` in `godot/presentation/ui/ui_test_act.gd`. The inspected
  `.github/workflows/wo000-delivery-spine.yml` does not list that scenario. This is coverage
  configuration, not proof that it fails everywhere. Body proportions also require visual review.

### 20. Smaller owner calls
- **D4 diegetic controls — partial:** `src/sim/forklift.hpp` describes Action taking controls;
  `src/sim/yard_crane.hpp` describes the remote and buttons. The requested grab of a physical
  forklift lever remains a historical missing finding pending a full control-path check.
- **C2 movement incentives — partial:** `godot/presentation/main_feel.gd` `_announce_summit`
  shows elapsed time and deaths. A search of `src`, `godot` and `tests` for `time_trial`, `best_time`, “time trial”
  and “ghost” found no named feature (a test uses “ghost mass”); unnamed implementations remain
  possible. `03_EXECUTION/handoffs/OWNER_DECISIONS_PENDING.md` item 10 treats a best time
  or ghost as an owner decision; do not implement on the search result alone.
- **D3 plate clamp — no named implementation found:** a search of `src`, `godot`, and `tests`
  for “plate clamp” and `plate_clamp` returned no match; an implementation under another name
  is not ruled out. `src/sim/yard_crane.hpp` contains magnet and spreader. Verify owner intent
  and operation before proposing a new machine part.
- **A4 frail first then Turn — partial:** `Matron::find_climb` and `follow_found_climb` in
  `src/sim/matron_found_climb.cpp` restrict a Pest outside the Turn while later stages can
  attempt the corresponding descent/journey. This is stage-based behavior, not proof of the
  full frail-to-strong presentation; visual and gameplay fit remain unverified.

### 21. Background and process gaps
- **Rule 39, identity and radio — partial:** `godot/presentation/ui/pause_menu.gd`
  `_build_work_id_page` creates `WorkIdPage`; `godot/presentation/main.gd` instantiates
  `StoryRadioPlayer`; `godot/presentation/audio/story_radio_player.gd`
  `VOICE_RECORDINGS_APPROVED` is false. Cue triggering and approved recordings need separate
  verification. The timing of this existing hookup versus rule 39 remains for owner review.
- **Rule 57, chart — partial:** `godot/presentation/uncanny/` has the chart and
  `voice_keeper.gd` requests microphone permission. `godot/export_presets.cfg` enables `permissions/record_audio`; no INTERNET or CAMERA entry
  appeared in the inspected preset. `godot/presentation/main.gd` instantiates `Uncanny`; `uncanny.gd` creates
  `HerChart`, uses native observations and records voice/echo events. A `chart|uncanny`
  search of `src/sim/matron*.cpp` and `*.hpp` returned no matches; indirect wiring or
  speech-to-text under other names is not ruled out. The workflow lists five related
  scenarios: `touch_her_chart`, `touch_her_chart_fast`, `touch_mic_consent`,
  `touch_voice_echo` and `touch_room_hearing`; their results require a run. Obtain in-game consent as asked.
- **Rules 56 and 27, ascent — unresolved:** `src/sim/vertical/vertical_machine.cpp` registers
  `sx.gravity_balance.v1` and `sx.rack_climber.v1`; registration is not placement or a playable
  route. The old audit says AS-007–AS-010 are used above 340 m despite rule 56 calling them
  non-canonical, and that no scenario plays the canonical route from the start. Check actual
  route and revision-specific runs; do not silently change the rule.
- **Rules 30 and 31, Matron travel — partial:** `src/sim/matron_pipe.cpp` uses `kStackPitch`
  and `src/sim/matron.cpp` retains level data; `tests/sim_tests_matron_pursuit.cpp` defines
  pursuit cases. Coverage above 176.25 m and below 132 m is unverified. Place-specific data
  alone does not prove that the general-skill requirement fails.
- **Housekeeping:** `godot/presentation/life/matron_look.gd` `dress` frees a mesh named
  `music_box`; `matron_view.gd` still creates and rotates a `box_cord` pivot. Whether the
  current GLB contains the mesh requires asset inspection; the old catwalk comment finding
  is historical. Do not infer visual or gameplay impact from these code paths alone.

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

## Historical corrections to the recovered list and other documents

These earlier findings require rechecking against the current revision before use as facts or assignments.

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
