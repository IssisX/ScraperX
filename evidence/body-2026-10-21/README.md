# Rigged first-person workwear

The player now sees weighted trousers, jacket and boots, with a matching head
and arm shadow. One 56-bone skeleton replaces the separate procedural leg
pieces. The existing leg owner still supplies stance, crouch, gait and actual
native foot anchors; the renderer never changes physics or creates support.

The adopted asset is from ScraperX revision
`82cb58992f9ecd300830e6289d25367e6bc0e6b7`. Its pinned MakeHuman input assets are
CC0; source and licensing details ship beside the model. The raw GLB is
789,148 bytes, with eight surfaces and no textures. Runtime GLTF decoding avoids
depending on an editor-generated import cache. The export filter includes the
exact raw asset and provenance; candidate recording rejects missing or changed
packaged copies.

## Observed acceptance

`BODY_MCP.json` contains the actual Godot MCP calls/results, source hashes and
test adapters. The 64-check body/contact unit run and 859-check native integration
run passed. Native integration includes real touch walking, crouch, stand,
jump, landing and tool selection, followed by the existing staged wall and
top-out contacts. The final-source existing body-view sequence passed 53 checks
with no failures. Its image capture branch was disabled for that command;
`shipping-touch-look-down.png` is a separate actual Main viewport capture from
ordinary arrival with the touch overlay visible.

The host preview uses software rendering at 540×720 and a temporary 512 shadow
atlas. Shipping rendering settings are unchanged. These observations do not
establish phone frame rate, Android export or device feel. Matching green CI
and APK acceptance are separate from the host observations.

The structural test changes replace expectations for obsolete procedural mesh
pieces with checks on actual skin bindings, anatomy and shadow participation.
Contact identity, planted sole accuracy, invalid/unreachable contact rejection,
release, rewind, death and operator visibility checks remain. Earlier failures
in the new touch driver came from bypassing Main and stopping before actual
landing; a live cached script initially hid the driver correction. No physics
law or gameplay acceptance was weakened to resolve those failures.

## Tuning ownership

`first_person_legs.gd` retains native-anchor admission, gait and crouch phase.
`first_person_body_rig.gd` owns garment scale, bounded torso lean, collar clearance
and shadow limb posing. Anatomy is generated from the asset's rig metadata;
changes must preserve actual sole placement and leg lengths. Model-building
tools remain optional; no external runtime package is required.
