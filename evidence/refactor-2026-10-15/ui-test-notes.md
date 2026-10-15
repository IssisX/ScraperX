# UI scenario modularization — 2026-10-15

Baseline: `ChatGPT` at `dc53244d99a833888f1004af1b74a392887b34d5`.
Owned scope: `godot/presentation/ui/ui_test_driver.gd`, new files in
`godot/presentation/ui/test_scenarios/`, and this note. Concurrent source and
untracked evidence were preserved. Project declares Godot 4.7.

## Result and API

The driver shrank from 5,564 to 297 lines. Its exact `begin(Node, String,
String) -> bool` interface, `SCENARIOS` values, fixture selection, deferred
`_run`, touch-device initialization, PASS/FAIL text and exit codes 0/31 remain.
Nine functional suites contain the extracted scenarios; there is one shared
345-line `Node` harness, inherited by the driver. Suites are `RefCounted`
objects holding a typed reference to that same harness, with no copied state.
The driver alone owns suite creation and process completion.

The harness owns native/context queries, real viewport event injection,
frame/time waits, device movement/walking, wrist sampling, capture and failure
reporting. Its four state fields remain `_main`, `_scenario`, `_capture_prefix`
and `_detail`. Existing render subclasses still inherit those fields/helpers;
suite `h._pose()` invokes their override on the same driver object. The
standalone `_single_hand_controls_honest(String) -> bool` driver entry point is
retained because `second_hand_reach_bridge_test.gd` calls it without `begin()`.

All suite entry points retain their original method names, signatures and
awaits. Cross-suite dependencies are explicit typed constructor arguments and
direct method calls. There is no generic dispatcher, forwarding via `call` or
`get`, copied state, suite registry, or cyclic preload dependency.

## Module map

| Module | Lines | Responsibility / typed dependencies |
| --- | ---: | --- |
| `audio_scenarios.gd` | 100 | Master-mix capture and ambience/footstep measurements; harness only |
| `campaign_scenarios.gd` | 528 | Connected ascent, service/balance lifts, tram, helix, mast, pitman; crown, frames, ladder |
| `controls_scenarios.gd` | 948 | Ground foundation, touch/pad/keyboard regressions, rigging; harness only |
| `crown_scenarios.gd` | 720 | Gondola, swing, hand catches, hanging posture and recovery; harness only |
| `frames_scenarios.gd` | 484 | West-brace and north-service frame traversal; opening |
| `haul_scenarios.gd` | 808 | Slab cart, taper inspection, gravity reclaim/retry; harness only |
| `ladder_scenarios.gd` | 268 | Suspended ladder and cargo net; crown |
| `launch_scenarios.gd` | 397 | Slingshot boarding, launch, rail aim, roof landing; harness only |
| `opening_scenarios.gd` | 825 | Pipe bridge, facade, stair, ballast, teeter and braced bay; ladder |
| `ui_test_harness.gd` | 345 | Shared input/timing/native/capture operations; no suite dependencies |

Construction follows dependency order: crown → ladder → opening → frames →
campaign. Independent launch, haul, controls and audio use only the harness.
The small native `_teeter_angle` query lives in the harness because both
opening and ladder scenarios use it.

## Checks actually run

- Static extraction comparison: **PASS 142/142 original method bodies**.
  Compare baseline against the assigned destination after removing only added
  `h.` / typed-suite owner prefixes and `begin`'s `_create_suites()` line;
  preserve all remaining code and string tokens. Comments/blank lines are
  excluded because adjacent method comments moved with their method.
- Exact original `SCENARIOS` dictionary text: **PASS**. Match-label coverage:
  **66/66 scenarios**, including aliases, with no additions or removals.
  `_run` body comparison separately preserves each method/argument mapping.
- Every shared harness and typed suite method reference resolves: **PASS**.
- Every preload/extends target exists and dependency graph is acyclic: **PASS**.
- Direct second-hand assertion compatibility entry point: **PASS (source)**.
- `git diff --check`: **PASS**.

Comparison command used on this host:
`python /data/data/com.termux/files/usr/tmp/check_ui_suites.py`.
It compares the captured baseline from the same worktree before editing;
baseline can also be recovered with `git show dc53244:godot/presentation/ui/ui_test_driver.gd`.

No Godot import, runtime, renderer, native build, APK or device check was run by
this worker. Root owns the shared combined Godot checks after all source is
stable. Static equivalence is source evidence, not runtime proof. No assertions,
input sequences, timeout values, audio thresholds, fixture staging, evidence
paths, or scenario expectations were changed.
