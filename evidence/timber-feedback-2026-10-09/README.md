# Native timber fracture feedback — 2026-10-09

Final private Godot 4.7 check passed, exit 0. Actual native gravity/contact supplied the existing staged 3 m fall; no manual force, velocity write, fracture hook or proxy physics was used. At native tick 432, three broken seams (`mask=112`, `serial=3`) produced one observed cue. Normal and Effects-muted independent native worlds produced the same actual receipt. The cue position `(-6.809073, 406.865448, -128.199997)` matched the mean of current native adjacent material endpoints within 20 μm, with authored strength 0.85.

Real own-weight elastic simulation, repeated damaged snapshots, startup into actual damaged state, explicit supported native restart and subsequent unmute produced no additional cue. Muted cues are intentionally counted as consumed receipts; they are never replayed. `feedback-receipt.json` distinguishes the two native audio receipts from three additional source-level pool diagnostics (five total counter increments).

The real production Main consumer was instantiated without adding Main to the tree, with the actual native object injected; its full-world `_ready`, input, camera and rendering paths did not run. The harness calls the exact production consumer and then the production AudioDirector method, matching Main's cue dispatch. It does not prove full gameplay presentation integration, audible output, phone mix or device haptics.

Two rejecting source-level checks are explicitly separate from native fracture evidence. Synthetic receipt sequencing tests rollback to a lower tick, a later new fracture receipt on that timeline, the nine-tick cascade cooldown and no delayed replay of consumed cascades. No synthetic receipt is described as native damage. Actual existing AudioStreamPlayer3D pool controls test Effects/Master mute, unmute, valid unmuted dispatch on Effects and explicit restart stop. For these controls only, the harness disables AudioDirector's Dummy early return so the real player branch can be inspected; it then stops all streams and clears references. The Dummy driver does not mix, and no speaker output is claimed.

## Tested identities

- Main: `f959472ace1e74be69b2fb030d12d53bf320236935a4b7e10c1ec23a70e91c1e`.
- AudioDirector: `b7566c13882ed36fee9426c3078642f8c341c1db1fce94fd161a5b8a59ca5dd8`.
- Recorded 1.1 s PCM snap: `766f2f61c1a8cfd3b15d880e81bd29a6b62dd9e2ebca32ffd47ef5bbe3367f57`.
- Actual native bridge: `8c5f73ee23e77f24ab09d3842e3a95da7d1942b5425b2931649475b47671d20c`.

Production Main/AudioDirector and the audio asset parsed/imported successfully in the private project. The initial harness had an inferred-Variant parse error; `preparation-first-failure.log` preserves it, and the explicitly typed local corrected the test helper. `pre-lifecycle-fix/` preserves the earlier successful native-only checks before the reviewed cooldown/mute repair; it is not the final tested source. Final raw log has no script/resource errors or leak diagnostics. The unrelated sound bank built 28 clips in 1162 ms under Dummy; this is not a gameplay or phone performance measurement.

## Reproduction

Existing private imported runtime command:

```sh
sh /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/timber-feedback/run_feedback.sh
```

`prepare_runtime.py` copies the production presentation/assets and attested existing visual bridge into a private project with independent imports. `feedback_probe.gd`, exact runner, import command/log, source manifest, final receipt and source-as-used are archived here. To reproduce the historical source after later edits, replace the prepared Main/AudioDirector with `source-as-used/` and verify the bridge/audio hashes before running. Engine, binaries and import caches are intentionally not duplicated into evidence. `sha256.json` covers archived files.
