# Reusable material-line crossing

The carried line now accepts a second ordinary beam clamp or existing rated eye.
It retains its installation while the player grips, traverses, releases or changes
tools. The movement stick advances alternating material grips through the existing
finite hands. Drop and Jump release; PACK requires local, unloaded, settled equipment.

## Model and boundaries

Two preallocated 0.25 kg translation-only material masses join three tensile cable
elements. Material coordinates and masses stay fixed during regrips. No twisting,
bending stiffness or remeshing is implemented. Three series elements use three
times the original stiffness/damping per element, preserving the whole-line axial
response and the existing 3,000 N strength. The masses are an explicit reduced-order
model, not a rope-density measurement.

Each actual hand point distributes its reciprocal reaction to its two material
endpoints. Existing 5,000 N/m, 180 Ns/m, 1,500 N hand limits remain. Traversal uses
the existing 500 W / 0.6 m/s target command and measured spring-work debit. Posture
correction uses that same budget. Float target components round toward the old
target when addition would exceed the requested stroke; work/speed checks remain.

Existing exact Box cable routing remains. A grip on a contacted element is rejected
until routed material-point derivatives exist. Other unobstructed elements remain
grippable. Cold restore saves node state, anchors and material topology, then rebuilds
rows without resurrecting grips or solver impulses.

## Observed local evidence

Base revision: `14c2b3a`; implementation and affected tests are in this change.
`P=/data/data/com.termux/files/usr/tmp/scraperx-portable-line-20261020`.
Exact compiler/runner commands and full observed output are retained in these receipts:

- `P/crossing/public/fixed-posts-results.json`: executable `fixed-posts` exited 0.
  Ordinary FreightYardArrival, two clamps on the existing fixed bearing posts,
  two alternating regrips, 48 loaded airborne ticks, receiving footing, intentional
  miss onto grade, same-life retry, local PACK, alternate two-ended installation
  on trestle 1491, recovered stock and onward walking; deaths 0, resets 0.
- `P/crossing/combined/bounded-command-results.json`: actual tool/hand/line fixture
  exited 0; 1,572 loaded intervals, eleven regrips, 3.00008 m travel, peak 1,923.82 N;
  exact release velocities, installation persistence, cold restore and loaded PACK
  rejection. This fixture alone does not establish receiving footing.
- `P/crossing/material-hand/bounded-target-results.json`: new material-hand checks
  3/3 and existing physical-hand checks 18/18, exit 0. Actual force saturation
  1,500 N; loaded target work 1.38884 J within 500 W times 1/360 s.
- `P/crossing/material-owner/results.json`: native material owner exited 0;
  both free off-center supports translate and rotate, momentum error
  2.26022e-6 kg m/s, exact cold node positions/velocities. This isolates load transfer.
- `P/crossing/span-path-results.json`: prior and new actual two-ended geometry
  checks exited 0, including compound leaves and COM-offset derivatives.
- `P/crossing/godot-mcp-crossing.json`: actual Godot MCP `run_project` plus
  `run_script` executed the shipping Main/native touch case: 19 checks, no failures,
  native tick 355, one actual regrip. A single staged start on existing trestle
  geometry; public native evidence above supplies normal arrival/airborne route.
  The later release-label assertion and rendered wrapper run in candidate CI.

The moving hanger-bar pair can exceed the line's rated extension after a miss;
the public retry route therefore uses the bay's existing bolted bearing posts.
Neither geometry nor strength tests were weakened. Initial failed route attempts
and the rejected rounding-sensitive posture run remain in `P/crossing`.

The candidate workflow builds/runs these actual native owners, retains the existing
public swing regression, and exercises the new touch wrapper plus rendered image.
Its eventual exact-source result owns APK acceptance. This record does not claim
candidate CI, rendered inspection, phone feel, phone FPS or heard audio before those
observations exist. No rendering, solver-rate or visual-quality reduction was made.
