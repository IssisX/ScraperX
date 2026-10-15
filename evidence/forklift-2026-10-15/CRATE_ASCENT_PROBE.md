# Original crate public ascent probe — 2026-10-15

Result: **PASS native runtime**, exit0. Read-only production diagnostic at checkout `ChatGPT`, HEAD `1f92dca096c007e95681dc0f73f400a171c17f94`, with existing concurrent dirty source preserved. No production or root test files changed. Owned private source/receipt/logs live in `/data/data/com.termux/files/usr/tmp/scraperx-forklift-crate-ascent-20261015`.

Behavior: original actual600kg crate A/entity2444 at(-100,0,-48),1.5m roof. Exactly one initial player debug stage(-100,.9,-46.35),45ticks to confirm actual grade footing, face(0,-1), move(0,0), one public `request_traversal()`. No cargo transforms, subsequent player poses, force/reach/clearance changes, fake contacts or coefficient sweeps. Probe bound900ticks; completed at237ticks after request, including90continuous ticks of grounded support2444 after traversal/hand release.

Commands run:

```sh
proot-distro login ubuntu -- python3 /data/data/com.termux/files/usr/tmp/scraperx-forklift-crate-ascent-20261015/build.py
python3 ci/mechanics_trial.py --state /data/data/com.termux/files/usr/tmp/scraperx-forklift-crate-ascent-20261015/window.json start --objective 'Original600kg crate2444 physical player topout using public Action; isolated bounded approach diagnostic for root cargo route' --budget 600
python3 ci/mechanics_trial.py --state /data/data/com.termux/files/usr/tmp/scraperx-forklift-crate-ascent-20261015/window.json run --stage original-crate-public-topout --source /data/data/com.termux/files/usr/tmp/scraperx-forklift-crate-ascent-20261015/probe.cpp --source /data/data/com.termux/files/usr/tmp/scraperx-forklift-route-20261015/libscraperx_sim.a --source /data/data/com.termux/files/usr/tmp/scraperx-ci-soft-velocity-20261015/jolt-build/libJolt.a --source src/sim/simulation.hpp -- proot-distro login ubuntu -- /data/data/com.termux/files/usr/tmp/scraperx-forklift-crate-ascent-20261015/probe
```

Build: native C++17, O3, pthread, Wall/Wextra/Wpedantic/Werror, JPH_DEBUG_RENDERER/JPH_OBJECT_STREAM/JPH_PROFILE_ENABLED/NDEBUG/SCRAPERX_HAS_JOLT=1. Exact compiler argv and header/test/archives hashes: private `build-receipt.json`. Compiler exit0. Host `/usr/bin/c++` absent; existing Ubuntu guest compiler used successfully. Guest launcher emitted binding warnings during compilation, with no build/runtime failure.

Exact runtime output (`window.trial-1.log`):

```text
T=0 player=-100,0.9,-46.35 crate=-100,-5.96046e-08,-48 grounded=1 support=1 state=0 phase=0 reject=0 receiver=0 hands=0 hand_N=0 foot_N=0 foot_tick=45 transfers=0 accepted=0 rejected=0 aborted=0 ledge=0 available=0 ledge_point=0,0,0 hand_point=0,0,0 target=0,0,0
T=1 player=-100,0.9,-46.35 crate=-100,-5.96046e-08,-48 grounded=1 support=1 state=4 phase=4 reject=0 receiver=2444 hands=2 hand_N=244.2 foot_N=0 foot_tick=46 transfers=0 accepted=0 rejected=0 aborted=0 ledge=0 available=0 ledge_point=0,0,0 hand_point=-99.78,1.5,-46.92 target=-100,2.42,-47.27
T=180 player=-100,2.4,-47.3009 crate=-100,-5.96046e-08,-48 grounded=1 support=2444 state=0 phase=0 reject=0 receiver=0 hands=0 hand_N=0 foot_N=0 foot_tick=225 transfers=1 accepted=1 rejected=0 aborted=0 ledge=0 available=0 ledge_point=0,0,0 hand_point=0,0,0 target=0,0,0
T=237 player=-100,2.4,-47.3009 crate=-100,-5.96046e-08,-48 grounded=1 support=2444 state=0 phase=0 reject=0 receiver=0 hands=0 hand_N=0 foot_N=0 foot_tick=282 transfers=1 accepted=1 rejected=0 aborted=0 ledge=0 available=0 ledge_point=0,0,0 hand_point=0,0,0 target=0,0,0
PASS actual crate topout and90ticks released-hand footing
```

SHA256 inputs recorded by mechanics runner:

- `/data/data/com.termux/files/usr/tmp/scraperx-forklift-crate-ascent-20261015/probe.cpp`: `ef961ac0e6b6a75cf4e41b008302a7be7c14b771fb0100fe65a125e05f0d2cf1`
- `/data/data/com.termux/files/usr/tmp/scraperx-forklift-route-20261015/libscraperx_sim.a`: `1b99a1332e46e44ebee52b5b96ea8f35c8f139ff546418263c64f7e80d1214ae`
- `/data/data/com.termux/files/usr/tmp/scraperx-ci-soft-velocity-20261015/jolt-build/libJolt.a`: `8711a784f394fdf91af3366ab0b6695e80322cb73b46a8d1659e775b20aced1b`
- `/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo/src/sim/simulation.hpp`: `2eae6317e29f857cd1ad82d70acd2e4a735cf7a6dc8d589bfe79e4df6962f99a`

Runtime elapsed: 9.41seconds. Result proves this original stationary crate geometry can accept real hands and complete finite native transfer onto actual cargo support. No causal failure identified; no repair warranted by this probe.

Coverage/proof gaps: full cargo pickup/transport/offset stacking/withdrawal and player gallery/onward/retry path remain root-owned acceptance. This diagnostic does not prove crate A after cargo placement, second crate transfer, gallery arrival, Godot input/rendering, Android build/device behavior or current later source.180tick observation intervals miss the short simultaneous hand/foot force phase; the existing production foot_transfer_count=1 is the completion receipt, while logged forces directly prove244.2N native hand load at tick1 and zero retained hand rows after arrival.

Next discriminator: inspect root full route's first actual failure. If crate A there fails, compare its settled physical pose and player approach to this passing original-case observation before changing mechanics.
