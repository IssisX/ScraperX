# Finite step input negative regressions

`tests/physical_step_intent_tests.cpp` adds two public `Simulation` cases against the repaired native owner. Each case warms the real world for 45 fixed ticks, uses one initial `debug_restart_at({-100.98, 1.074, -57.65})` tread fixture, then drives the real truck approach with ordinary move/facing/Action inputs. The test never assigns support, pending state, geometry, forces, velocities, gravity or actuator readback. Explicit-instantiation private access observes only `physical_step_pending_`; actual hands and motion come from the public snapshot.

1. An invalid Action while a finite walking step is rising must acquire no hands and preserve the pending walking intent. The player faces away from the guard for the actual rejected reach. This catches a repair that clears pending merely because Action was requested. The next real contact is the transient rounded riser contact, so it also catches the previous grounded-only completion condition.
2. A deliberate Action during the later rising airborne phase must acquire actual finite hand constraints, clear pending and retain gravity. This catches an overbroad suppression that blocks explicit Action along with automatic catches.

Observed native AArch64 result on the Termux host through Ubuntu proot, using the frozen source-v6 native archive:

```
STEP_INTENT invalid-action-before x=-100.979 y=1.09185 vy=1.69157 grounded=0 pending=1 hands=0
STEP_INTENT invalid-action-after x=-100.977 y=1.10989 vy=1.58257 grounded=1 pending=1 hands=0
STEP_INTENT valid-action-before x=-100.923 y=1.2054 vy=0.862468 grounded=0 pending=1 hands=0
STEP_INTENT valid-action-after x=-100.909 y=1.21402 vy=0.722973 grounded=0 pending=0 hands=2
PASS STEP_INTENT invalid Action retains intent; explicit rising Action catches
```

The strict compile used the existing `forklift_boarding_tests` command from `/data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015/native-build-receipt-v6.json`, replacing only test source/output. Flags include C++17, O3, NDEBUG, Wall/Wextra/Wpedantic/Werror, pthread, and the matching Jolt definitions. Compile and execution both exited 0.

Native archive SHA256: `dde1e3e70704a32dd1b68de62430b918681734cc79cbaa219358362cfffb0c81`.

Logs: `/data/data/com.termux/files/usr/tmp/scraperx-forklift-operator-20261015/build-v6/physical_step_intent_compile.log` and `physical_step_intent_run.log` in the same directory. This is native host runtime evidence; it does not establish x86 CI or Android device behavior. The targeted fixture covers invalid and valid Action during a real step. It does not claim descending recovery or ordinary Jump coverage.
