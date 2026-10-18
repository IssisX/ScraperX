# Workshop wall-run receiving deck

The workshop wall-run now branches onto a ground-bearing steel stand. Run
along the workshop front, use Jump to push away from a real wall plant,
then steer onto the stand. Walk off toward the south yard to continue.
A missed kick lands on grade; return beside the wall for another attempt.

The stand is 4.0 x 2.8 m, with a top at 0.65 m. Its world centre is
(-38.5, 0.65, -40.6). Four posts and two braces support the steel plate.
All seven parts have actual exported native collision and steel sound
identity. Paint has no collision. Existing 1,792 boxes and 666 hulls retain
their geometry; seven boxes and seven sound tags are appended.

No movement actuator, force, work, stroke, duration, gravity or recovery law
changed. The original straight departure remains available. The new route
uses the same two finite wall plants and spends the remaining second-plant
work when Jump requests a push-off; it receives no additional impulse.

## Actual host evidence

The persistent experiment and command receipt is
`/data/data/com.termux/files/usr/tmp/scraperx-wall-exit-20261018/wall-exit-window.json`.
Its trial 6 records the actual public-input test command, source/archive
hashes and exit 0. Output:

```text
SCRAPERX_WALL_EXIT PASS public_arrival=1 wall_runs=3 same_plant_kicks=3 steel_receivings=2 firm_ticks=90 onward=1 ground_miss=1 recovery=1 retry=1 resets=0 deaths=0 tick=4647
```

The unchanged `physical_wall_stride_tests` flow, jump and cancel modes run
once against the final geometry in trial 7, exit 0:

```text
ORIGINAL_WALL_RUN_REGRESSIONS PASS modes=3
```

Actual exposed Godot MCP tools ran the shipping Main/native graph. Initial
deck footing was staged only for this focused presentation/input check;
the separate native test above proves the complete unstaged route.
Keyboard Jump, actual steel impact metadata and ordinary stable ground
exit passed: `SCRAPERX_WALL_EXIT_VIEW PASS`, 162 checks, zero failures.
The complete tool calls and output are in the same temporary directory's
`mcp-receipt.json`. The interaction run disabled host 3D; a later 640x360
softpipe screenshot timed out after 20 seconds. It does not prove visuals
or phone performance. Exact-source rendered CI captures remain required.

## Corrections supported by observed failures

- The first receiver position was 0.4 m too far out. Trial 4 shows the
  capsule hitting the rounded edge below top height and sliding to grade.
  Translating the complete stand 0.4 m closer yields actual top contact
  and 90 consecutive firm ticks without changing the physical actuator.
- The initial route fixture incorrectly assumed arrival support entity 1.
  The real freight arrival supplies entity 2446; the test now accepts actual
  grounded nonzero support, matching the existing public yard contract.
- A south-side missed-jump return ran into the existing access stairs.
  Their native bounds are X[-36.8,-35.2], Z[-47.4,-44.2]; the adjoining
  plinth is X[-37.6,-34.4], Z[-44.2,-41.4]. The test and paint now use the
  clear front corridor at Z=-38.6. The stairs remain intact.
- The focused Godot check initially assumed 45 ticks was enough to stop
  after stepping off. Actual landing recovery still carried 2.628 m/s.
  It now waits, within a finite bound, for real stable footing below
  0.5 m/s. The momentum and recovery laws remain intact.

Candidate and full workflows are separate. Source-matched CI/APK delivery
is recorded in `WALL_EXIT_DELIVERY.json` when available. Phone feel and
heard audio require play on the matching APK.
