# Loaded hanging shoulder assignment

Base: ChatGPT2440aec plus the separately verified monorail waypoint correction.
The owner explicitly authorized this separate hanging repair after its baseline
reproduction. Consequential native physics and grip ownership remain unchanged.

## Failing before and causal control

Both touch_hang_drop and touch_hang_climb fail the existing20mm wrist-anchor
assertion. Unchanged d06b365 reproduces the drop failure (receipt in the monorail
evidence folder). At the failing instant native state remains Hanging (1), hand
poses remain[4,4], and native grips stay(8.62,3.6,4.22)/(8.62,3.6,3.78).
During0.1s the player center moves from(8.12816,2.211757,4) to
(8.107823,2.140135,4), and displayed wrist error becomes34.4818mm.
Removing both yaw and pitch changes produces identical states and error. The
camera did not detach a native hand; elapsed-time catch sag exposed a rig error.
The longer scratch observation bypasses the early assertion solely to observe
settling; its final PASS is not a shipping-gate pass.

## Source owner and final correction

The Hanging wrist target already follows the indexed native left/right grip.
The shoulder frame instead used traversal-normal cross up, whose handedness is
opposite the native lip pair in this case. Both arms therefore reached across
the torso. Catch sag exhausted the ordinary lean cap and _solve_arm clamped the
rendered wrists short of their actual anchors.

Derive the Hanging shoulder axis from native hand_right minus hand_left, matching
the same indexed pair that owns the wrists. Retain the existing fallback for a
degenerate pair. Arm lengths, shoulder height/span, MAX_LEAN0.30m, wrist targets,
native bodies, forces and input behavior are unchanged. Only this presentation
frame is corrected. No larger/unlimited lean allowance is shipped.

Both original hang scenarios now pass, including Drop or climb-up departure:
rendered wrist error0, shoulder adjustment0.2693m and spacing0.3685m. The added
regression checks native-pair shoulder assignment and0.33m forearm length in
addition to the unchanged20mm wrist criterion. It indexes rig hands by side,
not incidental array order. The failure message now names the actual visual
condition instead of asserting that camera movement detached native ownership.

Read-only final review found no blocking defect. Normal native grips are distinct;
the coincident-grip fallback and arbitrary asymmetric/tilted grips are not newly
proven. This first-person rig evidence is not device feel/performance evidence.

## Verification and delivery boundary

Godot4.7 character test passes; settings3078checks/0failures passes. Existing
native33/33 evidence is reused because no native code changes in this stabilization.
The21 passing adjacent input cases from the monorail candidate remain unchanged
in their behavior; both previously failing hang scenarios now pass. Combined
full ordinary campaign verification is recorded below once complete. No APK or
new remote run is claimed by these local receipts.

Final combined-source campaign: **PASS** `touch_campaign_to_121`, ordinary grade spawn, cargo net and upper route traversed, loaded swing completed, supported height121m, support11, deaths0, slingshot work0J. See `campaign-green.log`. Together with the21 adjacent cases recorded in the monorail receipt and the two corrected hanging cases, all23 selected input cases have passing evidence. No native changes were made. Remote exact-source Android verification remains required.
