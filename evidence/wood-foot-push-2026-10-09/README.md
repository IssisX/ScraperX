# Mounted wood foot push — observed evidence, 2026-10-09

Source parent: `cb19b6ea09b05fc082e7e1612be52f7aa0534a80`, exact `IssisX/ScraperX` / `ChatGPT`. Native C++17/GCC 15 Release, ARM64 Ubuntu proot; Jolt 5.6 pinned `e77f175595e64cb44218cc9d9d56fc365ad0e36a`, 90 Hz gameplay/four collision substeps. Godot 4.7 stable `5b4e0cb0f`. Private host-source adapter imports the pinned Jolt library and uses the repository src/tests; it is not the production CMake file. Root alone built shared targets.

## Passing final evidence

| Receipt | Observed result |
| --- | --- |
| native-final-build.log | Relevant native executables and Godot bridge compile/link successfully. Later driver-only cleanup/settling/Balance CLI changes were rebuilt successfully. |
| isolated-final.log | Exit 0: preload/sign/actual point torque/rotated offset/unilateral/reach/work gates. Real row saturation 9.722222328 N·s = 3500/360. |
| native-player-final.log | Jump component PASS: peak Vy 0.961710393 m/s, apex 0.146911621 m, 524.999499 J command debit, repeated-input rejection, intact landing and steel exit. This combined run then failed an obsolete cleanup assertion; see final cleanup below. |
| native-cleanup-final.log | Exit 0: Drop, active-push saved-checkpoint restore and ordinary static Jump 5.390995502 m/s after first gravity tick. |
| native-retained-plank-final.log | Exit 0: all eleven actual grounded seams, Jump/landing/unload/steel exit, real two-hand grip/release, actual 3 m fall fracture mask112 and ordinary tray recovery. |
| native-connected-route.log | Exit 0: actual inspection396.25→418.25 primary5982 ticks, native gravity/zero deaths/stable exit. |
| native-moving-support.log | Exit 0: existing Balance loaded ride/exit/paid reset/recall and moving departure; support Vy −1.22725, takeoff Vy3.94618 m/s. |
| viewport-touch.log + viewport-touch-receipt.json | Exit 0: ordinary viewport-touch1432 native ticks, real wood Jump/landing and firm steel exit, no deaths. Peak Vy0.964341 m/s, apex0.147766 m, peak leg1387.82 N. |

Executed native paths in `host-build`: `scraperx_physical_foot_push_tests`, `scraperx_wood_foot_push_player_tests` (Jump and separate `cleanup`), `scraperx_deformable_plank_player_tests`, `scraperx_inspection_junction_route_tests`, `scraperx_supplied_machine_route_tests balance`. Root used `proot-distro login ubuntu -- cmake --build ... -j2`. Viewport helper ran `run_touch.sh final-v2 jump .../final-v2-godot` with Godot `--headless --audio-driver Dummy --fixed-fps 60 --resolution 432x432 -- --uitest=touch_wood_foot_push`; exact helpers, prepared-source manifest and tested scene/driver are archived here.

## Preserved failures and their classification

Earlier native-player and retained-player logs document the full0.30 m prototype causing real landing overload. This was an introduced prototype failure; strength/E were not weakened. Normal wood now requests0.15 m. Later cleanup assertions incorrectly treated checkpoint commit eligibility as enduring support; the driver was corrected to inspect actual saved restore and real footing, preserving production checkpoint logic. Earlier180-tick quiet-settle allowance expired without fracture; only the driver wait budget became360 ticks, retaining the identical30 consecutive real support-relative quiet criterion. The earlier unsupported `balance` command returned2; the CLI now selects the existing Balance test without changing its default full sequence. Old failures remain visible and are not claimed as passes.

## Delivery and limits

Parent [Actions38001791173](https://github.com/IssisX/ScraperX/actions/runs/38001791173), exactcb19b6e, completed47/52 native PASS. All new north/pipe/inspection and both plank gates passed. Five inherited failures remain: gravity_cart_integration, athletic_traversal, swing_stair_route_2/5/7; independent artifact comparison found no introduced failure. Godot/Android/APK steps were skipped. This foot source requires its successor normal CI; no new APK is claimed.

The measured intact landing has peak strength ratio approximately0.974, a narrow margin for that trajectory. Full physical energy closure, anatomical legs, unobserved catches/landings, phone feel, Android performance and a commercial quality certificate are not established. Command work is a conservative bound, not measured delivered work. Original stop deadline00:14:17UTC remains unchanged.
