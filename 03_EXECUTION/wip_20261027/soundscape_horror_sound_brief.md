# Soundscape research: horror_sound_brief (workflow wf_5917c048-441, stopped for usage)

# The Matron's soundscape

She is a 220 kg woman, not a creature. Every sound she makes is her body, her mouth, or steel under her weight. All of it is synthesized in `sound_bank.gd` (22,050 Hz mono).

## 1. Principles

1. **A readout, never a fake.** Mr. X's steps through RE2's station; Alien: Isolation's alien really is in the vent you hear; Dead Space's per-enemy "fear emitters" add layers as you get closer.
2. **Silence, then violence.** When she listens, she and the world go quiet, and the next sound is the attack.
3. **The human made wrong.** The Last of Us clickers are actors making clicks in the throat. Ju-On's croak and Hereditary's cluck are each a single human noise. People hate slurping, swallowing and smacking most (misophonia research). These are hers, dry and in your ear.
4. **Weight and flesh.** Outlast's Walker and Marta and RE7's Bakers are people heard as mass and grunts; The Thing gives wet tearing. On a phone, weight comes from harmonics: formants ×0.85 sound like a bigger body, thumps get synthesized overtones, and sub-bass is a headphone bonus.
5. **Violence is tearing, cracking, and his scream** (the climber's `break_arm`/`break_leg` voice). Her own sound is effort and relish, never pain.
6. **Measured triggers.** Loudness flutter at 30–150 Hz makes screams alarming (Arnal 2015). Energy at 2.5–5.5 kHz flickering at 1–16 Hz makes chalkboard sounds unbearable (Kumar 2008), and that is the phone's best band. Subharmonics, chaos and pitch breaks are horror's grammar (Blumstein 2010).
7. **Dissonance, never melody.** Use beating pairs 3–9 Hz apart, tritones and inharmonic steel. No note moves to another, and nothing is pretty.

## 2. Palette

New helpers:
- **GLOT**(f0, jitter, shimmer, sub, tilt): glottal pulses; every 2nd ×sub (period doubling = growl); one-pole low-pass at tilt (500 Hz breathy, 2 kHz pressed); *chaos*: f0 jumps ±30% for 40–80 ms.
- **FMT**(F1, F2, F3): parallel `_resonate`, Q 5/6/8, gains 1/0.6/0.35. **BIG** = FMT(650, 1,190, 2,360), a woman's /ʌ/ ×0.85; **OPEN** = FMT(810, 1,160, 2,670).
- **SAT**(d): tanh(d·x)/tanh(d) + 0.15x². **ROUGH**(r, depth): amplitude modulation. **GLIDE**(a→b): swept band-pass.
- **GRAINS**(rate, ms, band): Poisson noise bursts, random levels. **BUB**: sine 500–1,500 Hz rising 30% in 15–40 ms, τ 10 ms. **SLIP**(rate): jittered stick-slip impulses into a resonance.
- **VBASS**: lows under 150 Hz through SAT(8), band-passed 150–500 Hz, added back.

One-shots get 3–4 takes; loops crossfade 0.3 s; ≥70% of every clip's energy in 0.3–6 kHz.

**Far**
1. **Footfall** `matron_step` (0.4 s): 5 ms slap (noise 0.7–5 kHz); VBASS thump 60 Hz, τ 70 ms; deck modes 190/430/720 Hz, Q 3, τ 50–90 ms (boom, never bell); flesh settling: soft slaps at +80/+150 ms, ROUGH(11 Hz); 1 in 3, sole peeling off steel (GLIDE 900→2,000 Hz, 25 ms). Hunting: palm slaps between. Far twin `matron_load` (0.8 s): SLIP(9–14 Hz) into plate modes, two bolt ticks, VBASS: the tower taking her weight.
2. **Pressure bed** `matron_bed_loop` (4.8 s, 0.6 s crossfade): saws at 146.8 and 151.1 Hz (4.3 Hz beat) and 207.6 Hz (tritone), SAT(3), band-pass 0.25–1.8 kHz; noise 0.3–1.2 kHz swelling at 0.4 Hz like breath. A rough twin adds ROUGH(38 Hz, 0.6).

**Near**
3. **Wet breath** `matron_breath` (1.4–2 s; replaces the dry wheeze). In: noise 0.5–3.5 kHz, GLIDE 900→1,400 Hz, ROUGH(32 Hz, 0.4): a snore sucked in. Out: noise 0.35–2.2 kHz plus GLOT(95 Hz) ×0.15 into BIG. Mucus GRAINS(40/s, 2–5 kHz); two catches (×0.2, 25 ms).
4. **Growl** `matron_growl_loop` (2.4 s, 0.4 s fade) and one-shots: GLOT(105 Hz ±8% drift, jitter 6%, shimmer 25%, sub 0.5, tilt 1 kHz) → BIG → SAT(4), aspiration 1–4 kHz gated per pulse. Rage set: SAT(8), sub 0.7, chaos 2/s. [0.25–4 kHz; 105/52 Hz implied]
5. **Croak** `matron_croak` (0.8–2.5 s): GLOT at 18–45 Hz, jitter 25%, each pulse distinct, into FMT(450, 1,100, 2,600) at Q 6–9; two swells; a wet click. [0.35–3 kHz]
6. **Mouth** `matron_mouth_loop` (2 s): GRAINS(20–70/s, 0.3–1.5 ms, 2–6 kHz), saliva strings (click pairs 3–8 ms apart). One-shots: cluck (1 ms impulse into 2.2 kHz Q 5, τ 8 ms, then 3.5 kHz tick); swallow (0.6 s: smack GLIDE 1,800→700 Hz in 20 ms, 3–6 BUB, GLOT 110 Hz gulp through GLIDE 450→220 Hz, throat click); sniff (3–5 pulls of 60–120 ms, noise into 2.8 kHz Q 3, ROUGH(70 Hz), wet snort).
7. **Joints** `matron_joints` (0.3–0.9 s; replaces `bone_crack`): 3–8 cavitation pops, gaps 160→40 ms: 0.7 ms impulse, 3 kHz Q 8 τ 4 ms, knock 250–450 Hz τ 20 ms; gristle SLIP(30 Hz, 0.6–1.8 kHz) between.
8. **Nails on the rail** `matron_rail_loop` (1.6 s): SLIP(6–14 Hz) exciting a squeal at 2.8–4.5 kHz (Q 12, wandering ±5%) over rail modes 0.7/1.3/2.1 kHz. That is Kumar's band at Kumar's rate. Plays while she hunts along a railing.
9. **Body drag** `matron_drag_loop` (2 s): friction noise 0.25–2 kHz under SLIP(3–8 Hz); skin squeaks (GLIDE 900→1,800 Hz, 40 ms); seam bumps (VBASS) every 0.7 s; cloth rasp 2–5 kHz.

**Contact**
10. **Shriek** `matron_shriek` (0.9–1.6 s; replaces the clean sine): 120 ms gasp; GLOT(380→950→700 Hz, tilt 2 kHz) with period doubling and a chaos burst midway, ROUGH(60–110 Hz, 0.6), OPEN with F1 lifted to f0 where f0 passes it, SAT(6), breath 2–6 kHz; 30 ms attack, two voice breaks late. [0.4–6 kHz]
11. **Effort** `matron_effort` (0.8–1.6 s): pressed GLOT(115 Hz, shimmer 35%, tilt 1.5 kHz) into a narrow FMT(400, 1,000, 2,400), rising 0.4–1 s with a 7 Hz tremor, bursting into OPEN at SAT(6) for 0.3 s. Plays on every pull and on the Kick. In the locker: low-pass 1.2 kHz, box modes 380/760/1,140 Hz, sheet "oil-can" pops. The burst releases her first open shriek.
12. **Arm tear** (sim-driven). Creak: SLIP 20→60 Hz rising with `wrist_strain_ns`/600, 0.5–2.5 kHz. On `arm_tear_count`:
    - shoulder pop (impulse, 250 Hz Q 4 τ 40 ms, VBASS);
    - rip: GRAINS 40→600/s over 0.6 s (1–4 ms, 0.8–3.5 kHz) with fiber pops (20/s, +6 dB), squelch (GLIDE 1,500→400 Hz, 0.25 s), 15–25 BUB;
    - green-stick snap (impulse, 40 splinter grains in 60 ms, 1.2–3 kHz);
    - suction pop (GLIDE 300→1,200 Hz, 40 ms);
    - 10–20 drips on steel.

    His `break_arm` scream goes on top, then her wet laugh (GLOT 140 Hz bursts, 6/s, on an exhale). When the arm is at her face: sniff, smack. Full level, dry.
13. **Fatality beats.**
    - Pin: VBASS thump, 3–5 rib cracks in 0.25 s, his air forced out.
    - Leg0/Leg1: the tear at pitch 0.8, 1 s rip, hip pop 180 Hz, creak from `leg_strain_ns`/1,200, his `break_leg`.
    - Lay, Drag: drag and breath.
    - Kick: effort and `catch_bang`.
    - Haul: her laugh under the rope.
    - Fall: VBASS, squelch, spatter.
    - Wipe: wet cloth rub, smack, swallow, then "Waste not." low, close and wet (generated like his voice).
    - Black: 0.8 s of nothing, then one wet breath.
14. **Hit** `matron_hit` (0.7 s): a winded "HUH" (noise 0.4–3 kHz, GLOT 140→100 Hz, 150 ms), flesh slap, ROUGH(10 Hz) wobble; 0.3 s later a SAT(8) snarl. A blow makes her louder, never hurt.

## 3. The system

**Rings** (with a deck between you and her: −10 dB, low-pass 1.5 kHz):
- Far, 15–55 m: `matron_load`, nails on rail, the bed (non-positional, −30→−12 dB as she nears: a fear emitter, not a beacon).
- Near, 3–15 m: steps, breath, growl (Dangerous stage and up), sniff and croak when searching, joints.
- Close, under 3 m: mouth, swallows, breath dry, +4 dB at 2–5 kHz, no reverb, so it sits in your ear. Contact sounds play at full level.

**3D.**
- Mono `AudioStreamPlayer3D`s sit at her mouth (`head_world_position`), feet and hands, not the hip.
- Set `attenuation_filter_cutoff_hz` to 3,000 and `attenuation_filter_db` to −18. On a phone, distance is heard as dulled highs and reverb.
- Set `area_mask` = 1 so Area3D reverb reaches them (4.7 defaults it to 0).

**Heat** = sim `rage`, +0.3 while hunting, 1.0 in the Turn. Heat drives:
- breath every 2.6→0.7 s;
- growl −24→−4 dB above 0.3;
- a `MatronVoice` bus whose `AudioEffectDistortion` (atan) drive goes 0.1→0.55, +3 dB at 3 kHz;
- the bed fading to its rough twin.

At the Changed stage, every sighting shrieks. Compressors on Ambience and Effects are keyed (`sidechain`) to `MatronVoice` at 4:1, 5/400 ms, so her shriek carves the world out.

**Silence.**
- Peering or listening: her loops are gone in 0.25 s and the existing dread duck applies, leaving only the climber's breath. One time in 3, a single croak or click comes from the dark.
- Turn windup (0.9 s): 0.4 s of nothing, then the gasp and her neck cracking.
- The Lunge: shriek and snapping steps.
- At her post: only breath and swallows, within 6 m.

**Never.**
- Delete `_lullaby`, `matron_lullaby_loop`, `matron_music` and the pitch warp in `matron_view.gd`.
- No tune, scale, consonant chord, bell, chime, choir, child, whistle, humming (it reads as a tune) or musical stinger.
- No animal or demon roar.
- Her voice is never shifted past ±15%.
- No whimper from her.

**Budget.** ≤ 7 of her voices at once; ≤ 30 s of new audio (phone build time).

Sources: [Kumar](https://eprints.ncl.ac.uk/150198), [Arnal](https://pmc.ncbi.nlm.nih.gov/articles/PMC4562283), [Blumstein](https://eprints.kingston.ac.uk/id/eprint/17660), [clickers](https://naughtydog.com/blog/the_last_of_us_part_i_hbo_tv_show_clickers), [fear-emitters](https://designingsound.org/2009/06/all-about-the-sound-of-dead-space/), [Alien-Isolation](https://pcgamer.com/the-audio-of-alien-isolation).

Files: /home/user/ScraperX/godot/presentation/audio/sound_bank.gd, /home/user/ScraperX/godot/presentation/audio/audio_director.gd, /home/user/ScraperX/godot/presentation/life/matron_view.gd.
