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
