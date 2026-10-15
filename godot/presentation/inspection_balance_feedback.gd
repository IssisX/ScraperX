extends Node3D
## Presentation only. KitView owns the native leaf and the same 25 kg block.
## Stop dust requires measured rib/stop proximity, never a desired hatch angle.

signal feedback(kind: StringName, strength: float)
signal span_strain(at: Vector3, strength: float)

var _span_sampled := false
var _span_previous := Quaternion.IDENTITY
var _span_cooldown := 0.0

const HATCH := 1979
const STOPS := 1997
const BLOCK := 1902
const METAL := preload("res://assets/audio/footsteps/metal_02.wav")
const NEAR_DISTANCE := 18.0
const FAR_DISTANCE := 30.0

var _sound: AudioStreamPlayer3D
var _dust: CPUParticles3D
var _sampled := false
var _previous_basis := Basis.IDENTITY
var _previous_speed := 0.0
var _cooldown := 0.0
var _gain := 1.0


func _ready() -> void:
	_sound = AudioStreamPlayer3D.new()
	_sound.name = "Bearing"
	_sound.stream = METAL
	_sound.max_distance = FAR_DISTANCE
	_sound.unit_size = 5.0
	_sound.max_polyphony = 1
	add_child(_sound)
	# Headless tests still observe event classification without audio/render work.
	if DisplayServer.get_name() == "headless":
		return
	_dust = CPUParticles3D.new()
	_dust.name = "StopDust"
	_dust.emitting = false
	_dust.one_shot = true
	_dust.amount = 8
	_dust.lifetime = 0.42
	_dust.explosiveness = 1.0
	_dust.local_coords = false
	_dust.direction = Vector3.UP
	_dust.spread = 50.0
	_dust.initial_velocity_min = 0.12
	_dust.initial_velocity_max = 0.35
	_dust.gravity = Vector3(0.0, -0.35, 0.0)
	_dust.scale_amount_min = 0.012
	_dust.scale_amount_max = 0.025
	var grain := SphereMesh.new()
	grain.radius = 1.0
	grain.height = 2.0
	grain.radial_segments = 4
	grain.rings = 2
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.43, 0.40, 0.34, 0.36)
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	grain.material = material
	_dust.mesh = grain
	_dust.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(_dust)


func set_volumes(master: float, effects: float) -> void:
	_gain = clampf(master, 0.0, 1.0) * clampf(effects, 0.0, 1.0)
	if _gain <= 0.0 and is_instance_valid(_sound):
		_sound.stop()


func update_from_native(native: Object, delta: float, reduced_motion := false) -> void:
	if native == null or delta <= 0.0:
		return
	_update_span(native, delta)
	_cooldown = maxf(0.0, _cooldown - delta)
	var hatch := int(native.get_kit_body_index(HATCH))
	if hatch < 0:
		_sampled = false
		return
	var pose: Transform3D = native.get_kit_body_transform(hatch)
	var player: Vector3 = native.get_player_position()
	var distance := player.distance_to(pose.origin)
	# Observe the native block, but never create another mesh or move its body.
	# Its native pose is already drawn by KitView, whether carried or set down.
	var block := int(native.get_kit_body_index(BLOCK))
	if block >= 0 and distance < NEAR_DISTANCE:
		var block_pose: Transform3D = native.get_kit_body_transform(block)
		distance = minf(distance, player.distance_to(block_pose.origin))
	var basis := pose.basis.orthonormalized()
	if not _sampled or delta > 0.15 or distance >= FAR_DISTANCE:
		_previous_basis = basis
		_previous_speed = 0.0
		_sampled = true
		if reduced_motion and _dust != null:
			_dust.emitting = false
		return
	var speed := _angular_speed(_previous_basis.get_rotation_quaternion(),
		basis.get_rotation_quaternion(), delta)
	var stopping := _previous_speed > 0.055 and speed < 0.06 \
		and speed < _previous_speed * 0.55
	var starting := _previous_speed < 0.035 and speed > 0.075
	var strength := clampf(maxf(speed, _previous_speed) / 0.65, 0.12, 1.0)
	var fade := 1.0 - smoothstep(NEAR_DISTANCE, FAR_DISTANCE, distance)
	if _cooldown <= 0.0 and (starting or stopping):
		var contact := _stop_contact(native, pose) if stopping else Vector3.INF
		var at := pose.origin
		var kind: StringName = &"bearing"
		if contact.is_finite():
			at = contact
			kind = &"stop"
			if not reduced_motion and _dust != null:
				_dust.global_position = contact
				_dust.restart()
				_dust.emitting = true
		feedback.emit(kind, strength * fade)
		if _gain > 0.0 and is_instance_valid(_sound) \
				and DisplayServer.get_name() != "headless":
			_sound.global_position = at
			_sound.pitch_scale = 0.58 if kind == &"stop" else 0.72
			_sound.volume_db = linear_to_db(_gain * fade * strength) - 12.0
			_sound.play()
		_cooldown = 0.45
	if reduced_motion and _dust != null:
		_dust.emitting = false
	_previous_basis = basis
	_previous_speed = speed


func _stop_contact(native: Object, leaf: Transform3D) -> Vector3:
	var index := int(native.get_kit_body_index(STOPS))
	if index < 0:
		return Vector3.INF
	var stops: Transform3D = native.get_kit_body_transform(index)
	# Local points match split_inspection_route.cpp's two real steel stops.
	# Compare in the stop body's frame so elastic receiver motion is respected.
	var leaf_from_stops := leaf.affine_inverse() * stops
	for x in [-1.15, 1.15]:
		for front in [false, true]:
			# Observe the actual stop edge against the rib underside plane.
			# Its fixed worldZ lever is1.9m; local ribZ varies with leaf pitch.
			var surface := Vector3(x, 0.06189 if front else 0.4,
				1.1 if front else -1.1)
			var rib: Vector3 = leaf_from_stops * surface
			if absf(rib.y + 0.2) <= 0.008 \
					and rib.z >= -1.008 and rib.z <= 2.008 \
					and absf(rib.x - x) <= 0.07:
				return stops * surface
	return Vector3.INF


static func _angular_speed(previous: Quaternion, current: Quaternion, delta: float) -> float:
	# acos(dot) rounds tiny real motion to zero in single precision. The vector
	# of the relative quaternion preserves it without inventing movement.
	var change := previous.inverse() * current
	return 2.0 * atan2(Vector3(change.x, change.y, change.z).length(), absf(change.w)) / delta


func _update_span(native: Object, delta: float) -> void:
	var index := int(native.get_kit_body_index(1978))
	if index < 0:
		_span_sampled = false
		return
	var pose: Transform3D = native.get_kit_body_transform(index)
	var rotation := pose.basis.orthonormalized().get_rotation_quaternion()
	_span_cooldown = maxf(0.0, _span_cooldown - delta)
	# The native span origin is its upper bearing. Its receiving toe remains
	# the same authored physical point in the body's rotating local frame.
	var toe := pose * Vector3(-28.0, -10.85, 0.0)
	var player: Vector3 = native.get_player_position()
	var at := Geometry3D.get_closest_point_to_segment(player, toe, pose.origin)
	var distance := player.distance_to(at)
	if not _span_sampled or delta > 0.15 or distance >= FAR_DISTANCE:
		_span_previous = rotation
		_span_sampled = true
		return
	var speed := _angular_speed(_span_previous, rotation, delta)
	if speed > 0.0004 and _span_cooldown <= 0.0:
		var strength := clampf(speed / 0.012, 0.15, 1.0)
		span_strain.emit(at, strength * (1.0 - smoothstep(NEAR_DISTANCE, FAR_DISTANCE, distance)))
		_span_cooldown = 4.5
	_span_previous = rotation
