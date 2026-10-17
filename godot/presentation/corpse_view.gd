extends Node3D

# The bodies deaths leave (sim/corpse.hpp), as the player sees them: each
# corpse's eleven parts as the native lays them (the climber's own
# proportions, get_corpse_plan), dressed as the arms are -- the work jacket,
# its hi-vis band at the cuffs, trousers, boots, bare head -- and the blood
# where each body hit: a burst of it thrown up off the hit, and the splash
# it leaves on the floor, both as big as the hit was hard. Every place is
# the native's; nothing here moves a corpse.

const MAX_CORPSES := 8   # Corpses::kMaxCorpses
const PARTS := 11
# Corpses::Part order: pelvis, chest, head, upper arm L, forearm L, upper
# arm R, forearm R, thigh L, shin L, thigh R, shin R.
const HEAD := 2
const FOREARMS := [4, 6]
const SHINS := [8, 10]
const LEGS := [0, 7, 8, 9, 10]
# The splash: this wide at the lethal speed, wider as it was faster.
const SPLASH_M_AT_LETHAL := 1.4
const SPLASH_MAX_M := 3.2
# The burst: drops thrown this fast at the lethal speed, faster as it was
# harder, as many as BURST_DROPS.
const BURST_DROPS := 64
const BURST_MPS_AT_LETHAL := 3.0
const BURST_MAX_MPS := 7.0

var _parts: Array = []      # MAX_CORPSES arrays of PARTS MeshInstance3D
var _splashes: Array[MeshInstance3D] = []
var _next_splash := 0
var _bursts: Array[CPUParticles3D] = []
var _next_burst := 0
var _jacket: StandardMaterial3D
var _band: StandardMaterial3D
var _trousers: StandardMaterial3D
var _boots: StandardMaterial3D
var _skin: StandardMaterial3D


func setup(native: Object) -> void:
	name = "CorpseView"
	_jacket = _material(Color("1f231c"), 0.93)
	_band = _material(Color("b8611d"), 0.55)
	_trousers = _material(Color("23262b"), 0.9)
	_boots = _material(Color("2a1c10"), 0.75)
	_skin = _material(Color("8a5a40"), 0.7)
	var plan: Array = native.get_corpse_plan()
	for corpse in MAX_CORPSES:
		var body: Array = []
		for part in PARTS:
			var shape := CapsuleMesh.new()
			var spec: Dictionary = plan[part]
			shape.radius = float(spec["radius"])
			shape.height = 2.0 * (float(spec["half_length"]) + float(spec["radius"]))
			shape.radial_segments = 10
			shape.rings = 3
			var instance := MeshInstance3D.new()
			instance.mesh = shape
			instance.material_override = _dress(part)
			instance.visible = false
			add_child(instance)
			body.append(instance)
		_parts.append(body)
	# The splash: a flat, wet, glossy sheet on the floor (a mesh, not a
	# decal: the compatibility renderer the game ships on draws it).
	var blood := StandardMaterial3D.new()
	blood.albedo_texture = _splash_texture()
	blood.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	blood.albedo_color = Color(0.62, 0.62, 0.62)
	blood.roughness = 0.35
	blood.metallic_specular = 0.4
	blood.cull_mode = BaseMaterial3D.CULL_DISABLED
	var sheet := PlaneMesh.new()
	sheet.size = Vector2.ONE
	sheet.material = blood
	for i in MAX_CORPSES:
		var splash := MeshInstance3D.new()
		splash.mesh = sheet
		splash.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		splash.visible = false
		add_child(splash)
		_splashes.append(splash)
	# The burst: dark drops flung up off the hit, falling back under gravity.
	var drop_material := StandardMaterial3D.new()
	drop_material.albedo_color = Color(0.2, 0.008, 0.006)
	drop_material.roughness = 0.55
	var drop := SphereMesh.new()
	drop.radius = 0.5
	drop.height = 1.0
	drop.radial_segments = 6
	drop.rings = 3
	drop.material = drop_material
	for i in 2:
		var burst := CPUParticles3D.new()
		burst.mesh = drop
		burst.amount = BURST_DROPS
		burst.lifetime = 1.1
		burst.one_shot = true
		burst.explosiveness = 0.92
		burst.emitting = false
		burst.local_coords = false
		burst.emission_shape = CPUParticles3D.EMISSION_SHAPE_SPHERE
		burst.emission_sphere_radius = 0.25
		burst.direction = Vector3.UP
		burst.spread = 70.0
		burst.gravity = Vector3(0.0, -9.81, 0.0)
		burst.scale_amount_min = 0.025
		burst.scale_amount_max = 0.07
		burst.damping_min = 0.5
		burst.damping_max = 1.5
		add_child(burst)
		_bursts.append(burst)


# The corpses this frame (get_corpses, oldest first). Riding the newest
# one's head, its head is not drawn: the view is inside it.
func update_view(corpses: Array, riding_newest: bool) -> void:
	for corpse in MAX_CORPSES:
		var shown := corpse < corpses.size()
		var parts: Array = corpses[corpse]["parts"] if shown else []
		for part in PARTS:
			var instance: MeshInstance3D = _parts[corpse][part]
			instance.visible = shown and not (riding_newest and part == HEAD and corpse == corpses.size() - 1)
			if shown:
				instance.global_transform = parts[part]


# A death just now: blood thrown up off where the body hit, and the splash
# it leaves there, as big as it hit hard.
func splash(at: Vector3, speed_mps: float, lethal_mps: float) -> void:
	var hard := clampf(speed_mps / maxf(lethal_mps, 1.0), 1.0, 3.0)
	var sheet := _splashes[_next_splash]
	_next_splash = (_next_splash + 1) % _splashes.size()
	var width := clampf(SPLASH_M_AT_LETHAL * hard, SPLASH_M_AT_LETHAL, SPLASH_MAX_M)
	sheet.scale = Vector3(width, 1.0, width)
	sheet.global_position = at + Vector3(0.0, 0.015, 0.0)
	sheet.rotation = Vector3(0.0, fmod(at.x * 7.3 + at.z * 3.1, TAU), 0.0)
	sheet.visible = true
	var burst := _bursts[_next_burst]
	_next_burst = (_next_burst + 1) % _bursts.size()
	var throw := clampf(BURST_MPS_AT_LETHAL * hard, BURST_MPS_AT_LETHAL, BURST_MAX_MPS)
	burst.initial_velocity_min = 0.4 * throw
	burst.initial_velocity_max = throw
	burst.global_position = at + Vector3(0.0, 0.2, 0.0)
	burst.restart()
	burst.emitting = true


# How many splashes lie on the floor now.
func splashes_shown() -> int:
	var shown := 0
	for sheet in _splashes:
		if sheet.visible:
			shown += 1
	return shown


# The newest corpse's head: where the view rides after a death.
static func head_of(corpses: Array) -> Transform3D:
	if corpses.is_empty():
		return Transform3D.IDENTITY
	var parts: Array = corpses[-1]["parts"]
	return parts[HEAD]


func _dress(part: int) -> StandardMaterial3D:
	if part == HEAD:
		return _skin
	if part in FOREARMS:
		return _band
	if part in SHINS:
		return _boots
	if part in LEGS:
		return _trousers
	return _jacket


func _material(color: Color, roughness: float) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = roughness
	return material


# A splash of blood, made once: a dense middle thrown out into droplets and
# runs, its edge broken, dark where it pooled. Fixed seed: the same splash
# every run.
func _splash_texture() -> ImageTexture:
	const SIZE := 128
	var image := Image.create(SIZE, SIZE, false, Image.FORMAT_RGBA8)
	image.fill(Color(1, 1, 1, 0))
	var rng := RandomNumberGenerator.new()
	rng.seed = 6612
	var centre := Vector2(SIZE, SIZE) * 0.5
	var blobs: Array = [[centre, SIZE * 0.22, 1.0]]
	for i in 26:
		var angle := rng.randf() * TAU
		var reach := rng.randf_range(0.12, 0.46) * SIZE
		blobs.append([centre + Vector2.from_angle(angle) * reach, rng.randf_range(1.5, 7.0), rng.randf_range(0.6, 1.0)])
	for i in 7:
		var angle := rng.randf() * TAU
		for step in 10:
			var reach := (0.1 + 0.035 * step) * SIZE
			blobs.append([centre + Vector2.from_angle(angle) * reach, 3.5 - 0.25 * step, 0.85])
	# Each blob laid over only the pixels it covers.
	var alpha := PackedFloat32Array()
	alpha.resize(SIZE * SIZE)
	for blob in blobs:
		var c: Vector2 = blob[0]
		var r := float(blob[1])
		var weight := float(blob[2])
		for y in range(maxi(0, int(c.y - r)), mini(SIZE, int(c.y + r) + 1)):
			for x in range(maxi(0, int(c.x - r)), mini(SIZE, int(c.x + r) + 1)):
				var d := Vector2(x, y).distance_to(c) / r
				if d < 1.0:
					var i := y * SIZE + x
					alpha[i] = maxf(alpha[i], weight * (1.0 - d * d))
	for y in SIZE:
		for x in SIZE:
			var a := alpha[y * SIZE + x]
			if a > 0.05:
				var pooled := clampf(1.0 - Vector2(x, y).distance_to(centre) / (SIZE * 0.3), 0.0, 1.0)
				image.set_pixel(x, y, Color(0.36 - 0.2 * pooled, 0.012, 0.01, clampf(a * 1.6, 0.0, 1.0)))
	return ImageTexture.create_from_image(image)
