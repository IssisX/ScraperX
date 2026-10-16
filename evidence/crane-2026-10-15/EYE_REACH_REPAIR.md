# Actual lifting-eye reach repair — 2026-10-15

The focused native regression reproduces rear lifting-eye self-occlusion in the existing full-world forklift, entity 2440. The repaired owner recognizes the target eye's actual steel top-bar leaf. The reach limit, firm-support conditions and obstruction rejection remain authoritative.

`tests/yard_crane_rigging_reach_tests.cpp` uses one fresh Simulation per eye. Only the initial public restart stages the player on existing grade at truck X ±1.15m and Z ±0.75m. Ninety actual native ticks settle the truck and establish grade contact. Each expected leg is independently authored as 0, 1, 2, 3. The test requires real grounded support, gravity 1, no traversal hand constraints and no pose stepping.

The negative case adds an actual Kit-owned static steel cube (entity 900001), 35 mm half extents, 70% along the hand ray to eye 0. It clears the player's capsule and the ground. One actual native tick refreshes the public snapshot; the cube must prevent rigging. Test-local explicit-instantiation access only reads body transforms and adds this owned solid; it never writes player/contact/support/reach state. Kit destruction cleans up the obstacle.

## Native result

- Frozen v6 baseline: build exit 0, run exit 1. Eyes 0 and 1 are reachable and the physical cube blocks eye 0. Rear eye 2 returns rigging_leg=-1 despite grounded=1, support=1, gravity=1, cab=0, reproducing the erroneous top-bar rejection.
- Repaired v7 archive: build exit 0, run exit 0. Eyes 0, 1, 2, 3 return their intended leg and reachable=1. The same physical cube produces leg=-1 and reachable=0.
- This is actual native Simulation/contact/query evidence. It does not establish loaded freight transfer, Godot runtime, Android device behavior or APK delivery.

Both compilations use the exact `forklift_boarding_tests` command from `native-build-receipt-v6.json`, replacing only test input, executable output and (for GREEN) the Simulation archive. Frozen v6 headers and pinned Jolt archive are shared. Complete argument arrays and logs are retained in the private build directory below.

Build directory: `/data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015`.

Commands: `proot-distro login ubuntu --` followed by the arrays in `eye-reach-red-command.json` / `eye-reach-green-command.json`. Runs: `proot-distro login ubuntu -- /data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015/eye-reach-red` (exit 1), and the corresponding `eye-reach-green` (exit 0).

## SHA-256 receipt

- test: `214daae8ff2e203343ba8a2a362e4cfd177adec67c7e47f6f9f56efdb52c1c0b` (`/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo/tests/yard_crane_rigging_reach_tests.cpp`)
- baseline_archive: `dde1e3e70704a32dd1b68de62430b918681734cc79cbaa219358362cfffb0c81` (`/data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015/build-v6/libscraperx_sim.a`)
- repaired_archive: `29e2c8ad94ce24dc7b977ab26b94dbb9fc63a3b03ac52def7ae082417cdb6eeb` (`/data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015/build-v7/libscraperx_sim.a`)
- red_binary: `6cea05cd5ebabaf7db850e1979c042f65a1e5b6999d23b6036e42b79aacf30e5` (`/data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015/eye-reach-red`)
- green_binary: `f0e4fa4edcb750e486070710fcfee4f6ab57c4a27d10e49600324dc6a42d53c6` (`/data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015/eye-reach-green`)
- red_log: `1ababd2f93b79e6b142dccb068ce3d076233667cd25a71de42c6b27d0a28545d` (`/data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015/eye-reach-red.log`)
- green_log: `8b4daa1cf6ee10ce934d1de2d57d5c2e4a447bfeac46aa25f572acd505535fd6` (`/data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015/eye-reach-green.log`)
