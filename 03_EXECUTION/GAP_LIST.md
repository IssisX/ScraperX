# Gap list: what the owner asked for that the game does not do yet

Built from the recovered owner calls (`00_START_HERE_ARCHIVE.md`, "Owner calls recovered from the
archived log and handoff"), the rules in force (`00_START_HERE.md`) and the code on trunk at
`2ac7fa1` (2026-10-29). Static reading only: nothing was run, and nothing visual or by ear was
judged (those say UNVERIFIED). Every file:line was read; line numbers move, so grep the name.

**Ranking** is by how much the gap hurts the player's belief in the game, worst first
(impact 1 = invisible, 5 = breaks the game), ties broken by how many players meet it. **Size** is
small (hours), medium (a day), large (several days), as work for one agent including its test.
A change to a test's checks, world geometry or her force limits is a protected change (PR only).
Gaps 0 to 5 are the ones to hand out first.

## 0. Coverage hole (put first by the integrator)

**`touch_matron_pipe` is not run by CI, and the other Matron checks that look at her are not
either.** The touch step's scenario loop (`.github/workflows/wo000-delivery-spine.yml:378-392`)
has `touch_matron`, `_ride`, `_line`, `_climb`, `_act`, `_heave` and `touch_room_hearing`, not
`touch_matron_pipe` (her pipe descent and its 4 s warning). The Godot headless tests CI runs are
`her_takes_test`, `matron_figure_test` and `matron_perception_climb_test`; `gore_shots_test.gd`,
`matron_presentation_test.gd` and `script_load_test.gd` are not in the workflow. The loop passes no
`--capture`, so CI takes no still of her body or her hold. The scenario also fails in the cloud
container even on trunk (`handoffs/OPERATIONS_NOTES.md`), so it has never been proved anywhere.
- Hurts: 3. Her corner-pipe descent is a feature the owner asked for (A6) and nothing watches it.
- Size: small to add it to the loop and the headless tests to the workflow; medium if it also
  fails in CI (it dies at the drum in the container, cause not found).

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

### 2. Her cluck and croak are still in the game (rule 41)
- Asked: *"She's not a freakin chicken or a frog bro."* (10-29): no cluck or croak.
- Status: **not done; the cue plays at every grab.**
- Evidence: `godot/presentation/audio/matron_sound.gd:171` (croak at a stage up), `:241`, `:317`,
  `:336` (croak); `:236`, `:339`, `:367` (her hand closes on his ankle), `:430` (the spare)
  (cluck). Both are code-made one-takes (`sound_bank.gd:166-167`; `matron/CREDITS.md` says so).
  Other human sounds still code-made: `matron_wake` (`sound_bank.gd:113,782`) and the climber's
  breath (`:82-83`), which rule 40 does not allow either.
- Hurts: 4. It is the first sound at a grab, and the owner has already rejected it by ear.
- Size: small to remove the 8 call sites, the 2 bank entries and the `her_takes.gd:36-37`
  families. Replacing the wake and the breath with real takes needs his picks by ear (candidates:
  `HER_SOUND_VARIATION_SPEC.md:169-170`).

### 3. She cannot pull him off a ladder (rule 35)
- Asked: *"pull"*: in reach she pulls him off a ladder by the ankle (10-28).
- Status: **not built, and the code does the opposite.**
- Evidence: `src/sim/physics_world.cpp:862-868` takes a hold only when
  `traversal_state_ != TraversalState::Climbing`; its comment says pulling him off the first rungs
  "is what kept the climb from ever starting". A pest or dangerous Matron shoves him instead
  (`:869-875`); a fully changed one does nothing (`Matron::shoves()`, `matron.hpp:1321`). No test
  covers a grab on a ladder.
- Hurts: 4. A ladder is a safe place from her hands, and ladders are most of the climb.
- Size: medium, and it can grow: the grab must come out of a climbing state with a physical
  grip tear-off and a recovery path (rule 29, "he can always recover"), plus tests.

### 4. She still reads his true position, so hiding does not work (rule 29)
- Asked: perception is evidence with an age (rule 29); her rage is fed only by sound that reaches
  her ear (A1).
- Status: **five leaks still present** from `MATRON_REDESIGN/PERCEPTION_AUDIT.md`:
  - B1 crumbs: `src/sim/matron.cpp:2399` `drop_crumb` from his true position every tick; read at
    `matron_moves.cpp:120-151`, used in Searching.
  - B2 hush: `matron.cpp:3115` `hush_step(dt, saw, d, loudness)`; `matron_moves.cpp:308,318` read
    raw loudness and `distance < 4.0`.
  - B3 journey yield: `matron_journey.cpp:206-219,330` `player_on` reads his position, no sight.
  - B4 handled stock: `matron.cpp:2317` `handled_ |=` from `player_holding_`; `:3407`.
  - B5 pace: `matron.cpp:3122`; `matron_moves.cpp:69-71` `distance > 15.0F ? 2 : 1`.
  Also C1 `player_on_deck_`, C2 `player_braced_` (`matron_hold.cpp:644`) and C4 the true ankle
  handed in every tick (`physics_world.cpp:718`).
- Hurts: 4. A player who goes quiet or hides is still found, which breaks the whole point of
  "she hunts by sound".
- Size: small each (the audit gives the replacement for each), about a day together. The mischief
  and crash/room tests lean on B4 and B1, so their checks change; that is a protected PR. The
  structural fix (an evidence record in place of `pre_step(dt, player, loudness)`) is large.

### 5. She has no voice lines (rule 43)
- Asked: a condescending, merciless nurse talking to a child, profane, emasculating; the last, at
  the apron wipe, *"Waste not."* (10-21).
- Status: **not built.** There is no line text and no voice asset for her; `voice/` and
  `voice_lines.gd` are the climber's. Only the sim hook exists: `Matron::waste_not_` is counted
  at the Wipe beat (`matron_fatality.cpp:70-72`) and exported (`scraperx_simulation.cpp:1848,1934`);
  no `.gd` reads it.
- Hurts: 4. The fatality is mute apart from grunts.
- Size: large: write the lines (he approves them), generate takes (pattern:
  `src/voice/generate_story_voice.py`), his picks by ear, cues per event through `her_takes.gd`.

### 6. The summit has no ending (C4)
- Asked: the top of the route (A + B built); *"the ending beyond it is the owner's"*.
- Status: **A and B built** (`src/sim/summit.hpp`, `sim_tests_summit.cpp:124,182-188`,
  `touch_summit`); **the ending: not built.** After it there is a "THE TOP" toast and a flash
  (`main_feel.gd:165-182`) and nothing else.
- Hurts: 4 for the few who finish.
- Size: large, and it is his call first (a decision in `handoffs/OWNER_DECISIONS_PENDING.md`).

### 7. Her death is permanent
- Asked: nothing directly; rule 64 parks "her death and return (she's done this before)".
- Status: **a bug against that design.** A landing of 16 m/s or more kills her
  (`matron.cpp:3322-3330`, `kLethalLandingMps` `matron.hpp:432`), which sets `dead_`. Nothing ever
  clears it, and `Matron::restore` skips a dead Matron (`matron.cpp:3635`), so she is gone for the
  rest of the run, across the player's deaths and checkpoint restores.
- Hurts: 3. A player can end the threat for good by dropping her off the catwalk.
- Size: small to make a restore bring her back as she was; the "comes back worse" design is
  parked (rule 64).

### 8. The hold is a camera, not yet a cinematic (rule 33)
- Asked: third person from grab to release or death; PUNCH works; *"a whole cinematic, dramatic
  experience"* (10-29).
- Status: **partly built.** Third person, orbit, slow motion at the let-go (0.5 s at 0.3x), a
  pull-out, a fade, a 6x sim while black (`godot/presentation/life/matron_hold_camera.gd:26,56-67,
  135,222,269-390`; applied at `main.gd:447-453`); gore always on (`gore_view.gd:9`); her sounds cued
  per beat (`matron_sound.gd:360-440`). PUNCH works only before the rip and in the hang window
  (`sim_tests_strike.cpp:204-236` requires it refused after the rip).
- Missing: a shot per beat (`beat_cuts` is a counter, `:98,412-418`; `fov_offset()` and
  `vignette()` return 0.0, `:121-127`), a tear beat, haptics for the grab, the rip and the hang
  (`main_feel.gd` covers only the pipe knocks and a "her" pulse), a hold-specific mix, her voice
  (gap 5). No test asserts third person from grab to release; the camera is only logged
  (`ui_test_act.gd:278-329,750`).
- Hurts: 3. Size: large and visual; it needs the Godot runtime for every iteration. The missing
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

### 12. The foreman radio is four lines and goes quiet at 220 m (B3)
- Asked: the foreman on the radio: furious, then the lifeline, then admitting he covered up her
  accident; her stage hints (*"She's quiet. Keep it that way."*) (A1).
- Status: **partly built.** `godot/presentation/life/foreman_radio.gd:20-26` has four fixed
  captions, no voice, cued by place or event once per launch (`:69-84`); wired at `main.gd:185-188`.
  Silent above 220.5 m. No cover-up line; "She's quiet" appears nowhere. The recorded set
  (`story_radio_player.gd`, 40 cues) is **not wired anywhere** (no `.gd`, `.tscn` or
  `project.godot` mentions it) and is cast Iona, Ro, Ari and Dev, not Brenner and Miller; 36 of
  the 40 are Kokoro takes (rule 40 does not allow them); the `VOICE_RECORDINGS_APPROVED` gate its
  `AUTHORING.md` cites exists in no code; `ci/verify_story_radio_package.py` is not run by CI.
- Hurts: 3. It is the only story channel. Size: medium for lines, triggers and wiring; the voices
  wait on his picks by ear. Which cast he means is his call (listed in the owner decisions).

### 13. The story's premise and its crew are missing (B1, B2)
- Asked: a worker sneaks onto his own site on a weekend (badging in), wakes a coworker exposed by
  a lost X-ray source, the only way off is up; she kills the overtime crew, bodies higher up.
- Status: **B1 not built**: the only traces are the word "Saturday" in a caption
  (`foreman_radio.gd:22`) and a comment on the badge (`:73`); no gate, badge-in, X-ray source,
  overtime crew or topping-out beam in any `.cpp`, `.hpp`, `.gd` or `.json`. **B2 not built**:
  workers cannot die (`src/sim/crew.hpp:37`), `matron.cpp` never touches `Crew`, about ten live
  workers stand from 22 to 418 m (`crew.cpp:61-79`), corpses are the climber's alone
  (`corpse.hpp:1-15`).
- Hurts: 3 each. Rule 50 wants every feature to have its reason in the story.
- Size: B1 large (a gate scene, a source prop, the crew story, a topping-out beat, and the cast
  conflict settled first). B2 medium (a dead-worker state or placed props, plus triggers).

### 14. Nothing is lived-in above 396 m (D1)
- Asked: little lived-in spots all over the structure.
- Status: **partly built.** The spot finder walks rings every 11 m to 143 m, then 176.25 + 22k for
  k = 0..10, so to 396.25 m (`loose_stock.cpp:734-741`); nothing from 396 to 802.64 m, about half
  the climb. Ziplines also stop at 396 m (`physics_world.cpp:340-345`). `sim_tests_stock.cpp:74`
  asserts at least 60 pieces, not coverage by height.
- Hurts: 3. Size: medium, and it changes body order, which makes it a whole-world change
  (`CLAUDE.md` item 4: the full native suite runs locally).

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
  against "fed only by sound"; the stage-up cue is the croak (gap 2); no test covers Changed back
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
  the touch-level check is in `touch_matron_pipe`, which CI does not run (gap 0). Size small.
- Body proportions ("not too thin") are checked by no test (`matron_stats.json` is read by no code).
  Size small, by eye with the runtime.
- Hurts: 2.

### 20. Smaller owner calls not met
- **D4 diegetic machine controls** (grab the forklift's real lever): not built; the forklift is
  "Action takes the controls" (`forklift.hpp:10-14`), the crane a remote and buttons
  (`yard_crane.hpp:14-19`); only the hanging handles are physical grabs. Hurts 2. Size medium for
  the forklift, large for every machine.
- **C2 movement as a reason to play**: no time trial, ghost or best time (only the "THE TOP"
  toast, `main_feel.gd:165-182`). Hurts 2. Size medium; the ideas were the agent's.
- **D3 plate clamp**: not built (zero hits in `src`, `godot`, `tests`, `03_EXECUTION`); the magnet
  and the spreader are. Hurts 1. Size medium, on the magnet's pattern.
- **A4 (A)** frail first then the Turn: built, but a dangerous or changed Matron is no longer
  frail (`matron_found_climb.cpp:127,324,365`).

### 21. Background and process gaps (low for the player)
- **Rule 39**: the story radio, the Photo ID page and the identity folder have no call site
  (`work_id_page.gd` has no instantiator); the rule says "after the Matron fixes", so not yet is
  consistent. Hurts 1 now. Size medium to wire (a node, `present(state)` feeds, a settings page, a
  scenario); large if the two casts are reconciled.
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
  per-route climb of 730 lines). Deck-to-deck up and down is proven among 132, 143, 154 and 176.25
  (`sim_tests_matron_pursuit.cpp:125,213,297`); the islands above 176.25 and below 132 are
  UNVERIFIED. `kStackPitch` 11 m is baked into the pipe logic. Hurts 1 to 2. Size large.
- **Housekeeping**: the music box mesh is still in `matron.glb` (freed at load,
  `matron_look.gd:177`; a dead `box_cord` pivot remains); `matron.hpp:13` still says the catwalk is
  "the one thing that kills her". Size small.

## Checked and built (nothing to do)

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

- The recovered B3 row says "built, gated": it is dead code, nothing instantiates it. The radio
  that runs is the four-line `foreman_radio.gd`.
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
- `matron/CREDITS.md` and `PARKED_FEATURES` both say her chart is "largely built"; it is built to
  its steps 1 and 3 and her hearing the room (`HER_CHART.md:100-115`).

## Waiting on him (rule 64): status only, not to be built unasked

- Dread before she is seen: **partly built.** The stalk (`matron_stalk.cpp:1-8`, slice
  `matron-stalk`), dread audio at first sight within 40 m (`matron_sound.gd:165-167`), locker
  knocks (`matron_view.gd:503,515`), a haptic pulse for pipe knocks within 14 m
  (`main_feel.gd:1359-1368`) and a "HER n M" marker (`main.gd:503-510`). No haptic, light,
  edge-of-view or HUD signal before she is seen.
- Her limbs torn off: **not built.** Her sim body is one capsule (`matron.cpp:78`); her limbs are
  drawn only; no harpoon, laser welder or nail gun exists. Size large.
- Her death and return: **partly built** (gap 7). No kill-shot weapon, no revive.
- More decks: **partly built**, see gap 21.
- Tower two: the broken bridge is built (`twin_tower.*`, native `twin`, `touch_twin`); the property
  damage climb was not checked.
- The chiller drop and glass tube: the chiller drop is built (`chiller_drop_spec.md`); the glass
  tube is a parked design.
- Lift governors: not checked.
