# The Matron's soundscape: spec

She sounds like a body: breath, throat, mouth, weight and hands. There is no tune, bell, chord or score. You can still hear her stage: breath means pest, a growl means dangerous, and a rage growl over a rough bed means changed.

All recipes below are working code, measured on this box. Prototype: `/tmp/claude-0/-home-user-ScraperX/9ad8486c-c39f-50e0-b6e3-49506d0469f4/scratchpad/her_sound/her_bank_proto.gd`. Probe: `/tmp/claude-0/-home-user-ScraperX/9ad8486c-c39f-50e0-b6e3-49506d0469f4/scratchpad/her_sound/her_bank_run.gd`. Band check: `/tmp/claude-0/-home-user-ScraperX/9ad8486c-c39f-50e0-b6e3-49506d0469f4/scratchpad/her_sound/analyze.py`.

## 0. Delete

- **`sound_bank.gd`:**
  - Line 67 and `_lullaby` (618-644).
  - Lines 68-70, with `_matron_breath` (647-659), `_matron_step` (760-766) and `_matron_shriek` (769-785). Her own bank rebuilds these three under the same names.
- **`audio_director.gd`:** `_matron_loop` (151-153, 259, 332-334), `matron_music` (826-833), and the loop cap at 1124-1125.
- **`matron_view.gd`:** `HEARD_MUSIC_M` (40), and the music block with its pitch warp (1483-1502).
- **The prop:** it is only decoration.
  - In `matron_look.gd`, `dress()` frees the `music_box` mesh.
  - Delete the `wood` material (128-130) and its `_parts` entry (169).
  - Delete `build_matron_body.py` 1108-1115.

## 1. Sound list

**New primitives in `sound_bank.gd`:**
- **GLOT** `_glottal(n, f0a, f0b, jitter, shimmer, sub, tilt_hz, chaos, breath)`: a smoothstep pulse, open for 60% of the period and closing over 20%. Each period:
  - f0 ×(1±jitter) and amplitude ×(1±shimmer).
  - Odd periods ×(1−sub).
  - A chaos/f0 chance of f0 ×U(.7, 1.3) for 40-80 ms.
  - The pulse's derivative goes through a one-pole low-pass at tilt_hz, plus 1-4 kHz noise ×breath×pulse.
- **FMT** `_formants(src, f1, f2, f3, q_mul)`: three RBJ band-passes in one loop, Q 5/6/8, gains 1/.6/.35. Three shapes are used:
  - BIG: 650/1190/2360.
  - OPEN: 810/1160/2670.
  - PRESSED: 400/1000/2400.
- **SAT**: tanh(d·x)/tanh(d). It must stay symmetric; an offset curve demodulated ROUGH into the lows.
- **ROUGH**: ×(1−depth·triangle), with the rate varying ±20% each cycle.
- **BP**: the band-noise filter, applied to a signal.
- **`_peak`**: normalizes up, because `_wav` only scales down.
- **GLIDE**: noise through a swept state-variable band-pass (updated every 16 samples), swelling as 4u(1−u).
- **GRAINS**: Poisson `_add_band_noise` bursts, with the rate ramped.
- **BUB**: a sine at 500-1,500 Hz, rising 30% in 15-40 ms.
- **SLIP**: stick-slip clicks fed into `_resonate`.
- **THUMP**: harmonics 1-8 at 1/k^0.7, so a phone speaker can carry the weight.

`build_her()` builds the clips below on a second SoundBank (seed 0x4D4154). Each bracket gives two numbers: the share of energy above 300 Hz, then the RMS in that band in dBFS.

**Loops**
- **`matron_bed_loop`, 3.0 s:** saws at 146.67 and 151 Hz (a 4.33 Hz beat) plus 207.33 Hz (a tritone), each a whole number of cycles per loop. Then SAT 3 and BP 450-2,200 twice. Over that, air at 400-1,400 Hz that swells twice per loop. [71%; −15.8]
- **`matron_bed_rough_loop`:** the same ×ROUGH(38 Hz, .6), played +2.3 dB.
- **`matron_growl_loop`, 2.3 s:** [80%; −14.5]
  - A 1.5 s exhale: GLOT(104→98, jitter .06, shimmer .25, sub .5, tilt 1 kHz, breath .35), then BIG, SAT 4 and BP 220-7,000.
  - A 0.75 s INTAKE: 0.5-3.5 kHz noise plus GLIDE 900→1,400 under ROUGH 32 Hz, catching twice.
- **`matron_growl_rage_loop`, 1.55 s:** a 1.0 s exhale (sub .7, chaos 2/s, SAT 8), then a 0.55 s INTAKE. [79%]
- **`matron_mouth_loop`, 1.7 s:** GRAINS 20→70/s at 2-6 kHz, saliva click pairs, and two smacks (GLIDE 1,800→700). [74%; −30]
- **`matron_strain_loop`, 1.25 s:** SLIP 30/s into 900 Hz and 2.1 kHz. [91%]

**One-shots**
- **`matron_breath`, ×3, 1.5-1.9 s:** an INTAKE, then an exhale of 0.35-2.2 kHz noise plus BIG(GLOT 95→86) at −10 dB, with mucus GRAINS. [88%; −17, old −18]
- **`matron_step`, ×3, 0.4 s:** [66-79%; −26, old −22]
  - A 0.7-5 kHz slap and a 60 Hz THUMP.
  - Deck modes at 190/430/720 Hz, Q 3: a boom, never a bell.
  - Flesh settling after.
  - One step in three peels off the steel.
- **`matron_shriek`, ×2, 1.2 s:** a gasp, then GLOT(420→900, sub .4, tilt 2 kHz, chaos 3/s) through OPEN, ROUGH(70 Hz) and SAT 6, with two breaks. [96%; −9, old −13.5]
- **`matron_joints`, ×3, 0.7 s:** 3-8 pops whose gaps close from 160 to 40 ms; each pop is a click, a 3 kHz ring and a 250-450 Hz knock. SLIP gristle between them. [73%; −30]
- **`matron_croak`, 1.2 s:** GLOT(36→23 Hz, jitter .25), then FMT 450/1100/2600 and BP 350-4,500. Two swells, then a wet click. [68%; −20]
- **Small mouth sounds:** [61-99%]
  - `matron_cluck`, 0.18 s.
  - `matron_swallow`, 0.6 s: BUB and a gulp.
  - `matron_sniff`, 0.8 s: pulls at 2.8 kHz.
- **`matron_effort`, 1.2 s:** GLOT(115→135) through PRESSED, rising under a 7 Hz tremor, then bursting into OPEN and SAT 6. [89%; −13]
- **`matron_hit`, 0.85 s:** a slap, a THUMP, a winded "HUH", then a snarl (sub .7, SAT 8). [84%; −14]
- **`matron_laugh`, 1.3 s:** BIG(GLOT 150→120), gated 6 times a second. [90%; −20]
- **`matron_rip`, ×2, 1.5 s:** [92%; −25]
  - A shoulder pop and a THUMP.
  - Fibers parting: GRAINS 40→600/s.
  - A squelch (GLIDE 1,500→400) with BUB.
  - A snap, then a suction pop (GLIDE 300→1,200).
  - Drips.

**Kept in the world bank:** the locker clips, `plank_crack`, `matron_wake`, `clang`, `scrape`, `hit_heavy` and `bone_crack`, which now belongs to the climber alone. Her voice stays within pitch 0.85-1.15.

## 2. Triggers

`data` = `get_matron_state()`, `hold` = `get_matron_hold_state()`. Trackers update every frame. Cues play only while she is out and within 140 m. d is the distance from her feet to the player. Levels are in dB, 0 unless given. Each trigger fires on its edge.

**Hunting and contact**
- **`sees_player` rises, d < 40, at most every 7 s:** below stage 2, `matron_dread` (the wake and the duck). At stage 2, a shriek and the duck.
- **`stage` rises:** croak −4, and the bed and growl loops swap.
- **`turn` 0→1:** the loops cut, then joints +3 at 0.4 s.
- **`turn` 1→2:** a shriek and the rage loop at full. Each time fmod(`turn_s`−0.9, 0.5) < 0.2 becomes true: step +3 at pitch 1.25.
- **`vaults`:** step +2 at pitch 0.7, joints −1.
- **`knockdowns`:** `hit_heavy` +4 and hit −2. Skip these when `pipe_slams` rose in the same frame.
- **`state` leaves 11:** joints +1.
- **`staggers`:** today's cues plus hit −8.
- **`shoves`, `throws`, `leaps`, `climbs`:** a shriek. Every shriek plays at −3, at least 0.6 s apart.
- **`catches`:** sniff.
- **`takes`:** cluck, then laugh −6.
- **`fooled`:** croak −2.
- **Steps:** today's stride test, at lerp(−11, 0, speed/6.8), at least 0.09 s apart.
- **Ladder, pipe, cable:** today's fields and cue names, because tests count `clang`. Her `bone_crack` cues become joints +3.
- **`alive` falls:** `hit_heavy` +4 at pitch 0.5, then all her layers go off.

**Listening and idle**
- **`state` 8 or `pipe_listening`:** her loops are gone in 0.25 s, and Ambience is held at −10 within 30 m. After 2.5 s, one time in three, a croak.
- **`state` 4, within 30 m:** a sniff or croak every 4-8 s.
- **`state` 5, or 7 standing still, within 8 m:** a cluck or swallow every 5-9 s.

**Her hands on the player (`hold`)**
- **`grabs`:** step −2 at pitch 1.4 at `matron_hand`, and a cluck.
- **`armplay_beat` == 1:** the strain loop at `matron_hand`, with s = `wrist_strain_ns`/`wrist_tear_ns`. It plays at lerp(−18, −2, s) and pitch 0.7-1.6. Effort −3 every 1.4 s.
- **`arm_tear_count` (silent today):**
  - Rip +4 at `severed_arm_position`.
  - `_audio.arm_break(hold.player_position)` for his crack and his `break_arm` scream.
  - A laugh 0.6 s later.
- **`armplay_beat` → 2:** a sniff, then a swallow.
- **`releases`, `spares`, `claims`:** a laugh, a cluck, a laugh, in that order.

## 3. The system

**Buses.** A new `Matron` bus sends to Effects and carries her throat and mouth clips. It runs AudioEffectDistortion in WAVESHAPE mode: drive 0.15+0.3·heat, post_gain −3·heat dB. The bed, body sounds and strain go to Effects. None of her sounds go to Ambience.

**Players.** These are hers alone, which frees the world's 5-voice pool.

| Player | Position | Unit / reach |
|---|---|---|
| Bed | non-positional | — |
| Growl | `head_world_position()` | 3 / 40 m |
| Mouth | her head; unfiltered, so dry and close in your ear | 1 / 6 m |
| Strain | her hand | 2 / 20 m |
| Body pool ×3 | where the cue happens | 6 / 90 m |
| Voice pool ×2 | where the cue happens | 4 / 60 m |

- **Pools:** a cue takes a free player, otherwise the one playing the lowest priority. Shriek, wake, rip and effort are priority 3; hit and laugh are 2. A cue never cuts off a higher one, and it is still counted.
- **3D settings:** inverse distance, max_db 3, filter at 3,000 Hz, −18 dB. `area_mask` stays 0, because nothing has Area3D reverb.
- **A deck between you** (|`floor_y` − your y| > 4.5 m): the filter drops to 1,500 Hz and her loops to −8 dB.

**Heat** is `rage` + 0.3 while Hunting, and 1 in the Turn. It changes by at most 0.5 per second.

**Layers**, set once a frame by `matron_layers()`:
- **Bed:** off past 55 m; −30 dB at 55 m, −12 at 15 m, −20 at 3 m. It switches to the rough bed at stage 2.
- **Voice:**
  - Pest: within 24 m, one breath per clip length plus a gap of 1.4→0.15 s as she nears, at lerp(−1, −14, d/24).
  - Dangerous: the growl at lerp(−20, −4, heat). Its 3/40 m curve handles distance.
  - Changed, or in the Turn: the rage loop, at the same level.
- **Under 3 m:** a pest gets the mouth loop, +3→−10.
- **Holding (states 12-18):** no bed and no growl; the mouth loop, and a breath every 2-3 s.
- **Down:** no growl, and a breath every 1-1.6 s.
- **Guarding and still:** breath only, within 6 m.

The growl and the mouth loop never play together.

**Fades.** Levels rise at 20 dB/s and fall at 240 dB/s. At −79 a loop is set to `stream_paused`. Loops start paused, and a swap calls `play()`.

**Ducking.** His voice ducks her loops by 0.75×`_duck_db`. A wake caps her loops at −60 for 3.6 s. That cap now lives inside `matron_layers()`, which fixes today's call-order bug.

**Silences**
- While she listens, which is exactly when the microphone hears the room clean.
- At the start of the Turn.
- Whenever she is not out: dead, restored, or past 140 m. Everything pauses, which fixes the loop that was never silenced.

Level ramps never make the 6 dB jump that would mask the microphone.

## 4. Budgets

- **Startup:**
  - The world bank sheds 0.29 s and 0.79 MB (lullaby 128 ms, breath 95, shriek 66, step 5), so `_bank_ready` arrives sooner.
  - `build_her()` runs after `build()` in the same worker task. It makes 18 names and 26 streams (1.44 MB) in 591-732 ms here. That range is from 3 runs at a load of 4.5; over the same runs the world bank itself varied from 2.02 to 2.33 s.
  - Net CPU is +0.3-0.45 s, and the bank grows from 5.24 to 5.89 MB.
- **Gate:** `SCRAPERX_MATRON_BANK msec` must be ≤600 on a quiet box. If it is over, cut to 2 breath takes and 1 rip and drop the rough bed, which saves 77 ms.
- **Per frame:**
  - No new native calls: `her_state` is reused, and `_feel_gore` keeps its hold read.
  - No allocation, and properties are written only when they change.
- **Polyphony:** 9 players, at most 7 sounding at once.

## 5. Must stay green

- **touch_matron_climb (≥3 `clang`) and touch_matron_act (≥8):** the cue name stays `clang`, and it is counted before any ready or silent check.
- **touch_matron:** `_update_locker` is untouched.
- **audio_mix and audio_fall:** she is never out in either, none of her sounds are on Ambience, and `_bank_ready` comes earlier.
- **footstep_contact_test:** `_exit_tree` waits on one task, adding about 0.6 s ×5 inside the 120 s limit.
- **touch_voice_echo:** none of her sounds write `caption`.
- **matron_figure_test:** 19 meshes (≥18 needed), all dressed.
- **No SCRIPT ERROR:** a missing clip plays nothing.
- **Left alone:** `pipe_holds` still knocks during a slide, because touch_matron_act counts those knocks.

## 6. Plan (slice 1, one sitting)

1. **`sound_bank.gd`:** the section 0 deletions, the primitives, and `build_her()` with its clips, copied from the prototype.
2. **`audio_director.gd`:**
   - The section 0 deletions, the `Matron` bus, and the players.
   - `_build_bank()` → `build_her()` → `_on_her_ready()`. The last prints `SCRAPERX_MATRON_BANK` and starts the loops paused, unless `_silent`.
   - `matron_cue` routes each name to its bank and pool.
   - Add `matron_layers()`, `matron_strain()` and `matron_dread(at, distance, stage)`.
3. **New `presentation/audio/matron_sound.gd`:** section 2, moved from `matron_view.gd` 1503-1621, as `update(data, delta, out_near, feet, head, player_position)` and `hear_hold(hold)`.
4. **`matron_view.gd`:**
   - Call `update` before the early return at 1469.
   - Change `bone_crack` to `matron_joints` at 1507, 1514, 1576, 1592 and 1595.
5. **`main_feel.gd` `_feel_gore`:** read the hold once, and pass it to gore and to `hear_hold`.
6. **`matron_look.gd` and `build_matron_body.py`:** the section 0 deletions.
7. **Run what it reaches:**
   - Headless at `--fixed-fps 60`: touch_matron, touch_matron_ride, touch_matron_line, touch_matron_climb, touch_matron_act, touch_matron_heave.
   - matron_figure_test and footstep_contact_test.
   - audio_mix and audio_fall under Movie Maker.
   - rule_check.
   - Push `<agent>/integrate-matron-sound` to run wo000, then listen on the Fold through its speaker and through headphones.

**Next (slice 2)**
- **Fatality beats** (`fatality_beat_edges`):
  - Pin: `matron_crunch`.
  - Legs: rip at ×0.8, with his `break_leg`.
  - Drag: `matron_drag_loop`. It and `matron_crunch` are prototyped, at +50 ms.
  - Kick: effort and `catch_bang`.
  - Wipe: a swallow, then "Waste not."
  - Black: 0.8 s of silence, then one breath.
- **Far sounds:** the tower creaking under her weight, and nails on the rail.
- **Body:** rebuild it without the `box` bone (`matron_view.gd` 534 and 1372, `matron_rig.gd` 112 and 192).