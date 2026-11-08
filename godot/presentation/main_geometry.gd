extends Node3D

# The main scene, layer 1 of 6: geometry. The primitives every builder
# lays (boxes, struts, cones, trees, signs), the industrial palette and its
# bump textures, and the batching of static dressing. Layers: main_geometry,
# main_world, main_machines, main_context, main_feel, main.gd (the scene's
# script); each extends the one before and calls only itself and below.

const LaunchShot := preload("res://presentation/launch_shot.gd")
const Plants := preload("res://presentation/plants.gd")

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
	if base.has_meta(&"surface"):
		return _surfaced(base.albedo_color.darkened(0.35), base.metallic, base.roughness,
			base.get_meta(&"surface") as Surface)
	return _material(base.albedo_color.darkened(0.35), base.metallic, base.roughness, Color.BLACK, 1.0,
		base.normal_texture as NoiseTexture2D)

# Corrugated steel, as a normal map: one rib per texture repeat, across u.
func _corrugation_texture() -> ImageTexture:
	var image := Image.create(32, 4, false, Image.FORMAT_RGB8)
	for i in 32:
		var slope := cos(TAU * (float(i) + 0.5) / 32.0) * 0.9
		var n := Vector3(-slope, 0.0, 1.0).normalized()
		for j in 4:
			image.set_pixel(i, j, Color(n.x * 0.5 + 0.5, 0.5, n.z * 0.5 + 0.5))
	return ImageTexture.create_from_image(image)

# Trees and scrub live in plants.gd (lane E); these delegates keep every
# caller unchanged.
func _add_tree(at: Vector3, height: float, rng: RandomNumberGenerator) -> void:
	Plants.add_tree(self, $TowerPresentation, at, height, rng)

func _add_bush(at: Vector3, spread: float, rng: RandomNumberGenerator) -> void:
	Plants.add_bush(self, $TowerPresentation, at, spread, rng)

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

# Paint on a structure's face (the owner, 2026-10-21, of the yard's slogans:
# "just put that on existing shit"): the text as large as fits inside `face`
# (width, height) less PAINT_MARGIN_M a side, centered at `at` on the face,
# PAINT_LIFT_M off it. No outline: paint has none.
const PAINT_MARGIN_M := 0.3
const PAINT_LIFT_M := 0.01

func _paint_text(text: String, at: Vector3, yaw: float, face: Vector2, color: Color) -> Label3D:
	var lift := Basis(Vector3.UP, yaw) * Vector3(0.0, 0.0, PAINT_LIFT_M)
	var label := _add_sign_text(text, at + lift, yaw, 1.0, color)
	label.outline_size = 0
	var font: Font = label.font if label.font != null else ThemeDB.fallback_font
	var text_px := font.get_multiline_string_size(text, HORIZONTAL_ALIGNMENT_CENTER, -1, label.font_size)
	label.pixel_size = minf((face.x - 2.0 * PAINT_MARGIN_M) / text_px.x,
		(face.y - 2.0 * PAINT_MARGIN_M) / text_px.y)
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
# grime map, blotches a meter or so across that darken the base color by up
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
	_surface_rust = Surface.open("rust", Vector2(1.0, 1.0), 1.0, false)
	_surface_concrete = Surface.open("concrete", Vector2(1.5, 1.5), 1.0, false)
	_surface_paint = Surface.open("paint", Vector2(1.0, 1.0), 1.0, false)
	_surface_galvanised = Surface.open("galvanised", Vector2(1.0, 1.0), 1.0, false)
	_surface_mill_scale = Surface.open("mill_scale", Vector2(1.0, 1.0), 1.0, false)
	_surface_timber = Surface.open("timber", Vector2(2.0, 0.5), 1.0, true)

# The structure's surfaces (environment plan, job 6): rust, concrete, paint,
# galvanising, mill scale and timber, each a color, roughness and relief map
# at a known size (tools/textures/gen_surfaces.py, made in code, seeded): 512
# texels to the meter on the steel, so they hold up at arm's length. The
# color map multiplies the part's own color (carried in the batch's
# vertices), so one set serves every color of its kind and the batcher still
# draws one material per finish. Batched, a part is mapped along its own axes
# in meters (Batcher.surface_frame), so timber grain runs along each plank and a
# brace's texture runs along the brace; broad variation over tens of meters
# is laid in its vertex colors (Batcher.broad_tint), so the tile's repeat
# never shows at 50 m. Unbatched (a moving body's parts, loose stock) it is
# mapped triplanar in the object's own space, so it rides with the body.
class Surface:
	const DIR := "res://presentation/textures/"
	var kind := ""
	var albedo: Texture2D
	var roughness: Texture2D
	var normal: Texture2D
	# Meters the maps cover: (along the part, across it).
	var tile := Vector2.ONE
	var normal_scale := 1.0
	# Each part its own place in the map (each plank its own tree), rather
	# than lined up with its neighbors.
	var own_offset := false

	static func open(name: String, metres: Vector2, depth: float, per_part: bool) -> Surface:
		var surface := Surface.new()
		surface.kind = name
		surface.albedo = load(DIR + name + "_albedo.png") as Texture2D
		surface.roughness = load(DIR + name + "_rough.png") as Texture2D
		surface.normal = load(DIR + name + "_normal.png") as Texture2D
		if surface.albedo == null or surface.roughness == null or surface.normal == null:
			push_error("SCRAPERX_SURFACE_MISSING %s (expected %s%s_{albedo,rough,normal}.png)" % [name, DIR, name])
			return null
		surface.tile = metres
		surface.normal_scale = depth
		surface.own_offset = per_part
		return surface

var _surface_rust: Surface
var _surface_concrete: Surface
var _surface_paint: Surface
var _surface_galvanised: Surface
var _surface_mill_scale: Surface
var _surface_timber: Surface

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
# a wall: the batcher lays them, STREAK_UV_SCALE), tiling in both
# directions. Columns below STREAK_CLEAR_U carry none: every face that is not
# a wall, and every mesh the batcher did not lay, samples there. Seeded: the
# same streaks every run.
const STREAK_UV_SCALE := 0.22
const STREAK_CLEAR_U := 0.0625
const STREAK_FLOOR_UV := Vector2(0.03125, 0.5)
# A face whose normal leans further from level than this is a floor or a
# ceiling: water runs down walls, not across floors (picture 12 of the
# environment plan: streaks lying across the 340 m floor).
const STREAK_WALL_MAX_NY := 0.5

func _make_streak_texture() -> ImageTexture:
	const SIZE := 256
	var alpha := PackedFloat32Array()
	alpha.resize(SIZE * SIZE)
	var rng := RandomNumberGenerator.new()
	rng.seed = 9137
	var clear := int(ceilf(STREAK_CLEAR_U * SIZE)) + 2
	for streak in 110:
		var x0 := rng.randi_range(clear, SIZE - 5)
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

# A porous surface (rust, concrete, timber, matt paint, cloth) hides much of
# its own face from the sky in its pits and pores. This renderer's sky
# reflection adds a rough surface's sheen whatever its color (its
# environment term, env.y * clamp(50 * f0, metallic, 1)), so a dark porous
# albedo read near-white under it (sky_cycle.gd). A specular level below
# 0.25 (f0 below 0.02) is how a material says its cavities occlude: at
# POROUS_SPECULAR the sheen is POROUS_SPECULAR / 0.25 of the open surface's.
const POROUS_SPECULAR := 0.22
const POROUS_ROUGHNESS := 0.8

func _material(color: Color, metallic: float, roughness: float,
		emission: Color = Color.BLACK, emission_energy: float = 1.0,
		bump: NoiseTexture2D = null) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.metallic = metallic
	material.roughness = roughness
	if metallic < 0.05 and roughness >= POROUS_ROUGHNESS:
		material.metallic_specular = POROUS_SPECULAR
	if emission != Color.BLACK:
		material.emission_enabled = true
		material.emission = emission
		material.emission_energy_multiplier = emission_energy
	if bump != null:
		material.normal_enabled = true
		material.normal_texture = bump
		if _weathering != null:
			material.albedo_texture = _weathering
			# The batcher may carry this one's color in its vertices: the
			# weathering is shared by every finish, never a picture of its own.
			material.set_meta(&"weathered", true)
		if _streaks != null and bump != _bump_timber:
			# On UV2, which only the batcher lays: down walls, never on floors.
			material.detail_enabled = true
			material.detail_albedo = _streaks
			material.detail_mask = _detail_mask
			material.detail_blend_mode = BaseMaterial3D.BLEND_MODE_MIX
			material.detail_uv_layer = BaseMaterial3D.DETAIL_UV_2
		material.uv1_triplanar = true
		material.uv1_triplanar_sharpness = 1.0
		material.uv1_scale = Vector3(0.22, 0.22, 0.22)
	return material

# A finish of the structure in one of its real surfaces (Surface, above):
# color, roughness and relief from the surface's maps, grime and streaks as
# every weathered finish has them.
func _surfaced(color: Color, metallic: float, roughness: float, surface: Surface) -> StandardMaterial3D:
	var material := _material(color, metallic, roughness)
	if surface == null:
		return material
	material.albedo_texture = surface.albedo
	material.roughness_texture = surface.roughness
	material.roughness_texture_channel = BaseMaterial3D.TEXTURE_CHANNEL_RED
	material.normal_enabled = true
	material.normal_texture = surface.normal
	material.normal_scale = surface.normal_scale
	material.set_meta(&"weathered", true)
	material.set_meta(&"surface", surface)
	if _streaks != null and surface != _surface_timber:
		material.detail_enabled = true
		material.detail_albedo = _streaks
		material.detail_mask = _detail_mask
		material.detail_blend_mode = BaseMaterial3D.BLEND_MODE_MIX
		material.detail_uv_layer = BaseMaterial3D.DETAIL_UV_2
	# Unbatched: triplanar in the object's own space (it rides with a moving
	# body), at the surface's size across.
	material.uv1_triplanar = true
	material.uv1_triplanar_sharpness = 4.0
	material.uv1_world_triplanar = false
	material.uv1_scale = Vector3.ONE / surface.tile.x
	return material

# The static world, drawn in batches. Every box, strut and cylinder of
# $TowerPresentation is built as its own node (the solids export reads them
# one by one), and every kit body's part comes from the native one by one;
# drawn that way the tower was ~9,700 draw calls looking down from the 750
# deck (measured, perf_top). After the build, everything that never moves is
# merged twice:
# - near: one mesh per material per section (BATCH_CELL_* meters), drawn to
#   BATCH_NEAR_RANGE; a lamp lights only the sections in its reach. Parts no
#   bigger than BATCH_FINE_SIZE get their own batches, drawn only to
#   BATCH_FINE_RANGE and casting no shadow;
# - far: one mesh per large section (BATCH_FAR_CELL meters) without those
#   small parts, each part's color carried in its vertices under one plain
#   material, drawn from BATCH_NEAR_RANGE out.
# Parts that differ only in color are drawn as one, their colors carried in
# their vertices. A moving kit body's parts
# become one mesh per material that moves with it. Transparent parts,
# labels, particles and anything with a script stay as they are.
const BATCH_CELL_XZ := 96.0
const BATCH_CELL_Y := 48.0
# A near section of the parts that are not fine is the far section's size
# (J-045, #248: a quarter of the draws up a full-height view). It is drawn
# exactly while its far section is not, as before, so nothing is drawn nearer
# or farther than it was; the fine parts keep BATCH_CELL_*, their draw
# distance being measured to their section's middle.
const BATCH_CELL_COARSE := 192.0
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

# Contact shading (environment plan, job 8): things sit on things. Every
# part the batcher lays is a box among the solids; each near vertex is
# darkened by the solids within OCCLUSION_REACH of it, the more so the nearer
# and the more they face it -- where a post meets a deck, under a ledge, in a
# corner -- and where the solid is a floor below it, tinted toward the dirt
# and rust bleed that gathers at feet. Baked once at start-up into the
# vertex colors, so it holds the same under either renderer, with SSAO or
# without. The floors are found from the solids themselves, never per place.
# Only parts square to the world shade (a diagonal brace's bounding box is
# mostly air); every part is shaded.
const OCCLUSION_CELL := 2.0
const OCCLUSION_REACH := 1.2
# How far off its face a vertex looks from: so a flush neighbor in the same
# plane (two deck plates edge to edge) barely shades, and a solid the face
# runs into (a deck under a post's foot) shades fully.
const OCCLUSION_OFFSET := 0.12
# Full occlusion darkens by this much; the sum of the solids' shares goes to
# full as 1 - exp(-sum).
const OCCLUSION_DARKEN := 0.42
# A vertex's shading is drawn across its face to the next vertex, so on a
# face whose vertices lie further apart than this it is scaled down by the
# ratio: a corner's contact must not dim a whole 10 m deck or beam (measured
# at S5: the shaded deck 26.7 -> 22.0 before this).
const OCCLUSION_SPAN := 1.5
const OCCLUSION_DIRT := Color(0.22, 0.14, 0.09)
const OCCLUSION_DIRT_MIX := 0.35
# A solid bigger than this across (grade, the great slabs, the rock) is
# hashed in coarse cells of OCCLUSION_BIG_CELL rather than thousands of fine
# ones; a part that big is not shaded itself (its few corners cannot carry
# it).
const OCCLUSION_BIG := 48.0
const OCCLUSION_BIG_CELL := 64.0
# Rings along a standing member: one every OCCLUSION_RING meters, at most
# OCCLUSION_RINGS, on members at least OCCLUSION_RING_MIN long that touch a
# solid at an end.
const OCCLUSION_RING := 0.6
const OCCLUSION_RINGS := 4
const OCCLUSION_RING_MIN := 1.2

class Occlusion:
	var lows := PackedVector3Array()
	var highs := PackedVector3Array()
	var cells := {}                  # Vector3i -> PackedInt32Array of solids
	var big_cells := {}              # Vector3i -> PackedInt32Array of big solids
	var ring_meshes := {}            # "size|axis|n" -> BoxMesh
	var rings_laid := 0

	# Registers a part (its world bounds and its basis); returns its number.
	func add(bounds: AABB, basis: Basis) -> int:
		var index := lows.size()
		lows.append(bounds.position)
		highs.append(bounds.end)
		var square := true
		for k in 3:
			var axis := basis[k].normalized().abs()
			if maxf(axis.x, maxf(axis.y, axis.z)) < 0.995:
				square = false
		if not square:
			return index
		var size := bounds.size
		var is_big := maxf(size.x, maxf(size.y, size.z)) > OCCLUSION_BIG
		var cell := OCCLUSION_BIG_CELL if is_big else OCCLUSION_CELL
		var table: Dictionary = big_cells if is_big else cells
		var low := Vector3i((bounds.position / cell).floor())
		var high := Vector3i((bounds.end / cell).floor())
		for x in range(low.x, high.x + 1):
			for y in range(low.y, high.y + 1):
				for z in range(low.z, high.z + 1):
					var key := Vector3i(x, y, z)
					if not table.has(key):
						table[key] = PackedInt32Array()
					var list: PackedInt32Array = table[key]
					list.append(index)
					table[key] = list
		return index

	# The solids that can shade a part: those within OCCLUSION_REACH of its
	# bounds, but itself. Gathered once a part; its vertices test only these.
	func near(bounds: AABB, self_index: int) -> PackedInt32Array:
		var out := PackedInt32Array()
		var size := bounds.size
		if maxf(size.x, maxf(size.y, size.z)) > OCCLUSION_BIG:
			return out
		var grown := bounds.grow(OCCLUSION_REACH)
		var found := {}
		for pass_index in 2:
			var cell := OCCLUSION_CELL if pass_index == 0 else OCCLUSION_BIG_CELL
			var table: Dictionary = cells if pass_index == 0 else big_cells
			var low := Vector3i((grown.position / cell).floor())
			var high := Vector3i((grown.end / cell).floor())
			for x in range(low.x, high.x + 1):
				for y in range(low.y, high.y + 1):
					for z in range(low.z, high.z + 1):
						var key := Vector3i(x, y, z)
						if table.has(key):
							for i in (table[key] as PackedInt32Array):
								found[i] = true
		for i in found:
			if i != self_index and grown.intersects(AABB(lows[i], highs[i] - lows[i])):
				out.append(i)
		return out

	# [occlusion 0..1, share of it from below] at a vertex, from `solids`.
	func occluded(world: Vector3, normal: Vector3, solids: PackedInt32Array) -> Vector2:
		var eye := world + normal * OCCLUSION_OFFSET
		var total := 0.0
		var below := 0.0
		for i in solids:
			var offset := eye.clamp(lows[i], highs[i]) - eye
			var d := offset.length()
			if d >= OCCLUSION_REACH:
				continue
			if d < 0.0001:
				# The face runs into it: full contact.
				total += 1.0
				if eye.y - lows[i].y > highs[i].y - eye.y:
					below += 1.0
				continue
			var near_share := 1.0 - d / OCCLUSION_REACH
			var share := near_share * near_share * clampf(0.3 + 0.7 * normal.dot(offset) / d, 0.0, 1.0)
			total += share
			if offset.y < -0.6 * d:
				below += share
		if total <= 0.0:
			return Vector2.ZERO
		return Vector2(1.0 - exp(-total), below / total)

	# `reach`: how much of the shading this vertex may carry (OCCLUSION_SPAN).
	func shaded(colour: Color, world: Vector3, normal: Vector3, solids: PackedInt32Array, reach: float) -> Color:
		if solids.is_empty() or reach <= 0.0:
			return colour
		var o := occluded(world, normal, solids)
		if o.x <= 0.0:
			return colour
		var amount := o.x * reach
		var dark := 1.0 - OCCLUSION_DARKEN * amount
		var dirtied := colour.lerp(OCCLUSION_DIRT, OCCLUSION_DIRT_MIX * amount * o.y)
		return Color(dirtied.r * dark, dirtied.g * dark, dirtied.b * dark, colour.a)

	# The box to lay for a part: ringed along its length if it is a long
	# member touching a solid at an end, else as it is.
	func ringed(box: BoxMesh, at: Transform3D, solids: PackedInt32Array) -> BoxMesh:
		var lengths := Vector3(box.size.x * at.basis.x.length(), box.size.y * at.basis.y.length(),
			box.size.z * at.basis.z.length())
		var k := lengths.max_axis_index()
		if lengths[k] < OCCLUSION_RING_MIN or solids.is_empty():
			return box
		var axis: Vector3 = at.basis[k].normalized()
		# Only a standing member (a post, a column, a leg): its foot is where
		# the eye looks for the contact. A beam keeps its corners' shading.
		if absf(axis.y) < 0.5:
			return box
		var centre := at.origin
		var half := 0.5 * lengths[k]
		var touching := false
		for end_sign in [-1.0, 1.0]:
			var end: Vector3 = centre + axis * (half * end_sign)
			# Looking from just past the end, along the member.
			if occluded(end, axis * end_sign, solids).x > 0.25:
				touching = true
		if not touching:
			return box
		var n := clampi(int(ceilf(lengths[k] / OCCLUSION_RING)) - 1, 1, OCCLUSION_RINGS)
		var key := "%s|%d|%d" % [str(box.size), k, n]
		if not ring_meshes.has(key):
			var made := BoxMesh.new()
			made.size = box.size
			match k:
				0: made.subdivide_width = n
				1: made.subdivide_height = n
				_: made.subdivide_depth = n
			ring_meshes[key] = made
		rings_laid += 1
		return ring_meshes[key]

class Batcher:
	# A batch under construction: one surface's arrays, the place it stands,
	# and the material it is drawn with.
	class Group:
		var vertices := PackedVector3Array()
		var normals := PackedVector3Array()
		var tangents := PackedFloat32Array()
		var uvs := PackedVector2Array()
		var colours := PackedColorArray()
		var uv2s := PackedVector2Array()
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
	# Contact shading (environment plan, job 8): the near parts wait here
	# until every part is known ([group, mesh, surface, at, color, mapping,
	# solid index]), then are laid shaded by the solids around them.
	var pending: Array = []
	var shade_ms := 0
	# The solids that shade: this batcher's parts, and any laid before it
	# (the dressing's, for the kit's statics). Set before the first add to
	# share one.
	var occlusion: Occlusion

	func add(mesh: Mesh, surface: int, at: Transform3D, material: Material, fine: bool) -> void:
		var bounds := at * mesh.get_aabb()
		var centre := bounds.get_center()
		var wet := centre.y >= WET_BAND_LOW and centre.y <= WET_BAND_HIGH
		var finish := _finish(material, wet)
		if wet and finish[2]:
			wet_parts += 1
		var cell_size := Vector3(BATCH_CELL_XZ, BATCH_CELL_Y, BATCH_CELL_XZ) if fine \
			else Vector3(BATCH_CELL_COARSE, BATCH_CELL_COARSE, BATCH_CELL_COARSE)
		var cell := (centre / cell_size).floor()
		# Each batch stands at its section's middle (its vertices relative to
		# that), so its draw distance is measured to the section.
		var group := _group(near, "%d/%d/%d/%s/%d" % [cell.x, cell.y, cell.z, finish[0], int(fine)],
			(cell + Vector3(0.5, 0.5, 0.5)) * cell_size, finishes[finish[0]])
		group.fine = fine
		var standard := material as StandardMaterial3D
		var real: Surface = standard.get_meta(&"surface") as Surface \
			if standard != null and standard.has_meta(&"surface") else null
		if occlusion == null:
			occlusion = Occlusion.new()
		pending.append([group, mesh, surface, at, finish[1], real, occlusion.add(bounds, at.basis)])
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
			var far_colour := standard.albedo_color if standard != null else Color(0.35, 0.33, 0.3)
			Batcher.append_arrays(far_group, mesh, surface, at,
				Batcher._wet_colour(far_colour) if wet and finish[2] and Batcher._porous(standard) else far_colour,
				null, real != null)
		parts += 1

	# A material's finish: what it looks like but for its color, which the
	# batch carries in its vertices. [signature, vertex color, wet]. A
	# material with a color texture or a shader of its own is drawn as it is,
	# never wet; so is glowing or see-through material (lamps, glass, water).
	func _finish(material: Material, wet: bool = false) -> Array:
		var standard := material as StandardMaterial3D
		if standard == null or (standard.albedo_texture != null and not standard.has_meta(&"weathered")):
			var own := "id%d" % material.get_instance_id()
			if not finishes.has(own):
				finishes[own] = material
			return [own, Color.WHITE, false]
		wet = wet and not standard.emission_enabled \
			and standard.transparency == BaseMaterial3D.TRANSPARENCY_DISABLED
		var normal := standard.normal_texture.get_instance_id() if standard.normal_enabled \
			and standard.normal_texture != null else 0
		var albedo := standard.albedo_texture.get_instance_id() if standard.albedo_texture != null else 0
		var detail := standard.detail_albedo.get_instance_id() if standard.detail_enabled \
			and standard.detail_albedo != null else 0
		var signature := "%.3f|%.3f|%.3f|%s|%.2f|%d|%d|%s|%d|%d" % [standard.metallic, standard.roughness,
			standard.metallic_specular, standard.emission.to_html() if standard.emission_enabled else "-",
			standard.normal_scale if standard.normal_enabled else 0.0, normal, standard.cull_mode,
			str(standard.uv1_scale), albedo, detail] + ("|wet" if wet else "")
		if not finishes.has(signature):
			var shared := standard.duplicate() as StandardMaterial3D
			shared.albedo_color = Color.WHITE
			shared.vertex_color_use_as_albedo = true
			shared.vertex_color_is_srgb = true
			if standard.has_meta(&"surface"):
				# Mapped in meters along each part (Batcher.surface_frame), not
				# triplanar: one texture read per map, not three.
				shared.uv1_triplanar = false
				shared.uv1_scale = Vector3.ONE
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

	# `mapping`: the real surface to lay UVs for (null keeps the mesh's own).
	# `vary`: lay the broad variation (broad_tint) in the vertex colors.
	# `shade`/`solids`: what shades its vertices, and the solids near it
	# (Occlusion.near; null: unshaded).
	static func append_arrays(group: Group, mesh: Mesh, surface: int, at: Transform3D, colour: Color,
			mapping: Surface = null, vary: bool = false, shade: Occlusion = null,
			solids: PackedInt32Array = PackedInt32Array()) -> void:
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
		# The part's own frame: unit axes in the world and its size along each.
		var axes: Array[Vector3] = [at.basis.x.normalized(), at.basis.y.normalized(), at.basis.z.normalized()]
		var size := mesh.get_aabb().size
		var extents := Vector3(size.x * at.basis.x.length(), size.y * at.basis.y.length(),
			size.z * at.basis.z.length())
		var round_part := mesh is CylinderMesh or mesh is CapsuleMesh
		# How far apart a box's vertices lie along each of its axes.
		var spacing := extents
		var box := mesh as BoxMesh
		if box != null:
			spacing = Vector3(extents.x / (box.subdivide_width + 1), extents.y / (box.subdivide_height + 1),
				extents.z / (box.subdivide_depth + 1))
		var part_seed := Batcher._part_hash(at.origin)
		var offset := Vector2.ZERO
		if mapping != null and mapping.own_offset:
			offset = Vector2(part_seed, Batcher._part_hash(at.origin + Vector3(17.0, 5.0, 3.0)))
		# The broad variation, sampled three times a part (its middle and the
		# ends of its longest side) and drawn between them: its wavelength is
		# far longer than nearly every part.
		var long_axis := extents.max_axis_index()
		var half_long := maxf(0.5 * extents[long_axis], 0.001)
		var middle := at * mesh.get_aabb().get_center()
		var tint_mid := 1.0
		var tint_slope := 0.0
		if vary:
			tint_mid = Batcher.broad_tint(middle, part_seed)
			var tint_end := Batcher.broad_tint(middle + axes[long_axis] * half_long, part_seed)
			var tint_start := Batcher.broad_tint(middle - axes[long_axis] * half_long, part_seed)
			tint_slope = 0.5 * (tint_end - tint_start) / half_long
		# A flat face's frame is the same at all its vertices (they share its
		# normal and come together): laid once a face.
		var frame_normal := Vector3.INF
		var frame_u := Vector3.ZERO
		var frame_v := Vector3.ZERO
		var frame_tangent := Vector3.ZERO
		var frame_sign := 1.0
		var tile := mapping.tile if mapping != null else Vector2.ONE
		for i in vertices.size():
			var world := at * vertices[i]
			var local_normal := normals[i] if i < normals.size() else Vector3.UP
			var normal := (normal_basis * local_normal).normalized()
			group.vertices.append(local * vertices[i])
			group.normals.append(normal)
			if mapping != null:
				var round_side := round_part and absf(local_normal.y) < 0.7
				if round_side:
					# Along the axis, around by the mesh's own u (its tangent).
					frame_u = axes[1]
					frame_tangent = (axes[1] - normal * normal.dot(axes[1])).normalized()
					var around := at.basis * Vector3(tangents[4 * i], tangents[4 * i + 1], tangents[4 * i + 2]) \
						if 4 * i + 3 < tangents.size() else normal.cross(frame_tangent)
					frame_sign = 1.0 if normal.cross(frame_tangent).dot(around) >= 0.0 else -1.0
					frame_normal = Vector3.INF
				elif local_normal != frame_normal:
					var frame := Batcher.surface_frame(normal, local_normal, Vector3.ZERO, axes, extents, false)
					frame_u = axes[frame[0]]
					frame_v = axes[frame[1]]
					frame_tangent = frame[2]
					frame_sign = frame[3]
					frame_normal = local_normal
				var uv: Vector2
				if round_side:
					var mesh_u := uvs[i].x if i < uvs.size() else 0.0
					uv = Vector2(world.dot(frame_u) / tile.x, -(mesh_u * PI * extents.x) / tile.y)
				else:
					uv = Vector2(world.dot(frame_u) / tile.x, -world.dot(frame_v) / tile.y)
				group.uvs.append(uv + offset)
				group.tangents.append(frame_tangent.x)
				group.tangents.append(frame_tangent.y)
				group.tangents.append(frame_tangent.z)
				group.tangents.append(frame_sign)
			else:
				var t := Vector3.RIGHT
				var w := 1.0
				if 4 * i + 3 < tangents.size():
					t = (at.basis * Vector3(tangents[4 * i], tangents[4 * i + 1], tangents[4 * i + 2])).normalized()
					w = tangents[4 * i + 3]
				group.tangents.append(t.x)
				group.tangents.append(t.y)
				group.tangents.append(t.z)
				group.tangents.append(w)
				group.uvs.append(uvs[i] if i < uvs.size() else Vector2.ZERO)
			group.uv2s.append(Batcher.streak_uv(world, normal))
			var c := colour
			if vary:
				var f := tint_mid + tint_slope * (world - middle).dot(axes[long_axis])
				c = Color(colour.r * f, colour.g * f, colour.b * f, colour.a)
			if shade != null and not solids.is_empty():
				# The spacing of this face's vertices: its two sides across.
				var a := local_normal.abs()
				var k := 0 if a.x >= a.y and a.x >= a.z else (1 if a.y >= a.z else 2)
				var span := maxf(spacing[(k + 1) % 3], spacing[(k + 2) % 3])
				c = shade.shaded(c, world, normal, solids, clampf(OCCLUSION_SPAN / maxf(span, 0.001), 0.0, 1.0))
			group.colours.append(c)
		if indices.is_empty():
			for i in vertices.size():
				group.indices.append(base + i)
		else:
			for index in indices:
				group.indices.append(base + index)

	# How a face lies in a real surface's maps: [u axis, v axis (indices into
	# the part's axes), tangent, binormal sign]. A face is mapped along the
	# part's own axes, in meters from the world's origin (so parts lined up
	# meet without a seam), u along the face's longer side: timber grain runs
	# along the plank, rolled steel's along the member. A round part's side
	# runs u along its axis (axis 1) and v around it, by the mesh's own u,
	# which goes once around (`around` is its direction). The tangent is dP/du
	# and the binormal -dP/dv, as Godot's own meshes have them.
	static func surface_frame(normal: Vector3, local_normal: Vector3, around: Vector3,
			axes: Array[Vector3], extents: Vector3, round_side: bool) -> Array:
		var iu := 1
		var iv := 0
		var tangent: Vector3
		var binormal: Vector3
		if round_side:
			tangent = axes[1]
			binormal = around if around.length_squared() > 0.5 else normal.cross(tangent)
		else:
			var a := local_normal.abs()
			var k := 0 if a.x >= a.y and a.x >= a.z else (1 if a.y >= a.z else 2)
			iu = (k + 1) % 3
			iv = (k + 2) % 3
			if extents[iv] > extents[iu]:
				var swap := iu
				iu = iv
				iv = swap
			tangent = axes[iu]
			binormal = axes[iv]
		# Square to the drawn normal (a round part's normals are smooth).
		var square := tangent - normal * normal.dot(tangent)
		if square.length_squared() > 1e-6:
			tangent = square.normalized()
		return [iu, iv, tangent, 1.0 if normal.cross(tangent).dot(binormal) >= 0.0 else -1.0]

	# Broad variation, laid in a real surface's vertex colors: each part a
	# shade off its neighbors (steel from different heats, paint from
	# different years), and a slow drift of value over tens of meters across
	# them, so neither the maps' meter repeat nor a run of identical members
	# reads as a pattern at 50 m. Seeded: the same every run.
	const BROAD_PART := 0.07
	const BROAD_VALUE := 0.14
	const BROAD_METRES := 18.0
	static var _broad: FastNoiseLite

	static func broad_tint(world: Vector3, part_seed: float) -> float:
		if _broad == null:
			_broad = FastNoiseLite.new()
			_broad.seed = 6151
			_broad.frequency = 1.0 / BROAD_METRES
			_broad.fractal_octaves = 2
		return (1.0 + BROAD_PART * (2.0 * part_seed - 1.0)) * (1.0 + BROAD_VALUE * _broad.get_noise_3dv(world))

	# A part's own number in 0..1 from where it stands, the same every run.
	static func _part_hash(at: Vector3) -> float:
		var h := int(floorf(at.x * 8.0)) * 73856093 ^ int(floorf(at.y * 8.0)) * 19349663 \
			^ int(floorf(at.z * 8.0)) * 83492791
		h = (h ^ (h >> 13)) * 1274126177
		return float((h ^ (h >> 16)) & 0xFFFF) / 65535.0

	# Where a vertex samples the streaks (UV2): down a wall, from its height
	# and its run along the wall in world meters, so streaks meet across
	# parts; a floor or a ceiling samples the streaks' clear columns.
	static func streak_uv(world: Vector3, normal: Vector3) -> Vector2:
		if absf(normal.y) > STREAK_WALL_MAX_NY:
			return STREAK_FLOOR_UV
		var along := Vector3(-normal.z, 0.0, normal.x).normalized()
		return Vector2(world.dot(along), -world.y) * STREAK_UV_SCALE

	static func commit(group: Group) -> MeshInstance3D:
		var arrays := []
		arrays.resize(Mesh.ARRAY_MAX)
		arrays[Mesh.ARRAY_VERTEX] = group.vertices
		arrays[Mesh.ARRAY_NORMAL] = group.normals
		arrays[Mesh.ARRAY_TANGENT] = group.tangents
		arrays[Mesh.ARRAY_TEX_UV] = group.uvs
		arrays[Mesh.ARRAY_TEX_UV2] = group.uv2s
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

	# Lays the waiting near parts, each vertex shaded by the solids around it.
	func _lay_pending() -> void:
		for record in pending:
			var mesh: Mesh = record[1]
			var at: Transform3D = record[3]
			# Boxes are shaded: the posts, decks, beams and walls, where things
			# meet. A round part (a pipe, a rail, a drum) carries a hundred
			# vertices for a box's 24 and is left as it is: start-up time.
			var box := mesh as BoxMesh
			var solids := occlusion.near(at * mesh.get_aabb(), record[6]) if box != null else PackedInt32Array()
			# A long member touching something at an end gets rings along
			# its length, so its foot darkens and a meter up does not.
			if box != null:
				mesh = occlusion.ringed(box, at, solids)
			Batcher.append_arrays(record[0], mesh, record[2], at, record[4], record[5], record[5] != null,
				occlusion if box != null else null, solids)
		pending.clear()

	# Adds the batches under `root`; returns how many.
	func emit(root: Node3D) -> int:
		var started := Time.get_ticks_usec()
		_lay_pending()
		shade_ms = (Time.get_ticks_usec() - started) / 1000
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
	var started := Time.get_ticks_usec()
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
	# The kit's statics are shaded by the dressing's solids too.
	_occlusion = batcher.occlusion
	for instance in merged:
		if instance.get_child_count() == 0:
			instance.queue_free()
		else:
			instance.mesh = null
	_batch_report += "dressing %d parts -> %d meshes, %d wet, %d ms (shading %d ms, %d ringed); " % [merged.size(),
		made, batcher.wet_parts, (Time.get_ticks_usec() - started) / 1000, batcher.shade_ms,
		batcher.occlusion.rings_laid if batcher.occlusion != null else 0]

func _surface_material(instance: MeshInstance3D, surface: int) -> Material:
	return instance.material_override if instance.material_override != null \
		else instance.get_active_material(surface)

var _batch_report := ""
# The solids that shade the batches (Occlusion), shared from the dressing to
# the kit's statics.
var _occlusion: Occlusion

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
