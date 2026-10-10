# Native finite wood Jump audio — 2026-10-09

Actual native ordinary-input path passed, exit 0. One initial stage on actual steel was followed by ordinary walking to loaded intact wood, Jump, 22 active Jump spam inputs, real takeoff, real quiet wood landing, early Drop cancellation, active checkpoint restart, ordinary backtracking to steel and an unchanged steel Jump. Each native sample was fed to production AudioDirector.update. No physics, force, velocity or native receipt was fabricated.

The wood Jump earned exactly one cue at native tick 874 after actual airborne motion with positive Vy 0.301515 m/s, foot start 1 and command work bound 256.666422 J. Peak Vy was 0.961395 m/s, below the old 2.5 m/s sound threshold. Repeated actual snapshots and active Jump spam did not duplicate the cue. Drop before takeoff and active checkpoint restart remained quiet. The steel Jump earned exactly one normal cue at tick 1496, Vy 5.390996 m/s, without an active wood foot constraint. Audible speaker output, full viewport/UI integration and device haptics remain unobserved under Dummy.

## Source distinction

The full native path used AudioDirector `f0d645ebc407a9b92011dfff9cc42e43a4b8d2a8e6c0fef243949779bc5f8b8d` and bridge `8c5f73ee23e77f24ab09d3842e3a95da7d1942b5425b2931649475b47671d20c`. Its exact source and manifest are archived separately.

The final small reviewed change clears pending wood receipts when the existing ordinary Jump cue fires. Final AudioDirector `302e5bc2f62addf58e7c80bf2a4ab74082ab3ec24e5bd920f4c7b2e00f640dec` passed focused synthetic receipt checks, exit 0: an ordinary Jump with a pending zero-load wood receipt emits once; later positive load on that receipt cannot emit again. Cancelled/invalid stop reasons 1–8 stay quiet and cannot replay on a later valid completion sample. These checks are explicitly synthetic and assert no new physical event. The native wood branch and normal steel criteria remained unchanged; the full native path was not rerun after this final duplicate-prevention change.

## Reproduction

Private imported runtime: `/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/wood-jump-feedback/runtime`. It currently contains the final source. `prepare_runtime.py` reuses independent imports/assets from the timber-feedback project and the attested bridge. For the exact historical native run, replace private `presentation/audio/audio_director.gd` with archived `native-audio-source-as-used.gd`, then run:

```sh
sh /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/wood-jump-feedback/run_jump.sh
```

For the focused final guard, install `final-audio-source-as-used.gd` and invoke `command-guard.txt`. Raw logs, receipts, both sources/manifests, helpers, commands and archive hashes are preserved. No engine, native binary or import cache is duplicated. No full route, foot-physics, fracture or visual campaign was rerun for this audio repair.
