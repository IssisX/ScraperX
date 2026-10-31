# Gap list: what the owner asked for that the game does not do yet

Built from the recovered owner calls (`00_START_HERE_ARCHIVE.md`, "Owner calls recovered from the
archived log and handoff"), the rules in force (`00_START_HERE.md`) and the original static audit at `2ac7fa1` (2026-10-29). Corrections below inspect trunk
`dea9cfcba0923242fc6046d88aada9726a1b97e1`; no game, device, visual or audio run was made
for this revision. Source and test definitions establish implementation or coverage, not a pass.
Older line numbers can drift; search the cited symbol. Issue #3 and open PRs own current assignments.

**Audit limit:** entries 0, 2–8, 12–13, 17 and the identity/radio part of 21 have targeted
current-source corrections below. The retained findings in 1, 9–11, 14–16, 18–20 and the
remaining parts of 21 are the earlier static audit, not fully reconciled at `dea9cfc`. Treat
their status, cited line numbers and proposed next work as UNVERIFIED until checked against
current source and workflow; do not use them alone to assign work.

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
- Asked: *"3D chutes and ladders"* is what he does not want; no new route ladder or pull-started
  lift; each stage a new action (rules 52, 53).
- Status: **not met** above 221 m, partly below.
- Evidence: 20 hanging-handle or lanyard entities (`src/sim/simulation.hpp:1312-1452`), 14 above
  221 m (wet D, F, header; plate G, H, I; crane J, K, L; midstack M, N, O). Three motor lifts
  started by Action (`src/sim/vertical/vertical_route.cpp:407-418`: cascade mast, barrel helix,
  crown gondola). A repeating "L of boards plus ladder" climb from 352 to 616 m
  (`band_plate_shop.cpp:476-483`, `band_facade_crane.cpp:525-533`), ladders at 242, 308 and 340
  (`band_wet_isolation.cpp:742-760`), at least six ladders on the 0-221 m climb
  (`band_stack.cpp:550,802,1155,1267,1619,1639`; `band_counterweight_well.cpp:1007`).
  C6, C7, the swing and the slingshot add none.
- Hurts: 4. His most repeated complaint, and every player climbing 340-640 m meets it.
- Size: large. A redesign per machine of about a day each; each new machine is talked through with
  him first (rule 52).

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
- Asked: carries and throws stock (built); shoves stock he placed off edges, resets or jams
  machines he set, more at higher stages; meddles only undoably (sends a lift away, waits at a
  choke point, boards the lift he called); learns a trick he repeats within a run.
- Status: **partly built.** Take and throw: `matron.cpp:3401-3527`, pest only (`:3426`), three
  throwables in the whole world (`loose_stock.hpp:123`, `loose_stock.cpp:579-585`: two toolboxes
  and a drum she cannot lift, `kTakeMaxKg` 25). Unrig (`matron.cpp:3177-3203`) and guarding the
  way out's mouth (`:3206-3225`) are built. **Not built**: shoving his placed stock off an edge
  (the only crate push, `:1711-1757`, never pushes over an edge), jamming or resetting machines,
  sending a lift away (a stated choice: she never sends a cage, `matron.hpp:85-86`), boarding the
  lift he called (she only clings to a floor already moving, `matron.cpp:915-960`), and any
  learning beyond `fooled_` (`:2613,2625,3133`).
- Hurts: 3. The "she wrecks your work" threat is nearly absent.
- Size: large. A lawful version needs her to perceive what he placed (gap 4), a general
  capped-force push, and machine use as one general skill, no per-machine code (rule 30).

### 10. Loud machines mostly do not draw her (A7)
- Asked: every real collision heard by material and by her; loud machines draw her too.
- Status: **partly built.** Carried path and per-deck muffling: `carry_m` (`matron.cpp:545-568`);
  hearing is not level-bound (`:2537`); tests `sim_tests_matron_hearing.cpp:52-196`.
  She hears only the impact ledger's hits by energy (`physics_world.cpp:685-691`), only loose
  stock, forklifts, crane devices and fabricated objects are tracked (`:388-399`), and the
  machines that draw her are the yard crane's drives (`:692-696`) and the forklift as the driver's
  noise (`physics_world_movement.cpp:1241-1252`). Cages, counterweights, plates, doors, the lifts
  and the player's own wall hits do not reach her. The shaft is a hard-coded box (`matron.hpp:237`).
- Hurts: 3. Running a lift or a winch is silent to her.
- Size: medium: a general machine-noise source from measured motion or force into `Matron::hear`.

### 11. The night above 143 m is the moon and machine lights (B5)
- Asked: morning to night; the night readable: floodlights, a helmet lamp, rings lit behind you,
  moon and city glow.
- Status: **partly built.** Sun, moon and stars with lamps boosted 6x at night
  (`sky_cycle.gd:17-35,143`); yard masts and four tower-foot floods (`main_world.gd:640-679`);
  stack lamps only on levels 1 to 13, up to 143 m (`:484-510`). **Missing**: a helmet lamp (no
  match for helmet, headlamp or torch), lit rings behind the climber, a city glow. The cycle loops
  on 24 real minutes and does not follow the climb (`settings_store.gd:86-87`).
- Hurts: 3. Size: medium; a helmet `SpotLight3D` is small, the rest needs tuning by eye.

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

### 14. Nothing is lived-in above 396 m (D1)
- Asked: little lived-in spots all over the structure.
- Status: **partly built.** The spot finder walks rings every 11 m to 143 m, then 176.25 + 22k for
  k = 0..10, so to 396.25 m (`loose_stock.cpp:734-741`); nothing from 396 to 802.64 m, about half
  the climb. Ziplines also stop at 396 m (`physics_world.cpp:340-345`). `sim_tests_stock.cpp:74`
  asserts at least 60 pieces, not coverage by height.
- Hurts: 3. Size: medium, and it changes body order, which makes it a whole-world change
  (see the conditional full-suite rule in `CLAUDE.md`, "Test only what a change reaches").

### 15. "He is never helpless" depends on stock being at hand (rule 34)
- Status: **partly built.** A knock-down works with thrown stock and machines
  (`take_blow` `matron.cpp:4040-4143`; `sim_tests_matron.cpp:377,450,481,591`; `touch_matron_heave`).
  By arithmetic, not run: the punch is at most 1,500 N (`player_strike_limits.hpp:14`) over 3
  contact ticks (`physics_world_strike.cpp:30`), about 50 N·s on 220 kg, 0.23 m/s, under the
  pest's 0.245 m/s absorb (`matron.hpp:660-661`), so a punch alone does not stagger her at any
  stage. The only seeded throwables are the two toolboxes on deck 154 (gap 9). At the dangerous
  and fully changed stages a chest hit only staggers her; only a low hit takes her feet.
- Hurts: 3. Size: small to retune; protected (her force limits and thresholds), by PR.

### 16. Nothing tests that her movement is continuous (rule 37)
- Status: **built in code, unproven.** Drawn between ticks (`matron_view.gd:2100,2314`;
  `simulation.cpp:691`); body bones clamped to 11.5 rad/s, 19 in a burst
  (`matron_rig.gd:254-259,520-560`); hair, eyes and fingers excluded. The only test is one wrist
  anchor within 0.03 m (`matron_perception_climb_test.gd`).
- Hurts: 3 (jerky motion was a complaint on 10-27, and a regression would not be caught).
- Size: small: a headless test through `_update_figure` recording the peak bone speed
  (`matron_figure_test.gd` has the harness).

### 17. Her mood hints, the stage-up cue and a missing test (A1 leftovers)
- Status: **core built.** `step_rage` `matron.cpp:1863-1896`, constants `matron.hpp:581-594`, no
  floor, no time-based gain; the look reads exposure (`matron_view.gd:2162`, `matron_look.gd:229-246`);
  stage drives pose and sound (`matron_view.gd:768-771,1701,1872`, `audio_director.gd:1358-1453`);
  no meter anywhere (`hud.gd`). Test `sim_tests_matron.cpp:2037-2106`. **Deviations**: pain also
  feeds her rage (`kStaggerRage`, `kKnockdownRage`, `matron.hpp:694-695`; `matron.cpp:4108,4140`),
  against "fed only by sound"; the former croak claim is obsolete (gap 2); no test covers Changed back
  to Dangerous (`grep kRageChangedDown tests godot` is empty); no radio hint (gap 12).
- Hurts: 2. Size: small each.

### 18. A wrecked machine leaves a way on: tested for some bands only (rule 51)
- Status: **partly built.** Wreckage tests: AS-006 B and C (`sim_tests_well.cpp:373,772`), AS-008 G,
  H, I (`sim_tests_plate.cpp:518,545,562`), AS-009 J, K, L (`sim_tests_crane.cpp:345,361,375`).
  None found for S1 to S3, AS-007 D to F, AS-010 M to O or the vertical machines (UNVERIFIED
  whether they leave a way on). The plan's backup path for 0-221 m is not built
  (`PARKED_FEATURES` item 9).
- Hurts: 2. Size: medium to prove or fix each.

### 19. Her body: fingers, cloth and the 4 s warning are thin (A6)
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
