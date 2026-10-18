# Loaded-climb clothing feedback

Base: `b800335b0df40bbfa91f23c1e1f7897fb495967f`.

Real hand placements retain their existing positional taps. A short, quieter clothing/glove pull now accompanies native positive hand work while real hands are loaded. The model reads the existing cumulative support-relative actuator work and force receipts; it changes no physical limits, input, support, stamina or movement.

The audio director uses the native ninety-hertz tick, independent of rendering rate. One shared Main landing-state/tick sample drives placement eligibility and effort. The existing bounded effect pool plays three seeded, non-voiced 240ms cloth variants. No new audio loop, voice pool or per-frame player allocation is added.

Presentation-only tuning is in `godot/presentation/climb_effort_feedback.gd`: 60J of fresh active work per cue, minimum 80W and 200N observed load, at least 50 native ticks between cues. Gaps over 45 ticks establish a quiet baseline. Cue strength affects only a restrained -21 to -14dB mix level. These are sound-design choices, not physiological or physics limits.

Steady hanging, passive support travel and repeated render samples create no cue. Release, missing/invalid receipts, death, rewind, restart, mute and shutdown consume or clear old work without replaying it. Hand placement audio keeps its own actual grip generations.

[Exact commands, hashes, outputs and scope](climb-effort-window.json) record the Godot4.7 affected-presentation run: `climb_effort_test.gd` prints `CLIMB_EFFORT PASS failures=0` without script errors; unchanged `footstep_contact_test.gd` passes all five carried/walking/rotating/static/airborne cases. The first test teardown tried to free an audio node during its asynchronous sound-bank task; the test now joins that owned task before freeing it. No substantive assertion was removed.

These checks exercise the production model, audio-director call and clip selection with synthetic native receipts. They do not exercise Jolt or establish what a phone speaker sounds like. The matching candidate workflow imports the shipping project and runs the same affected audio check; native movement evidence remains owned by the corresponding movement receipts. Phone listening remains unobserved.
