# Player-built loose gangway

Source base: `904ddb07aedf83896193a9c29dd963c70a5d6399`, branch `ChatGPT`.

The player can move a free staging board from loose dunnage onto two loading
plinths west of the workshop, walk across it, continue onto a receiving perch
and step back down to the yard. It is an optional practice crossing, not a
claim that this low gap requires machinery or cannot be jumped.

## Mechanism and play

The board starts near `(-71, 0.46, -28)`. Approach from its north side, face
its central GRIP mark and use contextual GRAB. Walk east along the clear
side aisle to lay it across the plinths near `(-65, 0.42, -28)`, then LET GO.
The west steps enter the board; the east perch continues north to grade.
The two loose timber blocks can be dragged with the same interaction.

The 4.8m by 0.8m rigid timber assembly has a selected density of450kg/m³,
about64.9kg total. Each loose0.4m by0.4m by0.8m crib is57.6kg. Part masses
determine actual compound COM/inertia. The board's handle is converted from
authored coordinates into the existing carry owner's COM frame exactly once.

Gravity, ordinary contact/friction and the existing player/carry owners supply
motion and reactions. All three props retain free translation and rotation.
There are no placement sockets, welds, support flags, rotation locks, new
force limits or automatic pose correction. Actual Kit parts also supply
rendered geometry and ordinary machine checkpoint capture/restore.

The low yard permits stepping off and trying a different placement. Loose
objects can also be moved away from this bay. This slice does not claim a
new generic lifting/rotation control, automated rebuilding, articulated limb
mechanics or flexible timber bending. The owner's2026-10-19 arcade-feel
guidance is now in AGENTS.md: prioritize visible consequences and responsive
play rather than insignificant/offscreen simulation detail.

## Observed execution

Local command/result receipts are in
`/data/data/com.termux/files/usr/tmp/scraperx-loose-gangway-20261019`.

- `build.py mechanics-test` exited0. `mechanics-window.json` trial3 ran the
  real Kit/Jolt program: PASS balanced-load, peak length-axis tilt0.185363,
  low end0.0589031m; PASS two-supports, board0.419998m,85kg load0.697996m.
  The fixture checks that neither setup contains a hidden constraint.
- `build.py world`, `build.py route` and `bridge-build.py` exited0. The
  coherent accepted native archive is overlaid with only the new route and
  world-builder objects; no dirty Simulation/bridge ABI is borrowed.
- `encounter-window.json` trial6 reached the bay from normal FreightYardArrival,
  picked up and physically transported the board, released it onto the two
  bearings, walked across actual board support and reached the stable onward
  perch. Its final test waypoint remained within the last step's capsule
  contact, so the program exited1. The endpoint was corrected fromZ=-31.8
  toZ=-32.4; final complete native execution is assigned to the candidate CI
  gate, not represented as locally passing.
- Actual Godot MCP `run_project`/`run_script`/`get_debug_output` ran the shipping
  Main/native contextual Action, keyboard transport, release and native-driven
  board view: `SCRAPERX_LOOSE_GANGWAY_VIEW PASS checks=19 failures=0`.
  Initial footing was staged. A separate real keyboard exit from staged
  receiver1490 reached grade1 at `(-61.9,0.9,-34.398)`, zero deaths.
  `mcp-receipt.json` owns these actual tool responses.
- The fresh local asset import initially terminated with signal7 despite a
  zero launcher exit. It was repeated after the native copy completed;
  `godot-import-safe.json` records completion without script/import errors.
  The first two MCP attempts therefore do not count as gameplay proof.
- An optional overview script and later MCP stop were rejected with
  `MCP tool call requires approval, but approval policy is never`. No security
  setting was changed. The screenshot with3D still disabled is not visual
  evidence. The candidate workflow renders/captures the actual interaction.
  Runtime cleanup remains explicitly pending until a supported stop succeeds.

## Causal decisions retained

Sideways transport accumulated about46degrees of yaw around the existing
single grip. The accepted layout instead provides lengthwise handling and
a clear side aisle; no stabilizing force was invented. The original normal
arrival also physically struck and displaced the stored board. The approach
now goes around its actual envelope atX=-76. These are encounter/input-path
corrections, not relaxed collision or force rules. Failed trials remain in
the same persistent mechanics/encounter windows.

## Delivery

The candidate workflow now builds/runs both new native programs and the
rendered Main interaction before APK acceptance. Existing gates remain.
Publication, terminal CI result and the matching APK are pending at this
source boundary. Phone feel and performance require playing that APK.
