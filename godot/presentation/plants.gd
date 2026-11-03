extends RefCounted

# Lane E, plants: the trees and scrub the world builders scatter. Built on
# the host scene's own primitives (_material, _add_cylinder, _add_cone,
# _add_sphere in main_geometry.gd), under the parent node given.

# One stylised conifer: a trunk and three descending, widening canopy tiers.
# Cheap enough to scatter by the dozen, varied enough per-instance (scale,
# yaw, a hue jitter across the greens) that a cluster doesn't read as one
# mesh copy-pasted.
static func add_tree(host: Node3D, parent: Node3D, at: Vector3, height: float,
		rng: RandomNumberGenerator) -> void:
	var hue_jitter := rng.randf_range(-0.03, 0.03)
	var canopy: Material = host._material(Color(0.16 + hue_jitter, 0.28 + hue_jitter, 0.14, 1.0), 0.0, 0.92)
	var trunk_material: Material = host._material(Color("362316"), 0.0, 0.9)

	var tree := Node3D.new()
	tree.name = "Conifer"
	tree.position = at
	tree.rotation.y = rng.randf_range(0.0, TAU)
	tree.scale = Vector3.ONE * rng.randf_range(0.85, 1.25)
	parent.add_child(tree)

	var trunk_height := height * 0.32
	host._add_cylinder("TreeTrunk", height * 0.045, trunk_height,
		Vector3(0.0, trunk_height * 0.5, 0.0), trunk_material, tree)
	for tier in range(3):
		var t := float(tier) / 2.0
		var tier_radius := lerpf(height * 0.34, height * 0.11, t)
		var tier_height := height * 0.4
		var tier_y := trunk_height + t * height * 0.5
		host._add_cone("TreeCanopy", tier_radius, tier_height,
			Vector3(0.0, tier_y + tier_height * 0.5, 0.0), canopy, tree)

# Low scrub and ground bushes: irregular clusters of squashed spheres, no
# trunk, filling the gap between bare grade and full trees. A lobe rests on
# the ground or on a lobe laid before it; one that would float clear of both
# is set down on the ground.
const SCRUB_SQUASH := 0.72

static func add_bush(host: Node3D, parent: Node3D, at: Vector3, spread: float,
		rng: RandomNumberGenerator) -> void:
	var hue_jitter := rng.randf_range(-0.04, 0.04)
	var bush_material: Material = host._material(Color(0.2 + hue_jitter, 0.3 + hue_jitter, 0.15, 1.0), 0.0, 0.94)
	var bush := Node3D.new()
	bush.name = "Scrub"
	bush.position = at
	parent.add_child(bush)
	var laid: Array[Vector4] = []   # each lobe laid: its middle and radius
	for _lobe in range(rng.randi_range(3, 5)):
		var lobe_at := Vector3(rng.randf_range(-spread, spread), rng.randf_range(0.1, spread * 0.5),
			rng.randf_range(-spread, spread))
		var radius := rng.randf_range(spread * 0.45, spread * 0.75)
		if lobe_at.y > radius * SCRUB_SQUASH and not _lobe_rests(lobe_at, radius, laid):
			lobe_at.y = radius * SCRUB_SQUASH
		laid.append(Vector4(lobe_at.x, lobe_at.y, lobe_at.z, radius))
		var lobe: MeshInstance3D = host._add_sphere("ScrubLobe", radius, lobe_at, bush_material, bush)
		lobe.scale.y = SCRUB_SQUASH

# Whether a squashed lobe touches one already laid: in the space where both
# are round again (heights over SCRUB_SQUASH).
static func _lobe_rests(at: Vector3, radius: float, laid: Array[Vector4]) -> bool:
	for other in laid:
		var apart := Vector3(at.x - other.x, (at.y - other.y) / SCRUB_SQUASH, at.z - other.z)
		if apart.length() < radius + other.w:
			return true
	return false
