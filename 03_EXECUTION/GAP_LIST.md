# Gap list: what the owner asked for that the game does not do yet

Built from the recovered owner calls (`00_START_HERE_ARCHIVE.md`, "Owner calls recovered from the
archived log and handoff"), the rules in force (`00_START_HERE.md`) and the original static audit at `2ac7fa1` (2026-10-29). Corrections inspect `dea9cfcba0923242fc6046d88aada9726a1b97e1` and the
`tests/sim_tests_stock.cpp` delta on trunk `65896fbd11ed2048bbd0a102b252008a8cc3db98`. J-142 (2026-11-14) rechecked every entry against trunk `d8a2d71f` and named the job holding each one. No game, device, visual or audio run was made for either revision. Source and test definitions establish implementation or coverage, not a pass.
Older line numbers can drift; search the cited symbol. `work/plan.md` and the board (#546) own current assignments.

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

**Still failing on trunk; held by J-088 (#406 open, owner:muse).** `touch_matron_pipe` is in no
`godot-touch` shard (workflow touch-2/touch-3 lists read); it stays a warned known gap in
`.github/scripts/check_touch_shard_coverage.py` — at the take-back the drum lies 17.8 m south of
where it was set down and the climber is on 143, so PICK UP is never offered (rechecked
2026-11-04, issue #134). PR #29 proposes a staging fix and is closed unmerged. J-088 wires the
scenario into CI with its finger-wrap fix. Do not infer visual approval from a headless run.

## Ranked gaps

### 1. Pulled starts and ladders still carry the route above 221 m (rules 52, 53)

**Partial; held by J-082 (#400 open, owner:claude, parked).** `band_plate_shop.cpp`
`build_climbing_route` builds board-and-ladder segments 340.25–484 m;
`band_facade_crane.cpp` continues the pattern to TP-640; `band_wet_isolation.cpp` places
route ladders at 242.25, 308.25 and the 340 m plate top. J-105 step 1 is done (#424 closed).
Policy fit and gameplay impact unverified; rule 52 governs proposed changes.

### 2. Rejected mouth cues and generated human sounds (rule 41)

**Done in source by J-098 (#416 closed, landed #540).** No `_matron_wake` or `_breath` function
exists in `sound_bank.gd`: `matron_wake` (4 takes) and breath are recorded takes
(`HER_RECORDED` in `sound_bank.gd`, `breath/CREDITS.md`), and no cluck or croak appears in
`matron_sound.gd` or `audio_director.gd` — `HER_VOICE_CLIPS` lists only her voice families.
Remaining: listen, and the owner's ear approval.

### 3. Pulling him off a ladder (rule 35)

**Implemented in source; held by J-086 (#404 open, owner:muse, parked).**
`physics_world_grab.cpp` applies the ladder pull while he climbs (capped by her hand pull);
`tests/sim_tests_matron_ladder_pull.cpp` defines finite grip, force budget and release checks.
Runtime unverified. Run the native and actual touch path at the revision being evaluated.

### 4. Perception and hiding (rule 29)

**Implemented in part; held by J-049 (#278 open, owner:muse, parked).** `Matron::pre_step`
(`src/sim/matron.cpp`) derives `d` from `ev.fix` or the heard lead and feeds crumbs from
`ev.seen_at` / `ev.heard_at` into `hush_step` and `choose_pace`. `matron_journey.cpp` still
has `player_on` checks — inspect their inputs before calling them a leak. Do not claim hiding
works, or fails, without a runtime test. Rage deviations remain in gap 17.

### 5. Her voice lines (rule 43)

**Line system built by J-089 (#407 closed); no approved takes ship yet.** `matron_speech.gd`
binds each line in `HER_LINES_DRAFT.md` to its sim event with a cooldown and deterministic
takes, and `Matron::fatal_next` counts `waste_not_`. But `MATRON_VOICE_APPROVED` is false
(`audio_director.gd`) and the picks were moved out of the shipped tree. Remaining: the
owner's picks by ear, then the gate. `voice_lines.gd` is the climber's voice, not proof of hers.

### 6. Seat the crew's last beam (C4)

**Done by J-110 (#429 closed).** Checked on trunk `a91973d8`, no code change needed:
`sim_tests_summit` passes (deaths 0), `touch_summit_ending` passes, rule_check clean.
`src/sim/summit.cpp` `build_ending` / `work` / `read_ending` seats the beam and tracks the
canopy return; the foreman's `last_beam` caption is cued by `foreman_radio.gd`.

### 7. Her death and checkpoint restore

**Implemented in source; the return design is an owner decision with no job holding it.**
`Matron::restore` (`src/sim/matron.cpp`) sets `dead_` from the saved state, and
`tests/sim_tests_matron.cpp` `run_matron_restore` defines the checks; J-059 landed the
injury restore (#343). The broader "comes back worse" design remains parked under rule 64.

### 8. The hold is a camera, not yet a cinematic (rule 33)

**Partly built; no job holds the remaining work.** Third person from grab to release is
asserted by J-079 (#397 closed, `ui_test_act.gd` fails if any held frame leaves first person
or any beat has no cut); `fov_offset()` / `vignette()` return the eased beat values and a
hold mix exists (`audio_director.gd` `hold_mix`, cued by `matron_sound.gd`); hold haptics
landed with PR #59 (merged 2026-10-31); PUNCH is refused once the rip has begun
(`tests/sim_tests_strike.cpp`). Still missing: a shot per beat (`beat_cuts` is only a
counter), her voice (gap 5), and runtime/visual quality review. Hurts: 3. Size: large.

### 9. She does not wreck his work (A3, A4 B and C)

**Partial; held by J-112 (#431 open, owner:muse, parked).** `notice_moved_stock`,
`fetch_stock`, `take_stock` and `throw_stock` implement the stock path (fetching limited to
Pest, `kTakeMaxKg` 25 kg), and `shove_stock` now picks a shove edge and pushes a throwable
over a lip (`matron.cpp`) — deliberate edge shoving exists in source, runtime unverified.
`fooled_` tracks noises leading to nobody only; nothing records a repeated player action.
General jamming/resetting, boarding a called lift and undoability remain unverified.

### 10. Loud machines mostly do not draw her (A7)

**Partial; held by J-076 (#391 open, owner:muse, in flight).** `physics_world.cpp` sends
energy-scaled `ImpactLedger` events and yard-crane drive noises to `Matron::hear`. PR #48
proposes Stage A hearing and is closed unmerged. Whether cages, counterweights, plates,
doors, lifts and wall hits draw her as direct sources is still unverified.

### 11. Night lighting and the climb (B5)

**Helmet lamp and city glow done by J-081 (#399 closed); lit rings still open, no job holds
them.** `main.gd` builds `HelmetLamp` (spot range 30 m, angle 55); `sky.gdshader`
`city_glow` is driven by `sky_cycle.gd`. No named implementation of rings lit behind the
climber was found. Night readability by eye is unverified.

### 12. Foreman radio and story cues (B3)

**Partly built; held by J-087 (#405 open, owner:muse, parked).** `foreman_radio.gd` has
place/event captions including the summit admission (`last_beam`); `main.gd` instantiates
`StoryRadioPlayer`, and its `VOICE_RECORDINGS_APPROVED` gate is false in
`story_radio_player.gd`. Instantiation does not establish that gameplay triggers these cues.
Which cast he means is his call. Hurts: 3. Size: medium.

### 13. The story's premise and its crew (B1, B2)

**Partial; no job holds it.** `foreman_radio.gd` references Saturday and the accident; no
X-ray-source or badge-in sequence was found (the one "badge" hit is a gate-log comment).
`src/sim/crew.cpp` builds 14 workers (6 yard, 8 on tower decks 22–418.25 m);
`Crew::State` has walking, working, yielding, startled and stumbling, with no death state.
Her killing the overtime crew and leaving bodies higher up remains open.

### 14. Lived-in spot finder stops at 396 m (D1)

**Done: `dress_tower` dresses rings above 396.25 m on trunk.** `loose_stock.cpp` has a third
pass over the bands' rings above 396.25 m and the top decks (`append_upper` /
`realize_upper`, bodies built after the deck-154 forklift so no pre-existing body moves).
J-111 (#430) was parked as stale — J-014 already dressed the upper rings. Extending stock
can change world body order; apply the conditional whole-world test rule in `CLAUDE.md`.

### 15. A hard blow and escape (rule 34)

**Partially implemented; no job holds it.** `Matron::take_blow` (`src/sim/matron.cpp`)
evaluates impulses, stance, strike height and stage before staggering or knockdown; the
player strike (`physics_world_strike.cpp`) uses three contact ticks capped by
`player_strike_limits.hpp`. The old 50 N·s punch arithmetic is not a runtime proof, and
useful stock along the whole route remains unverified. Any threshold change is protected.

### 16. Movement continuity (rule 37)

**Implemented in part; held by J-078 (#393 open, owner:muse).** `matron_rig.gd` `govern`
limits bone rotation (`GOVERN_RAD_S` 11.5, burst 19.0) and joint speeds are capped at
19.9 rad/s (`JOINT_GLOBAL_RAD_S`, landed #396). Hands reaching each hold and complete
runtime coverage are unverified; a production-path time series of bone speeds plus visual
review would address the rest.

### 17. Her mood hints, the stage-up cue and a missing test (A1 leftovers)

**Core built; the named-test gap is closed.** `tests/sim_tests_matron.cpp` now names
`kRageChangedDown` in the "matron rage down" checks. **Remaining deviations, no job holds
them:** pain also feeds her rage (`kStaggerRage`, `kKnockdownRage`) against "fed only by
sound"; the requested radio hint remains to be checked (gap 12). Hurts: 2. Size: small each.

### 18. A wrecked machine leaves a way on (rule 51)

**Partial test coverage; held by J-015 (#122 open, owner:claude, parked) and J-109 (#428
open, owner:claude, parked).** Wreck checks exist for B/C (`sim_tests_well.cpp`), G/H/I
(`sim_tests_plate.cpp` `run_plate_wreckage`) and J/K/L (`sim_tests_crane.cpp`
`run_crane_wreckage`). No equivalent checks were identified for S1–S3, D–F, M–O or vertical
machines; test existence alone is not a pass.

### 19. Her fingers, cloth and pipe warning (A6)

**Partial; held by J-088 (#406 open, owner:muse, in flight).** Fingers are per-joint pivots
(four fingers of three joints, thumb of two, `matron_view.gd`); the skirt is a real verlet
cloth proxy (`matron_skirt.gd`, J-011), not a canned sway. Ankle/rung wrap is J-088's work.
Pipe warning: the four-second assertion is in `tests/sim_tests_matron.cpp`
(`warned_s >= 4.0`); the touch check is `touch_matron_pipe` (gap 0, not in a shard yet).

### 20. Smaller owner calls

- **D4 diegetic controls — partial; held by J-083 (#401 open, owner:muse, parked).**
  `forklift.hpp` describes Action taking the controls; `yard_crane.hpp` describes the remote
  and buttons. The grab of a physical forklift lever remains unverified.
- **C2 movement incentives — partial; no job holds it.** `main_feel.gd` `_announce_summit`
  shows elapsed time and deaths. No named best-time or ghost feature was found; that is an
  owner decision (`OWNER_DECISIONS_PENDING.md` item 10), not a build.
- **D3 plate clamp — no named implementation found; no job holds it.** A search for
  "plate clamp" and `plate_clamp` returned no match; `yard_crane.hpp` contains magnet and
  spreader. Verify owner intent before proposing a new machine part.
- **A4 frail first then Turn — partial; no job holds it.** `find_climb` and
  `follow_found_climb` (`matron_found_climb.cpp`) restrict a Pest outside the Turn while
  later stages can attempt the descent. Stage-based behavior, not proof of the full
  frail-to-strong presentation.

### 21. Background and process gaps

- **Rule 39, identity and radio — partial; no job holds it.** `pause_menu.gd`
  `_build_work_id_page` creates `WorkIdPage`; `main.gd` instantiates `StoryRadioPlayer`;
  `VOICE_RECORDINGS_APPROVED` is false. Cue triggering and approved recordings need
  separate verification.
- **Rule 57, chart — partial; held by J-113 (#432 open, owner:muse, parked).**
  `godot/presentation/uncanny/` has the chart and `voice_keeper.gd` requests microphone
  permission; `export_presets.cfg` enables `permissions/record_audio` with no INTERNET or
  CAMERA entry. The workflow lists `touch_her_chart`, `touch_her_chart_fast`,
  `touch_mic_consent`, `touch_voice_echo` and `touch_room_hearing`; their results require
  a run. Obtain in-game consent as asked.
- **Rules 56 and 27, ascent — unresolved; held by J-084 (#402 open, owner:claude,
  parked).** `vertical_machine.cpp` registers `sx.gravity_balance.v1` and
  `sx.rack_climber.v1`; registration is not placement or a playable route. AS-007–AS-010
  are still called non-canonical while no canonical machines stand between 340 and 750 m;
  do not silently change the rule.
- **Rules 30 and 31, Matron travel — done by J-085 (#403 closed, landed #437).**
  Pursuit cases are in `tests/sim_tests_matron_pursuit.cpp`; `matron_pipe.cpp` uses
  `kStackPitch`. Place-specific data alone does not prove the general-skill requirement
  fails.
- **Housekeeping — done by J-114 (#433 closed, landed #465).** No `music_box` mesh and no
  `box_cord` pivot remain anywhere in `src`, `godot` or `tests`.

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

- Dread before she is seen: **partly built.** The stalk (`matron_stalk.cpp`, slice
  `matron-stalk`), dread audio at first sight within 40 m (`matron_sound.gd`, `DREAD_M`
  40.0), locker knocks (`matron_view.gd`, `locker_knock` cue, hastening) and a "HER n M"
  marker (`main.gd`, `_hud.her_marker` bearing/distance). No haptic, light,
  edge-of-view or HUD signal before she is seen.
- Her limbs torn off: **not built.** Her sim body is one capsule (`matron.cpp:78`); her limbs are
  drawn only; no harpoon, laser welder or nail gun exists. Size large.
- Her death and checkpoint restore are implemented in source (gap 7); the broader return design remains parked. No kill-shot weapon was found in the earlier audit.
- More decks: **partly built**, see gap 21.
- Tower two: the broken bridge is built (`twin_tower.*`, native `twin`, `touch_twin`); the property
  damage climb was not checked.
- The chiller drop and glass tube: the chiller drop is built (`CLIMB_REDESIGN/chiller_drop_spec.md`); the glass
  tube is a parked design.
- Lift governors: not checked.
