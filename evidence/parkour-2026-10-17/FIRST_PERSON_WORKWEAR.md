# First-person workwear and body presentation — 2026-10-18

Looking down now shows shaped work trousers, reinforced knees, leather boot
collars/laces and beveled soles. A hip garment joins both thighs. The visual
hip line sits16cm behind the eyes so the waist does not hide the knees.
This is presentation of the existing reduced-order player, not articulated
whole-body physics or a change to collision/support.

`first_person_leg_meshes.gd` builds shared, indexed, single-surface meshes once.
The two legs plus hips contain2,992triangles and11mesh instances. Cloth,
leather and rubber each share one material across both sides; vertex shades
carry seams/reinforcement without additional material surfaces or lights.
The existing rig still solves the same.44m thigh/shin lengths and bounded hip
lean. Anatomical mesh +Y faces the hip; local-Z faces forward. Knees turn with
the body. Unloaded boots roll with the supplied gait phase and retain floor
clearance. Real planted soles override the artistic gait and keep the exact
native material point/normal.

Main supplies its existing crouch-camera fraction to the hip pose. Crouch
therefore lowers the camera and visible hips together without another gait
clock. Release, rewind, death and operator-station hiding retain their existing
contracts. Native physics, input, foot force/stroke limits and world collision
are unchanged.

## Evidence and limits

Local host: pinned Godot4.7 Linux ARM64 under proot; actual unchanged native
upper-gallery bridge. B is
`/data/data/com.termux/files/usr/tmp/scraperx-body-workwear-20261018`.
B/source-receipt.json binds the coherent runtime source bytes and exact
commands. Root Main's unrelated dirty grapple/hook hunk is excluded.

- `godot-mcp-godot --headless --path W/source/godot --audio-driver Dummy --script res://tests/first_person_legs_test.gd`:
  exit0, `SCRAPERX_LEGS PASS checks=61`. Supplied observations only. Covers
  exact sole point/normal, moving support, free look, real visual contact
  admission/release, crouch-phase invariance, lifecycle and shared workwear.
- Same host/project, `--script res://tests/first_person_legs_native_test.gd`:
  exit0, `SCRAPERX_LEGS_NATIVE PASS checks=858`. Actual native wall plants and
  top-out receiving contact reach the shipping Main leg adapter. Headless run
  excludes the two viewport-save checks exercised by the existing CI gate.
- Isolated procedural mesh contract check, actual command/log in the receipt:
  exit0, all six shapes finite/indexed/nondegenerate, normals/UVs/colors and
  winding valid, boot clearance and exact sole minimumY=-.0125m retained.
- Direct Godot MCP recorded the original standing baseline and rejected the
  first waist cap because it hid the legs. A profile-only correction was still
  insufficient. The final eye-to-hip alignment changes the actual posed body
  arrangement. Its standing frame was inspected. Final MCP follow-up motion
  scheduling/state reads timed out; the runtime was stopped. Same-source CI
  completed standing, walking, crouch, Jump and landing: PASS68. All five body
  images and the two native-contact images were inspected.

The candidate retains the existing native-contact gate and adds the actual
look-down movement view plus the affected unit check. Host screenshots/checks
do not establish phone frame time, sustained performance or human game feel.
The host has no ALSA output and uses Dummy audio; no heard-sound claim.

Matching source a28937b8f6c966d415517467cba24021b59018a6 is published.
Candidate39156155944 completed GREEN; build214513419 is directly available at
https://github.com/IssisX/ScraperX/releases/download/chatgpt-candidate-a28937b/ScraperX-ChatGPT.apk . Native-contact rendered checksPASS860 and unitPASS61;
exact delivery/command ownership is in FIRST_PERSON_WORKWEAR_DELIVERY.json.
Separate full-release proof and phone play remain distinct.
