extends RefCounted

# A view response to actual support-relative landing events. These springs
# move only the lens; their state never enters the native player solver.
var translation := Vector3.ZERO # Camera right, up, forward, in metres.
var rotation := Vector2.ZERO # Pitch and roll, in radians.
var _translation_velocity := Vector3.ZERO
var _rotation_velocity := Vector2.ZERO
var _last_landing_count := -1


func reset() -> void:
	translation = Vector3.ZERO
	rotation = Vector2.ZERO
	_translation_velocity = Vector3.ZERO
	_rotation_velocity = Vector2.ZERO
	_last_landing_count = -1


func update(state: Dictionary, delta: float, enabled: bool,
		right: Vector3, forward: Vector3) -> void:
	if state.is_empty():
		reset()
		return
	var count := int(state.get("landing_count", 0))
	if not enabled:
		reset()
		_last_landing_count = count # Quiet events are never replayed later.
		return
	if _last_landing_count < 0 or count < _last_landing_count:
		reset()
		_last_landing_count = count
	var normal_speed := maxf(float(state.get("landing_normal_speed_mps", 0.0)), 0.0)
	var tangent_energy := maxf(float(state.get("landing_tangent_energy_j", 0.0)), 0.0)
	var normal_energy := maxf(float(state.get("landing_approach_energy_j", 0.0)), 0.0)
	var slip: Vector3 = state.get("landing_slip_velocity", Vector3.ZERO)
	var side_slip := slip.dot(right)
	var forward_slip := slip.dot(forward)
	if count > _last_landing_count:
		# Compression begins with the measured approach to the support normal.
		# Tangential recoil follows actual remaining slip, including platforms.
		_translation_velocity.y -= minf(normal_speed * 0.55, 5.0)
		_rotation_velocity.x -= minf(normal_speed * 0.035, 0.32)
		var tangent_weight := smoothstep(4.0, 500.0, tangent_energy)
		_translation_velocity.x -= clampf(side_slip * 0.05, -0.25, 0.25) * tangent_weight
		_translation_velocity.z -= clampf(forward_slip * 0.04, -0.20, 0.20) * tangent_weight
		_rotation_velocity.y -= clampf(side_slip * 0.08, -0.40, 0.40) * tangent_weight
	_last_landing_count = count
	var recovering := bool(state.get("landing_recovering", false))
	var imbalance := 1.0 - clampf(float(state.get("landing_balance", 1.0)), 0.0, 1.0)
	var severity := smoothstep(40.0, 3442.5, normal_energy) # 85 kg at 9 m/s is 3442.5 J.
	var target := Vector3.ZERO
	var rotation_target := Vector2.ZERO
	if recovering:
		target.y = -0.07 * severity * imbalance
		target.x = clampf(side_slip / 5.5, -1.0, 1.0) * imbalance * 0.035
		target.z = clampf(forward_slip / 5.5, -1.0, 1.0) * imbalance * 0.025
		rotation_target.x = -0.02 * severity * imbalance
		rotation_target.y = -clampf(side_slip / 5.5, -1.0, 1.0) * imbalance * 0.045
	var dt := maxf(delta, 0.0)
	# Closed-form critically damped evolution is stable even on a slow
	# rendering frame and has the same result when a frame is subdivided.
	var compression := _step(translation.y, _translation_velocity.y, target.y, 18.0, dt)
	translation.y = compression.x
	_translation_velocity.y = compression.y
	for axis in [0, 2]:
		var response := _step(translation[axis], _translation_velocity[axis], target[axis], 14.0, dt)
		translation[axis] = response.x
		_translation_velocity[axis] = response.y
	for axis in 2:
		var response := _step(rotation[axis], _rotation_velocity[axis], rotation_target[axis], 14.0, dt)
		rotation[axis] = response.x
		_rotation_velocity[axis] = response.y
	if not recovering and translation.length_squared() < 1.0e-10 \
			and rotation.length_squared() < 1.0e-10 \
			and _translation_velocity.length_squared() < 1.0e-10 \
			and _rotation_velocity.length_squared() < 1.0e-10:
		translation = Vector3.ZERO
		rotation = Vector2.ZERO
		_translation_velocity = Vector3.ZERO
		_rotation_velocity = Vector2.ZERO


func _step(position: float, velocity: float, target: float,
		frequency: float, delta: float) -> Vector2:
	var displacement := position - target
	var coupled := velocity + frequency * displacement
	var decay := exp(-frequency * delta)
	return Vector2(target + (displacement + coupled * delta) * decay,
		(velocity - frequency * coupled * delta) * decay)
