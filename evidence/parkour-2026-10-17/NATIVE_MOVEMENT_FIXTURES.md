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
