# Soundscape research: audio_system_map (workflow wf_5917c048-441, stopped for usage)

Short names: sb = godot/presentation/audio/sound_bank.gd, ad = godot/presentation/audio/audio_director.gd, mv = godot/presentation/life/matron_view.gd, mf = godot/presentation/main_feel.gd, vk = godot/presentation/uncanny/voice_keeper.gd, wf = .github/workflows/wo000-delivery-spine.yml.

1. Synthesis
- Format: 22050 Hz, 16-bit mono AudioStreamWAV (sb:21, sb:1284-1308). One-shots are normalized to a 0.9 peak with an 8 ms fade-out (sb:1288-1294).
- Helpers: noise (sb:1140), band noise (sb:1156, sb:1170), drifting resonance (sb:1191), sine (sb:1214), band-passed struck modes (sb:1235, sb:1254), decay (sb:1280), variants (sb:1122), crossfaded loops (sb:1312).
- One seeded RNG (sb:32). New clips go last so earlier clips keep their samples (sb:60, sb:111). Replacing _lullaby in place (sb:67) changes every clip built after it; no test checks samples.
- Built once per AudioDirector on the WorkerThreadPool (ad:276, ad:302-305); prints SCRAPERX_AUDIO_BANK when ready (ad:307-310). Earlier cues are counted but silent (ad:1196, ad:1208, ad:829).
- Measured here (/tmp/claude-0/-home-user-ScraperX/9ad8486c-c39f-50e0-b6e3-49506d0469f4/scratchpad/bank_probe.gd): 67 names, 125 streams, 5.24 MB, 2159 ms. The "~0.35 s" comment at ad:273 is stale.
- Memory: 44.1 kB per variant-second; hers ≈1.8 MB, mostly the lullaby (406 kB) and four wakes (600 kB).

2. Playback
- Buses: Effects, Ambience and Interface feed Master; Voice feeds Effects (ad:219-225). Master has a hard limiter, +6 dB in, -0.8 dB ceiling (ad:230-236). vk adds HerMic and HerEcho (vk:155-198). She plays on Effects; no bus of her own.
- Players: 6 non-positional (ad:237-241) and 5 positional, 8 m unit, 160 m reach (ad:242-243); round-robin, so play() restarts the oldest.
- Her loop is one 3D player with a 5 m unit size and 90 m reach (ad:259). It starts at -80 dB and is never stopped (ad:332-334). matron_music sets its position, volume and pitch (ad:828-833).
- One-shots: matron_cue counts, then plays (ad:1089-1091).
- Dread: matron_dread ducks Ambience 8 to 22 dB, deeper when nearer (distance/40 m), and plays matron_wake (ad:1098-1104). _update_dread holds 2.4 s, releases over 1.2 s (≤10 dB/s), and caps the loop at -60 dB (ad:1113-1125).
- By call order (not run), that cap is overwritten each frame: _audio.update (mf:943) precedes update_view (mf:971) → matron_music (mv:1502).
- Ducking: the bed drops 8 dB under the climber's voice (ad:596-599); underwater low-pass (ad:1372-1392).
- Captions: only the climber's lines set ad.caption (ad:685, main.gd:440); hers have none.
- Haptics, separate from audio: her pipe holds and slams (mf:1289-1302), the bay leaf (mf:1310-1316).

3. Her state driving audio
Her state comes from get_matron_state (src/bridge/scraperx_simulation.cpp:1646-1750), read once a frame (mf:970).
- Locker (mv:305-379): strain drives the knock and its rate; bolts play bolt_pop; the burst plays locker_burst plus dread. The burst's dread uses a distance of 20 m until she has been drawn once (mv:59).
- Planks: an unbolted plank plays plank_crack (mv:447-448).
- Figure (mv:1484-1620) runs only while she is released, alive, out and within 140 m (mv:1459, mv:1469-1471).
  - Music: volume -4 to -42 dB over 55 m; pitch 0.84 to 1 over 30 m. In the Turn or at stage ≥2, pitch jumps randomly 12 times a second; stage 1 runs ×1.08; still at her post, -80 dB.
  - Event counters play cues: vaults, knockdowns, staggers, shoves, throws, leaps, unrigs, climbs, pipe holds/flanges/slams, turn.
  - Repeating cues: climbing (a clang per rung), pipe sliding, on_line.
  - Steps follow stride, breath follows distance, dread follows first sight under 40 m, a shriek follows her fall.
  - Shared clips she also uses: clang, scrape, hit_heavy, bone_crack.
- Nothing silences the loop when she dies or is restored into the locker; it keeps its last level and position.
- The bay groan comes from get_bay_state (ad:1285-1286).
- get_matron_hold_state drives no audio, so the arm tear and fatality are silent. sever_arm counts arm_tear_count (src/sim/physics_world_arm.cpp:77-82), not arm_break_count, so arm_break (mf:347) never fires for her.

4. Tests and CI
- touch_matron_climb needs ≥3 "clang" cues while she climbs (ui_test_chase.gd:591, ui_test_chase.gd:601-603).
- touch_matron_act needs ≥8 "clang" cues on the pipe (ui_test_act.gd:885-889, ui_test_act.gd:928-929).
- touch_matron needs the door torn off and ≥3 bolts popped (ui_test_chase.gd:143-145).
- Counted headless: the count precedes the silence check (ad:1090).
- audio_mix (wf:394-413, ui_test_base.gd:467-502) needs ≥3 steps, the bed above 300 Hz at -40 dBFS or louder, steps 4 dB over it, and a peak at -0.5 dBFS or lower. It starts at grade (ui_test_driver.gd:228), out of her range.
- audio_fall (ui_test_base.gd:509-569) needs the voice 6 dB over Ambience and no clipping.
- Both need the bank ready within 150 frames (ui_test_base.gd:473, ui_test_base.gd:518, ui_test_base.gd:658-665).
- touch_voice_echo needs ad.caption empty during the echo (ui_test_uncanny.gd:364-369).
- footstep_contact_test waits for the bank five times inside 120 s (wf:443-451, ad:279-282).
- matron_figure_test needs every part dressed, and the music_box is one of them (tools/body/build_matron_body.py:1108-1112, matron_look.gd:169).
- A SCRIPT ERROR fails the step (wf:392, wf:413). No native test touches audio. A missing clip name plays nothing and raises no error (sb:116-120).
- Breakable: rung or pipe cues moved off "clang"; captions in ad.caption; her sound reaching grade or Ambience; a slower bank.

5. Budgets
- Startup: 2.16 s against the 150-frame wait; her clips cost about 0.8 s of it.
- Per frame: reuse her_state (mf:970); add no get_matron_state calls.
- Polyphony: her one-shots share the 5 positional voices with everything else. Her scrapes every 0.08-0.16 s (mv:1582, mv:1588) and a pipe slam's 3 cues (mv:1575-1577) cycle the pool in under a second, cutting off the 3.4 s wake.
- Microphone: a 6 dB jump in Master's output masks the microphone's room reading for 0.35 s (vk:82-83, vk:365-369). A pulsing soundscape could keep her deaf to the microphone.
