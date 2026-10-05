# Monorail waypoint driver diagnosis and bounded repair

Base: ChatGPT `2440aec43a3a594a065daf67f353ba7931ec7250`.
[Actions run 37334374774](https://github.com/IssisX/ScraperX/actions/runs/37334374774)
passed native and preceding cargo/touch gates, then failed
`touch_campaign_to_121` at monorail `(12.97421,23.17807,-125.2)`.
No APK was produced. This document records local follow-up, not CI delivery.

## Causal evidence

The exact CI command (Godot 4.7, headless, fixed-fps 60, ordinary full campaign)
reproduces the same position and exit 31. Unchanged d06b365 Godot source plus
the previously archived d06b365 native simulation library reproduces identical
upstream ticks and failure. Bridge objects and all other native sources are
unchanged by the release repair. Thus this blocker predates both repair commits.

The physical waypoint helper eases native input toward zero in proportion to
remaining horizontal distance. A capsule contacting the rounded edge of the
0.3 m-wide monorail repeatedly steps upward, contacts support 2563, slides off
and lands again. Persistent horizontal error never produces more input.
At 0.474 m error the requested native magnitude is only 0.172. This is above
the step trigger minimum; the problem is sustained approach/settling effort,
not a rejected Drop or hidden traversal state.

A scratch-only 1.5-second full-stick intervention from the same uninterrupted
state crosses the rail (x12.3195 at t7.02), then overshoots. No poses, velocity,
geometry or physical parameters were modified. This rules out an impassable
physical obstruction; it does not certify subjective player feel.

The bounded test-driver correction accumulates nearby position error (gain
0.4/s, command cap0.35, reset outside0.75m) and brakes measured support-relative
velocity (gain0.12). These are CHOSEN automated-pilot parameters, not physical
constants or relaxed acceptance limits. All commands still pass through real
touch/gamepad/keyboard input. Targets, timeouts, tolerances, grounded/low-slip
arrival conditions and the entire ordinary route are unchanged.

A paired falsifier changes only this waypoint's controller; upstream ticks and
poses stay identical. Velocity feedback without accumulation still fails at
`(12.81891,23.13187,-125.2001)`. Adding accumulation reaches the original settled
monorail endpoint and the required ladder-leap stance. These paired diagnostic
runs intentionally terminate at that endpoint; they are not full-campaign proof.
The general corrected helper separately passes the uninterrupted ordinary-grade
campaign to supported +121 m, support11, zero deaths and zero launcher work.

Only the shared Godot test driver changes. Native physics and gameplay input
code remain unchanged. Independent read-only review supports fixture repair;
it notes bounded windup, accumulation during bouncing, passive stop inside
arrival tolerance, and broader physical-waypoint scope. Do not claim this is
a production monorail physics/feel repair.

## Remaining verification boundary

The current native33/33 receipt is reused: this change does not alter native
code. Adjacent input checks discovered a separate existing failure in the
hanging head-rotation assertion. `touch_hang_drop` and `touch_hang_climb` report
`head rotation detached a loaded hanging wrist`; unchanged d06b365 reproduces
the same drop assertion. That scenario does not call the changed waypoint
helper. Report this blocker before widening scope. No new Actions run or push
has been made for this candidate. Device/Fold execution remains untested.

Adjacent input suite completed: **21/23 pass**. Both failures are the shared
hanging head-rotation assertion above. All keyboard/gamepad scenarios pass.
Per-scenario logs are retained under `input-gates/`. The entire campaign green
run uses the candidate helper; the repository copy differs only in its explanatory
comment. Diff whitespace checks pass. Candidate remains saved and uncommitted.

## Approved blocker closure

The owner subsequently approved the separate hanging presentation repair. A no-turn control disproved camera movement as the cause; the native hand pair and shoulder frame had opposite handedness. The bounded frame correction and red/green receipts are in `../hanging-shoulder-2026-10-05/`. Both previously failing hanging cases now pass, and the combined final source passes the full ordinary campaign again. The earlier21/23 result above is retained as historical evidence, not the current blocker status. Android delivery still requires a green exact-source Actions run.
