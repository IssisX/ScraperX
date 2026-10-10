# Cage carry-exit repair at 89e21f14ff6f63dc24efbdbea91170dd1d924075

Owned maintained file: `tests/simulation_tests.cpp`. No production/Godot/docs edit, commit, push, publish or child launch. Concurrent swing/doc work preserved.

## Supported observation and cause

Actual CI38060174319 fails the FIRST default carry exit, not the later explicitly jumping belt route. Its log has no cage poses. Exact early ARM path passes, so this is **not claimed as a reproduced exact x86 state**.

The private early probe uses the same native archive and the complete command-mashing/bar-clear/door-travel/block-take sequence, without the preceding athletic campaign. Original default exit passes with the exact ARM state and a 10cm shorter northward bar retreat. Ordinary bar retreat extended by 10cm and 25cm produces different real dropped-bar poses and fails the original 4-second first-leg arrival. Bar moves toward the doorway under player contact; one receipt reports actual bar support 55. Held block 56 remains present, door is travelled, floor support remains native. The walking-only helper cannot reliably handle the dropped bar, and previously selected Jump only by route identity.

Correction: both paths inspect the current rotated bar long axis and request one ordinary Jump while genuinely grounded, north of its center, within 0.75m horizontal approach to the 1.025m half-length bar segment. The player otherwise follows the original waypoints/tolerance and two 4-second budgets. Native carry, collisions, Jump forces, supports and loads remain authoritative. The helper also rejects lost carry during the first leg. No pose/velocity/force writes or clearance/slip changes.

Focused red/green: exact early ARM and -10/+10/+25cm ordinary retreat variations all clear both legs with the state-aware Jump; +10/+25cm fail the original helper. See `focused-probe-receipts.json`, private sources and logs. No claim of exhaustive bar states or x86/phone proof.

## Preservation/build

`assertion-preservation.json`: all754 require messages retained;753 calls byte-identical; sole changed call removes the now-unneeded `true` route flag. All main walking budgets unchanged; cage exit remains4+4seconds. `changes.diff` is the assigned file diff.

Only shared target `scraperx_sim_tests` built; build exit0. Native archive remains a816cc5fa7d197aa9f5071a0e33cbf3396bf130271bc1a23c9eee64ca14d7094; pinned Jolt e77f175595e64cb44218cc9d9d56fc365ad0e36a archive026916df9f91e8e3e2e54d5099624bed1dfd19eb41b27a94ac779696a807ac9b. Source/archive/executable hashes and exact commands: `source-receipt.json`.

## Final runtime

One full changed athletic executable completed: exit0, 27 PASS receipts, no FAIL. Both AS-003 exits and the complete executable pass on local ARM. `athletic-final.log` and `athletic-final.exit` own its actual result. Shared host-build target released. No54-gate duplicate campaign. Root owns independent x86 capability, both-owner integration and one next normal exact-source full Actions/APK delivery.
