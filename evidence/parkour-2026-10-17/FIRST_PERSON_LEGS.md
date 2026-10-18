# Native-contact first-person legs

The player's visible boot follows the finite foot owner's retained material point and support surface normal. The bridge supplies render interpolation only while support, start serial and source contact tick match. Release immediately removes the planted state; an unloaded boot returns over0.12s. New plants never interpolate across an old contact.

Shipping Main reuses its existing landing dictionary, body pose and head-bob phase. Walking/crouch stride and airborne tuck are presentation; only the native contact receipt grants a planted sole. Two-bone legs hide an unreachable plant rather than invent a reachable point. Native foot actuators have no left/right identity, so alternating leg selection is presentation only. Ten non-shadow-casting mesh surfaces share resources built once. No collider, articulated limb inertia or physical actuator change is claimed.

## Observed evidence

- Godot4.7 supplied-state module test: `proot-distro login ubuntu --bind /data/data/com.termux/files/usr/tmp:/data/data/com.termux/files/usr/tmp -- /data/data/com.termux/files/usr/tmp/scraperx-godot-arm64/Godot_v4.7-stable_linux.arm64 --headless --path /data/data/com.termux/files/usr/tmp/scraperx-legs-9gwg1y9v --audio-driver Dummy --script res://tests/first_person_legs_test.gd` exited0; `SCRAPERX_LEGS PASS checks=53`. Log: `/data/data/com.termux/files/usr/tmp/scraperx-legs-9gwg1y9v/refinement-test.log`. Source copies matched the delivered module/test. This is presentation evidence from supplied observations.
- `first_person_legs_native_test.gd` passed Godot4.7 `--check-only`; runtime and visual evidence come from its exact-source candidate CI execution, not the parser.

## Affected acceptance

Candidate CI runs `first_person_legs_native_test.gd` against the shipping Main and matching host bridge, capturing two actual wall plants, unloaded/release behavior and a receiving Tower88 foot. Full native CI includes the existing wall-run cases with added point/normal/lifecycle assertions. Results remain pending until those commands actually finish. Android device appearance and feel require playing the matching APK.

Presentation tuning is in `godot/presentation/first_person_legs.gd`: THIGH/SHIN lengths, MAX_HIP_LEAN, ANKLE_HEIGHT and RETURN_TIME, all in metres/seconds. They do not modify physical reach, work or contact acceptance.
