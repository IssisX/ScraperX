# Physical movement repair and contact-aware regression fixtures

The earlier fixture corrections below change setup and contact expectations.
The blocked-step section records a subsequent production walking repair;
its finite force, energy, reach and lifetime limits remain unchanged.

## Drop during a trailing plant

The intent-flow fixture requests Drop two ticks after leaving stock. It
has no hand grips, but still has a real foot constraint: the diagnostic
observed support 51 and 450.796 N. Commit b31341f deliberately permits
release of this foot; existing walking-foot, wood-foot and wall release
checks independently require accepting that release.

The corrected case first requires the actual trailing foot and its solved
contact identity, then expects Drop to accept its release. The original
assertion that Drop removes the foot and cancels upward takeoff remains.

## Crouch walking after a moving landing

The parkour-flow fixture retained 5.5 m/s after a walking jump, then crouched.
It correctly entered a real slide, with sprint reporting false throughout.
The movement spec and commit 37adde7 require preserving this earned speed;
the existing slide suite separately covers its finite drag and release.

The corrected crouch-walk case uses neutral input to reach actual slow
grounded footing within a finite bound before crouching. It rejects an
unexpected slide throughout the walking sample. The original false-sprint
and 2.6 m/s maximum expectations remain unchanged.

## Actual affected execution

`/data/data/com.termux/files/usr/tmp/scraperx-drop-contract-20261018/repair-receipt.json`
binds both changed sources to the coherent e014 native archive and exact
compiler/execution commands. Both complete test programs exited 0:

```text
PASS physical parkour intent all
PASS scraperx_sim step2 sprint: peak=8 air_slowest=8 sprint_jump_m=8.97771 walk_jump_m=6.16802 crouched_peak=2.475 across_peak=5.5
PASS scraperx_sim parkour flow
```

This proves the affected host cases. The broader full native suite has
other captured failures; these two repairs do not establish full-green
release acceptance or device play.


## Stair approach and hall arrival

The lower stair approach now targets the real landing centres at X=+/-8m. The former +/-7.5m path hits the end of the frozen StairRail collision; the trace identified entity51 during the raised capsule sweep. The handrail, stair geometry and physical stepper are unchanged.

Both hall approaches now use the existing settled walking controller toward X=-7.4m, inside the west deck strip [-10,-4.8]. The old target -4.51m lies in the well. A natural SKIN replay recorded first hall contact at X=-5.223m while moving east at3.093m/s; neutral input let it return to upper-flight support44. The repaired replay holds actual hall support49 through the original two-second neutral interval, at Y=41.0872m with zero deaths. All original final guards remain.

Command: `python /data/data/com.termux/files/usr/tmp/scraperx-hall-arrival-20261018/run-skin-repaired.py` exited0. Exact commands/output: `skin-repaired-result.json` and `skin-repaired-run.log` in that directory.

Combined command: `python /data/data/com.termux/files/usr/tmp/scraperx-hall-arrival-20261018/run-arrival-repair.py`. Strict build exited0. The complete original athletic executable passed the +24m intake and both +40m routes, then exited1 at the later world-crouch return-to-standing assertion. This is scoped stair acceptance, not full-suite acceptance. `arrival-repair-result.json` and `arrival-run.log` preserve the actual commands/output against the unchanged coherent e014 native archive. The later crouch assertion is corrected below; separate full-suite acceptance remains distinct.


## Standing height above actual support

The beam exit stands on a raised surface: actual support point Y=0.279592m
and standing body Y=1.17663m. The capsule had already stood correctly.
The final assertion now compares body clearance above each actual support
point, keeping its original 0.03m tolerance, grounded/standing requirement,
beam exit, speed and under-beam jump guards.

`python /data/data/com.termux/files/usr/tmp/scraperx-hall-arrival-20261018/run-world-crouch.py`
built and ran the complete original world/crouch module with exit0 against
the unchanged e014 native archive. The shared landing-header loaded caller
also passed: `run-hook5.py` exited0 with the original Hook5 transfer,
held-block and restore guards. Their existing result JSON and logs retain
exact commands; neither was rerun to write this report.

## Consistent public walking input

The ascent and swing-stair fixture drivers used an entity-dependent approach
scale that reduced input near physical supports. A timed duct arrival missed
its unchanged 0.03m tolerance by about0.0006m. The fixture drivers now use the
existing ordinary settled-walking feedback (position error times1.8 minus
support-relative velocity times0.28), retaining normalization, every deadline,
position tolerance, grounding, slip and checkpoint check.

This exposed a real separate 30cm monorail step stall. The original full
public-input swing-stair route is retained as the integration acceptance.

## Production blocked low-step lift

The stalled capsule repeatedly began lower-deck foot strokes at X~12.98m,
then expired after0.244444s with only~20J spent and about5cm of lift. The
walking target depended on forward progress that the steep riser prevented.
The existing3500N/5000W/1285.625J bounds were not exhausted.

Normal approach timing is retained. A newly begun walking stroke can commit
its validated finite height only when the original capsule sweep already
hits an opposing, nonwalkable riser at zero travel. All previous clearance,
receiver-rise and fresh actual lower-foot requirements still apply. The new
owner operation cannot create an attachment or recommit an aged stroke.
Neutral, reverse, release and restart retain their existing cancellation.
Actual receiving contact still retires the foot; no position, velocity,
gravity, force, reach, work or lifetime override was added.

A rejected candidate shortened all approach distances and failed the
unchanged ordinary-stairs case. Its patch and trial1 remain preserved.
The accepted candidate changes blocked starts only. Exact experiments use
one persistent limiter record:
`/data/data/com.termux/files/usr/tmp/scraperx-hall-arrival-20261018/monorail-step-window.json`.

Accepted host commands (strict C++17, actual Jolt, Release checks enabled):

- `python .../blocked-step/run.py walking`: all6 original modes exited0.
  Ordinary input0.5 completed10 steps in124ticks; full input completed9 in175.
  Both recorded zero compulsory slow ticks and peak force3500N.
  Neutral/cancel/reverse/restart retained their original unpowered-row guards.
- `python .../blocked-step/run.py route`: the complete original public-input
  grade -> pipe -> facade -> swing stair route exited0 at supported+44m,
  tick14896, grounded, gravity1 and zero deaths.
- `python .../blocked-step/run.py ascent`: the complete original grade-to-facade
  route exited0 at supported+33m, including its deliberate fall and real
  checkpoint restore. `ascent-result.json` records the exact command/output.
- `python .../blocked-step/run.py owner`: the complete physical-foot program,
  including the new attachment/no-renewal case, exited0. The new case retains
  every shared finite, force, work, stroke and lifetime check. Walking uses
  the existing motor event-work ledger, while spring cases retain their
  original force-times-target-extension ledger. The isolated test uses
  production gravity; the initial zero-gravity and wrong-profile failures
  remain in the experiment evidence and were corrected without changing
  production code or weakening any prior test.

Here `...` is `/data/data/com.termux/files/usr/tmp/scraperx-hall-arrival-20261018`.
The directory's `blocked-step/{commands,walking-result,route-result,owner-result}.json`
contains the full actual commands and output paths, not abbreviated commands.
The native artifact is `blocked-step/libblocked-step.a`, SHA256
`1340ac91d0d46d137c6f7b613478c552af658c354058acecaee060e95636d7ad`;
it replaces only locomotion and physical-foot objects in the coherent e014
archive. The focused header overlay preserves the existing Simulation ABI.
Separate GitHub full-native results and phone play/feel remain distinct.
