# Fall-voice mixer fixture repair — 2026-10-02

Source `1055609d84f0313e69c1dee6f3c2360c20fc8c8c`, [Actions run 37072892935](https://github.com/IssisX/ScraperX/actions/runs/37072892935), job `111056196548`, failed at “Measure the mix a player hears”. [CI failure](ci-failure.log) records `FALL_VOICE_FAIL decoded voice mix peak 1.019748`; no APK was produced. The launcher mixer passed 159 checks and the general ambience/footsteps mixer passed, both at Master peak −0.80 dB.

The unchanged standalone fixture created Effects without the production AudioDirector and captured that bus before Master. The authored +3 dB fear-voice gain relies on the shipping Master hard limiter. Random recording selection made this a pre-existing stochastic fixture failure: the fall fixture, Reactions source and five OGGs are identical to preceding green source `df3a483`. Its green CI peak 0.534986 matches the quieter `wtf_3` recording.

[Same-engine five-take comparison](diagnostic-clip-mix.log) decodes the actual Vorbis recordings at the authored gain through the production AudioDirector. This ARM64 Godot 4.7 Movie Maker probe exited 0. The first local llvmpipe launch crashed environmentally; the softpipe replay completed in eight seconds. The original complete failed fixture is captured from CI, not claimed as a local rerun.

| Recording | Effects peak before Master | Production Master peak |
| --- | --- | --- |
| wtf_1.ogg | 0.764173508 | 0.912013471 |
| wtf_2.ogg | 1.019747615 | 0.912015736 |
| wtf_3.ogg | 0.534985781 | 0.912011564 |
| fuck_1.ogg | 0.534938157 | 0.912011564 |
| fuck_2.ogg | 0.335314721 | 0.647615552 |

The loudest take reproduces the failed CI peak exactly; shipping Master remains at or below −0.80 dB. [The retained diagnostic script](diagnostic_clip_probe.gd) is a debug-only comparison, outside the shipping Godot project, not gameplay or a final acceptance gate. Its recording playback is an isolated decoding fixture rather than native falling-state proof.

The repair is confined to `godot/presentation/audio/fall_reactions_test.gd`: establish the real production buses/limiter, quiesce unrelated background sound, measure Master after the limiter and decode all five recordings deterministically. Preserve existing ordinary-jump, pause, repetition, recovery, native lethal-fall/death and canopy assertions, voice gain and clipping/audibility thresholds. No production audio, assets or native physics change is needed.

Affected local replay completed on Godot 4.7 ARM64 against the existing private native runtime; no native rebuild was needed because only the fixture changed:

- [Headless result excerpt](headless.log), exit 0: native reactions 2, failures 0; Dummy without Movie Maker intentionally skips decoded audio.
- [Actual Movie Maker replay](movie-maker.log), exit 0: all five recordings checked, native reactions 2, Master/recording peak 0.912016 (−0.80 dB), failures 0. Production AudioDirector setup and original reaction gain are used.

Reproduce against an imported Godot project containing the matching native library:

```sh
godot --headless --fixed-fps 60 --path godot --script res://presentation/audio/fall_reactions_test.gd
godot --fixed-fps 60 --path godot --write-movie /tmp/fall-voice-mix.avi --script res://presentation/audio/fall_reactions_test.gd
```

The local replay uses the established private ARM64 runtime via Ubuntu/proot and softpipe under Xvfb; its rendered blank frames make no gameplay-visual or performance claim. The retained loud-take pre-limiter diagnostic and failed CI fixture provide the red discriminator; the repaired all-five sweep includes that same take.

Repaired exact-source Actions/APK delivery remains pending. Start Here owns the eventual immutable APK receipt. Device audio and hardware vibration remain unverified.
