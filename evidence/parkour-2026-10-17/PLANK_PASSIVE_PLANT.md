# Bounded plank fracture fixture correction

Base repository: ScraperX ChatGPT at 0b1247ba1c17ae0296542c3e125d196ca8ca7a7f. Only tests/deformable_plank_player_tests.cpp changed. Production sources remain unchanged. Isolated git-archive headers avoid the dirty shared Simulation ABI.

Supported cause: the pre-coyote fixture treated every active foot row as a powered push. The existing native contract explicitly retains a zero-command passive trailing plant from fresh real contact (parkour_intent.cpp:retain_trailing_plant; physical_foot_push.hpp:begin_passive_plant; PARKOUR_MOVEMENT_SPEC_20261015.md phase1). Existing physical_foot_push_tests.cpp:passive_plant_activation_retains_row independently requires zero-command/zero-preload passive acquisition and distinguishes real external-compression reaction from activation. The shared passive_receiver_plant_observation.hpp is already used for the wood receiver test.

Baseline untouched impact executable exited1 at tick187: actual steel1936 contact, active1 with zero work/stroke/peak. Diagnostic retained identical inputs and all fracture/recovery checks, continuing only past the all-active prohibition. It exited0 with actual rupture tick72, firm recovery tick252 and stable recovery/damage tick432. Every active sample tick187-208 had fresh current steel1936 contact, starts0, work0J, stroke0m, impulse0Ns, peak0N. Height stayed404.399963379-404.399993896m. The row supplied no lift.

Correction: retain original zero-impulse and zero-start negatives, add zero command work/stroke/peak/player force; allow active only through the existing bounded passive observation plus fresh actual steel1936 contact both before and after, tray height within0.01m and per-tick height change below0.001m. Check the same invariant across the final180neutral ticks. No staging, input, fall/strength, fracture, firm recovery or persistent damage requirement changed.

Receipts: build-final-receipt.json contains exact compile argv and source/archive/executable hashes. mechanics-window.json and trial logs contain exact run argv, exits, source hashes and full output. test-correction.patch is the owned diff. The final full execution and its source hashes are included in PLANK_PASSIVE_PLANT_COMMANDS.json.

Limit: Linux ARM64 native evidence does not establish x86, Android or device behavior. Parent owns published x86 gate and release.

Final observed execution: final-plank-all exited0 in44.296523s, with PASS DEFORMABLE_PLANK_PLAYER, PASS DEFORMABLE_PLANK_GRIP, and PASS DEFORMABLE_PLANK_PLAYER_FRACTURE. Shared test and archived built test remained SHA256 2d6cbeb45af8d7926bbec82ea3324e167a6382c00c061d08cdbea09c5fd24bd0. git diff --check exited0. No other source was changed.

Root inspected the actual diff, shared helper and foot owner; accepted the supported test-contract correction. The published CI request runs deformable_plank_player and wood_foot_push_player against one x86 native archive. No full-release or phone-FPS claim.

Published9b6e1c7 native-repair39076086572 is GREEN: deformable_plank_player and wood_foot_push_player both pass, tests2/failures0 on one x86 archive. Exact receipt: PLANK_PASSIVE_PLANT_CI.json.
