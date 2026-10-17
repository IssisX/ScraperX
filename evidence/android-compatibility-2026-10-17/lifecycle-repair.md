# Android focus and early configuration repair

Base: `87464b53d8fc72a7d8ceccfa88fd80de9159036f`, branch `ChatGPT`.
This change preserves unrelated dirty work and all native physics.

## Observed failures and repairs

- Controlled run `39053295105` changed Android font scale from 1.0 to 1.25
  immediately after first launch. Android reported activity recreation with
  configuration mask `0x40000000`; stock engine teardown prevented startup.
  The final ChatGPT manifest now adds only that `fontScale` bit to the pinned
  Godot activity. The signing verifier rejects other manifest/payload changes.
  No unsupported explanation is assigned to the earlier spontaneous failure.
- Tablet run `39053372849` showed the actual expanded notification shade
  while the native observer remained unpaused and ticks advanced. Main now
  checks Android's real activity window focus before input/native advance,
  routing loss through its existing pause and held-input cleanup. Returning
  focus does not resume the game. The original tablet acceptance is unchanged.
- Small-drag and foreign-emulator-dialog evidence is recorded in
  [resize-input-repair.md](resize-input-repair.md). These fixes are integrated
  without larger synthetic gestures or weaker movement requirements.

The existing foldable matrix now runs its full original compatibility driver
through `ci/android_startup_replay.py --startup-font-scale 1.25`, retaining
real fold/unfold, controls, persistence, same-instance and cleanup checks.
The other three profiles retain their original routes.

## Evidence and limits

Executed in this checkout:

```sh
python3 -m unittest discover -s ci -p test_android_manifest_policy.py -v
python3 -m unittest discover -s ci -p test_build_info.py -v
```

Observed: 4 manifest-policy and 9 existing build-identity tests OK. They cover
exact allowed byte changes, UTF-8/UTF-16 pools, ambiguous/wrong identities,
unchanged default behavior and final-manifest tampering rejection.

Godot 4.7 Linux ARM64 import and the existing `--uitest=touch_pause` regression
both exited 0; the latter printed `SCRAPERX_UITEST PASS touch_pause`.
Exact commands/results and logs are in
`/data/data/com.termux/files/usr/tmp/scraperx-android-compatibility-20261017/android-repair-host/result.json`.
That private host project uses published presentation plus these focus/touch
edits, the existing cached Linux bridge and private Linux ARM64 extension
aliases. The first import exposed missing host aliases; it was environmental,
and no shipping platform configuration was changed to resolve it.
This is script/pause regression evidence, not execution of Android APIs or
proof of the new native policy on that cached bridge.

UNVERIFIED: edited-source Android startup, tiny touch delivery and shade pause.
The representative API24/35/36 workflow must exercise those real paths.
Emulator success cannot establish physical-phone frame rate or a hardware floor.

## Primary API evidence

- [Godot 4.7 AndroidRuntime](https://docs.godotengine.org/en/4.7/tutorials/platform/android/javaclasswrapper_and_androidruntimeplugin.html): `getActivity()` exposes the current activity.
- [Android 16 Activity.java](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-16.0.0_r1/core/java/android/app/Activity.java#L4264): `hasWindowFocus()` reads the main decor view's window focus, distinct from activity resume state.
- [Android configuration changes](https://developer.android.com/guide/topics/resources/runtime-changes): a declared handled configuration change avoids activity replacement and leaves resource handling with the app. Current game UI is viewport based; future native text widgets need their own font-resource handling.

## First edited-source Android results and harness correction

Run `39059867482`, game source `0d6e4cbcd3bb4eb8e9da5654b3a42fec7ff872da`:
compact API24 passed 27 original checks. Foldable API35 reached real native
Main after font scale 1.0 to 1.25 (recorded readback 1.25), then the new focus
harness failed before controls. Its `window-frames.txt` contains the game
window and `mTopFocusedDisplayId=0` but no `mCurrentFocus` field. Logcat contains
no script error; the lifecycle capture contains no game activity relaunch.
Tall API36 reported the same missing-field harness failure. These runs do
not establish the remaining Android controls or shade acceptance.

This failure was introduced by querying `dumpsys window -a windows` for a
field modern Android prints under display content. The query now requests
the full `dumpsys window -a` dump. Focus parsing selects the actual top display,
supports the older global field and refuses absent/ambiguous focus. A visible
game window alone still cannot establish input focus. Timeout captures the
last complete dump. Existing dialog recovery and deadlines remain intact.

Executed after this correction:

```sh
PYTHONDONTWRITEBYTECODE=1 python -m unittest discover -s ci -p test_android_compatibility.py -v
```

Observed: `Ran 16 tests ... OK`. Added cases cover different focus on two
displays, missing top display/field, ambiguous focus and legacy/single-display
formats. The command expectation changed because the inspected Android dump
scope was wrong, not to bypass input focus. Full corrected-source Android
execution remains required.

Primary format owner:
[Android 15 DisplayContent.dump](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-15.0.0_r1/services/core/java/com/android/server/wm/DisplayContent.java#L3355).

## Corrected focus run and remaining startup/input repairs

Run `39060680132`, source `ab94f4415b72d798d8da28fafe7eee17d8959550`:

- Compact API24 passed 27 checks.
- Foldable API35 passed 33 checks, including the controlled early font-size
  change and real fold/unfold/lifecycle path. The previous missing-field
  harness failure is resolved for this run.
- Tablet API36's screenshot shows Android's actual “Viewing full screen”
  tutorial over the game's Pause menu. Focus is truthfully reported as
  `ImmersiveModeConfirmation` despite the recorded prelaunch setting readback.
  The harness now captures the UI hierarchy, finds the unique enabled native
  system confirmation button, rechecks the focused window and performs an
  ordinary tap. It must then observe game focus and fresh native identity;
  normal Resume remains necessary. Other dialogs and actual crashes retain
  their previous failure/recovery rules.
- Tall API36 passed 64 checks before missing pulse15's MOVE. Pulses14/15
  repeat `(65,385)` to `(65,378)`. Pinned Godot forwards a sub-scroll-slop
  MOVE but retains its exact endpoint cache across UP, then suppresses the
  next identical single-MOVE endpoint. The bounded ordinary interpolation
  correction and endpoint rejection tests are described in the input note.

Root command after the tutorial repair:

```sh
PYTHONDONTWRITEBYTECODE=1 python -m unittest discover -s ci -p test_android_compatibility.py -v
```

Observed: `Ran 20 tests ... OK`. System identity, button bounds, duplicate or
disabled/invalid targets and focus changes before tapping are covered.
The source-edited tablet acknowledgment and final corrected four-device run
remain unverified until actual Android execution.

The ordinary profile collector initially exited before its watcher's first
snapshot existed. Its process was confirmed absent, then the reader was
corrected to wait for the first complete JSON snapshot and resumed. No
GitHub job was restarted due to that observation failure. Terminal profile
logs and prior failure output remain preserved under the existing Q directory.

Native tutorial button owner:
[Android 16 ImmersiveModeConfirmation](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-16.0.0_r1/packages/SystemUI/src/com/android/systemui/statusbar/ImmersiveModeConfirmation.java#L430).

## Separate tablet startup failure: capture the actual recreation reason

Source `586711289839f71d7dc421c551f2de3874db58cd`, run `39062072573`:
foldable API35 again passed all33 checks. Tablet API36 produced no Main
observations, so the fullscreen tutorial acknowledgment was never reached.
The failure screenshot shows Android Home. Logcat records game process2720
loading Godot, then receiving updated navigation and dynamic-theme resource
paths. Godot receives Pause, Stop and Destroy before startup succeeds; the
recreated activity reuses the initialized engine and Godot then kills its own
process with SIG9. This is not evidence of an out-of-memory kill or graphics
overload. The separate Google Play Services ANR does not establish causation.

The ordinary logcat capture lacks the activity-relaunch configuration mask.
The tablet matrix now uses the existing startup replay wrapper with
`--startup-font-scale none`. It records continuous Android lifecycle events
and before/after configuration around the unchanged full tablet driver.
No font input, failed-launch retry, deadline extension or new activity
configuration flag is added. A resource-path change is observed; its precise
role in this recreation remains unverified until the OS event records it.

Source owner for the observed application-info message:
[Android 16 ActivityThread.handleApplicationInfoChanged](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-16.0.0_r1/core/java/android/app/ActivityThread.java#L6966).

## Resume failure: separate input delivery from button activation

Tablet run39063290401/source3d031c4 reached Main and passed27 checks. Its
notification shade paused native tick750 and kept it frozen; after collapse,
Android recorded focus entering the game at19:47:36.990UTC. The driver observed
the visible pause menu at19:47:38.661 and tapped its measured Resume center
(138,144). Native stepping never resumed. Focus loss is not established as the
cause, and no focus-policy change is justified by this capture.

The same log has456 input-pool capacity warnings, all at observation sequence262.
Those warnings clear before successful controls and earlier Resume checks at
sequences296/301. The failing shade interaction starts at312 and still has no
new game pointer event at320 onward. The warnings alone do not explain this
later missing tap; gyro aim and its sensor remain intact.

The emulator-only read-only observer now reports the last raw pointer event,
pause-button down/up/pressed counters and the activity's actual window-focus
readback. Button listeners only record signals; they never activate a button,
resume the tree or change native state. This observation must distinguish a
missing platform event, failed GUI activation or immediate re-pause before a
repair is selected. Shipping phone exports do not install this observer.
