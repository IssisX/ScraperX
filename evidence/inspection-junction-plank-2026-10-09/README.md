# Inspection junction and first deformable plank — 2026-10-09

Source parent: `32277306177aad94c122ec698a64165ce278d06c`, exact `IssisX/ScraperX` / `ChatGPT`. The accompanying source receipt records the changed source hashes. These are local native/Godot observations, not an Android release or phone playtest.

## Completed gameplay

The normal-world inspection junction connects the existing 396.25 m west roof to the existing 418.25 m floor. Ordinary inputs traverse the lower north brace, board the passive 3 m hanger, pump through finite body work, release onto the actual 407 m receiver, cross the timber interruption and ascend the upper brace to secure onward roof footing. The primary driver completed all of this with gravity enabled and zero deaths. The place is authored as exposed inspection/maintenance access: steel arrival footing, a timed moving-bar commitment, a narrow loaded timber crossing, and a lower catch tray with a real return brace. This is authored experience, not verified phone feedback.

The 2.4 m plank has twelve real native timber bodies and eleven elastic SixDOF joints. Its 85 kg standing load adds 31.30 mm sag; ordinary walking crosses all eleven seams. A real two-hand catch adds 25.25 mm sag, with measured hand forces below 1,500 N. Passive release reaches the actual recovery tray and the unloaded shape returns. A staged three-metre gravity fall breaks three internal sections, separates native fragments and allows ordinary steering onto checkpointable steel. Damage remains after landing; the right-hand fragment reaches the tray after losing its finite roller bearing.

## Evidence boundaries

- `native-elastic-fracture.log`: isolated signed-axis/frame/spring tests; gravity-off pin/roller and cantilever calibration; interior overload, real fragment contact/query, exact momentum continuity at separation, and module + Kit topology restoration. Pure pin/roller is ideal and axially unbounded; production guided bearing is finite.
- `native-player.log`: normal-world standing, walking eleven seams, finite Jump reaction/landing, two-hand catch, passive release, unload and onward steel. This runs with strength failure enabled. Its Jump is a short hop, about 0.20 m/s upward, because the current instantaneous work cap accounts for the actual light segment's point inertia. It does not establish normal 5.5 m/s takeoff on timber.
- `native-player-impact.log`: one explicitly staged airborne start, followed by ordinary gravity and stick input. Native 85 kg impact produces mask 112 and peak strength ratio 1.34115, then a real 403.5 m recovery with persistent damage. This is an impact scenario, not proof of a reachable three-metre launch approach or post-damage production restart.
- `native-junction-route.log`: final connected normal-world 396.25 → 418.25 m primary route with plank present, including actual receiving contact and onward walking.
- `native-junction-recovery-before-plank.log` and `native-crown-regression-before-plank.log`: explicitly earlier, scoped boundaries. Recovery brace geometry and original Crown pivot/profile were demonstrated there; these are not new final-source campaigns.
- `native-build.log` / `native-impact-build.log`: supported C++17 ARM64 Release native/bridge build and subsequent impact-driver build. Pinned Jolt 5.6 and existing 90 Hz/four-substep production scheduling are unchanged.
- `visual.log`, visual receipts and PNGs: existing Godot 4.7/Xvfb/softpipe, filtered fourteen-body harness. All twelve rendered segment transforms match native render transforms, grain coordinates stay attached, and actual player loading visibly bends the plank. The low-resolution side view does not establish full-game appearance or phone readability/performance.

| Load | Jolt | Discrete | Continuum | Jolt/discrete error | Discrete/continuum error |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1,000 N centre, pin/roller | 37.30376 mm | 37.34369 mm | 36.83213 mm | −0.10692% | +1.38889% |
| 100 N physical cantilever tip | 59.07540 mm | 59.13603 mm | 58.93141 mm | −0.10253% | +0.34722% |

The partial energy check rejects earlier numerical runaway; full three-axis/contact energy closure is not claimed. Interior brittle strength is an explicit authored surrogate (40 MPa tension, 45 MPa compression, 6 MPa shear), not certified timber grading. Removing a failed spring discards an estimated strain energy without adding recoil. Cantilever root fracture, grain splitting, fatigue and resolved crack propagation are not implemented.

Actual commands used the existing Ubuntu proot host build: `scraperx_deformable_plank_tests` (default axes + elastic + fracture), `scraperx_deformable_plank_player_tests` (walking + grip at its recorded source boundary), `scraperx_deformable_plank_player_tests impact`, and `scraperx_inspection_junction_route_tests`. No broad native regression campaign or physical-device test was run.

## Remaining work

Native finite-duration foot push-off on very light articulated supports; connected touch/phone route feel; full-game plank and encounter visual quality, fracture sound/feedback, and Android frame-budget measurement. The isolated solve costs about 5 ms/tick for the supported member on this host, so adding many such members requires targeted optimization or profiling first. New Actions APK delivery is pending. Wider canonical machine, parkour, world and visual requirements remain active.
