# Ground swing bearing sound

Base source: `36a4abf057fae373c0316d550dc2cd104386716f`.

Nearby players now receive sparse steel-bearing creaks from each moving ground-bay bar. Native COM velocity supplies motion; the native body transform supplies its bearing position. The existing creak variations and five-voice positional pool own playback. Moving faster makes the cue louder; holding a stationary bar cannot manufacture a sound. No native physical state, forces or control law changes.

The observer samples native ninety-hertz ticks, consumes repeated render frames and resets on death, rewind, restart or a long gap. Muting consumes motion rather than saving a burst for unmute. The observer avoids native body queries outside twenty-eight metres and caches the two body indices inside the bay. Shutdown disables new cues.

Presentation hypotheses are declared in `swing_bearing_feedback.gd`: at least0.12m/s actual COM speed,0.4m sampled travel per cue and54native ticks between each bearing's cues. They control sound cadence only. Source identity comes from the real route's2480/2481 bodies. Mute and master/effects volume remain under the existing audio buses.

[swing-bearing-window.json](swing-bearing-window.json) records the exact executed commands, source hashes and outputs. Godot4.7 affected-project import exited0; `--script res://tests/swing_bearing_test.gd` exited0 with `SWING_BEARING PASS failures=0`. The test exercises production director routing, native-sample cadence, repeated renders, idle/distant/missing/invalid observations, mute/unmute, restart/death/rewind and shutdown without adding audio voices.

These are supplied-observation presentation checks under Dummy audio, not a heard mix or another physics test. The accepted36a4abf physical route is unchanged and its native evidence is reused. Matching candidate CI imports the whole shipping Main/native project, runs the new focused sound check and exports Android. Phone speaker quality and play feel remain unobserved.
