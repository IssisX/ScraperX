# Active goal: progress on ScraperX-Claude

On 2026-10-09 the owner set the ten-phase goal, plus Phase 10, paid-release readiness. Its full
specification is `03_EXECUTION/ACTIVE_GOAL_PHASED_SCOPE.md` on branch `ChatGPT`. The owner chose to
pursue the same goal on this branch: *"Same goal, ScraperX-Claude"*. Read that file from `origin/ChatGPT`.
This branch's plans of record stay `PLANNING/MECHANISM_ASCENT_PLAN.md` and `PLANNING/RELEASE_PLAN.md`.

The percentages are rough estimates. Each one says what it rests on. A phase marked *not assessed*
has not been checked against its scope here.

| Phase | Estimate | Basis on this branch |
|---|---|---|
| 0 Preserve and set the frontier | 80% | Every route and repair below is kept and passing (the native suite and 55 touch scenarios, local, on `ea58a2b`'s tree). This file sets the frontier. |
| 1 Finish the current connection | 95% | The tram, stone wheel and slab incline are placed, played on touch and green in CI (`ea58a2b`). The one-run climb from grade to 803.5 m passes locally (`a4a7a19`); CI has not yet run on it. |
| 2 Responsive locomotion, momentum | not assessed | The ground and air constants are 22 and 14 m/s² (`simulation.cpp:81,97`), and riders inherit a moving support's motion. The scope's support-contact, braking and air items have not been checked. |
| 3 Continuous climb, mantle, vault, catch, swing | ~35% | Mantle closing and the two-phase vault are native. There is no body-driven pumping: the swing (AS-012) is struck, not pumped. Catches are not checked against the compliant-catch items. |
| 4 Impact recovery, rolls, stumbles, balance | ~5% | Native has no roll, stumble or XCoM (`grep` over `simulation.cpp`: 0 hits for stumble and XCoM). The 20 m/s lethal rule and a presentation landing dip exist. |
| 5 Controls and tactile feedback | ~40% | Touch, pad and keyboard share one verb set. Haptics are thin: the balance profile is defined but never fired (release audit). |
| 6 Coherent places and encounters | ~30% | Many stages are distinct, but all eight supplied machines are worked the same way (board, press RAISE, wait, exit). The scope names that as a defect. |
| 7 The gravity-fed wheel | ~70% | Worked with the body (2026-10-09): the feed plank opens the hopper and holds the wheel, stone alone drives it, and the brake only resists. Proven: too little stone doesn't lift the rider, and a missed boarding can be retried (native `wheel`, `touch_wheel`). Open: the stone's look and sound (high-quality material, spill and pile), reload of the finite hopper, the phone. |
| 8 Remaining supplied designs | ~60% | Eight of ten designs are placed and played on touch. The rack climber and gravity balance are not placed. The press-RAISE repetition is under phase 6. |
| 9 Verification and delivery | ~60% | The suite is proportional and CI builds a signed APK. No device run has been recorded. |
| 10 Paid-release readiness | ~15% | The unbroken route (no slingshot) is done. Save and continue is done (native `save`, `touch_save`). Title, ending, release build, store identity, licences and device measurement are open (`RELEASE_PLAN.md`). |

**Frontier (2026-10-09): Phase 7, the gravity-fed wheel as a complete encounter.** Phase 4 is less
complete, but the owner ranks the wheel as the important machine priority. Making it an encounter also
removes its press-RAISE control, the phase-6 defect. Save and continue (phase 10) followed. Next: phase 4's landing recovery
and rolls, and phase 10's title and the top.
