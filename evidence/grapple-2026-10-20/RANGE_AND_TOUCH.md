# Longer-range grapple and coherent touch controls

The owner clarified that weak meant launch distance. LONG is now the carried gun's default: finite900J versus the previous180J, with60m line instead of18m. NEAR selects the existing180J shot for close work. Both use the actual2kg hook, reciprocal85kg recoil, Jolt flight/collision and unchanged rope/reel limits. Range selection is an ordered native input; it cannot change a live line or fabricate an anchor.

One lower-left TOOLS selector opens named Grapple / Traversal Line / Rigging choices. Primary Action stays on the right. Only the equipped tool offers its operations; a ready gun shows LONG SHOT / NEAR SHOT, an attached line shows take-up/pay-out. Duplicate automatic-reel UI is removed. Switching tools explicitly stows the previous owner. Existing locomotive, operator, pause and Thumb Flow behavior remains governed by its owners.

A read-only native hand approach supplies the reachable recovery offer. A slow unloaded hook can be recovered while paying out, before Lock Line consumes Action. Nearby recovery attempts remain distinct from shooting; full hand/steel withdrawal still decides pickup. Steel inventory returns only on actual allowed recovery; spent launch cartridges remain spent.

## Observed acceptance

Exact commands and output are in RANGE_TOUCH_NATIVE.json; actual MCP returns are in RANGE_TOUCH_MCP.json. Final focused Godot commands, outputs and source hashes are in RANGE_TOUCH_AFFECTED.json.

- Native hook module23/23, including default/near profiles, finite work/recoil, actual46.0436m upward range, payout recovery, normal gravity pickup, blocked retaining steel and overspeed negatives.
- Public Simulation long-shot input:14.972778m rise after45ticks, one899.999895J cartridge. Original complete close-work crossing passes using the real NEAR input; no force, contact, retention or footing gate weakened.
- Public gravity miss → actual loose-hook recovery → physical stairs → departure return passes at tick2761, zero deaths, no cartridge refund.
- Actual Godot MCP shipping Main/touch toolbelt:34checks, no failures; all three tool-owner transitions, native range acknowledgement, actual long shot14.610555m, deliberate release and onward movement. Ordinary yard start, no player relocation.
- Actual Godot MCP near-target grapple crossing:5692checks over2188native ticks, no failures; one initial roof stage, then touch fire/reel/passive hanging/receiving footing/release/onward exit. This is not continuous ascent or phone evidence.
- Final affected Godot checks pass: primary touch19, real Main/native gun105, rigging native input19 and toolbelt144layout combinations. STOP REEL remains available while the hook comes within reach; manual payout/take-up appears only after locking the line.
- Focused toolbelt144layout combinations pass under Godot4.7. Exact-source range-result.json and full logs remain at P/toolbelt-ui. The earlier open-tray render was inspected; it is a UI-only prototype, not a tower/performance image.

## Decisions and limits

A naive always-full-power close shot missed its receiver after a high arc encountered overhead geometry. The new player-visible NEAR/LONG control resolves that aiming tradeoff rather than altering receivers or granting contact. Existing near-target drivers now explicitly select NEAR through the real native/touch input; a separate default-LONG case proves the increase.

A full-power thin-plate miss exposed opposed contacts inside Jolt's20mm default penetration band. One isolated0.1mm-slop diagnosis recovered physically; production world settings were not changed. That difficult edge remains recorded, and trapped/embedded hooks are not erased through steel. Broader contact-tolerance/engine work is deferred; do not turn it into an indefinite feature prerequisite or claim it repaired.

One new profile test initially tried to shoot through its still-near first hook; letting that real hook clear the muzzle corrected the fixture. A newly added native long-range mode was initially missing from the test executable's input allowlist; that harness error was corrected before its actual pass. The old close-work touch fixture now selects the real NEAR control and walks on the actual roof to separate repeated motor commands from reachable pickup. It exposed a real missing STOP REEL action when a hook became reachable; Main now preserves that stop action and a dedicated assertion covers it. Original failed trials remain under P/grapple-touch. No game failure was hidden by changing expected success.

P=/data/data/com.termux/files/usr/tmp/scraperx-portable-line-20261020. Matching green-CI APK acceptance and phone observations remain separate from these host results.
