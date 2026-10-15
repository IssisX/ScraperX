# Read-only climbing/vault audit —2026-10-15

Sol/medium explorer inspected currentChatGPT0db3c5d plus preserved dirty work; no tests or source edits by the auditor. Root persists its findings here.

Production already routes ground mantle, hanging top-out and low-vault requests through finite `begin_cargo_transfer`, not the fixture gravity-off driver (`legacy_traversal.cpp:17,37,110,162,204`; `locomotion.cpp:163`). Preserve that accepted migration. The low-vault redirect does not check the far-side landing probe and pulls onto the obstacle: compression-only palms and a real running far-side exit are missing.

Authoritative seams: `locomotion.cpp::apply_traversal_commands`, `climb.cpp::{probe_hang_ledge,try_begin_hang,begin_climb,missing_radial_grip,sync_radial_profile}`, `hand_transfer.cpp::{record_foot_contact,try_begin_transfer_foot,begin_cargo_transfer,finish_cargo_transfer,update_shimmy}`, and traversal_plan/affordance/receiver owners.

Actual gaps: top-out finish requires receiver-relative horizontal speed<0.20m/s, enforcing a stop; replace its stability purpose with actual support coverage/receiving-contact continuity, not simply deletion. Acquisition ranks current sweep/aim distance rather than bounded ballistic/moving-support opportunity. Deliberate Action supports one rigid hand, automatic exposed-lip catch still requires two lips. Shimmy excludes material and different-body transfer; no claim of universal handoff. Radial profile already exits on lateral intent, so that gate is not itself a bug. VaultProp enum exists but latch/accepts are hardcoded LedgeCatchTopOut: no implemented palm vault is implied.

Existing targeted tests: alternating_shimmy_tests, physical_topout_tests (shared loads, no-contact/blocked/release/restart/frame/onwardJump), second_hand_recovery_tests. Some stage once with debug_restart_at: local production-path proof, not ascent from grade. parkour_flow vault speed test uses RegressionFixtures, not physical production vault. Reuse matching accepted evidence; do not rerun merely for this audit.

Phase2 should extend existing candidate/per-hand acquisition and actual receiving-foot flow. Phase3 adds compression-only palm actuation, prospective far-side clearance and fresh receiving contact in the existing planner; tensile hanging hands cannot serve as palm props. All releases retain solved momentum.
