# CI53 loaded stair handoff diagnostic assignment

Baseline: ChatGPT / d1af237bba8abd1123d7724b08088a8b1cece3fe. Sole repository write: tests/simulation_tests.cpp. Root owns concurrent changes, integration, commit, push and delivery.

## Finding and change

The supplied actual x86 Ubuntu24/GCC13.3 run38063033252 fails the existing combined handoff assertion. Its support and held-body operands were unobserved. A focused ARM64/GCC15.2 probe preserves the complete Hook hoist/slew/release/cradle/belt/cage/block/door/loaded stair sequence and captures each turn and climb. It succeeds: all six landing contact heights are 4.1872, 8.1872, 12.1872, 16.1872, 20.1872 and 24.1872m; support44 remains real stair support and held56 survives. Final stopped handoff is support45/held56, y25.0818, deaths0, traversalNone. No exact x86 reproduction is claimed.

Source: walk_to(..., settled_arrival=true) proves horizontal tolerance, grounded and low horizontal speed; it does not independently identify elevation or support. The combined assertion rejects invalid handoff states. The six preceding walk_toward intervals are timed; actual snapshot support groups all six native flights under entity44. The native carry can release when hand/handle separation exceeds0.90m and grows. None of this establishes which x86 operand failed.

The maintained change is diagnostics only. It prints foot, each turn/climb and the handoff BEFORE either handoff assertion. Each receipt includes driver flight command, tick/time, target, real position/velocity, grounded/support/contact, held ID, block position, deaths, traversal and traversal target, with explicit on_handoff and block_held booleans. A separate waypoint_reached receipt survives a failed walk_to. Contact elevation identifies actual footing without inventing a native per-flight ID.

All754 require messages survive;753 require calls remain byte-identical. The other assertion now reads the stored result of the identical walk_to call, after its snapshot is printed. All loaded route walk arguments, budgets, strong support/block assertions and ordinary native inputs remain unchanged. No production or native physics changes; no teleports, setposes, velocity manipulation, fake footing, alternate authority or recovery loophole.

## Exact next discriminating observation

Run this diagnostic source on actual x86 CI and inspect INFO AS-003 loaded stair. First distinguish on_handoff from block_held at phase=handoff, then inspect preceding turn/climb contact heights and held IDs. A wrong elevation with waypoint_reached=1 proves arrival on lower footing; a missing held56 with correct contact elevations directs investigation to load/hand contact and the earliest changed phase. deaths reveals a fall/reset rather than a successful ascent. Native carry escalation needs actual supporting evidence. No speculative driver correction was shipped.

## Private receipts

- loaded-stair-probe.cpp/log/exit: single focused new ARM observation; never the unchanged full baseline.
- baseline.cpp / final-source.cpp / changes.diff: exact assigned before/after source.
- assertion-preservation.json:754 messages, unchanged loaded walk calls.
- build.log/exit: one affected target build.
- athletic-final.log/exit: one final maintained athletic execution (result appended below when terminal).
- source-receipt.json: source/executable/archive hashes and honest platform limits.

No other53 gates, Godot, Android or device checks were run. No commit/push/publish or agents/services invoked. Shared host-build is released only after the final executable terminates.

Final verification: build exit0; maintained athletic executable exit0,27 PASS receipts,0 FAIL. Source stayed unchanged through runtime; native/Jolt archive hashes stayed frozen. Every PASS receipt is byte-identical to the reused CI52 ARM evidence. Shared host-build released. Source SHA256: 0f166e9ad10d03556d2182462f644903f3f62f5891a8d8cf12bd9bc0d1a62336.
