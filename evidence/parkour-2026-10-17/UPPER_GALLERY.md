# Connected upper-gallery branch — 2026-10-18

The west-apron course now has a higher precision-jump branch: climb the north
maintenance steps, follow the painted arrows across three alternating inspection
perches, rejoin the gallery and descend the south steps. Missing a jump leaves
an ordinary ground route back to the north steps for another attempt.

The new decks are1.4m wide and1.8m long, at3.0/3.2/3.0m. Their beams and
diagonal brackets attach to existing gallery post heads. No new ground columns
block the lower recovery aisle, and the existing storage cages remain intact.
The original lower gallery remains available alongside this branch.

## Implementation

`godot/presentation/parkour_yard.gd` owns the authored geometry. Its local
coordinates use the existing(+4,0,0)m yard translation. The new world-space
deck-top centres are(-69.8,3.0,-67.5),(-74.2,3.2,-78.5) and(-69.8,3.0,-89.5).
`parkour_route_markings.gd` adds inset edge paint, launch/receiving arrows and
entry/exit labels. Paint supplies no native collision or support.

The full Main SolidExport produces1792boxes and666hulls. The original1771box
prefix and all hull records/points are preserved exactly;21physical deck,
beam and bracket shapes are appended. The matching native world-geometry
owner and bridge were rebuilt. No movement force, gravity, actuator law,
collision size or native90Hz timing changed. Existing controls apply.

## Observed execution

Exact commands, source/archive hashes and execution records are in
[UPPER_GALLERY_COMMANDS.json](UPPER_GALLERY_COMMANDS.json). The local native
base is the accepted unchanged native cache plus this geometry; the clean
published GitHub candidate is a separate source/build gate.

- Actual Godot4.7 Main export under proot: `--export-solids=G/world_solids.inc`,
  exit0;1792boxes/666hulls. `python3 G/build_gallery.py` exited0 for changed
  geometry compilation, archive replacement, bridge link and both route builds.
- `ci/mechanics_trial.py --state G/upper-gallery-window.json run` executed
  `proot-distro login ubuntu --bind /data/data/com.termux/files/usr/tmp:/data/data/com.termux/files/usr/tmp -- G/upper-route`.
  Final trial5 exited0:
  `SCRAPERX_UPPER_GALLERY PASS perches=3 miss_recovery=1 retry=1 deaths=0 public_input=1 no_reset=1 tick=8098`.
  The test uses ordinary public movement, sprint and Jump from freight arrival,
  with actual receiver contact/footprint, flight and stable footing checks.
  The intentional miss, ground return, retry and south exit occur in the same
  Simulation without pose/velocity setters or restart.
- Same archive, existing `G/lower-route`, trial3 exit0 at6454ticks:
  `SCRAPERX_PARKOUR_YARD PASS public_arrival=1 jumps=6 crouch=1 blocked_stand=1 recovery=1 retry=1`.
- Direct Godot MCP `run_project`, deferred `run_script`, `get_debug_output`
  and `stop_project` exercised `upper_gallery_view_test.tscn` in actual Main
  with the matching native bridge:
  `SCRAPERX_UPPER_GALLERY_VIEW PASS checks=34 failures=0 staged_footing=1`.
  Each initial perch footing is explicitly staged for the rendered check;
  real keyboard Jump then passes through the shipping input router and Main.
  This does not replace the separate full public-input native route above.
  All three actual viewport captures were inspected. Runtime stopped cleanly.
- Scoped `git diff --check` exited0; the candidate workflow parsed with33unique
  step names. Its new native route and rendered gates retain existing checks.

G is `/data/data/com.termux/files/usr/tmp/scraperx-upper-gallery-20261018`.

## Failure corrections and limits

The first driver approach hit the preserved ground-swing frame1480 atx=-48,
z=-57. Its corrected walking route bypasses the frame north of the utility
plinths, then passes through their existing gap. The second run reached the
stairs but began its final run-up too close to the gallery riser: the capsule
stopped atx=-70.15, its.35m radius before thex=-70.5 face. The corrected
driver begins on the east side of each real step, preserving grounded run-up
and all receiving-contact requirements. Physics and geometry were unchanged.
Both failures and the evidence-based reassessment remain in the same trial log.

The host has no ALSA device and fell back to Dummy audio. Its software renderer
is not phone performance evidence. The matching APK, separate full-release
acceptance and physical-device play remain distinct; no device FPS, touch feel
or commercial-quality claim follows from these host checks.
