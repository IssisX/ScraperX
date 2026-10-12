# Completed extension restores ordinary climbing

Actions38295353835 at e235d6f stopped at native shared_hanging_posture test13/63;
twelve preceding tests, including the repaired cart, passed.

The same registered `parkour_tuck` mode reproduced RED. Neutral radial extension
left a2.98023223877e-8m ledger residue after zero actual delivery. Resumed Cartesian
climbing therefore retained the posture500W/0.6m/s limiter. Prior north geometry
also failed; rung headroom was sufficient. This is a native completion defect.

`advance_hanging_posture` now settles the ledger only after zero delivery when the
remaining stroke cannot change the commanded-span float. Actual targets, motion,
force/work accounting and all existing acceptance thresholds remain unchanged.
Independent read-only High review found no blocker on the valid radial path;
mean-span equality also relies on zero delivery and existing valid-state invariants.

Unchanged full test: RED rate_ratio0.734327 → PASS1.03277, exit0,201.334s.
Static/moving hangs, regrips, extension, Jump mantle, Drop momentum, missed-release
recovery/retry and tucked1985receiver/Tower11 exit with deaths0 all passed.
[Receipt](receipt.json) binds actual source/test/archive hashes and O3/Werror commands;
[baseline](baseline-receipt.json) preserves the original rejecting execution.
[Full output](full-parkour-tuck.log) SHA256
`ca66defdc7875d94484b9b72878ba25183ffa5569493ebb96c350fa1869fca34`.

This is focused native evidence. Full Actions/Godot/Android delivery and phone feel
remain separate gates; current delivery state lives only in the live checkpoint.
