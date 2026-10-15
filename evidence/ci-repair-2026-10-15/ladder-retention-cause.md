# Suspended ladder receiver hand retention: one private probe

Result: the first release is the existing actual-hand reach guard, with both real rigid attachments intact and loaded. The shimmy never advances its ledge or regrips before that release. The source has a specific acquisition/shimmy spacing mismatch at this corner; correcting that owner is justified, but a passing route after that correction is not established here.

## Provenance and scope

Repository `ChatGPT` at `c2b2517d67d19e8ae4843db6177048db546277ad`, preserving the existing dirty gate, presentation and test work. No shipping code changed. Private directory:

`/data/data/com.termux/files/usr/tmp/scraperx-ci-ladder-retention-probe-20261015/`

Copied frozen source/headers from `scraperx-ci-west-reverse-probe-20261015/src` and the current `tests/suspended_ladder_tests.cpp` driver. Replaced only exact archive members `climb.o`, `hand_transfer.o`, `affordance.o`, and `physical_hand_climb.cpp.o` in a private copy of native archive SHA256 `18cc2d81b7628927b2acbbf1997a417e670e1851a738d4f2270d00673b12bb0d`. The unchanged Jolt archive SHA256 is `8711a784f394fdf91af3366ab0b6695e80322cb73b46a8d1659e775b20aced1b`; frozen public header SHA256 is `57f165f39595c6100f3e8a094f7df7646ca6248c54a29d9be7c974c2b8285e1f`. No live gate headers were mixed into this binary.

Instrumentation reads the real owner before clearing: per-hand body/generation/local anchor/world anchor/force/relative velocity/target/extension, COM, profile and work ledgers. It adds no force, pose, timing or input. `run.py`, `setup.py`, `receipt.json`, `analysis.json` and `run.log` retain the exact probe. The internal `state_.tick_index` field printed zero in this owner, so use sample ordinal, not those printed tick labels: there are30 shimmy samples, with29 fixed intervals (0.322222s) from first to release sample.

Build initially failed linking the rebuilt hand owner because Jolt omits C++ RTTI. Recompiled that owner with `-fno-rtti`, then linked successfully. One gameplay run executed,65.608s, exit1 at the unchanged “inboard shimmy retains the actual receiver lip and both hands” assertion. No second run or sweeps. The private trajectory differs from the parent's earlier failure coordinate, so its numerical values describe this probe, not an exact replay of that earlier binary.

## Actual first failing transition

At first shimmy sample: COM `(7.479304790,118.482353210,-179.919540405)`, velocity `(3.978832006,-4.996974468,1.822597146)`m/s. Commands throughout are `(1,0)`, no Jump, Action or Drop. Normal is `(0,0,1)`; radial/transfer/swing profiles all remain false, with two Cartesian hand rows.

At first reach breach, immediately before release:

- COM `(7.715818882,117.425529480,-179.924316406)`; velocity `(-1.759725332,-1.592013597,-0.542094529)`m/s.
- Both hands are rigid/nonmaterial, attached to body sequence8391013/entity1960. Physical generations19/17 remain unchanged throughout the30 samples. World-traversal hand1 generation is15; those are distinct existing ledgers.
- Hand0 world anchor `(7.861814022,119,-179.279998779)`, COM distance1.707459331m, COM-local support anchor `(-.154640675,6.297035217,1.279083252)`.
- Hand1 world anchor `(8.141814232,119,-179.279998779)`, COM distance1.753731608m, COM-local support anchor `(.125359535,6.297035217,1.279083252)`.
- Actual solved forces are `(51.275054932,866.025390625,280.120330811)`N and `(51.277446747,866.025390625,280.120330811)`N, norms911.645N each. Relative velocities equal the player velocity because this receiver is static.
- Cartesian targets are `(-.228559196,.236899421,-.052443642)`; extensions approximately `(-.0530954,.875931680,.036513466)`m. Commanded player position is `(7.662723541,118.301460266,-179.887802124)`.
- Power bound3000W; cumulative conservative command debit34569.864627J, last debit33.333332J; supplied spring-rest-target work17421.759346J, absorbed1840.186051J, last supplied0J. These cumulative ledgers include prior traversal; they are not catch-only work.

`actual_hands_in_reach` rejects hand1 at1.753731608m against1.75m. `update_shimmy` calls `release_hang`, whose preclear dump still shows these loaded identities; then `clear_traversal` removes them. This is not material loss, invalid identity, force/work expiry, a radial transition or an explicit release command. The earlier duplicate reach-failure print comes from the shared posture eligibility query and does not itself clear the rows.

## Supported upstream cause and minimal owner correction

The real acquisition uses `probe_lip_pair` in `climb.cpp`, which tries half-spans `.22,.14,.08`m. Here it acquires the genuine `.14`m pair centered atx8.001813889. In contrast, `update_shimmy` in `hand_transfer.cpp` requires probes at the full `kHangHandSpan=.22`m. Its first+X step is `.6/90=.006666667`m, putting the trailing probe atx`8.001813889+.006666667-.22=7.788480556`,11.519mm outside this plate's minx7.8. This invalid full-width pair blocks the update before ledge commit or regrip.

Every trace sample retains hold `(8.001813889,117.949996948,-179.809997559)`, unchanged generations and zero cooldown. No `SHIMMY_MOVE` line is reached. Maximum sampled COMx is7.861999512, below the requested8.25.

The smallest owner-correct correction to evaluate is reuse `probe_lip_pair(at.ledge,traversal_normal_,at.body,...)` for shimmy's pair validation, retaining actual-body, height, clearance, distinct-hand, reach, placement and cooldown checks. That shares the existing honest narrow-edge contract rather than widening reach or manufacturing support. No shipping implementation or acceptance of this candidate occurred in this diagnosis assignment.

The loaded catch is also consequential: both upward Cartesian axis forces are already capped at866.025390625N per hand. For85kg and9.81m/s² gravity, maximal net upward acceleration is10.567068m/s². Braking the initial4.996974m/s descent ideally needs0.472882s and1.181489m of fall. The actual guard releases after0.322222s while still descending1.592014m/s. This explains why unchanged finite-force hands cannot instantly stabilize the catch while shimmy is blocked. It does **not** prove all corrected shimmy sequences physically impossible; changing actual hand placement changes the reach geometry. Increasing force or reach is not supported by this evidence.

Remaining risk: shared narrow-pair validation fixes a demonstrated owner inconsistency, but downstream capture/regrip/clearance and the complete hand-to-foot receiver route still require the parent's bounded implementation and affected acceptance path. This probe is native failure evidence, not APK or device proof.
