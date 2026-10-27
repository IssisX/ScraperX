# Soundscape research: her_moments (workflow wf_5917c048-441, stopped for usage)

Fields come from get_matron_state() (src/bridge/scraperx_simulation.cpp:1646) and get_matron_hold_state() (:1757). Every cue today fires in godot/presentation/life/matron_view.gd: the locker at :305-378, the planks at :434-449, and her own at :1483-1618. Each fires when a counter goes up or a state or bool changes. Her own cues fire only while she is out and within 140 m of the camera. godot/presentation/life/gore/gore_view.gd (:128-162) reads the hold and fatality edges but plays no sound.

**Locker (once a run)**
- Wake: `state` 0→1, from a loud sound within 30 m. No cue on the edge.
- Straining, 16 s: `strain` goes 0..1. `bolts` = floor(strain×4), so a bolt pops every 4 s. Now: locker_knock on a view timer (every 1.5 s, speeding to 0.16 s) and bolt_pop.
- Burst: `state` 1→2 plays locker_burst + dread. Emerging lasts 2.2 s.
- Catwalk: `planks[i*9+7]` 1→0 when a plate's bolts shear under weight (up to 11 a run). Plays plank_crack. Its landing is not heard.

**About the deck**
- Steps: each time `stride` passes 0.7 m (0.385 m when Hunting) with `speed` > 0.3. About 18/s at her 6.8 m/s hunt, 1.4/s shuffling.
- Breath: view timer every 0.7–2 s within 24 m (6 m at her post). Muted while Peering (8). The music box is muted while she is Guarding (7) and still.
- `sees_player` false→true, under 40 m, 7 s cooldown: dread plays and ambience ducks for 3.6 s.
- Unrigging (9): a shackle rattle on a view timer. `unrigs` +1 when the rope comes off the eye: clang.
- Exported, no sound yet:
  - `fooled` +1: she ran to a noise and nobody was there.
  - `takes` +1: her hands take the player's toolbox.
  - `stage` edges: pest/dangerous/changed, both ways.
  - `rage`, and `noise` (the loudest sound at her ear, 0..1).

**The Turn** (cooldown 45/18/4 s by stage)
- `turn` 0→1: 0.9 s windup, her spine cracking (bone_crack). 1→2: 3.2 s lunge (shriek + crack).
- Lunge bursts can be worked out but nothing uses them: a burst while fmod(`turn_s`−0.9, 0.5) < 0.2, about 6 foot-snaps per lunge.
- `vaults` +1: her hands slam onto what she goes over. `leaps` +1: she leaps a gap (shriek).
- `on_line`: going down a bare cable on her forearms (scrape every 0.1–0.16 s). `cable_slides` +1 when she clips on: no sound.

**Her hands on the player** (src/sim/physics_world.cpp:828)
- `catches` +1: her hand starts for the ankle (Grab). No sound.
- `shoves` +1, at most once per 1.5 s: shriek + thud.
- `throws` +1, at least 6 s apart: shriek. The 0.35 s rear-back before the throw has no field.

**Climbing**
- `climbs` +1: she takes hold of a climb (shriek). The rung clang is played every 0.3 m of `stride` while `climbing`. It is a guess from distance, not her real grips.
- No sound from any of these:
  - `climb_move` (RungUp/Down, Mantle, Pole, JumpCatch, HangDrop, GapJump) and `climb_phase`.
  - `climb_airborne` (launch and landing edges) and `climb_sliding` (slipping down a pole).
  - `climb_leg_driving`, `mantling`, `climb_force_n` (her effort, in N) and `clinging`.
- `climb_grips` counts only the old ladder climb's holds. `climb_releases` counts only failed climbs.

**Corner pipe**
- `pipe_holds` +1 every 0.45 m (about 6/s going down, 1.6/s going up): a knock. `pipe_flanges` +1 per deck (every 11 m): a bong.
- `pipe_slams` +1: a blow tears her off, she slides on her legs onto a flange: boom + crack + shriek. `pipe_sliding`: her skin scraping on the steel.
- No sound: `pipe_head_down` (changed, coming down head first in jerks), `pipe_stunned`, `pipe_listening`, `pipe_slam_mps`.

**Blows and falls**
- `staggers` +1 (0.4–1.2 s): stamp + scrape. When a stagger turns into a knockdown, this counter goes back down by one.
- `knockdowns` +1: from a blow, any landing at 7 m/s or more, and every pipe slam. She stays down 4/2.5/1.2 s by stage. Plays boom + crack + winded breath.
- No sound:
  - `blow_mps`: how hard the blow was.
  - `down_left_s` at 0 while she is down: pinned, crawling out.
  - State 11→4: she gets up.
- Falling faster than 6 m/s below her floor (worked out in the view): one shriek.
- Death, landing at 15.34 m/s or more: `alive` goes false, state 6, body removed. No cue.
- Bay trap: `trap` 1–5, `traps`, `bracing`, `support_n` (her palms holding the leaf up). No sound.

**Hold** (states 12–18; only the music box, breath and steps play)
- `grabs` +1 / `grip_on`: her fingers close on the ankle, then a 0.9 s lift, head down.
- `carries` +1: she walks to the rail with the body upside down at her side, up to 10 s.
- `dangles` +1: she heaves the body out over the rail for 3–6 s. Continuous: `grip_load_n`, `grip_strain_ns`, `thrash`, `braced`.
- `armplays` +1, then `armplay_beat`:
  - 0: reach for the wrist.
  - 1: pull. `wrist_strain_ns`/`wrist_tear_ns` gives 0..1 as the shoulder gives. `arm_tear_count` +1 is the tear.
  - 2: the arm up to her face, 1.5 s.
  - 3: she swings it, 1.5 s.
  - 4: over her shoulder. `arm_throw_count` +1 when it is thrown. The arm's landing has no edge.
- `releases` +1: she lets go onto a ledge. `spares` +1: she sets the player back and straightens the collar. `claims` +1: both arms gone.
- Where to place sounds: `hand`, `hand2`, `knee`, `severed_arm_position`, `severed_legs`.

**Fatality** (`fatality_beat_edges[10]` counts each beat's start; a stuck beat gives up after 20 s)
- 0 Lay: carries the body by the ankle and lowers it to the deck.
- 1 Pin: her knee on the chest (`knee_pin_count` +1).
- 2/3 Legs: `leg_strain_ns`/`leg_tear_ns` and `leg_load_n` while she pulls. `leg_tear_count` +1 when the hip tears; she tosses the leg.
- 4 Drag: by the collar to the bollard. `knot_count` +1 when she knots it onto the shackle; `knot_load_n` is the rope's load.
- 5 Kick: her boot wrenches the trip handle (`brake_kick_count` +1).
- 6 Haul: the 800 kg skip drops and the torso rises (`haul_count`, `haul_peak_y`).
- 7 Fall: the knot parts (`knot_part_count` +1), then the torso lands (`fall_landing_count`, `fall_landing_speed_mps`).
- 8 Wipe: her palms down her apron for 2.6 s (`wipe_s`). `waste_not_count` +1 is the "Waste not." voice hook.
- 9 Black: 1.5 s, then `runs_ended_by_matron`.

**Gaps**
- On MatronSnapshot but not sent to Godot:
  - `pace`/`pace_changes`.
  - `hushed`/`creaks`: she freezes to listen, and creaks as she moves again.
  - `step_ups`, `clambers`, `stalls`, `sidesteps`, `reroutes`.
  - `grab_misses`, `grips_torn`, `brace_losses`, `carry_drops`, `arm_throws`.
- Not sent to Godot at all:
  - The climber's per-hand grips, slips, catches, misses and launches (traversal ClimbView).
  - `collar_count` and `leg_toss_count`.
  - Her landing speed, the throw's wind-up, and the timing of her head-down jerks on the pipe.
- get_impacts only tracks stock and machines. Nothing is heard when her body, the planks, the severed limbs, the torso or the skip hit steel.

**Bugs found**
- The music-box loop is never silenced. matron_music is only called after the early return for "not out or beyond 140 m". So after her death, a restore, or when she is far away, the loop keeps its last volume and position.
- A pipe slam also adds to `knockdowns`, so the slam cues and the knockdown cues fire together.
- `pipe_holds` keeps counting during the slide, when her hands are off the pipe. So knocks play under the scrape.
