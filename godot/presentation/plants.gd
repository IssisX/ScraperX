extends RefCounted

# Lane E, plants: the trees and scrub the world builders scatter. Built on
# the host scene's own primitives (_material, _add_cylinder, _add_cone,
# _add_sphere in main_geometry.gd), under the parent node given.

# Conifers, drawn in batches. Every tree joins the batch of the ground cell
# it stands in (CONIFER_CELL_M square): one MultiMesh of the near tree, a
# trunk under whorls of drooping, ragged branch tiers, and one of the far
# impostor, two crossed cards painted with the same tree. The near batch
# gives way to the impostor at CONIFER_NEAR_M, so a forest costs two draws
# per cell instead of four per tree, and the far hills stay cheap on
# Android. Per tree: its ground point and height as given, a yaw, a size and
# a hue across the greens, the same three draws of rng as ever, so whatever
# the caller scatters after a tree lands where it always did.
const CONIFER_CELL_M := 160.0
const CONIFER_NEAR_M := 300.0
const CONIFER_HANDOFF_M := 20.0
const CONIFER_FAR_END_M := 4200.0
# The old tiered tree topped out at 1.22 of the height asked for.
const CONIFER_TOP := 1.22
const CONIFER_TIERS := 9
const CONIFER_SPOKES := 11
const CONIFER_TRUNK_SIDES := 6

static var _conifer_cells := {}
static var _conifer_near_mesh: ArrayMesh
static var _conifer_far_mesh: ArrayMesh

static func add_tree(host: Node3D, parent: Node3D, at: Vector3, height: float,
		rng: RandomNumberGenerator) -> void:
	var hue_jitter := rng.randf_range(-0.03, 0.03)
	var yaw := rng.randf_range(0.0, TAU)
	var size := rng.randf_range(0.85, 1.25)

	var cell := _conifer_cell(parent, at)
	var tall := height * size * CONIFER_TOP
	# Narrower or fuller crowns and a slight lean, from draws already made.
	var girth := 1.0 + hue_jitter * 4.0
	var lean := Basis(Vector3(cos(yaw * 3.0), 0.0, sin(yaw * 3.0)), 0.025 * sin(yaw * 5.0))
	var basis := lean * Basis(Vector3.UP, yaw) * Basis.from_scale(Vector3(tall * girth, tall, tall * girth))
	var xform := Transform3D(basis, at - cell.position)
	var tint := Color(1.0 + hue_jitter * 2.0, 1.0 + hue_jitter * 3.0, 1.0 - hue_jitter, 1.0)
	_conifer_push(cell, xform, tint)
	_conifer_solid(host, parent, at, height, hue_jitter, yaw, size)

# What the sim stands on: the tree's trunk and three canopy cones, the shapes
# world_solids.inc was baked from, kept exactly as they were and hidden. The
# batches draw the tree; these only answer the solid export.
static func _conifer_solid(host: Node3D, parent: Node3D, at: Vector3, height: float,
		hue_jitter: float, yaw: float, size: float) -> void:
	var canopy: Material = host._material(Color(0.16 + hue_jitter, 0.28 + hue_jitter, 0.14, 1.0), 0.0, 0.92)
	var trunk_material: Material = host._material(Color("362316"), 0.0, 0.9)
	var tree := Node3D.new()
	tree.name = "ConiferSolid"
	tree.position = at
	tree.rotation.y = yaw
	tree.scale = Vector3.ONE * size
	tree.visible = false
	parent.add_child(tree)
	var trunk_height := height * 0.32
	# Each part hidden itself too: the static batcher merges any mesh whose
	# own flag is set, whatever its parent's.
	host._add_cylinder("TreeTrunk", height * 0.045, trunk_height,
		Vector3(0.0, trunk_height * 0.5, 0.0), trunk_material, tree).visible = false
	for tier in range(3):
		var t := float(tier) / 2.0
		var tier_radius := lerpf(height * 0.34, height * 0.11, t)
		var tier_height := height * 0.4
		var tier_y := trunk_height + t * height * 0.5
		host._add_cone("TreeCanopy", tier_radius, tier_height,
			Vector3(0.0, tier_y + tier_height * 0.5, 0.0), canopy, tree).visible = false

# The cell batch a tree at `at` (in parent space) joins, made on first use.
static func _conifer_cell(parent: Node3D, at: Vector3) -> Node3D:
	var cx := floori(at.x / CONIFER_CELL_M)
	var cz := floori(at.z / CONIFER_CELL_M)
	var key := "%d:%d:%d" % [parent.get_instance_id(), cx, cz]
	var cell: Node3D = _conifer_cells.get(key)
	if cell != null and is_instance_valid(cell) and cell.get_parent() == parent:
		return cell
	cell = Node3D.new()
	cell.name = "ConiferCell"
	cell.position = Vector3((float(cx) + 0.5) * CONIFER_CELL_M, 0.0, (float(cz) + 0.5) * CONIFER_CELL_M)
	parent.add_child(cell)
	var near := MultiMeshInstance3D.new()
	near.name = "ConiferNear"
	near.multimesh = _conifer_multimesh(_conifer_near())
	near.visibility_range_end = CONIFER_NEAR_M
	near.visibility_range_end_margin = CONIFER_HANDOFF_M
	cell.add_child(near)
	var far := MultiMeshInstance3D.new()
	far.name = "ConiferFar"
	far.multimesh = _conifer_multimesh(_conifer_far())
	far.visibility_range_begin = CONIFER_NEAR_M
	far.visibility_range_begin_margin = CONIFER_HANDOFF_M
	far.visibility_range_end = CONIFER_FAR_END_M
	far.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	cell.add_child(far)
	cell.set_meta(&"trees", [])
	_conifer_cells[key] = cell
	return cell

static func _conifer_multimesh(mesh: Mesh) -> MultiMesh:
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.use_colors = true
	mm.mesh = mesh
	return mm

# A MultiMesh's instance count clears its buffer, so a cell lays every one of
# its trees again when one joins: a few dozen per cell, once, at build.
static func _conifer_push(cell: Node3D, xform: Transform3D, tint: Color) -> void:
	var trees: Array = cell.get_meta(&"trees")
	trees.append([xform, tint])
	for child in cell.get_children():
		var mm: MultiMesh = (child as MultiMeshInstance3D).multimesh
		mm.instance_count = trees.size()
		for i in trees.size():
			mm.set_instance_transform(i, trees[i][0])
			mm.set_instance_color(i, trees[i][1])

static func _conifer_material(texture: Texture2D) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.vertex_color_use_as_albedo = true
	material.vertex_color_is_srgb = true
	material.roughness = 0.93
	if texture != null:
		material.albedo_texture = texture
		material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR
		material.alpha_scissor_threshold = 0.5
		material.cull_mode = BaseMaterial3D.CULL_DISABLED
		material.texture_filter = BaseMaterial3D.TEXTURE_FILTER_LINEAR_WITH_MIPMAPS
	return material

const BARK := Color("3a2618")
const NEEDLE_DEEP := Color(0.03, 0.07, 0.035)
const NEEDLE_MID := Color(0.06, 0.12, 0.055)
const NEEDLE_TIP := Color(0.12, 0.19, 0.08)

# Crown radius at height t (0 at the lowest whorl, 1 at the leader).
static func _crown_radius(t: float) -> float:
	return 0.31 * pow(1.0 - t, 0.85) + 0.015

# The near tree, one unit tall, one surface: a tapered trunk and
# CONIFER_TIERS whorls. Each whorl is a skirt of CONIFER_SPOKES branch tips
# that droop at the end and ragged gaps between them, closed underneath
# toward the trunk, so it reads from below as well as from the side. Tips
# catch the light, the inside of the crown stays dark.
static func _conifer_near() -> ArrayMesh:
	if _conifer_near_mesh != null:
		return _conifer_near_mesh
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	# Trunk.
	var trunk_top := 0.82
	for side in CONIFER_TRUNK_SIDES:
		var a0 := TAU * float(side) / float(CONIFER_TRUNK_SIDES)
		var a1 := TAU * float(side + 1) / float(CONIFER_TRUNK_SIDES)
		var b0 := Vector3(cos(a0) * 0.022, 0.0, sin(a0) * 0.022)
		var b1 := Vector3(cos(a1) * 0.022, 0.0, sin(a1) * 0.022)
		var t0 := Vector3(cos(a0) * 0.006, trunk_top, sin(a0) * 0.006)
		var t1 := Vector3(cos(a1) * 0.006, trunk_top, sin(a1) * 0.006)
		_tri(st, [b0, t0, t1], [Vector3(cos(a0), 0.0, sin(a0)), Vector3(cos(a0), 0.0, sin(a0)),
			Vector3(cos(a1), 0.0, sin(a1))], [BARK, BARK.darkened(0.2), BARK.darkened(0.2)])
		_tri(st, [b0, t1, b1], [Vector3(cos(a0), 0.0, sin(a0)), Vector3(cos(a1), 0.0, sin(a1)),
			Vector3(cos(a1), 0.0, sin(a1))], [BARK, BARK.darkened(0.2), BARK])
	# Whorls, from the lowest (widest) up to the leader.
	var low := 0.16
	var tier_gap := (0.96 - low) / float(CONIFER_TIERS)
	for tier in CONIFER_TIERS:
		var t := float(tier) / float(CONIFER_TIERS - 1)
		var base_y := low + float(tier) * tier_gap
		var radius := _crown_radius(t)
		var rise := tier_gap * 1.9
		var droop := radius * 0.28
		var apex := Vector3(0.0, base_y + rise, 0.0)
		var core := Vector3(0.0, base_y + rise * 0.15, 0.0)
		var twist := float(tier) * 0.61
		var ring: Array[Vector3] = []
		for k in CONIFER_SPOKES * 2:
			var a := twist + TAU * float(k) / float(CONIFER_SPOKES * 2)
			var tip := k % 2 == 0
			# Each spoke a touch longer or shorter: a fixed pattern, not rng.
			var reach := radius * (1.0 + 0.12 * sin(float(k * 7 + tier * 3))) if tip else radius * 0.62
			var y := base_y - (droop if tip else droop * 0.25)
			ring.append(Vector3(cos(a) * reach, y, sin(a) * reach))
		for k in ring.size():
			var p0 := ring[k]
			var p1 := ring[(k + 1) % ring.size()]
			var tip0 := k % 2 == 0
			var tip1 := not tip0
			var c0 := NEEDLE_TIP if tip0 else NEEDLE_MID
			var c1 := NEEDLE_TIP if tip1 else NEEDLE_MID
			var n0 := Vector3(p0.x, radius * 0.9, p0.z).normalized()
			var n1 := Vector3(p1.x, radius * 0.9, p1.z).normalized()
			# Upper face of the skirt.
			_tri(st, [apex, p0, p1], [Vector3.UP, n0, n1], [NEEDLE_DEEP, c0, c1])
			# Underside, back to the trunk.
			_tri(st, [core, p1, p0], [Vector3.DOWN, -n1.slide(Vector3.UP) * 0.3 + Vector3.DOWN,
				-n0.slide(Vector3.UP) * 0.3 + Vector3.DOWN], [NEEDLE_DEEP.darkened(0.3), NEEDLE_DEEP, NEEDLE_DEEP])
	# The leader.
	var leader_base := 0.96
	for side in 5:
		var a0 := TAU * float(side) / 5.0
		var a1 := TAU * float(side + 1) / 5.0
		var q0 := Vector3(cos(a0) * 0.02, leader_base, sin(a0) * 0.02)
		var q1 := Vector3(cos(a1) * 0.02, leader_base, sin(a1) * 0.02)
		_tri(st, [Vector3(0.0, 1.0, 0.0), q1, q0], [Vector3.UP, q1.normalized(), q0.normalized()],
			[NEEDLE_TIP, NEEDLE_MID, NEEDLE_MID])
	_conifer_near_mesh = st.commit()
	_conifer_near_mesh.surface_set_material(0, _conifer_material(null))
	return _conifer_near_mesh

static func _tri(st: SurfaceTool, points: Array, normals: Array, colors: Array) -> void:
	for i in 3:
		st.set_color(colors[i])
		st.set_normal((normals[i] as Vector3).normalized())
		st.add_vertex(points[i])

# The far impostor: two crossed cards, one unit tall, painted with the
# near tree's silhouette and shading (tiers, dark core, lit tips).
static func _conifer_far() -> ArrayMesh:
	if _conifer_far_mesh != null:
		return _conifer_far_mesh
	var w := 64
	var h := 128
	var image := Image.create(w, h, false, Image.FORMAT_RGBA8)
	image.fill(Color(NEEDLE_MID.r, NEEDLE_MID.g, NEEDLE_MID.b, 0.0))
	var low := 0.16
	var tier_gap := (0.96 - low) / float(CONIFER_TIERS)
	for y in h:
		var v := 1.0 - (float(y) + 0.5) / float(h)   # height up the tree
		for x in w:
			var u := ((float(x) + 0.5) / float(w) - 0.5) * 0.72   # across, in tree units
			var c := Color(0, 0, 0, 0)
			if absf(u) < 0.016 and v < 0.84:
				c = BARK
			for tier in CONIFER_TIERS:
				var t := float(tier) / float(CONIFER_TIERS - 1)
				var base_y := low + float(tier) * tier_gap
				var radius := _crown_radius(t) * (1.0 + 0.1 * sin(u * 90.0 + float(tier)))
				var rise := tier_gap * 1.9
				var bottom := base_y - radius * 0.28 * (absf(u) / maxf(radius, 0.001))
				var top := base_y + rise * (1.0 - absf(u) / maxf(radius, 0.001))
				if absf(u) < radius and v > bottom and v < top:
					var lit := clampf(absf(u) / radius * 0.8 + (v - bottom) / maxf(top - bottom, 0.001) * 0.2, 0.0, 1.0)
					c = NEEDLE_DEEP.lerp(NEEDLE_TIP, lit * lit)
					c.a = 1.0
			if v > 0.96 and v < 1.0 and absf(u) < 0.02 * (1.0 - v) / 0.04:
				c = NEEDLE_TIP
			if c.a > 0.0:
				image.set_pixel(x, y, c)
	image.generate_mipmaps()
	var texture := ImageTexture.create_from_image(image)
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	for card in 2:
		var across := Vector3(0.36, 0.0, 0.0) if card == 0 else Vector3(0.0, 0.0, 0.36)
		var face := Vector3(0.0, 0.0, 1.0) if card == 0 else Vector3(1.0, 0.0, 0.0)
		var n := (face + Vector3.UP * 1.5).normalized()
		var corners := [-across, across, across + Vector3.UP, -across + Vector3.UP]
		var uvs := [Vector2(0, 1), Vector2(1, 1), Vector2(1, 0), Vector2(0, 0)]
		for i in [0, 1, 2, 0, 2, 3]:
			st.set_color(Color.WHITE)
			st.set_normal(n)
			st.set_uv(uvs[i])
			st.add_vertex(corners[i])
	_conifer_far_mesh = st.commit()
	_conifer_far_mesh.surface_set_material(0, _conifer_material(texture))
	return _conifer_far_mesh

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
