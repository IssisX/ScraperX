# Shared survival implementation and adoption plan

User-authorized side-branch work; the current request overrides the repository's older instruction to write/merge ScraperX-Claude. Base: 5f34d3dcefdbe2dd3eff3fa2dcac4fc34848e879.

Goal: a native, opt-in encounter controller coupling finite physical hand support with isolated/restartable machinery, exercised against real Jolt bodies. No new physics integrator, crane replacement, campaign coordinates, or shipping claim. Detailed results and remaining seams: `docs/SHARED_SURVIVAL_HANDOFF.md`.

Architecture: `shared_survival.hpp/.cpp` borrows existing bodies and hinge constraints. Surface queries and actual foot contacts permit compliant hand forces with equal reaction, bounded by grip and stance moment. Jolt owns all motion and a placed loose brace carries weight through ordinary collision. District channels operate existing hinge motors and passive friction brakes; mains restoration never starts a channel. `Matron` borrows the controller and suspends pursuit/catching only while hands carry load, then resumes ordinary sensing. The actor cannot attack, climb, or throw while bracing.

Files: new controller and `tests/shared_survival_tests.cpp`; register with CMake; small opt-in hooks in `matron.hpp/.cpp`; adoption notes in `docs/SHARED_SURVIVAL_HANDOFF.md`.

- [x] Write native falsifiers and observe failure before implementation.
- [x] Implement support, isolation, coasting/brakes, restart interlocks and Matron hook.
- [x] Run native fixture and existing regression suites; record exact limits.
- [x] Review diff and audit workflow; commit/publish only this side branch.

Fixture: a 100 kg hinged service flap, a 220 kg actor, ground and a loose steel prop. The actor supports only part of the flap's moment. Removing footing or adding excessive load must cause physical descent. An independent rotor on the same district must keep turning after isolation without a brake, then dissipate energy under a bounded brake. Restart must require stopped machinery, clear hands, closed physical guard and a fresh start command.

Core falsifiers: noncontact force; support above capacity; equal-reaction violation; overload held indefinitely; isolation cancelling velocity; brake injecting energy; mains restoration restarting machinery; bracing actor catching player. Tests report each separately. Campaign placement, save schema, visible hands, panel prompts, acoustic masking and device proof remain explicit integration work unless verified here.
