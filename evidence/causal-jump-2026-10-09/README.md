# Causal platform jump: charge upward work after braking

Baseline: exact IssisX/ScraperX, ChatGPT,3f295ca85bd5d9913cae94b689c50d20dc29e2c9; clean before work. Native C++17/Jolt5.6.0 at pinned e77f175,90Hz,85kg; Ubuntu GCC15 host. Existing original worktrees preserved.

## Observed defect and correction

On the actual static first-taper girder1932, supported staging followed by60ordinary downhill-input ticks yields−2.029333m/s vertical velocity. A single supported Jump then enters normal gravity1/free-hands flight. The old native work receipt reports1110.6017J, whereas the measured upward takeoff requires1285.6254J. It undercounts by175.0237J because it subtracts the downward energy removed by braking. This is a demonstrated receipt defect, not a demonstrated static jump overspend.

The native causal-support owner now uses the existing ordinary-support positive-work model for both cap and receipt. With relative vertical velocity u[m/s], upward impulse J[N·s] and effective inverse point mass k[kg⁻¹], positive work is uJ+kJ²/2 when u≥0, otherwise k max(0,J+u/k)²/2. For negative u the budget cap is−u/k+sqrt(2B/k). Apply this to the delivered float impulse, retaining the existing cap margin. The causal budget1285.625J and ordinary3000J remain distinct. Equal/opposite dynamic-support point impulse, existing rotational/cluster inverse mass and horizontal departure velocity are preserved.

Changed source: src/sim/simulation.cpp only for runtime; tests/taper_inspection_route_tests.cpp adds the real-downhill falsifier and a jump-work mode that also reuses the existing2m unsupported-gap/landing helper. No controller architecture, geometry, new actuator framework, bridge schema or dependency change.

## Actual verification

[Receipt](receipt.json) records commands, exit codes and source/binary hashes. The added receipt criterion [fails the original owner](causal-jump-regression-before.log), then [passes the correction](causal-jump-regression-after.log). Takeoff remains5.5m/s; horizontal momentum is retained. The existing real2m gap Jump reaches stable landingB with gravity1, free hands, no vault and zero deaths. Native test target and extension builds pass. Independent physics source audit finds no blocker.

Dynamic receiver recoil and a binding negative-velocity cap were not separately runtime-probed. No new Godot/phone/play-feel/performance evidence is claimed; no extra per-tick query is added. The receipt is isolated positive push-off command work, not complete energy closure.

## Scope decision: legacy vault remains pending

A bounded native probe captures gravity-off/velocity-prescribed vaulting and large transient velocity discontinuities on fixture rail5; [baseline samples](baseline.csv) and [summary](summary.json) preserve it. That rail is created only in RegressionFixtures. One [normal-world teeter contact](normal-teeter.csv), documented in [its scenario](NORMAL_TEETER.md), instead enters Climbing with real hands/gravity1. This inspection establishes no shipping legacy-vault encounter. Its finite replacement remains part of the full goal; this pass does not advertise a fixture-only repair as game progress or launch a stance-search campaign. Existing repaired mantle, jump/vault controls and physical routes are preserved.

Previous CI37972296060 at3f295ca failed; it predates this correction. No new green APK or phone behavior is established. Next: return to coherent strategic parkour authoring at the next existing route connection, using actual geometry and finite native movement rather than continuing unrelated CI or fixture work.
