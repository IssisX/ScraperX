# West-apron playable ground loop — 2026-10-17

The normal receiving-yard opening now has a connected physical practice network:
approach from actual freight spawn, five running platform jumps, a deliberate
floor recovery through the gallery, crouch passage beneath the service duct,
return to open air and a sixth jump on retry. Docks, retaining faces and the
raised gallery provide optional branches; their full upper routes are unproven.
Movement phases2–8 remain separate work. This change adds terrain, not new
vault/wall-run/climbing actuator claims.

## Implementation

`ParkourYard` builds once at the end of the normal world builder. It appends59
exported box/strut collision records while retaining every existing box in the
same order and leaving all existing hull records/points intact. The entire yard
is translated4m east to clear the existing storage cage. Its recovery route
passes beneath the gallery; its east service aisle avoids existing timber.
No existing stock, collider, obstacle size or physical actuator law was removed.

Shared `WorldGeometry.add_strut` now converts its parent-local destination/up
into world coordinates for `look_at`. The prior mixed-frame call skewed braces
when their parent was translated. The existing static-batch test includes actual
endpoint checks with a translated/rotated parent. Its default complete check is
retained; the focused argument exercises this geometry-only path on headless hosts.

`ParkourStructure`/`ScrambleRung` export into native collision. `ParkourPaint` is
flush presentation only. Fifty-three shared-material edge/approach marking meshes and
five service-aisle marks enter existing spatial batches. A depth-tested floor
label and east-pointing paint cue directly ahead of spawn identify WEST APRON /
FOOT ACCESS; no additional lights,
shaders, frame callbacks, Godot colliders or gameplay authority were introduced.
The helper remains separate from the physical layout.

## Observed execution

Source: parentf09e034 plus this isolated integration; exact commands, inputs,
archive/source hashes and outputs are saved under
`/data/data/com.termux/files/usr/tmp/scraperx-west-apron-20261017` (Q).
The fixture excludes unrelated dirty grapple work.

- Pinned Godot4.7 Linux ARM64 under proot: imported actual project resources,
  then normal Main `--export-solids=Q/world_solids.inc` completed without script
  errors:1771boxes/666hulls;59additional physical shapes. Existing box prefix,
  hull records and hull point data are preserved.
- `Q/update_world_route.py` rebuilt the changed world owner, linked the actual
  native archive/bridge and ran `Q/build/parkour_yard_route_tests` through proot.
  `Q/shifted-route-receipt.json`:resultPASS, commandexit0. Final actual output:
  `SCRAPERX_PARKOUR_YARD PASS public_arrival=1 jumps=6 crouch=1 blocked_stand=1 recovery=1 retry=1`.
  Final native tick6453. No restart, pose/velocity setter or test-only support is
  used. Receiving platforms require actual grounded state and exported-world support.
- Same archive, existing `ground_yard_tests`:actual output
  `PASS native yard entry, back-wall collision, stock landing and clear lane`.
  Exact build/run commands and output:Q/ground-yard-command.json and ground-yard.log.
- `godot --headless --path Q/source/godot --script res://tests/static_world_batch_test.gd -- --strut-transform-only`:
  `SCRAPERX_STRUT_TRANSFORM_TEST failures=0` (pinned engine through proot;
  actual full invocation retained in the execution log/checkpoint).

## Failures, corrections and limits

The initial route hit existing stock atx=-82,z=-48. The corrected approach goes
north atz=-43.5. A later path hit the existing storage cage. It is retained;
the yard was moved4m east and the floor return uses the gallery's real headroom.
Moving the parent exposed the brace local/world coordinate defect above;
corrected endpoints, export and the final whole route were exercised afterward.
Earlier failure logs and the skewed generated include remain underQ.

A new geometry-check setup initially called material with too few arguments;
that introduced parse error was corrected before the successful focused check.
The full static-batch check on this headless host reports219foliage/pose failures;
the original unmodified test reports the same219failures. An independent empty
BoxMesh/camera project also aborts in local OpenGL with exit134 and --shm-helper
errors, as did the Main capture. These observations do not establish a new game
render defect. Local screenshots/rendered acceptance remain unavailable; see
Q/render-control-receipt.json, render-control.log, static-baseline.log and capture.log.
No graphics setting or test expectation was changed to hide these failures.

Signed matching APK, actual rendered/mobile appearance, touch feel/FPS and full
required release proof remain distinct gates. A successful native route or APK
export does not establish console-quality feel or a fully green release.

Entrance-cue follow-up: actual Main export instantiates the new label/arrow without
script errors; the generated native include is byte-identical to the accepted
route source. Reuse that physical route result; no repeat physics campaign is
required for this presentation-only cue. Q/entrance-export.log retains output.
