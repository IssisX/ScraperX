# Android input repair — 2026-10-17

Source checkout: `IssisX/ScraperX`, branch `ChatGPT`, worktree
`/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo`.
Inspected base: `87464b53d8fc72a7d8ceccfa88fd80de9159036f`.
All existing dirty and untracked work was preserved. No commit, push, Android
emulator run, native change, or gameplay acceptance change was performed here.

## Captured failures and supported causes

Run `39053372849`, APK source
`5bd7ccd77cea010d13fa971d5926929e94ecaa72`, artifacts under
`/data/data/com.termux/files/usr/tmp/scraperx-android-compatibility-20261017/emulator-5bd7ccd/artifacts/`:

- **Compact API24:** `failure.png` visibly shows “Messenger has stopped” over
  the running game. `window-frames.txt` identifies current focus as
  `Window{9fa8e4 u0 Application Error: com.google.android.apps.messaging}`.
  The shell tap targets the correct visible crouch point `(1812,790)`, but
  actual game input history remains empty through sequence196/tick795.
  This is a bundled emulator app crash stealing input, not evidence that a
  short crouch tap or the canvas conversion is broken.
- **Tall API36:** the fifth freight pulse sends ordinary DOWN `(65,385)` and
  MOVE `(61,365)`. Actual DOWN is recorded at canvas `(268.04,1587.91)`,
  consistent with the reported `800x450` OS window and `3299x1856` canvas.
  Ownership becomes true, but event count stays17 and drag count stays4
  until finally-UP; world movement remains zero. Prior fourth MOVE
  `(55,365)` was delivered and produced real native movement. Their distances
  are20.40px and22.36px. Captured Android display density is2.625. The failure
  boundary fits Android's default8dp scroll threshold (21px at that density).
  The precise device ViewConfiguration threshold was not separately queried;
  the absent MOVE and correct coordinate conversion are directly observed.
- **Tablet API36:** `failure.png` visibly shows the expanded notification
  shade. The observer stays unpaused and advances native ticks through858.
  This is a separate lifecycle-owner problem assigned to root. The shade
  acceptance remains unchanged.
- **Foldable API35:** the captured run passed its existing route. This is
  baseline evidence, not a pass for the edited source.

The previous stale-geometry hypothesis is rejected for the captured tall
failure: actual DOWN coordinates match the current small window. No coordinate
rescaling rewrite or larger synthetic drag was added.

## Changes

`godot/project.godot` enables
`input_devices/pointing/android/disable_scroll_deadzone=true` in production.
This delivers small analog touch movements to the existing game input owner;
the game's own stick deadzone, native physics and real CI gestures remain intact.

The exact setting and startup wiring were checked against pinned Godot4.7
primary sources:

- [ProjectSettings.xml](https://github.com/godotengine/godot/blob/4.7-stable/doc/classes/ProjectSettings.xml#L1685): setting exists and defaults to false.
- [Godot.kt](https://github.com/godotengine/godot/blob/4.7-stable/platform/android/java/lib/src/main/java/org/godotengine/godot/Godot.kt#L750): setup reads the setting and calls `disableScrollDeadzone`.
- [GodotGestureHandler.kt](https://github.com/godotengine/godot/blob/4.7-stable/platform/android/java/lib/src/main/java/org/godotengine/godot/input/GodotGestureHandler.kt#L150): enabled setting forwards small MOVE events before scroll recognition.
- [GodotInputHandler.java](https://github.com/godotengine/godot/blob/4.7-stable/platform/android/java/lib/src/main/java/org/godotengine/godot/input/GodotInputHandler.java#L240): ordinary MOVE delivery otherwise depends on the gesture handler.

`Session.prepare_input_focus()` inspects actual Android startup focus. Only a
focused crash dialog for the exact observed bundled Messenger package can be
recovered, once, by force-stopping that foreign package in the disposable AVD.
It saves the prior window dump and screenshot, then requires observed game
focus and a fresh same-instance observation before ordinary controls resume.
ScraperX crashes, unknown app crashes, repeated Messenger crashes and absent
focus return remain failures. No global error-dialog suppression was added.

The touch driver implementation, original displacement requirements,
native-held tick duration, same-instance checks, finally-UP cleanup and
deadlines remain unchanged. Tests add a negative case using the captured
20.40px gesture: DOWN plus advancing native ticks cannot pass when MOVE is absent.

## Host evidence

Executed in the checkout above on the edited working tree:

```sh
PYTHONDONTWRITEBYTECODE=1 python -m unittest discover -s ci -p 'test_android_touch_driver.py' -v
PYTHONDONTWRITEBYTECODE=1 python -m unittest discover -s ci -p 'test_android_compatibility.py' -v
git diff --check -- ci/run_android_compatibility.py ci/test_android_compatibility.py ci/test_android_touch_driver.py godot/project.godot
```

Observed: touch driver8/8 PASS; compatibility13/13 PASS; diff whitespace
check exited0. Host tests cover exact foreign-dialog recovery, no recovery
for an already focused game, rejection of own/unknown crashes, rejection of
missing focus return, missing-MOVE rejection and actual release cleanup.
These deterministic fixtures do not establish Android gameplay.

## Required Android observation

Root must publish the combined repair and run the existing representative
API24/35/36 workflow. Observe real Messenger dismissal/focus return if the
foreign dialog occurs, original full-size crouch/stand, the unchanged small
freight gestures reaching native movement, full lifecycle/resize/fold checks,
and cleanup. No Android runtime pass or release claim is made for these edits.
