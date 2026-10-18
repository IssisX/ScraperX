extends Node3D
## Read-only contact feedback. Native contacts own every landing and slide.
const POOL_SIZE := 3
const PARTICLES_PER_BURST := 16
const SLIDE_INTERVAL := 0.18
const MIN_LANDING_MPS := 4.0

var bursts := 0
var _pool: Array[CPUParticles3D] = []
var _busy: Array[bool] = []
var _last_landing := -1
var _last_deaths := -1
var _slide_clock := 0.0


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_PAUSABLE
	setup()


func setup() -> void:
	if not _pool.is_empty():
		return
	var radial := Gradient.new()
	radial.offsets = PackedFloat32Array([0.0, 0.35, 1.0])
	radial.colors = PackedColorArray([Color.WHITE, Color(1, 1, 1, 0.45), Color(1, 1, 1, 0)])
	var texture := GradientTexture2D.new()
	texture.gradient = radial
	texture.width = 24
	texture.height = 24
	texture.fill = GradientTexture2D.FILL_RADIAL
	texture.fill_from = Vector2(0.5, 0.5)
	texture.fill_to = Vector2(1.0, 0.5)
	var material := StandardMaterial3D.new()
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	material.vertex_color_use_as_albedo = true
	material.albedo_texture = texture
	material.roughness = 1.0
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	var mesh := QuadMesh.new()
	mesh.size = Vector2(0.18, 0.18)
	mesh.material = material
	var fade := Gradient.new()
	fade.offsets = PackedFloat32Array([0.0, 0.15, 0.65, 1.0])
	fade.colors = PackedColorArray([Color(0.72, 0.68, 0.59, 0.0),
		Color(0.72, 0.68, 0.59, 0.30), Color(0.64, 0.61, 0.54, 0.12),
		Color(0.64, 0.61, 0.54, 0.0)])
	for index in POOL_SIZE:
		var particles := CPUParticles3D.new()
		particles.name = "ContactDust%d" % index
		particles.emitting = false
		particles.visible = false
		particles.one_shot = true
		particles.amount = PARTICLES_PER_BURST
		particles.lifetime = 0.42
		particles.lifetime_randomness = 0.15
		particles.fixed_fps = 30
		particles.explosiveness = 1.0
		particles.local_coords = false
		particles.emission_shape = CPUParticles3D.EMISSION_SHAPE_SPHERE
		particles.emission_sphere_radius = 0.12
		particles.spread = 65.0
		particles.gravity = Vector3(0, 0.12, 0)
		particles.damping_min = 1.0
		particles.damping_max = 2.0
		particles.color_ramp = fade
		particles.mesh = mesh
		particles.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		particles.draw_order = CPUParticles3D.DRAW_ORDER_VIEW_DEPTH
		particles.visibility_aabb = AABB(Vector3(-2, -1, -2), Vector3(4, 3, 4))
		particles.finished.connect(_finished.bind(index))
		_pool.append(particles)
		_busy.append(false)
		add_child(particles)


func reset() -> void:
	_last_landing = -1
	_last_deaths = -1
	_slide_clock = 0.0
	_stop()


func update_from_native(state: Dictionary, contact: Vector3, relative_velocity: Vector3,
		grounded: bool, support: int, delta: float, deaths: int, enabled: bool) -> void:
	setup()
	var landing := int(state.get("landing_count", _last_landing))
	var rewound := _last_landing >= 0 and landing < _last_landing
	var baseline := _last_landing < 0 or deaths != _last_deaths or rewound
	var new_landing := not baseline and landing > _last_landing
	_last_landing = landing
	_last_deaths = deaths
	if baseline or not enabled or not is_finite(delta) or delta <= 0.0:
		_stop()
		_slide_clock = 0.0
		return
	_slide_clock = maxf(0.0, _slide_clock - delta)
	# Support identity plus a fresh native receipt is required. A remembered
	# impact after the player has left contact does not paint dust in the air.
	if not grounded or not contact.is_finite() or not relative_velocity.is_finite() \
			or support not in [1, 35, 43, 45, 51, 1460]:
		return
	var impact := _number(state.get("landing_normal_speed_mps", 0.0))
	if new_landing and int(state.get("landing_support_entity_id", 0)) == support \
			and impact >= MIN_LANDING_MPS:
		_emit(contact, relative_velocity, clampf(impact / 12.0, 0.3, 1.0))
		_slide_clock = SLIDE_INTERVAL
	elif bool(state.get("player_sliding", false)) \
			and _number(state.get("slide_speed_mps", 0.0)) >= 4.95 and _slide_clock <= 0.0:
		_emit(contact, relative_velocity, 0.25)
		_slide_clock = SLIDE_INTERVAL


func _emit(contact: Vector3, relative_velocity: Vector3, strength: float) -> void:
	for index in _pool.size():
		if _busy[index]:
			continue
		var particles := _pool[index]
		var tangent := Vector3(relative_velocity.x, 0, relative_velocity.z)
		particles.global_position = contact + Vector3.UP * 0.025
		particles.direction = (Vector3.UP - tangent.normalized() * 0.45).normalized()
		particles.initial_velocity_min = 0.1 + strength * 0.15
		particles.initial_velocity_max = 0.3 + strength * 0.65
		particles.scale_amount_min = 0.16 + strength * 0.12
		particles.scale_amount_max = 0.3 + strength * 0.65
		_busy[index] = true
		particles.visible = true
		particles.restart()
		particles.emitting = true
		bursts += 1
		return


func _number(value: Variant) -> float:
	if not (value is int or value is float):
		return 0.0
	var number := float(value)
	return maxf(number, 0.0) if is_finite(number) else 0.0


func _stop() -> void:
	for index in _pool.size():
		_pool[index].emitting = false
		_pool[index].visible = false
		_busy[index] = false


func _finished(index: int) -> void:
	_pool[index].visible = false
	_busy[index] = false
