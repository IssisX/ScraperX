# Neutral carriage drift diagnosis — 2026-10-16 00:34 UTC

Read-only assignment, repo ChatGPT/5acc89a with preserved concurrent unpublished crane changes. Probe links immutable source-v7 headers/build-v7/libscraperx_sim.a (SHA25629e2c8ad94ce24dc7b977ab26b94dbb9fc63a3b03ac52def7ae082417cdb6eeb) and existing pinned Jolt library. No repository/source/dependency edits.

Authoritative owner: src/sim/warehouse_forklift.cpp creates/commands the real mast↔carriage Jolt SliderConstraint. Neutral is motor Off plus22kN friction. GetCurrentPosition uses relative COM and actual rotated local anchors. The actual assembly fixture is copied from tests/yard_crane_tests.cpp; private main performs finite stow, actual four-leg rig attachment, finite360tick crane raise and90neutral ticks, then9000neutral native90Hz ticks with4collision substeps. No body pose/velocity/gravity modifications. No full accepted cycle repeated.

Measurements from the same real constraint: q plus exact derivative Jv = n·(v2-v1+omega2×r2-omega1×(r1+u)), sampled each collision substep. Logs and actual source/archive hashes recorded in window.json.

Results:
- Grounded tilted neutral100s: dq+0.00000244379m, integral Jv+0.0000000876736m, max friction1090.67N.
- Suspended tilted neutral100s: q0.3454454→0.255750m, dq−0.0896954m, integral Jv−0.00447778m, max |Jv|0.0000487207m/s, max friction1102.18N, unchanged22kN limit. Reproduces the fullworld order of drift.
- Same suspended run, existing Kit::set_accurate_integration enabled only on chassis/mast/carriage/fork frame before actual rig attachment: dq−0.0388988m, integral Jv−0.00444909m, max friction1101.98N. This is a discriminator, not a supported fix. Large material residual remains.

Supported causal boundary: suspension induces real relative-anchor position changes outside the velocity-friction solution. Brake cap undersizing and simple absolute-world readout error are contradicted. >95% of baseline coordinate displacement is absent from integrated slider velocity; the existing accurate-integration option changes this displacement materially, establishing a numerical contribution. Pinned SliderConstraint::SolvePositionConstraint intentionally corrects transverse/rotation/limit rows but not the neutral axial friction row; other attached constraints also perform split position corrections without modifying physical velocity. Which exact projection/rounded update supplies the remaining drift is unresolved. Do not claim one precise lower-level cause yet.

Next cheapest sufficient discriminator, if this becomes blocking: track q immediately before/after velocity integration and each constraint's position solve in this private suspended fixture, identifying the first accumulating axial displacement. Compare that phase in baseline vs existing accurate integration. Repair belongs at the demonstrated integration/coupled position projection owner; no increase to brake ratings, position servo, remembered-height lock, velocity overwrite, gravity cancellation, or larger stow to mask drift is supported by this evidence.

A future repair acceptance should use this actual suspended fixture, show neutral displacement within a derived tip-clearance/mechanical error allowance over the needed freight duration, retain unsaturated finite passive brake/no battery work, and preserve powered lowering, force-limited slip under overload, release, and reactions. No new regression campaign is warranted before an actual owner change. The1cm check.py criterion is only a deliberately red diagnostic detector, not a proposed shipping tolerance.

Fullworld trial5 first failure is separately diagnosed by root as rail clearance collision, before dock landing. It does not demonstrate that lift drift caused that route failure.

The next private perphase probe resolved integration versus projection attribution without a shared edit: HYDRAULIC_DRIFT_PHASE_CAUSE.md. Coupled joint projections account for85.1373mm of89.6954mm drop, exact closure. No production repair is yet established.
