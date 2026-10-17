# Owner-approved death-rule correction —2026-10-17

Owner explicitly requested ending catch-net experiment churn and proposed lowering the player's death threshold. This supersedes the earlier20m/s invariant and root's intervening suggestion to retire the lethal-rebound requirement. Keep the required first-rebound spill, actual lethal impacts, overload, clearance, energy, recontact, settling and restore behavior.

Chosen shared rigid-impact threshold:18m/s instead of20m/s, using existing public Simulation::kLethalImpactSpeedMps. This is an intentional global gameplay-policy change, not a repaired contact solver or more energetic net. Existing baseline player40 spill has actual18.6011m/s strike, which is lethal under this rule. It is not a promise that every arbitrary rebound kills. Normal support-relative impact measurement and cloth-specific load-dose overload remain unchanged. The surviving roll fixture has approximately17.7m/s normal approach; focused landing acceptance must verify it and both17.5/18.5m/s policy-boundary falls.

All root experimental production physics was removed: no cartridges, support saddle, damping relief, bulk-drag removal, restitution change, new force law, mass or Jolt change ships. Published finite cloth, piston, frame, seals, tether and render code remain byte-identical. Rejected patches and their actual failed results are retained in the local catch-return artifacts and existing trial ledger; no additional mechanical variants run.

Catch model/player acceptance now reads the same shared threshold as production. This changes the physical danger rule at its actual owner rather than deleting the lethal-spill test or lying about launch energy. Added landing checks require an actual ground contact just below the new boundary to survive and one above it to die. Existing parachute/survivable-drop and affected landing/recovery checks remain required; full native Actions detects other global consequences. The bridge danger gauge already reads this public rule through get_lethal_impact_speed_mps(), so no independent Godot tuning constant is added.

## Observed local acceptance

Source: published5bd7ccd plus only the curated shared18m/s header and affected tests. Complete71 native translation units and all3 test executables built with C++17 strict warnings; all compile/link commands exit0. Exact private receipt: /data/data/com.termux/files/usr/tmp/scraperx-android-compatibility-20261017/catch-return/lethality-native/native-build.json. Native archive SHA2569f46c034c42dbd8281d1bb05aafd6c7a727426eb9a27c0cc7fad34b120566568; curated publicheader SHA256bab4da700ecb25b8a8eb084f7026ca326f920918c28d9bbad9e144fc4269e1ed.

Executed through ci/mechanics_trial.py persistent ledger (trials16–18), using `proot-distro login ubuntu -- <lethality-native/build/executable>`:

- `catch_net_model_tests force`: exit0, all original reciprocal-force/tether/piston checks, `PASS catch native model`.
- `catch_net_player_tests spill`: exit0, actual rigid strike18.6049m/s, death1 at tick589, `PASS catch deliberate first-bounce spill remains lethal`.
- `landing_tests`: exit0, real ground contacts straddle18m/s; full existing landing/recovery/roll/slide/moving-support cases also pass. Exact measured boundary output retained in trial18.

Focused x866checks, full GitHub Actions and matching APK remain pending. This local result is not Android device play or a full release claim.
