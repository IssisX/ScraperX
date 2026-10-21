extends Node3D

# The main scene, layer 1 of 6: geometry. The primitives every builder
# lays (boxes, struts, cones, trees, signs), the industrial palette and its
# bump textures, and the batching of static dressing. Layers: main_geometry,
# main_world, main_machines, main_context, main_feel, main.gd (the scene's
# script); each extends the one before and calls only itself and below.

const LaunchShot := preload("res://presentation/launch_shot.gd")

# Locomotion-feel camera response. Pure presentation, driven every frame by
# native player position/velocity/grounded state already read below -- never
# the other way around, and never touching _native itself. Every term is a
# function of CURRENT state (speed, ground contact, the last frame's own
# vertical velocity), not an accumulating drift, so it always returns to
# exactly baseline (no dip, no bob, FOV_BASE) the moment the player is
# grounded and stationary -- the CI runtime proof's own hold, where
# the player stands still, depends on that landing on the same fov=82.0 the
# scene is built with.
const FOV_BASE := 82.0

# Mirrors scraperx::sim::FallState.
const FALL_GROUNDED := 0
# Continue: the committed checkpoint, machines included, on the device's own
# storage. Written when a new checkpoint is committed (at most every 2 s) and
# when the app leaves the foreground; loaded on launch; NEW CLIMB discards it.
const SAVE_PATH := "user://climb.save"

# Slightly darker variant of a base material, for members that should sit back
# a value step without defining a whole new palette entry.
func rust_deepen(source: Material) -> Material:
	var base := source as StandardMaterial3D
	if base == null:
		return source
	return _material(base.albedo_color.darkened(0.35), base.metallic, base.roughness)

# Corrugated steel, as a normal map: one rib per texture repeat, across u.
func _corrugation_texture() -> ImageTexture:
	var image := Image.create(32, 4, false, Image.FORMAT_RGB8)
	for i in 32:
		var slope := cos(TAU * (float(i) + 0.5) / 32.0) * 0.9
		var n := Vector3(-slope, 0.0, 1.0).normalized()
		for j in 4:
			image.set_pixel(i, j, Color(n.x * 0.5 + 0.5, 0.5, n.z * 0.5 + 0.5))
	return ImageTexture.create_from_image(image)

# One stylised conifer: a trunk and three descending, widening canopy tiers.
# Cheap enough to scatter by the dozen, varied enough per-instance (scale,
# yaw, a hue jitter across the greens) that a cluster doesn't read as one
# mesh copy-pasted.
func _add_tree(at: Vector3, height: float, rng: RandomNumberGenerator) -> void:
	var hue_jitter := rng.randf_range(-0.03, 0.03)
	var canopy := _material(Color(0.16 + hue_jitter, 0.28 + hue_jitter, 0.14, 1.0), 0.0, 0.92)
	var trunk_material := _material(Color("362316"), 0.0, 0.9)

	var tree := Node3D.new()
	tree.name = "Conifer"
	tree.position = at
	tree.rotation.y = rng.randf_range(0.0, TAU)
	tree.scale = Vector3.ONE * rng.randf_range(0.85, 1.25)
	$TowerPresentation.add_child(tree)

	var trunk_height := height * 0.32
	_add_cylinder("TreeTrunk", height * 0.045, trunk_height,
		Vector3(0.0, trunk_height * 0.5, 0.0), trunk_material, tree)
	for tier in range(3):
		var t := float(tier) / 2.0
		var tier_radius := lerpf(height * 0.34, height * 0.11, t)
		var tier_height := height * 0.4
		var tier_y := trunk_height + t * height * 0.5
		_add_cone("TreeCanopy", tier_radius, tier_height,
			Vector3(0.0, tier_y + tier_height * 0.5, 0.0), canopy, tree)

# Low scrub and ground bushes: irregular clusters of squashed spheres, no
# trunk, filling the gap between bare grade and full trees. A lobe rests on
# the ground or on a lobe laid before it; one that would float clear of both
# is set down on the ground.
const SCRUB_SQUASH := 0.72

func _add_bush(at: Vector3, spread: float, rng: RandomNumberGenerator) -> void:
	var hue_jitter := rng.randf_range(-0.04, 0.04)
	var bush_material := _material(Color(0.2 + hue_jitter, 0.3 + hue_jitter, 0.15, 1.0), 0.0, 0.94)
	var bush := Node3D.new()
	bush.name = "Scrub"
	bush.position = at
	$TowerPresentation.add_child(bush)
	var laid: Array[Vector4] = []   # each lobe laid: its middle and radius
	for _lobe in range(rng.randi_range(3, 5)):
		var lobe_at := Vector3(rng.randf_range(-spread, spread), rng.randf_range(0.1, spread * 0.5),
			rng.randf_range(-spread, spread))
		var radius := rng.randf_range(spread * 0.45, spread * 0.75)
		if lobe_at.y > radius * SCRUB_SQUASH and not _lobe_rests(lobe_at, radius, laid):
			lobe_at.y = radius * SCRUB_SQUASH
		laid.append(Vector4(lobe_at.x, lobe_at.y, lobe_at.z, radius))
		var lobe := _add_sphere("ScrubLobe", radius, lobe_at, bush_material, bush)
		lobe.scale.y = SCRUB_SQUASH

# Whether a squashed lobe touches one already laid: in the space where both
# are round again (heights over SCRUB_SQUASH).
static func _lobe_rests(at: Vector3, radius: float, laid: Array[Vector4]) -> bool:
	for other in laid:
		var apart := Vector3(at.x - other.x, (at.y - other.y) / SCRUB_SQUASH, at.z - other.z)
		if apart.length() < radius + other.w:
			return true
	return false

func _add_sphere(node_name: String, radius: float, at: Vector3,
		material: Material, parent: Node3D = null) -> MeshInstance3D:
	var sphere := SphereMesh.new()
	sphere.radius = radius
	sphere.height = radius * 2.0
	sphere.radial_segments = 10
	sphere.rings = 6
	var instance := MeshInstance3D.new()
	instance.name = node_name
	instance.set_meta(&"part", node_name)
	instance.mesh = sphere
	instance.material_override = material
	instance.position = at
	var host: Node3D = parent if parent != null else $TowerPresentation
	host.add_child(instance)
	return instance

# A banner's board: its size, and how high its middle stands over its spot.
const BANNER_BOARD := Vector3(3.2, 4.6, 0.12)
const BANNER_BOARD_Y := 4.6
# The slogan's size: BANNER_PIXEL_M a pixel, smaller when its longest line
# would come nearer than BANNER_MARGIN_M to the board's edge (the owner,
# 2026-10-21: "It's not even on the sign all the way").
const BANNER_PIXEL_M := 0.0095
const BANNER_MARGIN_M := 0.2

func _add_banner(at: Vector3, lines: Array, backing: Material) -> void:
	_add_box("BannerBacking", BANNER_BOARD, at + Vector3(0.0, BANNER_BOARD_Y, 0.0), backing)
	var label := Label3D.new()
	label.name = "BannerCopy"
	label.text = "\n".join(lines)
	label.font_size = 40
	var font: Font = label.font if label.font != null else ThemeDB.fallback_font
	var text_px := font.get_multiline_string_size(label.text, HORIZONTAL_ALIGNMENT_CENTER, -1, label.font_size)
	label.pixel_size = minf(BANNER_PIXEL_M, minf((BANNER_BOARD.x - 2.0 * BANNER_MARGIN_M) / text_px.x,
		(BANNER_BOARD.y - 2.0 * BANNER_MARGIN_M) / text_px.y))
	label.modulate = Color(0.92, 0.88, 0.8)
	# Paint on the board: lit as the board is, never glowing in the dark.
	label.shaded = true
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.position = at + Vector3(0.0, BANNER_BOARD_Y, 0.07)
	$TowerPresentation.add_child(label)
	# The mark 0.35 m under the board's top, all of it on the board.
	_add_chevron_mark(at + Vector3(0.0, BANNER_BOARD_Y + BANNER_BOARD.y * 0.5 - 0.35, 0.07), 0.55, backing)

func _add_chevron_mark(at: Vector3, scale: float, accent: Material) -> void:
	# Original angular chevron-K mark, built from three thin boxes. Not a copy
	# of any existing logotype.
	var mark := Node3D.new()
	mark.name = "ChevronMark"
	mark.position = at
	$TowerPresentation.add_child(mark)
	_add_box_to("ChevronSpine", Vector3(0.14, 0.9, 0.05) * scale, Vector3(-0.32, 0.0, 0.0) * scale, accent, mark)
	_add_box_to("ChevronUpper", Vector3(0.62, 0.14, 0.05) * scale, Vector3(0.05, 0.24, 0.0) * scale, accent, mark, -0.55)
	_add_box_to("ChevronLower", Vector3(0.62, 0.14, 0.05) * scale, Vector3(0.05, -0.24, 0.0) * scale, accent, mark, 0.55)

# A mountain: its solid cone (what the world collides with, read by
# solid_export.gd) kept in the scene but hidden, and a peak drawn on the same
# foot, carved down from the cone and never above it (mountain_mesh.gd; drawn
# all at once by _build_mountains_and_waterfall). Snow lies above
# `snowline`, a fraction of the height; 0 draws the peak bare.
var _peaks: Array = []

func _add_peak(node_name: String, radius: float, height: float, foot: Vector3, cone_material: Material,
		rock: Color, variant: int, snowline: float) -> void:
	_add_cone(node_name, radius, height, foot + Vector3(0.0, height * 0.5, 0.0), cone_material).visible = false
	_peaks.append({"foot": foot, "radius": radius, "height": height, "variant": variant, "rock": rock,
		"snowline": snowline})

func _add_cone(node_name: String, radius: float, height: float, at: Vector3, material: Material,
		parent: Node3D = null) -> MeshInstance3D:
	var cone := CylinderMesh.new()
	cone.top_radius = 0.0
	cone.bottom_radius = radius
	cone.height = height
	cone.radial_segments = 9
	var instance := MeshInstance3D.new()
	instance.name = node_name
	instance.set_meta(&"part", node_name)
	instance.mesh = cone
	instance.material_override = material
	instance.position = at
	var host: Node3D = parent if parent != null else $TowerPresentation
	host.add_child(instance)
	return instance

# A member spanning two points. Almost every diagonal in a steel frame is one
# of these -- braces, buttress legs, jib booms, tie rods -- and computing the
# pose from the endpoints is far less error-prone than hand-solving rotations.
func _add_strut(node_name: String, from: Vector3, to: Vector3, thickness: float,
		material: Material, parent: Node3D = null) -> MeshInstance3D:
	var delta := to - from
	var length := delta.length()
	if length < 0.01:
		return null
	var host: Node3D = parent if parent != null else $TowerPresentation
	var instance := _add_box_to(node_name, Vector3(thickness, thickness, length),
		from + delta * 0.5, material, host)
	var up := Vector3.UP if absf(delta.normalized().y) < 0.99 else Vector3.RIGHT
	instance.look_at(to, up)
	return instance

func _add_cylinder(node_name: String, radius: float, height: float, at: Vector3,
		material: Material, parent: Node3D = null) -> MeshInstance3D:
	var cylinder := CylinderMesh.new()
	cylinder.top_radius = radius
	cylinder.bottom_radius = radius
	cylinder.height = height
	cylinder.radial_segments = 12
	var instance := MeshInstance3D.new()
	instance.name = node_name
	instance.set_meta(&"part", node_name)
	instance.mesh = cylinder
	instance.material_override = material
	instance.position = at
	var host: Node3D = parent if parent != null else $TowerPresentation
	host.add_child(instance)
	return instance

# Painted text on the structure. Label3D keeps this readable at distance
# without needing an atlas, which is what sells the company's presence in the
# references -- the building talks at you.
func _add_sign_text(text: String, at: Vector3, yaw: float, height_meters: float,
		color: Color, parent: Node3D = null) -> Label3D:
	var label := Label3D.new()
	label.text = text
	label.font_size = 64
	label.pixel_size = height_meters / 64.0
	label.modulate = color
	# Paint: lit as what it is painted on is, never glowing in the dark (at
	# night unlit text hung in the air over its unseen board).
	label.shaded = true
	label.double_sided = true
	label.horizontal_alignment = HORIZONTAL_ALIGNMENT_CENTER
	label.vertical_alignment = VERTICAL_ALIGNMENT_CENTER
	label.position = at
	label.rotation = Vector3(0.0, yaw, 0.0)
	var host: Node3D = parent if parent != null else $TowerPresentation
	host.add_child(label)
	return label

# Real bump mapping, not flat-shaded boxes. gl_compatibility (what CI renders
# with) has no SSAO/SSIL/SDFGI -- those are Forward+-only -- so per-pixel
# normal perturbation is the actual lever available here for surface detail,
# and it is a basic, renderer-agnostic feature confirmed working by direct
# probe (NoiseTexture2D.as_normal_map). One shared texture per kind of
# surface, not one per material instance, since the grain frequency is what
# distinguishes them and dozens of unique noise textures would cost more than
# they are worth. The relief goes with the surface, not the metal under it:
# rust, concrete and timber are rough and pitted, a few centimeters to a pit;
# paint, enamel, galvanizing, mill scale and copper under its patina are
# smooth (a drum, a gas bottle, the forklift's propane tank, a machine's
# frame, a deck plate, a rail, a pipe), and take only a sheet's slight
# waviness, about a meter across and shallow. Pitted, a painted tank read as
# rock (the owner's playtest, 2026-10-20).
var _bump_rust: NoiseTexture2D
var _bump_timber: NoiseTexture2D
var _bump_concrete: NoiseTexture2D
var _bump_smooth: NoiseTexture2D
# Weathering (task 93, the look pass): every bumped surface also takes a shared
# grime map, blotches a metre or so across that darken the base colour by up
# to a third, and steel and concrete take rust and water streaks running down
# their faces (a detail layer, triplanar like the bumps: on a wall the streaks
# hang straight down). Three shared textures for the whole world.
var _weathering: NoiseTexture2D
var _streaks: ImageTexture
var _detail_mask: ImageTexture

func _build_bump_textures() -> void:
	_bump_rust = _make_bump_texture(1, 0.5, 1.8, false)
	_bump_timber = _make_bump_texture(2, 0.7, 2.6, true)
	_bump_concrete = _make_bump_texture(3, 0.7, 1.3, false)
	_bump_smooth = _make_bump_texture(4, 0.03, 0.6, false)
	_weathering = _make_weathering_texture()
	_streaks = _make_streak_texture()
	var white := Image.create(4, 4, false, Image.FORMAT_L8)
	white.fill(Color.WHITE)
	_detail_mask = ImageTexture.create_from_image(white)

func _make_weathering_texture() -> NoiseTexture2D:
	var noise := FastNoiseLite.new()
	noise.seed = 41
	noise.frequency = 0.018
	noise.fractal_octaves = 5
	var ramp := Gradient.new()
	ramp.set_color(0, Color(0.64, 0.61, 0.58))
	ramp.set_color(1, Color(1.0, 1.0, 1.0))
	ramp.add_point(0.42, Color(0.86, 0.84, 0.81))
	var tex := NoiseTexture2D.new()
	tex.width = 256
	tex.height = 256
	tex.seamless = true
	tex.generate_mipmaps = true
	tex.color_ramp = ramp
	tex.noise = noise
	return tex

# Streaks that start somewhere and fade as they run down (texture v is down
# the face under triplanar mapping), tiling in both directions. Seeded: the
# same streaks every run.
func _make_streak_texture() -> ImageTexture:
	const SIZE := 256
	var alpha := PackedFloat32Array()
	alpha.resize(SIZE * SIZE)
	var rng := RandomNumberGenerator.new()
	rng.seed = 9137
	for streak in 110:
		var x0 := rng.randi_range(0, SIZE - 1)
		var width := rng.randi_range(1, 4)
		var y0 := rng.randi_range(0, SIZE - 1)
		var length := rng.randi_range(24, 210)
		var strength := rng.randf_range(0.18, 0.62)
		for dy in length:
			var t := float(dy) / float(length)
			var a := strength * (1.0 - t) * (1.0 - t)
			for dx in width:
				# Soft at the streak's sides.
				var side := 1.0 if width <= 2 else (0.6 if dx == 0 or dx == width - 1 else 1.0)
				var index := ((y0 + dy) % SIZE) * SIZE + (x0 + dx) % SIZE
				alpha[index] = maxf(alpha[index], a * side)
	var image := Image.create(SIZE, SIZE, true, Image.FORMAT_RGBA8)
	for y in SIZE:
		for x in SIZE:
			image.set_pixel(x, y, Color(0.30, 0.15, 0.07, alpha[y * SIZE + x]))
	image.generate_mipmaps()
	return ImageTexture.create_from_image(image)

func _make_bump_texture(seed_value: int, frequency: float, strength: float,
		directional: bool) -> NoiseTexture2D:
	var noise := FastNoiseLite.new()
	noise.seed = seed_value
	noise.frequency = frequency
	noise.fractal_octaves = 4
	noise.fractal_lacunarity = 2.1
	if directional:
		# Wood grain: stretched noise reads as fibrous rather than pitted.
		noise.frequency = frequency * 0.2
		noise.fractal_type = FastNoiseLite.FRACTAL_RIDGED
	var tex := NoiseTexture2D.new()
	tex.width = 256
	tex.height = 256
	tex.seamless = true
	tex.generate_mipmaps = true
	tex.as_normal_map = true
	tex.bump_strength = strength
	tex.noise = noise
	return tex

func _material(color: Color, metallic: float, roughness: float,
		emission: Color = Color.BLACK, emission_energy: float = 1.0,
		bump: NoiseTexture2D = null) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.metallic = metallic
	material.roughness = roughness
	if emission != Color.BLACK:
		material.emission_enabled = true
		material.emission = emission
		material.emission_energy_multiplier = emission_energy
	if bump != null:
		material.normal_enabled = true
		material.normal_texture = bump
		if _weathering != null:
			material.albedo_texture = _weathering
		if _streaks != null and bump != _bump_timber:
			material.detail_enabled = true
			material.detail_albedo = _streaks
			material.detail_mask = _detail_mask
			material.detail_blend_mode = BaseMaterial3D.BLEND_MODE_MIX
			material.detail_uv_layer = BaseMaterial3D.DETAIL_UV_1
		material.uv1_triplanar = true
		material.uv1_triplanar_sharpness = 1.0
		material.uv1_scale = Vector3(0.22, 0.22, 0.22)
	return material

# The static world, drawn in batches. Every box, strut and cylinder of
# $TowerPresentation is built as its own node (the solids export reads them
# one by one), and every kit body's part comes from the native one by one;
# drawn that way the tower was ~9,700 draw calls looking down from the 750
# deck (measured, perf_top). After the build, everything that never moves is
# merged twice:
# - near: one mesh per material per section (BATCH_CELL_* metres), drawn to
#   BATCH_NEAR_RANGE; a lamp lights only the sections in its reach. Parts no
#   bigger than BATCH_FINE_SIZE get their own batches, drawn only to
#   BATCH_FINE_RANGE and casting no shadow;
# - far: one mesh per large section (BATCH_FAR_CELL metres) without those
#   small parts, each part's colour carried in its vertices under one plain
#   material, drawn from BATCH_NEAR_RANGE out.
# Parts that differ only in colour are drawn as one, their colours carried in
# their vertices. A moving kit body's parts
# become one mesh per material that moves with it. Transparent parts,
# labels, particles and anything with a script stay as they are.
const BATCH_CELL_XZ := 96.0
const BATCH_CELL_Y := 48.0
const BATCH_FAR_CELL := 192.0
const BATCH_NEAR_RANGE := 260.0
const BATCH_FINE_SIZE := 1.5
const BATCH_FINE_RANGE := 160.0
const BATCH_FADE := 20.0
# The far batches' render layer, which no lamp lights; and how far a lamp
# lights at all (it fades over BATCH_FADE beyond): this renderer draws an
# object once more for every lamp that reaches it.
const BATCH_FAR_LAYER := 2
# Wet Isolation (AS-007, 220 -> 340 m) is wet: its standing structure is
# drawn as wet. A porous surface (rust, concrete, timber, paint) soaks the
# water up: darker, and glossy with its film. Bare metal does not soak:
# only a little glossier. (The sky is not mirrored -- sky_cycle.gd -- so a
# glossy metal would show black but for pinpoint glints off its bump map:
# observed on C7's walkway.) A part is wet by where its middle stands.
# Moving bodies keep their own finish (they travel between bands).
const WET_BAND_LOW := 219.0
const WET_BAND_HIGH := 341.0
const WET_POROUS_BELOW_METALLIC := 0.5
const WET_DARKEN := 0.8
const WET_ROUGHNESS := 0.35
const WET_METAL_ROUGHNESS := 0.85
const WET_ROUGHNESS_MIN := 0.12

class Batcher:
	# A batch under construction: one surface's arrays, the place it stands,
	# and the material it is drawn with.
	class Group:
		var vertices := PackedVector3Array()
		var normals := PackedVector3Array()
		var tangents := PackedFloat32Array()
		var uvs := PackedVector2Array()
		var colours := PackedColorArray()
		var indices := PackedInt32Array()
		var origin := Vector3.ZERO
		var material: Material
		var fine := false
		# The far section this one is drawn in at a distance ("" for a fine
		# batch, which no far section carries).
		var far_key := ""

	var near := {}
	var far := {}
	var finishes := {}    # finish signature -> the shared material drawing it
	var far_material: StandardMaterial3D
	var parts := 0
	var wet_parts := 0

	func add(mesh: Mesh, surface: int, at: Transform3D, material: Material, fine: bool) -> void:
		var bounds := at * mesh.get_aabb()
		var centre := bounds.get_center()
		var wet := centre.y >= WET_BAND_LOW and centre.y <= WET_BAND_HIGH
		var finish := _finish(material, wet)
		if wet and finish[2]:
			wet_parts += 1
		var cell := Vector3(floorf(centre.x / BATCH_CELL_XZ), floorf(centre.y / BATCH_CELL_Y),
			floorf(centre.z / BATCH_CELL_XZ))
		# Each batch stands at its section's middle (its vertices relative to
		# that), so its draw distance is measured to the section.
		var group := _group(near, "%d/%d/%d/%s/%d" % [cell.x, cell.y, cell.z, finish[0], int(fine)],
			(cell + Vector3(0.5, 0.5, 0.5)) * Vector3(BATCH_CELL_XZ, BATCH_CELL_Y, BATCH_CELL_XZ), finishes[finish[0]])
		group.fine = fine
		Batcher.append_arrays(group, mesh, surface, at, finish[1])
		if not fine:
			# A near section (BATCH_CELL_*) lies inside one far section.
			group.far_key = "%d/%d/%d" % [floorf(centre.x / BATCH_FAR_CELL), floorf(centre.y / BATCH_FAR_CELL),
				floorf(centre.z / BATCH_FAR_CELL)]
			if far_material == null:
				far_material = StandardMaterial3D.new()
				far_material.vertex_color_use_as_albedo = true
				far_material.vertex_color_is_srgb = true
				far_material.roughness = 0.9
			var far_cell := Vector3(floorf(centre.x / BATCH_FAR_CELL), floorf(centre.y / BATCH_FAR_CELL),
				floorf(centre.z / BATCH_FAR_CELL))
			var far_group := _group(far, "%d/%d/%d" % [far_cell.x, far_cell.y, far_cell.z],
				(far_cell + Vector3(0.5, 0.5, 0.5)) * BATCH_FAR_CELL, far_material)
			var standard := material as StandardMaterial3D
			var far_colour := standard.albedo_color if standard != null else Color(0.35, 0.33, 0.3)
			Batcher.append_arrays(far_group, mesh, surface, at,
				Batcher._wet_colour(far_colour) if wet and finish[2] and Batcher._porous(standard) else far_colour)
		parts += 1

	# A material's finish: what it looks like but for its colour, which the
	# batch carries in its vertices. [signature, vertex colour, wet]. A
	# material with a colour texture or a shader of its own is drawn as it is,
	# never wet; so is glowing or see-through material (lamps, glass, water).
	func _finish(material: Material, wet: bool = false) -> Array:
		var standard := material as StandardMaterial3D
		if standard == null or standard.albedo_texture != null:
			var own := "id%d" % material.get_instance_id()
			if not finishes.has(own):
				finishes[own] = material
			return [own, Color.WHITE, false]
		wet = wet and not standard.emission_enabled \
			and standard.transparency == BaseMaterial3D.TRANSPARENCY_DISABLED
		var normal := standard.normal_texture.get_instance_id() if standard.normal_enabled \
			and standard.normal_texture != null else 0
		var signature := "%.3f|%.3f|%s|%.2f|%d|%d|%s" % [standard.metallic, standard.roughness,
			standard.emission.to_html() if standard.emission_enabled else "-",
			standard.normal_scale if standard.normal_enabled else 0.0, normal, standard.cull_mode,
			str(standard.uv1_scale)] + ("|wet" if wet else "")
		if not finishes.has(signature):
			var shared := standard.duplicate() as StandardMaterial3D
			shared.albedo_color = Color.WHITE
			shared.vertex_color_use_as_albedo = true
			shared.vertex_color_is_srgb = true
			if wet:
				shared.roughness = maxf(WET_ROUGHNESS_MIN, standard.roughness *
					(WET_ROUGHNESS if Batcher._porous(standard) else WET_METAL_ROUGHNESS))
			finishes[signature] = shared
		var colour := standard.albedo_color
		if wet and Batcher._porous(standard):
			colour = Batcher._wet_colour(colour)
		return [signature, colour, wet]

	static func _porous(standard: StandardMaterial3D) -> bool:
		return standard.metallic < WET_POROUS_BELOW_METALLIC

	static func _wet_colour(colour: Color) -> Color:
		return Color(colour.r * WET_DARKEN, colour.g * WET_DARKEN, colour.b * WET_DARKEN, colour.a)

	static func _group(groups: Dictionary, key: String, origin: Vector3, material: Material) -> Group:
		if not groups.has(key):
			var group := Group.new()
			group.origin = origin
			group.material = material
			groups[key] = group
		return groups[key]

	static func append_arrays(group: Group, mesh: Mesh, surface: int, at: Transform3D, colour: Color) -> void:
		var arrays := mesh.surface_get_arrays(surface)
		var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
		var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL] if arrays[Mesh.ARRAY_NORMAL] != null \
			else PackedVector3Array()
		var tangents: PackedFloat32Array = arrays[Mesh.ARRAY_TANGENT] if arrays[Mesh.ARRAY_TANGENT] != null \
			else PackedFloat32Array()
		var uvs: PackedVector2Array = arrays[Mesh.ARRAY_TEX_UV] if arrays[Mesh.ARRAY_TEX_UV] != null \
			else PackedVector2Array()
		var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX] if arrays[Mesh.ARRAY_INDEX] != null \
			else PackedInt32Array()
		var local := Transform3D(Basis(), -group.origin) * at
		var normal_basis := at.basis.inverse().transposed()
		var base := group.vertices.size()
		for i in vertices.size():
			group.vertices.append(local * vertices[i])
			group.normals.append((normal_basis * normals[i]).normalized() if i < normals.size() else Vector3.UP)
			if 4 * i + 3 < tangents.size():
				var t := (at.basis * Vector3(tangents[4 * i], tangents[4 * i + 1], tangents[4 * i + 2])).normalized()
				group.tangents.append_array(PackedFloat32Array([t.x, t.y, t.z, tangents[4 * i + 3]]))
			else:
				group.tangents.append_array(PackedFloat32Array([1.0, 0.0, 0.0, 1.0]))
			group.uvs.append(uvs[i] if i < uvs.size() else Vector2.ZERO)
			group.colours.append(colour)
		if indices.is_empty():
			for i in vertices.size():
				group.indices.append(base + i)
		else:
			for index in indices:
				group.indices.append(base + index)

	static func commit(group: Group) -> MeshInstance3D:
		var arrays := []
		arrays.resize(Mesh.ARRAY_MAX)
		arrays[Mesh.ARRAY_VERTEX] = group.vertices
		arrays[Mesh.ARRAY_NORMAL] = group.normals
		arrays[Mesh.ARRAY_TANGENT] = group.tangents
		arrays[Mesh.ARRAY_TEX_UV] = group.uvs
		arrays[Mesh.ARRAY_COLOR] = group.colours
		arrays[Mesh.ARRAY_INDEX] = group.indices
		var mesh := ArrayMesh.new()
		mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
		mesh.surface_set_material(0, group.material)
		var instance := MeshInstance3D.new()
		instance.name = "Batch"
		instance.mesh = mesh
		instance.position = group.origin
		return instance

	# A kit body's parts by material, with SurfaceTool (they move as one).
	static func append(groups: Dictionary, key: String, mesh: Mesh, surface: int, at: Transform3D,
			signature: String, fine: bool) -> void:
		if not groups.has(key):
			var tool := SurfaceTool.new()
			tool.begin(Mesh.PRIMITIVE_TRIANGLES)
			groups[key] = [tool, signature, fine]
		(groups[key][0] as SurfaceTool).append_from(mesh, surface, at)

	static func instance(tool: SurfaceTool, material: Material) -> MeshInstance3D:
		var instance := MeshInstance3D.new()
		instance.name = "Batch"
		instance.mesh = tool.commit()
		instance.mesh.surface_set_material(0, material)
		return instance

	# Adds the batches under `root`; returns how many.
	func emit(root: Node3D) -> int:
		var far_instances := {}
		for key in far:
			var instance := Batcher.commit(far[key])
			far_instances[key] = instance
			instance.name = "FarBatch"
			# Out of every lamp's light: a far section spans many lamps, and
			# each would draw it again.
			instance.layers = BATCH_FAR_LAYER
			# Past the furthest shadow cascade (ULTRA's 300 m reach is from the
			# camera, these start at BATCH_NEAR_RANGE): casting would only add
			# them to the cascades they overlap.
			instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
			instance.visibility_range_begin = BATCH_NEAR_RANGE
			instance.visibility_range_begin_margin = BATCH_FADE
			instance.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
			root.add_child(instance)
		for key in near:
			var group: Group = near[key]
			var instance := Batcher.commit(group)
			root.add_child(instance)
			if group.fine:
				instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
				instance.visibility_range_end = BATCH_FINE_RANGE
				instance.visibility_range_end_margin = BATCH_FADE
				instance.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
			else:
				# Drawn exactly while its far section is not. Each measured its
				# own distance (to its own middle, 96 m and 192 m sections), so
				# by a section's edge both were drawn, the far copy over the
				# near, or neither (the owner's playtest, 2026-10-20: the view
				# from about 700 m messed up). The far section's one distance now
				# decides for both.
				instance.visibility_parent = instance.get_path_to(far_instances[group.far_key])
		return near.size() + far.size()

func _batch_static_dressing(root: Node3D) -> void:
	var batcher := Batcher.new()
	var merged: Array[MeshInstance3D] = []
	var to_root := root.global_transform.affine_inverse()
	var stack: Array[Node] = root.get_children()
	while not stack.is_empty():
		var node: Node = stack.pop_back()
		stack.append_array(node.get_children())
		var instance := node as MeshInstance3D
		if instance == null or instance.mesh == null or not instance.visible or instance.get_script() != null:
			continue
		var mesh := instance.mesh
		var local := to_root * instance.global_transform
		var bounds := local * mesh.get_aabb()
		var fine := bounds.size[bounds.get_longest_axis_index()] <= BATCH_FINE_SIZE
		var surfaces := mesh.get_surface_count()
		var mergeable := surfaces > 0
		for surface in surfaces:
			var material := _surface_material(instance, surface)
			var base := material as BaseMaterial3D
			if material == null or (base != null and base.transparency != BaseMaterial3D.TRANSPARENCY_DISABLED) \
					or (mesh is ArrayMesh and (mesh as ArrayMesh).surface_get_primitive_type(surface) \
						!= Mesh.PRIMITIVE_TRIANGLES):
				mergeable = false
		if not mergeable:
			continue
		for surface in surfaces:
			batcher.add(mesh, surface, local, _surface_material(instance, surface), fine)
		merged.append(instance)
	var made := batcher.emit(root)
	for instance in merged:
		if instance.get_child_count() == 0:
			instance.queue_free()
		else:
			instance.mesh = null
	_batch_report += "dressing %d parts -> %d meshes, %d wet; " % [merged.size(), made, batcher.wet_parts]

func _surface_material(instance: MeshInstance3D, surface: int) -> Material:
	return instance.material_override if instance.material_override != null \
		else instance.get_active_material(surface)

var _batch_report := ""

func _add_box(node_name: String, size: Vector3, at: Vector3, material: Material,
		roll: float = 0.0) -> MeshInstance3D:
	return _add_box_to(node_name, size, at, material, $TowerPresentation, roll)

func _add_box_to(node_name: String, size: Vector3, at: Vector3, material: Material,
		parent: Node3D, roll: float = 0.0) -> MeshInstance3D:
	var mesh := BoxMesh.new()
	mesh.size = size
	mesh.material = material
	var instance := MeshInstance3D.new()
	instance.name = node_name
	instance.set_meta(&"part", node_name)
	instance.mesh = mesh
	instance.position = at
	if not is_zero_approx(roll):
		instance.rotation = Vector3(0.0, 0.0, roll)
	parent.add_child(instance)
	return instance

func _box_mesh(size: Vector3) -> BoxMesh:
	var mesh := BoxMesh.new()
	mesh.size = size
	return mesh

func _cylinder_mesh(radius: float, height: float) -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius
	mesh.bottom_radius = radius
	mesh.height = height
	mesh.radial_segments = 12
	return mesh

# A presentation hash in 0..1, the same every run.
static func _hash01(i: int, j: int) -> float:
	return fposmod(sin(float(i) * 12.9898 + float(j) * 78.233) * 43758.5453, 1.0)

# A soft round puff, made once and shared.
var _puff: ImageTexture
func _soft_puff() -> ImageTexture:
	if _puff == null:
		var size := 32
		var image := Image.create(size, size, false, Image.FORMAT_RGBA8)
		for y in size:
			for x in size:
				var d := Vector2(x - size * 0.5 + 0.5, y - size * 0.5 + 0.5).length() / (size * 0.5)
				var a := clampf(1.0 - d, 0.0, 1.0)
				image.set_pixel(x, y, Color(1.0, 1.0, 1.0, a * a))
		_puff = ImageTexture.create_from_image(image)
	return _puff

func _lay_segment(instance: MeshInstance3D, from: Vector3, to: Vector3) -> void:
	var delta := to - from
	var length := delta.length()
	if length < 0.001:
		instance.visible = false
		return
	var along := delta / length
	var up := Vector3.UP if absf(along.y) < 0.99 else Vector3.RIGHT
	var side := up.cross(along).normalized()
	instance.transform = Transform3D(Basis(side, along.cross(side), along * length),
		from + delta * 0.5)
	instance.visible = true
