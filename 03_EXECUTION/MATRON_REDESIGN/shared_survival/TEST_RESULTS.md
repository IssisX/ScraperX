# Native verification — 2026-10-21

Base: `5f34d3dcefdbe2dd3eff3fa2dcac4fc34848e879`; branch: `dot/matron-shared-survival`.
GCC 14.2.0, C++17, Release, repository-pinned Jolt, bridge OFF. Complete native build exited 0 with warnings-as-errors. No dependency pins, signing, identity, release workflow or APK assets changed.

| Final command | Result |
|---|---|
| `cmake --build /workspace/build-survival --parallel 4` | exit 0 |
| `ctest --test-dir /workspace/build-survival -R 'shared_survival\|parkour_flow' --output-on-failure` | 2/2 passed, exit 0 |
| `/workspace/build-survival/scraperx_shared_survival_tests` | exit 0; proof lines below |
| `SCRAPERX_ONLY=matron /workspace/build-survival/scraperx_sim_tests` | exit 0; existing Matron regression group |
| `SCRAPERX_ONLY=crane /workspace/build-survival/scraperx_sim_tests` | exit 0; existing yard-crane regression group |
| `git diff --check` | exit 0 |

The initial broad baseline CTest run was stopped deliberately after ~703 s while the monolithic traversal test was still running. CTest reports that subprocess terminated (exit 8 aggregate), and its parkour test passed. This is an **incomplete baseline run**, not a passing full suite or an observed simulation assertion failure. Final targeted checks above completed after the final source edits.

The initial new-test compile failed on the missing feature header; physics tests then exposed an invalid fixture overlap and an insufficient Matron warm-up, both corrected. Independent review exposed substantive contact, force, AI and lifecycle defects; their fixes and adversarial cases are described in the handoff. No unresolved failing targeted test remains.

## Shared-survival fixture output

```text
  support y=1.45799 N=544.949 cap=925.743
PASS shared survival finite load and equal reaction
PASS shared survival facing and inclined-contact torque conservation
PASS shared survival sensors carry no load
PASS shared survival contact, footing loss and overload collapse
PASS shared survival loose prop carries route after release
PASS shared survival district coasting, finite brakes, selective restart and interlocks
PASS shared survival power loss -> hand load -> loose brace -> released actor -> powered route
PASS shared survival interruption, reacquisition, stale handles, cycling, deterministic replay and step sensitivity
  Matron bracing ticks=120 of 120
PASS shared survival actual Matron hands occupied then hostile release
```

## Existing Matron proof lines

```text
PASS scraperx_sim matron mind: guarded at -10.693,-129.011 peered_short_m=5.81361 fooled_twice_stayed_m=0.565748
PASS scraperx_sim matron crash: heard_m=17.1888 loudness=0.555059 closest_m=0.00541816
PASS scraperx_sim matron vault: feet_up_m=1.50095 past_m=1.86481 crate_moved_m=0.000742076
PASS scraperx_sim matron ride: with_her_s=10.3889 alone_s=9.65556 island_s=6 leaps=1
PASS scraperx_sim matron unrig: unrigs=1 rehooked=1 rode=1
PASS scraperx_sim matron line: slides=1 level=143
PASS scraperx_sim matron below: walk_nearest_m=11.6995 sprint_heard=0.8 lead_floor=143
PASS scraperx_sim matron shove: shoves=1 dz=-0.602615
PASS scraperx_sim matron mischief: took_s=9.44444 throws=1 nearest_m=0.48988
PASS scraperx_sim matron hoard: from_post_m=0.306478
PASS scraperx_sim matron collision: closing_mps=15.0467 shoves=0
PASS scraperx_sim matron balance pest: blow_mps=0.362492
PASS scraperx_sim matron balance dangerous: staggers=2 knockdowns=2
PASS scraperx_sim matron balance: knockdowns=2 staggers=3
PASS scraperx_sim matron heave: blow_mps=0.690402
PASS scraperx_sim matron pipe dangerous: warned_s=8.21111 holds=37
PASS scraperx_sim matron pipe changed: warned_s=9.94444
PASS scraperx_sim matron pipe drum: slam_mps=10.1923
PASS scraperx_sim matron room: off_m=27.8144 heard=1 came_m=22.5111
PASS scraperx_sim matron rage: sprint_s=31.4 peak=0.46272 pest_after_s=62.6333
PASS scraperx_sim matron two below: sprint_heard=0.8 lead_floor=132 stand_off_m=0.585699
PASS scraperx_sim matron no way: fooled=1
PASS scraperx_sim matron beside: sprint_heard=0.8
PASS scraperx_sim matron climb: frail_s=7.21111 climbs=1 peak_mps=2.20001
```

## Existing yard-crane proof lines

```text
PASS scraperx_sim crane: forked_up_44_m=0.363625 forklift_x=-22.5205 stall_n=75000
```

Not verified: full final monolithic suite, Godot bridge build, rendered encounter, Android APK, installation, Fold performance, campaign placement, persistence round-trip, or acoustic masking. Native tests are not shipping/runtime proof.
