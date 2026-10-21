# Hold-to-charge grapple and world aim guide

The owner replaced the NEAR/LONG touch selector with one hold gesture. Hold the gun's main action to aim and charge; release fires one actual hook. Short release uses180J; one second of actual native collision time reaches900J. Longer holds stay capped and never auto-fire. The60m line,2kg hook,85kg reciprocal recoil, finite inventory and existing rope/reel limits remain unchanged.

Small finger motion while holding adjusts aim. Sliding off cancels. OS cancellation, touch reset, opening TOOLS, pause, tool switching, native stow/release/restart and invalid traversal/carry ownership cancel. Native ordered Begin/Fire/Cancel commands are bounded; cancellation supersedes a queued shot and cannot be dropped by a full queue. Existing discrete Action callers retain a quick shot. Legacy native profile APIs remain compatible; the separate touch range button is hidden.

The world guide uses the same launch-impulse law, actual inherited player velocity and native gravity. A finite swept envelope includes barrel clearance and stops at the first current obstruction or spool limit. It samples at most33points and updates collision queries at most30Hz only while charging. One depth-tested reused surface draws a cyan arc/open endpoint; obstruction is amber. This is a prospective guide: moving geometry, hook rotation, contact and later rope interaction still decide actual flight and retention. It creates no anchor or reaction and grants no capture.

## Observed acceptance

- Native25/25 includes intermediate/full/quick energy, finite cap, no-spend hold, cancellation, duplicate-release rejection, wall-limited preview and no preview mutation. Existing physical module requirements remain intact.
- Ordered public Simulation charge/cancel/cap/shot: actual14.972778m rise after45ticks,899.999895J. Original complete native primary crossing passes.
- Actual Godot MCP ordinary-yard touch/native case27checks, failures[]: hold/acknowledgement, world guide through shipping render path, cancel, real pause/resume, tool switch, clamped hold, physical release,14.610555m rise and onward movement.
- Actual Godot MCP full close-work trip5702checks/2191ticks, done exit, failures[]. One initial roof stage, then actual touch/physics/reel/receiver/release/onward movement. No route/load/force/deadline assertion weakened.
- Isolated view24 and primary touch/router/Main mocked-native30pass; root actual Godot toolbelt144layouts and gun controls102pass. Native/MCP/affected receipts are CHARGE_NATIVE.json, CHARGE_MCP.json and CHARGE_TOUCH.json.

The first new unit compile called the wrong existing fixture helper and was corrected from step to tick. The first MCP trip invocation accidentally called inherited winch run; its actual wrapper uses begin/advance. Corrected invocation passes, and the failed invocation is preserved in the MCP receipt. No game failure was waived.

## Tuning and limits

Native full_charge_seconds=1.0 and precision_energy_j=180 / cartridge_energy_j=900 are the charge-feel bounds. These are tuning hypotheses, not measured device-preference claims. No new force caps, global contact tolerances or spring constants were added. The recorded full-power thin-plate jam remains unresolved; embedded steel is not silently deleted. Hook recovery returns reusable steel, not spent launch cartridges.

Host rendering/input/native evidence does not prove phone look, feel, hearing or FPS. Matching green-CI APK remains the delivery gate. The earlier RANGE_AND_TOUCH.md describes the initial selector version; this document records its owner-requested replacement.
