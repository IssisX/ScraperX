class_name ReclaimDust
extends Node3D

# Read-only, negligible-mass powder. Native bodies own every consequential
# fragment, collision and load; these short-lived billboards have none.
const POOL_SIZE := 3
const MAX_PARTICLES := 32
const BURST_INTERVAL := 0.12
const PENDING_LIFETIME := 0.24
const MAX_DISTANCE := 42.0
const MIN_IMPACT_SPEED := 0.8

var _pool: Array[CPUParticles3D] = []
var _busy: Array[bool] = []
var _last_impact_count := -1
var _last_break_serial := -1
var _cooldown := 0.0
var _pending: Dictionary = {}
var _pending_age := 0.0
var _reduced_motion := false


func _ready() -> void:
	setup()


func setup() -> void:
	if not _pool.is_empty():
		return
	var mesh := _powder_mesh()
	var fade := Gradient.new()
	fade.offsets = PackedFloat32Array([0.0, 0.12, 0.55, 1.0])
	fade.colors = PackedColorArray([Color(0.72, 0.40, 0.23, 0.0),
		Color(0.72, 0.40, 0.23, 0.34), Color(0.65, 0.39, 0.25, 0.17),
		Color(0.65, 0.39, 0.25, 0.0)])
	for index in POOL_SIZE:
		var particles := CPUParticles3D.new()
		particles.name = "ReclaimPowder%d" % index
		particles.emitting = false
		particles.visible = false
		particles.one_shot = true
		particles.amount = MAX_PARTICLES
		particles.lifetime = 0.72
		particles.lifetime_randomness = 0.25
		particles.explosiveness = 1.0
		particles.local_coords = false
		particles.direction = Vector3.UP
		particles.spread = 68.0
		particles.gravity = Vector3(0.0, -9.8, 0.0)
		particles.damping_min = 1.4
		particles.damping_max = 2.8
		particles.emission_shape = CPUParticles3D.EMISSION_SHAPE_SPHERE
		particles.emission_sphere_radius = 0.08
		particles.scale_amount_min = 0.35
		particles.scale_amount_max = 0.95
		particles.color_ramp = fade
		particles.mesh = mesh
		particles.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		particles.draw_order = CPUParticles3D.DRAW_ORDER_VIEW_DEPTH
		particles.visibility_aabb = AABB(Vector3(-3.0, -4.0, -3.0), Vector3(6.0, 7.0, 6.0))
		particles.finished.connect(_burst_finished.bind(index))
		_pool.append(particles)
		_busy.append(false)
		add_child(particles)


func set_reduced_motion(enabled: bool) -> void:
	if enabled == _reduced_motion:
		return
	_reduced_motion = enabled
	if enabled:
		_stop_bursts()


func reset() -> void:
	_last_impact_count = -1
	_last_break_serial = -1
	_stop_bursts()


func update_from_native(state: Dictionary, delta: float) -> void:
	setup()
	var elapsed := maxf(delta, 0.0) if is_finite(delta) else 0.0
	_cooldown = maxf(0.0, _cooldown - elapsed)
	_pending_age += elapsed
	if _pending_age > PENDING_LIFETIME:
		_pending.clear()
	var impact_count := int(state.get("rubble_impact_count", _last_impact_count))
	var break_serial := int(state.get("rubble_break_serial", _last_break_serial))
	# A restored receipt invalidates both pending and active presentation. Seed
	# the restored counts quietly so historical contacts never produce a cloud.
	if (_last_impact_count >= 0 and impact_count < _last_impact_count) \
			or (_last_break_serial >= 0 and break_serial < _last_break_serial):
		reset()
		_last_impact_count = impact_count
		_last_break_serial = break_serial
		return
	var new_impact := _last_impact_count >= 0 and impact_count > _last_impact_count
	var new_break := _last_break_serial >= 0 and break_serial > _last_break_serial
	_last_impact_count = impact_count
	_last_break_serial = break_serial
	if _reduced_motion:
		return
	if new_impact:
		var speed := _nonnegative_number(state.get("rubble_impact_speed_mps", 0.0))
		if speed >= MIN_IMPACT_SPEED:
			_offer(state.get("rubble_impact_position"), clampf(speed / 10.0, 0.08, 1.0),
				clampf(speed * 0.16, 0.15, 2.5), state)
	if new_break and bool(state.get("rubble_break_valid", false)):
		var force := _nonnegative_number(state.get("rubble_break_force_n", 0.0))
		var torque := _nonnegative_number(state.get("rubble_break_torque_nm", 0.0))
		# Load tunes powder density only; it is not a fracture-energy model or
		# an invented chunk velocity. Contact burst speed uses real closing speed.
		var strength := clampf(maxf(force / 12000.0, torque / 2400.0), 0.15, 1.0)
		_offer(state.get("rubble_break_position"), strength, 0.45 + strength * 0.4, state)
	for index in _pool.size():
		if _busy[index] and not _near_observer(_pool[index].global_position, state):
			_stop_burst(index)
	if _pending.is_empty() or _cooldown > 0.0:
		return
	if not _near_observer(_pending["position"], state):
		_pending.clear()
		return
	for index in _pool.size():
		if _busy[index]:
			continue
		var particles := _pool[index]
		var strength: float = _pending["strength"]
		var speed: float = _pending["speed"]
		particles.global_position = _pending["position"]
		particles.amount = clampi(roundi(8.0 + strength * 24.0), 8, MAX_PARTICLES)
		particles.initial_velocity_min = speed * 0.35
		particles.initial_velocity_max = speed
		particles.visible = true
		_busy[index] = true
		particles.restart()
		particles.emitting = true
		_cooldown = BURST_INTERVAL
		_pending.clear()
		break


func _offer(position_value: Variant, strength: float, speed: float, state: Dictionary) -> void:
	if not position_value is Vector3 or not position_value.is_finite():
		return
	if not _near_observer(position_value, state):
		return
	if not _pending.is_empty() and float(_pending["strength"]) > strength:
		return
	_pending = {"position": position_value, "strength": strength, "speed": speed}
	_pending_age = 0.0


func _near_observer(burst_position: Vector3, state: Dictionary) -> bool:
	var observer: Variant = state.get("player_position")
	if not observer is Vector3 or not observer.is_finite():
		if not is_inside_tree():
			return false
		var camera := get_viewport().get_camera_3d()
		if camera == null:
			return false
		observer = camera.global_position
	return burst_position.distance_squared_to(observer) <= MAX_DISTANCE * MAX_DISTANCE


func _nonnegative_number(value: Variant) -> float:
	if not (value is float or value is int):
		return 0.0
	var number := float(value)
	return maxf(number, 0.0) if is_finite(number) else 0.0


func _stop_bursts() -> void:
	_pending.clear()
	_pending_age = 0.0
	_cooldown = 0.0
	for index in _pool.size():
		_stop_burst(index)


func _stop_burst(index: int) -> void:
	var particles := _pool[index]
	particles.visible = false
	particles.restart()
	particles.emitting = false
	_busy[index] = false


func _burst_finished(index: int) -> void:
	_pool[index].visible = false
	_busy[index] = false


func _powder_mesh() -> QuadMesh:
	var radial := Gradient.new()
	radial.offsets = PackedFloat32Array([0.0, 0.30, 0.72, 1.0])
	radial.colors = PackedColorArray([Color.WHITE, Color(1.0, 1.0, 1.0, 0.70),
		Color(1.0, 1.0, 1.0, 0.16), Color(1.0, 1.0, 1.0, 0.0)])
	var texture := GradientTexture2D.new()
	texture.gradient = radial
	texture.width = 32
	texture.height = 32
	texture.fill = GradientTexture2D.FILL_RADIAL
	texture.fill_from = Vector2(0.5, 0.5)
	texture.fill_to = Vector2(1.0, 0.5)
	var material := StandardMaterial3D.new()
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.shading_mode = BaseMaterial3D.SHADING_MODE_PER_PIXEL
	material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	material.vertex_color_use_as_albedo = true
	material.albedo_texture = texture
	material.roughness = 1.0
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	var mesh := QuadMesh.new()
	mesh.size = Vector2(0.24, 0.24)
	mesh.material = material
	return mesh
