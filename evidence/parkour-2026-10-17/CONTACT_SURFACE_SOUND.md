# Contact-linked west-apron footsteps — 2026-10-18

Steel gallery decks and concrete platforms both use world-solid entity51.
The old ordinary-footstep selector treated that entity as concrete above
0.5m, so the steel gallery selected a concrete recording. Entity identity is
not a material identifier.

The ParkourYard's concrete and steel materials now carry explicit sound-only
metadata. SolidExport reads each box's active material and appends a sparse,
box-indexed tag table. The existing 1,792 boxes and666 hulls retain byte-identical
coordinates/order:20 concrete and60 steel boxes are tagged. Untagged geometry
retains its current fallback; no world-wide material inference is introduced.

Each tagged static native box stores the sound class on its Jolt shape, leaving
body entity51 intact. The read-only support_sound_surface() observer requires
current grounded contact, the actual body identity and the same live shape.
Missing/stale/unannotated support returns Unknown. There is no ray-based
contact, new force, friction change, collision change or Snapshot layout change.

The existing get_landing_state dictionary carries optional support_sound_surface:
0 Unknown,1 Concrete,2 Metal. This describes current footing, not a historical
landing event. Only ordinary stride playback consumes it. Existing landing,
wall-stride, foot-transfer and unannotated fallback behavior remains unchanged.
Stride distance, relative support motion, crouch gain, variant selection,
pitch variation and effects-bus routing retain their existing owners.

A focused supplied-state audio check covers explicit selection, malformed/missing
metadata, airborne/sliding exclusion and riding a moving support without steps.
The actual shipping Main/native scene separately stages initial footing, walks
with ordinary keyboard input on two supports sharing entity51, and checks the
actual recorded stream given to the real audio player. Jump must clear current
surface metadata; gravity must reacquire the steel perch. Returning to the
unannotated arrival must keep Unknown. Staging is a test setup, not a route claim.

Evidence, commands, exact source and matching APK will be recorded in the
existing delivery receipt. Host Dummy audio verifies player/stream selection,
not sound heard on the phone or subjective mix/quality.

Host proof: direct Godot MCP run_project/run_script/get_debug_output/stop_project
completed the actual Main/native/audio scene: SCRAPERX_CONTACT_SOUND PASS
checks=46 failures=0 actual_steps=2 staged_footing=1 heard_audio=0. 3D rendering
was disabled only for this audio-path run after the software renderer timed out
on a screenshot. Matching candidate CI retains the rendered check. Initial
final-return staging was corrected to start5cm above the real arrival contact;
debug_restart_at requires2cm clearance. Acceptance expectations were unchanged.

The Sol/medium audio worker ran contact_surface_audio_test.gd with Godot4.7
headless/Dummy: exit0, CONTACT_SURFACE_AUDIO PASS checks=38. Root inspected the
diff and reused this supplied-state evidence. The three changed native/bridge
owners compiled and linked with -Wall/-Wextra/-Wpedantic/-Werror, exit0.
Exact build/MCP commands and matching source hashes are in
/data/data/com.termux/files/usr/tmp/scraperx-contact-sound-20261018/source-receipt.json.
No existing ABI data layout changed or unrelated dirty hook code was included.
