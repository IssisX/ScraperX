# Full proof failure readback — 2026-10-15

Source `8569c721432e4139df6286b8e8d1cce94b2db9ca`, full-proof run [38809891198](https://github.com/IssisX/ScraperX/actions/runs/38809891198), job `moving-support-truth`: terminal FAILURE. Native build completed; CTest reported 92/101 passed, nine failures, exit1. Subsequent presentation and Android export steps were skipped. Exact-source candidate run38809891195 remains GREEN; its APK evidence does not establish full-proof success or device behavior.

Readback inputs: `/storage/emulated/0/Download/ScraperX-builds/8569c72-candidate/full-proof-status.json` and `full-proof-failed.log`. Log line references below refer to the latter.

| Failed check | Exact assertion / observed state | Supported classification |
| --- | --- | --- |
| `scraperx_sim.physical_entry_overlap` | L767: `FAIL ENTRY_OVERLAP phase=ordinary_receiver_exit reason=active foot remains attached to real material, not steel or world`; active foot support1935. | Newly observed assertion incompatibility. Test restricts every active foot to2880–2891; new rigid trailing-plant owner permits real rigid contact. |
| `scraperx_sim.physical_entry_natural` | L801: same assertion/phase; active foot support1935. | Same newly observed incompatibility. |
| `scraperx_sim.wood_foot_push_player` | L855: `FAIL WOOD_FOOT_PUSH_PLAYER phase=unchanged_static_steel_jump reason=active foot remains attached to real material, not steel or world`; active support1935, zero commanded work/stroke. | Same newly observed incompatibility. |
| `scraperx_sim.suspended_ladder_route` | L937: `FAIL AS-026 actual receiver hands arrest the fall before lateral transfer`; pose8.71501,117.471,-179.392, traversal0/hands absent. | Pre-existing matching failure: saved2018 logL1345 has the same assertion and pose8.71497,117.471,-179.392. |
| `scraperx_sim.catch_net_model` | L976: `FAIL catch native model: first rebound can supply rigid fatal grade descent`. | Pre-existing matching assertion in saved baselineL1295 and c2bL954. |
| `scraperx_sim.catch_net_player` | L1061: `FAIL catch player deliberate first rebound spill causes fatal ground impact`. | Pre-existing matching assertion in saved c2bL1040 and refactorL1050. |
| `scraperx_sim.taper_inspection_route` | L1122: `FAIL TAPER_INSPECTION ordinary route retains gravity without traversal, vault, chute or rescue`; preceding tick1815 during middle-miss recovery has traversal1/hands2, gravity1/deaths0. | Newly observed regression against saved2018L1160 PASS; exact causal production owner unresolved. Primary ascent passed before recovery failed. |
| `scraperx_sim.north_transfer_route` | L1246: `FAIL NORTH_TRANSFER phase=429_onward_to_upper_toe reason=bounded ordinary walking did not reach target`; terminal Hanging1938 near(-0.03739,428.087,-129.536). | Newly observed regression against saved2018 route PASS; exact cause unresolved. |
| `scraperx_sim.north_transfer_recovery` | L1446: same phase/assertion and terminal pose/support. | Newly observed regression against saved2018L1269 PASS; exact cause unresolved. |

Prior logs inspected, without rerunning:

- Saved2018: `/data/data/com.termux/files/usr/tmp/scraperx-apk-watch-20261015/2018-full-failed.log`.
- Savedc2b: `/data/data/com.termux/files/usr/tmp/scraperx-apk-watch-20261015/c2b-full-failed.log`; L865/869/881 also show the three material-foot checks previously passed.
- Baseline: `/data/data/com.termux/files/usr/tmp/scraperx-refactor-20261015/baseline-ci-failure.log`; L1710/1712/1714 show prior north/taper/recovery passes.
- Refactor: `/data/data/com.termux/files/usr/tmp/scraperx-refactor-20261015/refactor-ci-failure.log`.

Assertion owners: `tests/physical_entry_overlap_tests.cpp:73`, `tests/wood_foot_push_player_tests.cpp:66`, `tests/suspended_ladder_tests.cpp:281`, `tests/catch_net_model_tests.cpp:602`, `tests/catch_net_player_tests.cpp:199`, `tests/taper_inspection_route_tests.cpp:54`, and `tests/north_transfer_route_tests.cpp:103`. New rigid plant behavior is owned by `src/sim/simulation/parkour_intent.cpp::retain_trailing_plant`; source and zero-work receipts support distinguishing it from the old material-only assumption. The route failures do not by themselves identify the responsible production transition or prove a physical repair.

Smallest useful follow-up: distinguish real passive rigid plants from powered material pushes in the three stale assertions while retaining contact and finite-force/work requirements. Capture the first unexpected taper/north traversal transition before selecting a production repair. Preserve the deferred matching ladder/catch failures. No environmental failure is evidenced.

Read-only investigation and this note only; no source repair, build, test rerun, commit, or new workflow. Parent-assigned Sol/medium; no live effort-control change claimed.
