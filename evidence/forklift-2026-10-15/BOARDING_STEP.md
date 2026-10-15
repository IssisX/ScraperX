# Physical side boarding step

Trial11 failed because the old side jump legitimately caught the new overhead guard (Hanging2440). The driver must approach the actual side doorway on foot. Added6mm steel treads top local−.42, x[.67,1.00] and[−1.00,−.67], z[−2.01,−1.29], welded through6mm mounting walls and two6mm ribs per side. Existing floor top−.24 remains authority. Settled approximate rises .188m and .18m; doorway and roof remain unchanged. Treads add38.009088kg total, computed from actual steel volumes×7800kg/m³.

Movement inspection: existing step maximum .35m; causal support predicate currently omits2440, so its fallback can write a step pose. Diagnostic explicitly rejects any step_up_pose_count increase. No locomotion/force/hand laws changed by this slice. Only initial grade approach is staged; public walk intent boards and exits. Runtime result pending.

## Actual first diagnostic result

StrictC++17 `-Wall -Wextra -Wpedantic -Werror` yardTU and publictest compilation/link passed against frozen operator archive07c6ecb0 (Linux GCC, pinned patchedJolt). New yardobject linked before archive, retaining both factorysymbols so new geometry is exercised.

Existing world-window trial12 `ordinary-side-boarding-step` FAILED after51.985s. At90tick sample, player(−100.943,1.12872,−57.6557) had actual2440treadsupport, with forbidden legacy step_up_pose_count1. Subsequently count2 and actualroofhang, finishing(−101.109,1.40649,−57.6558), traversalHanging, nooperatorcontrolauthority. Positivefirsttreadcontact is proven; normalfloorboarding and either-sideexit are NOT proven. Log `world-window.trial-12.log` retains failure. SourcecppSHA733248d2501d2fb655055f33b83c4120b88a613aad19d42c807f1ce677bfe6d2; testSHAeae7f6e449d3525180bb34629dc3dc12e5485d06934f61c72d1115bf57b811c2.

Supported next correction belongs at existing movementclassification owner: forklift2440 must use existing finite physicalstep path instead of fallback SetPosition. That path already sets causal_airborne_/physical_step_pending_ and accounts reciprocal work. Root owns that correction; this worker did not modify it. Do not accept this result as walkproof or advance fullcargodriver based on it. Window has onefailednewtrial after reassessment; nosecondtrial run.

## Corrected public entry and exit accepted

Root added explicit2440support/obstacle membership to the existing physicalstep predicate in locomotion.cpp (SHAc4cae89a56e2ff88fa492dbeb8317c53d3990b0ff59c57c7c5ee6dea0bbc6ea0). Same finite reciprocal impulse/work calculation; no movement law or tread change. Existingwindow reassessed from actualtrial12log; correctedtrial13 `physical-side-boarding-step` PASS in69.038s. Both sides ordinarywalk toreal2440floor/controlsreachable, retain90ticksneutral, walkbackofftoactualgrade, authorityrevoked, traversalNone andzero step_up_pose_count throughentirepath. NoJump/Action/crouch used. Historicaltrial12failure preserved.

Exact publicsequence: stageonly initialgrade(−102,.9,−57.65) or(−98,.9,−57.65),45settleticks, faceinward. Each90Hz tick set_move_input(clamp(1.2*(truck.x−player.x)−.45*velocity.x,−.3,.3), clamp(1.2*(truck.z−1.65−player.z)−.45*velocity.z,−.15,.15)). Stop atactualcontrols_reachable, neutral90ticks. Exit same side with(.3*side,0) untilgroundedgrade y<1, noauthority andside*(player.x+100)>1.5; thenneutral45ticks. Bothsideboards occur atxoffset~±.568,y~1.2534.

Build: LinuxGCC C++17 O3/NDEBUG, pinnedJolt definitions/includes, PIC/pthread and-Wall/-Wextra/-Wpedantic/-Werror; locomotionobject additionally-fno-rtti. Yardobject andchangedlocomotionobject linkedBEFORE unchanged07c6ecb0archive. Matchingselectednativeheaders; noSimulationABIchange. FinaltestbinarySHA6c92ee825fffbb7e68bbc3bbc0beaee9af497f7d8c2fdf78e0bd09e45f4166d9. Trial13receipt records allsource/object/archivehashes. This proves boundednativepublicboardingandexit, notfullcargo/world/Main/APK acceptance fornewsource.
