# Selectable thumb-zone parkour controls

## Player result

Settings → Touch Profile → Thumb Flow replaces the four on-foot parkour
buttons with a two-thumb input surface. Buttons remains the default for
existing and missing settings. Keyboard/gamepad bindings, explicit equipment
controls and forklift/crane decks retain their existing contracts.

| Input | Intent |
| --- | --- |
| Left drag / push past the movement ring | Move / sprint |
| Left quick stationary tap | Crouch, slide, tuck or stand, when available |
| Right drag | Look; pausing during that drag cannot arm a command |
| Right quick stationary tap | Jump, contacted wall kick or top-out through the existing native Jump owner |
| Right tap during a solo hold or swing | Request the next hand/bar; never the native Jump-as-release path |
| Hold right still, stroke up | Reach or current contextual interaction |
| Hold right still, stroke down | Alternative stance input |
| Hold right still, stroke inward, hold until ready, lift | Deliberate Drop/release, if still valid |
| Stroke outward from the armed fan | Cancel without an action |
| Lift the Reach finger before real acquisition | Cancel that finger's pending airborne Reach |

A real acquired grip survives finger lift. A left contact that has moved cannot
become a stance tap when it returns to rest. Commands use the existing named
verbs and ordered input path; gestures cannot invent anchors, support, forces,
trajectories, gravity changes or completion.

The input owner supplies the gesture clock once per caller frame. Focus/pause,
profile switches, size changes and operator transitions clear pending captures.
A Drop armed for an older hand generation cannot release a new grip. Ordinary
look, a short inward stroke, OS cancellation or a reused finger ID cannot
commit Drop. An armed Drop waits for lift and can be cancelled by moving away.

## Research that changed the design

EA describes Catalyst's Up action as contextual movement intent, with manual
aiming retained for expert departures. This supports a small vocabulary routed
through real context rather than separate buttons for each named trick.
[EA developer explanation](https://www.ea.com/amp/news/movement-in-mirrors-edge-catalyst-the-basics).

The official manual groups crouch, slide, coil and landing roll under Down and
uses timing-sensitive direct inputs. The initial fan-only stance proposal
therefore gained a direct left tap. Ubisoft separately documents splitting
stance from parkour descent to prevent accidental descent; ScraperX preserves
that distinction with a guarded release.
[EA manual, printed pages7–8](https://eaassets-a.akamaihd.net/eahelp/manuals/mec-manuals_xb1_en.pdf),
[Ubisoft parkour overview](https://quebec.ubisoft.com/en/assassins-creed-shadows-parkour-system-overview/).

These sources support grouping and safety principles. They do not establish
touchscreen comfort, the selected timings or equivalence to a controller bumper.
The selectable profile needs a human phone trial before an ergonomics claim.

## Tuning and ownership

`presentation/ui/thumb_parkour_gestures.gd` owns recognition only:
`ARM_SECONDS=0.16`, `DROP_HOLD_SECONDS=0.35`, direction dominance1.35.
TouchControls derives slop/stroke from the existing safe-area control scale,
with6/24pixel minimums. These are input tuning hypotheses, not physical limits.
No native force, catch range, work, contact,90Hz or momentum parameter changes.

SettingsStore owns the typed persisted profile. TouchControls owns captures,
feedback and native-context permission checks. InputRouter advances recognition
before collecting its existing verbs. Main dispatches those verbs unchanged.

## Evidence

- Godot4.7 standalone settings/menu: `SCRAPERX_THUMB_PROFILE_SETTINGS PASS checks=52 standalone_ui=1`, exit0. Exact command:
  `~/.local/bin/godot-mcp-godot --headless --path /data/data/com.termux/files/usr/tmp/scraperx-thumb-settings-z_1nc_qx --script res://tests/thumb_profile_settings_test.gd`.
  Actual log: that scratch project's `check.log`; returned bytes match the owned source files.
- Actual viewport touch/router check: `SCRAPERX_THUMB_CONTROLS PASS checks=54`, exit0. Exact command:
  `timeout 90s ~/.local/bin/godot-mcp-godot --headless --path /data/data/com.termux/files/usr/tmp/scraperx-air-catch-jz8afyr5/source/godot --audio-driver Dummy --script res://tests/thumb_parkour_controls_test.gd`.
  Includes60/90/360caller-rate recognition and accidental-release negatives. This check has no native Main.
- Actual Main/native headless check: `SCRAPERX_THUMB_FLOW_INPUT PASS checks=289 native=1`, exit0. Exact command: `timeout 180s ~/.local/bin/godot-mcp-godot --headless --path /data/data/com.termux/files/usr/tmp/scraperx-air-catch-jz8afyr5/source/godot --audio-driver Dummy res://tests/thumb_flow_input_test.tscn -- --quit-after-thumb-flow-check`. The observed sprint receipt was `(0,0,-8.000001)m/s` with actual ground contact. This is input/native evidence, not phone FPS or human comfort.
- Direct Godot MCP validates and launches the same scene on the coherent Linux ARM64 native cache. The final rendered run and its two captures are in progress; do not infer their success from the headless result. Host inspection caps only rendering at5FPS, while native input is explicitly stepped at90Hz; no shipping quality or FPS setting is changed.
- The candidate workflow runs the new settings/routing and actual Main/native rendered path, and retains its output/captures before exporting the matching APK. A completed green candidate, actual artifact and source identity are required for delivery.

The first local routing fixture used fixed pixel coordinates outside the right
zone on the project's expanded canvas; it was corrected to read the actual safe
rect without changing expectations. The first Main menu fixture clicked before
container layout; it now waits for the real clickable rectangle. Those are
introduced test-fixture failures, not evidence of a shipped physics defect.

The first complete native run also exposed two invalid fixture assumptions: the restart-menu grade point faced a real post (sprint input was true but solved speed0), and a transient catch later lost its finite physical grip without a Drop command. The corrected run uses the native ExteriorGrade clear approach and the existing cargo-edge lowering fixture. The assertion still requires real supported footing until guarded release, followed by actual native Lowering. It also requires the existing two-hand top-out to reach stable upper footing and walk onward. No force limit, safety expectation or native law was relaxed.
