# Player-built counterweight route — 2026-10-20

A player in the Freight Yard can walk to the west rigging bay near(-120,0,-36), connect the real platform/guide/tray eyes, load three separate25kg weights, board and pull the real release lever. The unballasted120kg tray cannot lift the85kg rider plus85kg platform. The three weights supply the missing gravitational work. No player/cargo pose or departure velocity is prescribed by the route.

Touch: RIG equips the assisted tool; aim at an eye and use contextual Action to clip/thread/connect. Stow keeps installed lines. Undo removes the last draft step. Unclip refuses a loaded line; hold Cut deliberately for0.75 native seconds. Walking and looking remain available. To retry the light rig, crouch while physically returning its lever, then unclip the unloaded line. Existing desktop support is preserved; this change adds no new desktop bindings.

The tray has a1.76m clear loading opening, a side eye aligned with its upper sheave and an8cm front retaining rim. Ten ordinary20cm risers lead to a2.2m loading shelf, clearing that rim even with rotated carried loads. These are actual shared collision/render parts, not invisible cargo attachments. The120kg base mass and existing force/governor/cable laws are unchanged.

## Executed evidence

The final combined native graph includes M1 plus published2b Kit/PipeBridge and grounded-audio owners. Command: `proot-distro login ubuntu --bind /data/data/com.termux/files/usr/tmp:/data/data/com.termux/files/usr/tmp -- /data/data/com.termux/files/usr/tmp/scraperx-rigging-20261020/integrated-m1/public-route`. Exit0: `PASS RIGGING_PUBLIC arrival=1 ballast=3 actual_platform_ticks=2368 receiver_ticks=433 retries=1 resets=0 deaths=0`. All three actual ballast bodies descend with the tray; their final tray-relative center heights are0.302/0.369/0.342m. The occupied platform holds2.32499m through the180-tick wait and onward approach; actual receiver1495 supplies433 grounded ticks. Exact compile/link/run receipts and output: B/integrated-m1/{commands.json,raised-loading-shelf.json,raised-loading-public.json,raised-loading-public.log}, B=/data/data/com.termux/files/usr/tmp/scraperx-rigging-20261020.

The earlier complete-route pass was insufficient: the third weight later slid out and the occupied platform drifted downward. Those traces and the initial rim-loading failure are preserved. The new retained-load/stable-height assertions remain enabled; no original ride/support/recovery assertion was relaxed. The COM lateral containment was updated for the independently widened physical tray.

Actual Main/native touch equip/stow/re-equip integration:16checks, process exit0, B/rigging-native-input-{command.json,log}. Touch cut cancellation, simultaneous movement/look and12 safe layouts: exit0, B/touch-only-controls.json. The current publication retains the same exercised touch paths. Core graph lifetime, loaded-unclip, cut, restore/generation and moving-guide/cable checks retain their existing foundation evidence.

Local3D capture did not complete within90seconds under the phone's proot/Xvfb/softpipe wrapper, even after correcting the test-only viewport scale. Cause remains unresolved; no rendered-pass or device-performance claim. Required matching x86CI includes the final native public route, real touch/router checks and actual shipping native view/capture before accepting its APK. No visual-quality reduction is included. Phone feel/performance/listening remain unverified until played.

This is milestone1 of the selected five-milestone rigging feature. Powered cooperation/checkpoint crossing, portable moving guides/clamps/2:1, physical traveller/brake and the connected signature job remain separate required milestones.
