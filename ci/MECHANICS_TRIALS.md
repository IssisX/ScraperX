# Local mechanics experiment limits

`mechanics_trial.py` enforces the existing execution budget for child processes.
Keep the state path in the live checkpoint and reuse it across sessions for the
same unresolved mechanism. Do not delete or rename state to reset a failed window.

```sh
python3 ci/mechanics_trial.py --state /absolute/evidence/window.json start --objective 'Complete the physical transfer' --budget 600
python3 ci/mechanics_trial.py --state /absolute/evidence/window.json run --stage primitive --source /absolute/actual-source.cpp -- /absolute/native-test selected-mode
python3 ci/mechanics_trial.py --state /absolute/evidence/window.json run --stage route --requires primitive --source /absolute/archive.a -- /absolute/route-test selected-mode
```

Failures propagate as exit codes. Two failures, elapsed budget, or an interrupted
pending execution block further trials. An identical command/source experiment
cannot repeat within the same window. A prerequisite must be the latest passing
result for its stage and its recorded files must still match. Timeout kills only
the trial's new process group. Detached external jobs remain outside this control.

Use `status` to inspect history. A causal reassessment requires `reassess
--evidence /absolute/actual-trace.log --cause 'supported cause' --change 'next
correction'`; it preserves prior trials and the evidence hash. This records an
engineering decision, not a safety approval or proof the decision is correct.

Accepted checks need no rerun to populate the tool. `reuse --stage primitive
--receipt /absolute/accepted.json` accepts a receipt containing `exit: 0` and a
nonempty `sources` map of absolute paths to SHA256 values that still match. The
receipt must describe actual execution evidence. A fabricated receipt remains
invalid regardless of whether a program can parse it.

The runner cannot control model judgment or a caller that deliberately bypasses
it; AGENTS requires its use. Exercise its failure boundaries with:

```sh
python3 -m unittest discover -s ci -p test_mechanics_trial.py
```

# Focused x86 diagnosis

`native-repair.yml` reacts to changes to the explicit request JSON or its runner
on `ChatGPT`. `run_native_repair.py` allows only the named owners, builds their
executables, runs their selected CTest groups, rejects an empty test selection,
and retains exact-source logs. This workflow never exports or certifies an APK.
Required full native, Godot and Android delivery gates remain unchanged.
