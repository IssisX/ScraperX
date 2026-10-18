# Contact-aware movement regression fixtures

Two existing native tests assumed behavior retired by the physical foot
and momentum-preserving slide owners. These corrections change test setup
and contact expectations; production physics and public APIs are unchanged.

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

Combined command: `python /data/data/com.termux/files/usr/tmp/scraperx-hall-arrival-20261018/run-arrival-repair.py`. Strict build exited0. The complete original athletic executable passed the +24m intake and both +40m routes, then exited1 at the later world-crouch return-to-standing assertion. This is scoped stair acceptance, not full-suite acceptance. `arrival-repair-result.json` and `arrival-run.log` preserve the actual commands/output against the unchanged coherent e014 native archive. The later crouch failure remains unresolved; x86 acceptance of these stair changes is pending.
