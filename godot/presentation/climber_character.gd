extends Node3D

## The launch rider is a visual rig, never a second physics body. Its origin
## is the native capsule midpoint; an upright 1.86 m rider faces local -Z.
## Every visible surface is an authored, bevelled or lofted ArrayMesh.
## Parts are batched by material on each moving joint for the mobile renderer.

const STATURE := 1.86
const SOLE_Y := -0.90
const HELMET_TOP_Y := 0.96
const HEAD_PROFILE := [Vector4(-0.145, 0.060, 0.069, -0.006),
	Vector4(-0.112, 0.094, 0.085, -0.003), Vector4(-0.043, 0.121, 0.105, 0.004),
	Vector4(0.030, 0.125, 0.115, 0.010), Vector4(0.096, 0.114, 0.107, 0.017),
	Vector4(0.146, 0.074, 0.078, 0.025), Vector4(0.163, 0.025, 0.036, 0.025)]

var _built := false
var _clock := 0.0
var _materials: Dictionary = {}
var _surfaces: Dictionary = {}
var _torso: Node3D
var _head: Node3D
var _pelvis: Node3D
var _limbs: Array[Dictionary] = []
var _fingers: Array[Dictionary] = []
var _reaction_seated := false
var _reaction_released := false
var _reaction_draw := 0.0
var _reaction_phase := -1.0
var _motion_amount := 1.0
var _comedy_charge := 0.0
var _native_state: Dictionary = {}
var _last_native_time := -1.0
var _pose_seated := false
var _body_offset := Vector3.ZERO
var _brace_weight := 0.0
var _pose_speed := 0.0
var _torso_motion := Vector2.ZERO
var _torso_spin := Vector2.ZERO
var _head_motion := Vector2.ZERO
var _head_spin := Vector2.ZERO
var _arm_motion := Vector2.ZERO
var _arm_spin := Vector2.ZERO
var _leg_motion := Vector2.ZERO
var _leg_spin := Vector2.ZERO

# Read-only articulated presentation: no extra collision/mass is introduced.
# Native a-g loads these bounded joint oscillators; native elapsed time keeps
# their inertia in the same clock as the pouch, including during bullet time.
const PELVIS_CONTACT := Vector3(0.0, -0.19, 0.0)
const RESPONSE_STEP := 1.0 / 360.0


func reaction(state: Dictionary, _velocity: Vector3 = Vector3.ZERO,
		launch_phase: float = -1.0) -> void:
	# Read-only native event context. update_pose supplies the actual velocity
	# every frame; none of these visual weights feed the simulation.
	_reaction_seated = bool(state.get("seated", false))
	_reaction_released = bool(state.get("released", false))
	_reaction_draw = clampf(float(state.get("draw_m", 0.0)) /
		maxf(float(state.get("max_draw_m", 1.0)), 0.001), 0.0, 1.0)
	_reaction_phase = launch_phase
	_motion_amount = 0.0 if bool(state.get("reduced_motion", false)) else 1.0
	_native_state = state


func _ready() -> void:
	build()


func build() -> void:
	if _built:
		return
	_built = true
	name = "LaunchRider"
	_palette()
	_torso = _joint(self, "WorkJacket")
	_pelvis = _joint(self, "HarnessPelvis")
	_head = _joint(self, "ExpressiveHead")
	_build_jacket()
	_build_harness()
	_build_face()
	_build_helmet()
	for side in [-1.0, 1.0]:
		_build_arm(side)
		_build_leg(side)
	_finish_batches()
	pose(0.0)


func update_pose(player_position: Vector3, player_velocity: Vector3,
		forward: Vector3, delta: float = 0.0, draw: float = 0.0) -> void:
	build()
	position = player_position
	var horizontal := Vector3(forward.x, 0.0, forward.z)
	if horizontal.length_squared() > 0.0001:
		rotation = Vector3(0.0, atan2(-horizontal.x, -horizontal.z), 0.0)
	var elapsed := maxf(delta, 0.0)
	if _native_state.has("simulation_time_seconds"):
		var now := float(_native_state.simulation_time_seconds)
		elapsed = maxf(now - _last_native_time, 0.0) if _last_native_time >= 0.0 else 0.0
		if _last_native_time < 0.0 or now < _last_native_time:
			_reset_response()
			_pose_seated = _reaction_seated
			_brace_weight = 1.0 if _pose_seated else 0.0
		_last_native_time = now
	else:
		# Authored standing/capture callers have no native clock. Gameplay does.
		_last_native_time = -1.0
		_brace_weight = smoothstep(3.0, 30.0, player_velocity.length()) * 0.72
		_pose_speed = player_velocity.length()
	if elapsed > 0.0:
		_pose_seated = _reaction_seated
		_pose_speed = player_velocity.length()
		_advance_response(minf(elapsed, 0.10), player_velocity)
	if _pose_seated and _native_state.has("seat_surface_position"):
		_body_offset = to_local(_native_state.seat_surface_position) - PELVIS_CONTACT
	pose(_pose_speed, _clock, maxf(draw, _reaction_draw if _pose_seated else 0.0))


func _reset_response() -> void:
	_torso_motion = Vector2.ZERO
	_torso_spin = Vector2.ZERO
	_head_motion = Vector2.ZERO
	_head_spin = Vector2.ZERO
	_arm_motion = Vector2.ZERO
	_arm_spin = Vector2.ZERO
	_leg_motion = Vector2.ZERO
	_leg_spin = Vector2.ZERO
	_body_offset = Vector3.ZERO
	_clock = 0.0
	_pose_speed = 0.0
	_comedy_charge = 0.0


func reset_response() -> void:
	# Explicit restart/cinema cancellation can preserve native tick numbering.
	# Discard old load history and seed from the next real state sample.
	_reset_response()
	_native_state = {}
	_last_native_time = -1.0
	_pose_seated = false
	_brace_weight = 0.0


func _angular_load(acceleration: Vector3, lever_y: float, lever_z: float, inertia_per_mass: float) -> Vector2:
	# r cross (-m*a_specific), divided by m-normalized inertia: rad/s².
	return Vector2(-lever_y * acceleration.z + lever_z * acceleration.y,
		lever_y * acceleration.x) / inertia_per_mass


func _joint_step(angle: Vector2, spin: Vector2, load: Vector2,
		target: Vector2, frequency: float, damping: float, limit: Vector2, h: float) -> Array[Vector2]:
	var omega := TAU * frequency
	spin += (load + (target - angle) * omega * omega - spin * (2.0 * damping * omega)) * h
	angle += spin * h
	for axis in 2:
		if absf(angle[axis]) > limit[axis]:
			angle[axis] = clampf(angle[axis], -limit[axis], limit[axis])
			if angle[axis] * spin[axis] > 0.0:
				spin[axis] = 0.0
	return [angle, spin]


func _advance_response(elapsed: float, _velocity: Vector3) -> void:
	var acceleration: Vector3 = _native_state.get("rider_specific_acceleration", Vector3.ZERO)
	acceleration = basis.inverse() * acceleration * _motion_amount
	var steps := maxi(1, int(ceil(elapsed / RESPONSE_STEP)))
	var h := elapsed / float(steps)
	for step in steps:
		_clock += h
		var brace := 1.0 if _pose_seated else (0.72 if _reaction_released else 0.0)
		_brace_weight = lerpf(_brace_weight, brace, 1.0 - exp(-8.0 * h))
		if not _pose_seated:
			var free_offset := Vector3(0, -0.30, 0) if _reaction_released else Vector3.ZERO
			_body_offset = _body_offset.lerp(free_offset, 1.0 - exp(-2.8 * h))
		var previous_spin := _torso_spin
		var torso := _joint_step(_torso_motion, _torso_spin,
			_angular_load(acceleration, 0.28, 0.08, 0.12), Vector2.ZERO,
			4.0, 0.48, Vector2(0.70, 0.46), h)
		_torso_motion = torso[0]
		_torso_spin = torso[1]
		var head := _joint_step(_head_motion, _head_spin,
			_angular_load(acceleration, 0.14, 0.035, 0.035) - (_torso_spin - previous_spin) / h * 0.55,
			_torso_motion, 6.0, 0.42, Vector2(0.85, 0.58), h)
		_head_motion = head[0]
		_head_spin = head[1]
		var arm := _joint_step(_arm_motion, _arm_spin,
			_angular_load(acceleration, 0.19, 0.035, 0.08), Vector2.ZERO,
			3.1, 0.37, Vector2(0.75, 0.55), h)
		_arm_motion = arm[0]
		_arm_spin = arm[1]
		var leg := _joint_step(_leg_motion, _leg_spin,
			_angular_load(acceleration, -0.17, -0.06, 0.12), Vector2.ZERO,
			3.6, 0.40, Vector2(0.55, 0.40), h)
		_leg_motion = leg[0]
		_leg_spin = leg[1]
		_comedy_charge = lerpf(_comedy_charge, _reaction_draw if _pose_seated else 0.0, 1.0 - exp(-14.0 * h))


func pose(speed: float, phase: float = 0.0, draw: float = 0.0) -> void:
	if not _built:
		build()
	var airflow := smoothstep(3.0, 30.0, maxf(speed, 0.0))
	var strain := clampf(draw, 0.0, 1.0)
	var tuck := _brace_weight if _native_state.has("simulation_time_seconds") else maxf(strain * 0.82, airflow * 0.72)
	var shiver := (sin(phase * 8.0) * airflow * 0.008 \
		+ sin(phase * 16.0) * _comedy_charge * 0.006) * _motion_amount
	var pelvis_basis := Basis.from_euler(Vector3(_torso_motion.x * 0.18, 0, _torso_motion.y * 0.18))
	_pelvis.basis = pelvis_basis
	_pelvis.position = _body_offset
	if _pose_seated and _native_state.has("seat_surface_position"):
		# Rotate around the actual underside contact, rather than sinking into
		# the bowl or moving a hidden native collision body to meet the mesh.
		_pelvis.position = to_local(_native_state.seat_surface_position) - pelvis_basis * PELVIS_CONTACT
	_torso.position = _pelvis.position + pelvis_basis * Vector3(0, 0.05 * tuck, 0.0)
	_torso.rotation = Vector3(tuck * 0.24 + _torso_motion.x, 0, _torso_motion.y)
	_head.position = _torso.position + _torso.basis * Vector3(0.0, 0.70, 0.0)
	_head.rotation = Vector3(tuck * 0.24 + _head_motion.x, shiver * 1.5,
		_head_motion.y - shiver)
	for limb in _limbs:
		var s: float = limb.side
		if limb.kind == "arm":
			var shoulder := _torso.position + _torso.basis * Vector3(s * 0.248, 0.424, 0.0)
			var wrist_offset := Vector3(s * lerpf(0.032, -0.065, tuck), lerpf(-0.48, -0.08, tuck), -0.30 * tuck)
			wrist_offset = Basis.from_euler(Vector3(_arm_motion.x, 0, _arm_motion.y)) * wrist_offset
			var wrist := shoulder + wrist_offset
			var arm := _two_bone(shoulder, wrist, Vector3(s, -0.45, 0.35), 0.29, 0.27)
			var elbow := arm[0]
			wrist = arm[1]
			_span(limb.upper, shoulder, elbow, 0.29)
			_span(limb.lower, elbow, wrist, 0.27)
			limb.hand.position = wrist
			limb.hand.basis = limb.lower.basis.orthonormalized()
			limb.hand.rotate_object_local(Vector3.RIGHT, -tuck * 0.5)
			limb.hand.rotate_object_local(Vector3.UP, s * tuck * 0.35)
		else:
			var hip := _pelvis.position + pelvis_basis * Vector3(s * 0.115, -0.105, 0.0)
			var ankle_offset := Vector3(s * 0.05, lerpf(-0.695, 0.18, tuck), lerpf(0.03, -0.56, tuck))
			ankle_offset = Basis.from_euler(Vector3(_leg_motion.x, 0, _leg_motion.y)) * ankle_offset
			var leg := _two_bone(hip, hip + ankle_offset, Vector3(0, 1, -0.35), 0.38, 0.34)
			var knee := leg[0]
			var ankle := leg[1]
			_span(limb.upper, hip, knee, 0.38)
			_span(limb.lower, knee, ankle, 0.34)
			limb.boot.position = ankle
			limb.boot.rotation = Vector3(-tuck * 0.24 + _leg_motion.x, s * 0.07, _leg_motion.y)
	for finger in _fingers:
		var curl := lerpf(0.19, 1.12, maxf(strain, airflow))
		finger.base.rotation.x = -curl
		finger.tip.rotation.x = -curl * 0.86


func _arm_bend(shoulder: Vector3, wanted_wrist: Vector3, pole: Vector3) -> Array[Vector3]:
	# Analytic visual arm: the joke never stretches an arm or alters the
	# rider's native centre/velocity. Keep the shoulder/elbow/wrist connected.
	return _two_bone(shoulder, wanted_wrist, pole, 0.29, 0.27)


func _two_bone(shoulder: Vector3, wanted_wrist: Vector3, pole: Vector3, upper: float, lower: float) -> Array[Vector3]:
	var direction := wanted_wrist - shoulder
	var distance := clampf(direction.length(), absf(upper - lower) + 0.001, upper + lower - 0.001)
	if direction.length_squared() < 0.000001:
		direction = Vector3.DOWN
	direction = direction.normalized()
	var across := pole - direction * pole.dot(direction)
	if across.length_squared() < 0.0001:
		var fallback := Vector3.RIGHT if absf(direction.x) < 0.8 else Vector3.UP
		across = fallback - direction * fallback.dot(direction)
	across = across.normalized()
	var along := (upper * upper - lower * lower + distance * distance) / (2.0 * distance)
	var height := sqrt(maxf(upper * upper - along * along, 0.0))
	return [shoulder + direction * along + across * height, shoulder + direction * distance]


func _palette() -> void:
	for entry in [
		["canvas", "455e52", 0.96, 0.0], ["panel", "617665", 0.96, 0.0],
		["fold", "33473d", 0.98, 0.0], ["seam", "8d9b83", 0.99, 0.0],
		["pants", "293b3b", 0.97, 0.0], ["pants_light", "3c4c49", 0.97, 0.0],
		["rubber", "202625", 0.90, 0.0], ["glove", "816346", 0.90, 0.0],
		["leather", "4a3b2c", 0.86, 0.0], ["stitch", "bba17a", 0.99, 0.0],
		["strap", "9f7b3c", 0.90, 0.0], ["gold", "d0ab50", 0.72, 0.05],
		["reflector", "e4d28d", 0.50, 0.12], ["metal", "aeb8b2", 0.36, 0.78],
		["skin", "bc8b68", 0.90, 0.0], ["skin_shadow", "a57557", 0.94, 0.0],
		["skin_light", "d0a181", 0.93, 0.0], ["hair", "342d27", 0.94, 0.0],
		["lip", "845549", 0.94, 0.0], ["eye", "dfd8c1", 0.65, 0.0],
		["iris", "415950", 0.55, 0.0], ["pupil", "192524", 0.52, 0.0],
		["lamp", "f1e7b8", 0.35, 0.18]]:
		var material := StandardMaterial3D.new()
		material.albedo_color = Color(entry[1])
		material.roughness = float(entry[2])
		material.metallic = float(entry[3])
		material.cull_mode = BaseMaterial3D.CULL_DISABLED
		_materials[entry[0]] = material


func _joint(parent: Node3D, label: String) -> Node3D:
	var result := Node3D.new()
	result.name = label
	parent.add_child(result)
	return result


func _build_jacket() -> void:
	# The chest has shoulder breadth, a tapered waist, front/back thickness,
	# hem overlap, collar and panelled construction rather than a pill.
	_loft(_torso, "canvas", [Vector4(-0.08, 0.172, 0.112, 0.0),
		Vector4(-0.025, 0.188, 0.125, 0.0), Vector4(0.17, 0.198, 0.132, 0.0),
		Vector4(0.35, 0.231, 0.146, 0.012), Vector4(0.44, 0.231, 0.135, 0.018),
		Vector4(0.50, 0.161, 0.105, 0.025)], 12)
	_loft(_torso, "fold", [Vector4(-0.08, 0.175, 0.115, 0.0),
		Vector4(-0.025, 0.190, 0.128, 0.0)], 12)
	for s in [-1.0, 1.0]:
		_poly(_torso, "panel", [Vector3(s * 0.020, 0.335, -0.133),
			Vector3(s * 0.170, 0.35, -0.096), Vector3(s * 0.195, 0.24, -0.087),
			Vector3(s * 0.024, 0.24, -0.137)], Vector3.FORWARD)
		# Chest pocket with an actual flap, stitch and brass snap.
		_box(_torso, "fold", Vector3(0.100, 0.089, 0.012), Vector3(s * 0.087, 0.259, -0.137), 0.005)
		_box(_torso, "panel", Vector3(0.101, 0.030, 0.014), Vector3(s * 0.087, 0.302, -0.148), 0.004)
		_box(_torso, "seam", Vector3(0.091, 0.003, 0.003), Vector3(s * 0.087, 0.288, -0.158), 0.001)
		_box(_torso, "gold", Vector3(0.012, 0.008, 0.004), Vector3(s * 0.087, 0.299, -0.158), 0.002)
		_poly(_torso, "panel", [Vector3(s * 0.075, 0.504, -0.087),
			Vector3(s * 0.149, 0.472, -0.099), Vector3(s * 0.118, 0.391, -0.129),
			Vector3(s * 0.043, 0.477, -0.108)], Vector3.FORWARD)
		_line(_torso, "seam", Vector3(s * 0.177, 0.0, -0.088), Vector3(s * 0.203, 0.24, -0.082), 0.004)
		_line(_torso, "reflector", Vector3(s * 0.07, 0.375, -0.137), Vector3(s * 0.205, 0.38, -0.068), 0.017)
		# The folded hem and side seams catch daylight as the orbit passes.
		_poly(_torso, "fold", [Vector3(s * 0.13, 0.06, -0.11),
			Vector3(s * 0.184, 0.025, -0.08), Vector3(s * 0.169, 0.00, -0.096)], Vector3.FORWARD)
	_box(_torso, "leather", Vector3(0.016, 0.45, 0.009), Vector3(0, 0.205, -0.143), 0.002)
	_box(_torso, "metal", Vector3(0.010, 0.425, 0.007), Vector3(0, 0.205, -0.151), 0.001)
	_box(_torso, "gold", Vector3(0.018, 0.027, 0.01), Vector3(0, 0.405, -0.162), 0.003)
	_loft(_torso, "skin_shadow", [Vector4(0.49, 0.071, 0.065, 0),
		Vector4(0.58, 0.067, 0.061, 0)], 10)
	_loft(_torso, "fold", [Vector4(0.475, 0.090, 0.079, 0.007),
		Vector4(0.533, 0.076, 0.074, 0.007)], 12)
	# Small climbing pack, padded back panel and a strapped rope coil.
	_box(_torso, "leather", Vector3(0.27, 0.35, 0.13), Vector3(0, 0.245, 0.178), 0.035)
	_box(_torso, "fold", Vector3(0.23, 0.27, 0.027), Vector3(0, 0.245, 0.250), 0.018)
	for y in [0.12, 0.21, 0.30, 0.39]:
		_box(_torso, "panel", Vector3(0.195, 0.045, 0.025), Vector3(0, y, 0.267), 0.009)
	for s in [-1.0, 1.0]:
		_line(_torso, "strap", Vector3(s * 0.10, 0.07, 0.274), Vector3(s * 0.10, 0.41, 0.274), 0.026)
	for ring in 3:
		_loop(_torso, "stitch", Vector3(0.0, 0.265, 0.295 + ring * 0.014), Vector2(0.091, 0.119), 0.007, 20)
	_box(_torso, "strap", Vector3(0.045, 0.09, 0.04), Vector3(0.0, 0.262, 0.329), 0.007)


func _build_harness() -> void:
	_loft(_pelvis, "pants", [Vector4(-0.19, 0.155, 0.112, 0.014),
		Vector4(-0.075, 0.182, 0.125, 0.0), Vector4(0.01, 0.171, 0.117, 0)], 12)
	_band(_pelvis, "leather", -0.035, 0.059, 0.194, 0.137, 0.011)
	_band(_pelvis, "strap", -0.036, 0.037, 0.196, 0.140, 0.014)
	_box(_pelvis, "metal", Vector3(0.064, 0.044, 0.018), Vector3(0, -0.035, -0.157), 0.006)
	_box(_pelvis, "rubber", Vector3(0.041, 0.023, 0.007), Vector3(0, -0.035, -0.171), 0.003)
	_box(_pelvis, "strap", Vector3(0.056, 0.15, 0.020), Vector3(0, -0.133, -0.124), 0.005)
	for s in [-1.0, 1.0]:
		var strap_points := [Vector3(s * 0.14, -0.035, -0.130),
			Vector3(s * 0.154, 0.15, -0.142), Vector3(s * 0.16, 0.34, -0.136),
			Vector3(s * 0.176, 0.447, -0.095), Vector3(s * 0.145, 0.493, 0.015),
			Vector3(s * 0.16, 0.385, 0.165), Vector3(s * 0.15, 0.01, 0.145)]
		for i in strap_points.size() - 1:
			_line(_torso, "leather", strap_points[i], strap_points[i + 1], 0.044)
			_line(_torso, "strap", strap_points[i] + Vector3(0, 0, -0.009),
				strap_points[i + 1] + Vector3(0, 0, -0.009), 0.029)
		_box(_torso, "metal", Vector3(0.05, 0.04, 0.025), Vector3(s * 0.158, 0.315, -0.160), 0.005)
		_box(_torso, "rubber", Vector3(0.029, 0.022, 0.012), Vector3(s * 0.158, 0.315, -0.179), 0.003)
		# Hip carabiners have open centres, gates and visible attachment loops.
		_loop(_pelvis, "metal", Vector3(s * 0.185, -0.096, -0.123), Vector2(0.030, 0.048), 0.006, 12)
		_line(_pelvis, "gold", Vector3(s * 0.205, -0.133, -0.132),
			Vector3(s * 0.211, -0.080, -0.132), 0.009)
		_line(_pelvis, "leather", Vector3(s * 0.18, -0.012, -0.124),
			Vector3(s * 0.182, -0.066, -0.124), 0.017)
		_line(_pelvis, "stitch", Vector3(s * 0.177, -0.11, 0.04),
			Vector3(s * 0.23, -0.20, 0.105), 0.009)
		_line(_pelvis, "stitch", Vector3(s * 0.23, -0.20, 0.105),
			Vector3(s * 0.20, -0.27, 0.11), 0.009)
	_loop(_pelvis, "metal", Vector3(0, -0.106, -0.166), Vector2(0.028, 0.027), 0.006, 12)


func _build_face() -> void:
	_loft(_head, "skin", HEAD_PROFILE, 16)
	# Close-cropped hair wraps behind the ears, with a shaped nape below
	# the helmet. It also gives the back of the head a readable silhouette.
	for i in 8:
		var a := PI * 0.5 + PI * float(i) / 8.0
		var b := PI * 0.5 + PI * float(i + 1) / 8.0
		for row in 5:
			var u := float(row) / 5.0
			var v := float(row + 1) / 5.0
			_poly(_head, "hair", [_scalp_point(a, lerpf(0.095, 0.030 + cos(a) * 0.105, u)),
				_scalp_point(b, lerpf(0.095, 0.030 + cos(b) * 0.105, u)),
				_scalp_point(b, lerpf(0.095, 0.030 + cos(b) * 0.105, v)),
				_scalp_point(a, lerpf(0.095, 0.030 + cos(a) * 0.105, v))],
				Vector3(sin((a + b) * 0.5), 0.1, -cos((a + b) * 0.5)).normalized())
	for s in [-1.0, 1.0]:
		# Individual ears, eye sockets, lids, irises and forward looking pupils.
		_box(_head, "skin", Vector3(0.042, 0.078, 0.052), Vector3(s * 0.127, -0.011, 0.008), 0.014)
		_box(_head, "skin_shadow", Vector3(0.012, 0.040, 0.028), Vector3(s * 0.149, -0.011, -0.005), 0.005)
		_face_patch("skin_shadow", [Vector2(s * 0.021, 0.025), Vector2(s * 0.043, 0.043),
			Vector2(s * 0.069, 0.044), Vector2(s * 0.092, 0.030),
			Vector2(s * 0.071, 0.011), Vector2(s * 0.039, 0.010)], 0.0025)
		_face_patch("eye", [Vector2(s * 0.025, 0.026), Vector2(s * 0.044, 0.038),
			Vector2(s * 0.068, 0.039), Vector2(s * 0.087, 0.030),
			Vector2(s * 0.069, 0.017), Vector2(s * 0.043, 0.015)], 0.005)
		_face_disc("iris", Vector2(s * 0.052, 0.027), Vector2(0.0095, 0.0105), 0.007)
		_face_disc("pupil", Vector2(s * 0.052, 0.027), Vector2(0.0045, 0.007), 0.009)
		_face_disc("eye", Vector2(s * 0.050 - 0.003, 0.031), Vector2(0.0022, 0.0025), 0.011)
		_face_patch("hair", [Vector2(s * 0.026, 0.064), Vector2(s * 0.087, 0.074),
			Vector2(s * 0.086, 0.063), Vector2(s * 0.027, 0.054)], 0.003)
		# Cheekbone planes and nasolabial folds give the face a real jaw.
		_poly(_head, "skin_light", [Vector3(s * 0.095, 0.004, -0.078),
			Vector3(s * 0.104, -0.028, -0.078), Vector3(s * 0.048, -0.043, -0.112)], Vector3.FORWARD)
		_line(_head, "skin_shadow", Vector3(s * 0.030, -0.048, -0.110), Vector3(s * 0.043, -0.074, -0.104), 0.003)
		_poly(_head, "skin_shadow", [Vector3(s * 0.029, 0.028, -0.111),
			Vector3(s * 0.020, -0.048, -0.129), Vector3(s * 0.006, -0.034, -0.139)], Vector3.FORWARD)
		_box(_head, "hair", Vector3(0.020, 0.055, 0.037), Vector3(s * 0.112, 0.069, 0.013), 0.006)
	# A shaped bridge, nose tip, separate nostrils, lips and chin.
	_poly(_head, "skin_light", [Vector3(-0.010, 0.037, -0.111),
		Vector3(0.010, 0.037, -0.111), Vector3(0.013, -0.027, -0.136),
		Vector3(0.0, -0.038, -0.142), Vector3(-0.013, -0.027, -0.136)], Vector3.FORWARD)
	_box(_head, "skin", Vector3(0.038, 0.019, 0.023), Vector3(0, -0.039, -0.124), 0.008)
	for s in [-1.0, 1.0]:
		_box(_head, "skin_shadow", Vector3(0.009, 0.004, 0.005), Vector3(s * 0.014, -0.047, -0.134), 0.001)
	_poly(_head, "lip", [Vector3(-0.035, -0.084, -0.096), Vector3(-0.012, -0.078, -0.112),
		Vector3(0.0, -0.081, -0.116), Vector3(0.012, -0.078, -0.112),
		Vector3(0.035, -0.084, -0.096), Vector3(0.012, -0.089, -0.111),
		Vector3(-0.012, -0.089, -0.111)], Vector3.FORWARD)
	_line(_head, "hair", Vector3(-0.028, -0.085, -0.109), Vector3(0.028, -0.085, -0.109), 0.0025)
	_poly(_head, "skin_light", [Vector3(-0.027, -0.095, -0.100),
		Vector3(0.027, -0.095, -0.100), Vector3(0.022, -0.120, -0.090),
		Vector3(-0.022, -0.120, -0.090)], Vector3.FORWARD)
	# Chin strap below the mouth and along the side of the face.
	for s in [-1.0, 1.0]:
		_line(_head, "leather", Vector3(s * 0.133, 0.050, 0.031), Vector3(s * 0.100, -0.096, -0.019), 0.013)
		_line(_head, "leather", Vector3(s * 0.100, -0.096, -0.019), Vector3(s * 0.027, -0.145, -0.054), 0.013)
	_box(_head, "metal", Vector3(0.023, 0.014, 0.014), Vector3(0.015, -0.145, -0.060), 0.003)


func _build_helmet() -> void:
	_loft(_head, "gold", [Vector4(0.094, 0.151, 0.140, 0.018),
		Vector4(0.142, 0.156, 0.144, 0.018), Vector4(0.198, 0.132, 0.128, 0.018),
		Vector4(0.241, 0.087, 0.090, 0.016), Vector4(0.259, 0.023, 0.030, 0.010)], 16)
	_band(_head, "gold", 0.103, 0.015, 0.174, 0.166, 0.013)
	_band(_head, "leather", 0.093, 0.012, 0.149, 0.139, 0.006)
	_box(_head, "reflector", Vector3(0.024, 0.012, 0.18), Vector3(0, 0.251, 0.009), 0.005)
	for s in [-1.0, 1.0]:
		for i in 3:
			_box(_head, "rubber", Vector3(0.009, 0.024, 0.036),
				Vector3(s * (0.138 - i * 0.005), 0.163 + i * 0.006, -0.018 + i * 0.044), 0.003)
		_box(_head, "leather", Vector3(0.023, 0.024, 0.066), Vector3(s * 0.15, 0.111, 0.041), 0.005)
		_box(_head, "metal", Vector3(0.009, 0.011, 0.014), Vector3(s * 0.166, 0.111, 0.025), 0.003)
	# A forehead lamp and two crossing black bars form an industrial X badge.
	_box(_head, "leather", Vector3(0.067, 0.041, 0.027), Vector3(0, 0.151, -0.133), 0.007)
	_box(_head, "metal", Vector3(0.047, 0.028, 0.011), Vector3(0, 0.151, -0.151), 0.005)
	_box(_head, "lamp", Vector3(0.035, 0.017, 0.008), Vector3(0, 0.151, -0.159), 0.005)
	_line(_head, "leather", Vector3(-0.050, 0.193, -0.109), Vector3(-0.030, 0.216, -0.091), 0.005)
	_line(_head, "leather", Vector3(-0.030, 0.193, -0.109), Vector3(-0.050, 0.216, -0.091), 0.005)


func _face_patch(key: String, contour: Array, projection: float) -> void:
	var points: Array[Vector3] = []
	for p: Vector2 in contour:
		var front := 0.010 - 0.115 * sqrt(maxf(1.0 - pow(p.x / 0.125, 2.0), 0.0))
		points.append(Vector3(p.x, p.y, front - projection))
	_poly(_head, key, points, Vector3.FORWARD)


func _scalp_point(angle: float, height: float) -> Vector3:
	for i in HEAD_PROFILE.size() - 1:
		var a: Vector4 = HEAD_PROFILE[i]
		var b: Vector4 = HEAD_PROFILE[i + 1]
		if height >= a.x and height <= b.x:
			var section := a.lerp(b, (height - a.x) / (b.x - a.x))
			return Vector3(sin(angle) * (section.y + 0.003), height,
				-cos(angle) * (section.z + 0.003) + section.w)
	return Vector3.ZERO


func _face_disc(key: String, center: Vector2, radius: Vector2, projection: float) -> void:
	var contour: Array[Vector2] = []
	for i in 12:
		var angle := TAU * float(i) / 12.0
		contour.append(center + Vector2(sin(angle) * radius.x, cos(angle) * radius.y))
	_face_patch(key, contour, projection)


func _build_arm(side: float) -> void:
	var label := "Left" if side < 0.0 else "Right"
	var upper := _joint(self, label + "UpperArm")
	var lower := _joint(self, label + "Forearm")
	var hand := _joint(self, label + "GlovedHand")
	_loft(upper, "canvas", [Vector4(0.021, 0.071, 0.075, 0),
		Vector4(-0.055, 0.094, 0.091, 0), Vector4(-0.172, 0.077, 0.072, 0),
		Vector4(-0.286, 0.062, 0.063, 0)], 10)
	_loft(upper, "panel", [Vector4(0.015, 0.0715, 0.076, 0),
		Vector4(-0.075, 0.094, 0.093, 0)], 10)
	_band(upper, "gold", -0.102, 0.038, 0.089, 0.081, 0.008)
	_band(upper, "reflector", -0.102, 0.010, 0.090, 0.083, 0.006)
	_loft(lower, "canvas", [Vector4(0.016, 0.065, 0.065, 0),
		Vector4(-0.070, 0.076, 0.073, 0.004), Vector4(-0.170, 0.061, 0.060, 0),
		Vector4(-0.254, 0.045, 0.046, 0)], 10)
	_box(lower, "fold", Vector3(0.100, 0.088, 0.030), Vector3(0, -0.027, 0.057), 0.014)
	_band(lower, "fold", -0.239, 0.029, 0.051, 0.053, 0.009)
	_band(lower, "seam", -0.237, 0.005, 0.055, 0.056, 0.005)
	_line(lower, "seam", Vector3(side * 0.064, -0.060, -0.018), Vector3(side * 0.046, -0.218, -0.016), 0.003)
	_poly(lower, "panel", [Vector3(-0.040, -0.13, -0.060), Vector3(0.040, -0.13, -0.060),
		Vector3(0.038, -0.153, -0.062), Vector3(-0.038, -0.163, -0.052)], Vector3.FORWARD)
	_build_hand(hand, side)
	_limbs.append({"kind": "arm", "side": side, "upper": upper, "lower": lower, "hand": hand})


func _build_hand(hand: Node3D, side: float) -> void:
	_loft(hand, "glove", [Vector4(0.010, 0.039, 0.033, 0),
		Vector4(-0.042, 0.050, 0.035, 0), Vector4(-0.096, 0.049, 0.029, -0.004)], 8)
	_band(hand, "leather", -0.010, 0.025, 0.043, 0.037, 0.009)
	_box(hand, "leather", Vector3(0.075, 0.040, 0.009), Vector3(0, -0.062, 0.036), 0.009)
	for i in 4:
		var x := -0.035 + float(i) * 0.023
		var length := 0.034 if i == 0 or i == 3 else 0.043
		var finger := _joint(hand, "Finger%d" % i)
		finger.position = Vector3(x, -0.09, -0.003)
		var tip := _joint(finger, "DistalJoint")
		tip.position.y = -length
		_loft(finger, "glove", [Vector4(0.008, 0.012, 0.014, 0),
			Vector4(-0.018, 0.012, 0.013, 0), Vector4(-length, 0.010, 0.011, 0)], 8)
		_loft(tip, "glove", [Vector4(0.004, 0.010, 0.011, 0),
			Vector4(-0.024, 0.009, 0.009, 0), Vector4(-0.032, 0.005, 0.005, 0)], 8)
		_box(finger, "leather", Vector3(0.018, 0.009, 0.006), Vector3(0, -0.018, 0.013), 0.003)
		_fingers.append({"base": finger, "tip": tip})
	var thumb := _joint(hand, "OpposedThumb")
	thumb.position = Vector3(side * 0.047, -0.033, -0.006)
	thumb.rotation = Vector3(-0.72, 0, -side * 0.58)
	_loft(thumb, "glove", [Vector4(0.006, 0.016, 0.018, 0),
		Vector4(-0.031, 0.016, 0.016, -0.004), Vector4(-0.060, 0.011, 0.011, -0.017),
		Vector4(-0.069, 0.005, 0.005, -0.020)], 8)


func _build_leg(side: float) -> void:
	var label := "Left" if side < 0.0 else "Right"
	var thigh := _joint(self, label + "Thigh")
	var shin := _joint(self, label + "Shin")
	var boot := _joint(self, label + "LacedBoot")
	_loft(thigh, "pants", [Vector4(0.02, 0.09, 0.095, 0),
		Vector4(-0.09, 0.105, 0.111, 0.007), Vector4(-0.24, 0.091, 0.095, 0.012),
		Vector4(-0.38, 0.074, 0.077, 0)], 10)
	_band(thigh, "leather", -0.09, 0.045, 0.111, 0.118, 0.008)
	_band(thigh, "strap", -0.09, 0.026, 0.115, 0.121, 0.008)
	_box(thigh, "pants_light", Vector3(0.032, 0.13, 0.087), Vector3(side * 0.086, -0.18, 0.003), 0.010)
	_box(thigh, "fold", Vector3(0.032, 0.032, 0.093), Vector3(side * 0.095, -0.125, 0.003), 0.005)
	_line(thigh, "seam", Vector3(side * 0.074, -0.16, -0.062), Vector3(side * 0.056, -0.34, -0.058), 0.003)
	_loft(shin, "pants", [Vector4(0.024, 0.075, 0.078, 0),
		Vector4(-0.11, 0.077, 0.084, 0.008), Vector4(-0.23, 0.065, 0.070, 0.012),
		Vector4(-0.34, 0.046, 0.051, 0)], 10)
	_box(shin, "leather", Vector3(0.132, 0.167, 0.037), Vector3(0, -0.030, -0.073), 0.021)
	_box(shin, "rubber", Vector3(0.111, 0.142, 0.039), Vector3(0, -0.025, -0.096), 0.024)
	for y in [-0.065, -0.024, 0.017]:
		_box(shin, "pants_light", Vector3(0.079, 0.008, 0.007), Vector3(0, y, -0.119), 0.003)
	_box(shin, "gold", Vector3(0.018, 0.025, 0.008), Vector3(side * 0.047, -0.071, -0.114), 0.003)
	_band(shin, "fold", -0.316, 0.044, 0.055, 0.056, 0.007)
	_build_boot(boot)
	_limbs.append({"kind": "leg", "side": side, "upper": thigh, "lower": shin, "boot": boot})


func _build_boot(boot: Node3D) -> void:
	# Sole and upper taper along the foot: ankle/heel, raised instep, broad
	# rounded toe, toe cap, lace rows and deep tread are separate surfaces.
	_foot(boot, "rubber", [Vector4(0.095, 0.063, -0.097, -0.057),
		Vector4(0.030, 0.070, -0.097, -0.056), Vector4(-0.130, 0.080, -0.097, -0.056),
		Vector4(-0.215, 0.072, -0.094, -0.060)], 0.013)
	_foot(boot, "leather", [Vector4(0.088, 0.058, -0.057, 0.08),
		Vector4(0.015, 0.064, -0.057, 0.10), Vector4(-0.080, 0.069, -0.057, 0.067),
		Vector4(-0.160, 0.074, -0.057, 0.018), Vector4(-0.208, 0.068, -0.061, 0.009)], 0.017)
	_foot(boot, "rubber", [Vector4(-0.133, 0.077, -0.061, 0.028),
		Vector4(-0.178, 0.076, -0.061, 0.023), Vector4(-0.212, 0.068, -0.061, 0.004)], 0.017)
	_loft(boot, "leather", [Vector4(0.014, 0.058, 0.054, 0.020),
		Vector4(0.114, 0.055, 0.049, 0.020)], 10)
	_band(boot, "rubber", 0.106, 0.022, 0.058, 0.052, 0.007)
	_box(boot, "glove", Vector3(0.040, 0.095, 0.013), Vector3(0, 0.065, -0.032), 0.005)
	for i in 4:
		var z := -0.022 - i * 0.027
		var y := 0.098 - i * 0.020
		_line(boot, "stitch", Vector3(-0.025, y, z), Vector3(0.025, y - 0.008, z - 0.012), 0.004)
		_line(boot, "stitch", Vector3(0.025, y, z), Vector3(-0.025, y - 0.008, z - 0.012), 0.004)
		for s in [-1.0, 1.0]:
			_box(boot, "metal", Vector3(0.010, 0.007, 0.008), Vector3(s * 0.028, y, z), 0.002)
	for z in [-0.18, -0.13, -0.08, -0.03, 0.025, 0.072]:
		_box(boot, "rubber", Vector3(0.125, 0.013, 0.018), Vector3(0, -0.100, z), 0.004)
	_line(boot, "glove", Vector3(-0.068, -0.046, -0.13), Vector3(-0.061, -0.046, 0.061), 0.005)
	_line(boot, "glove", Vector3(0.068, -0.046, -0.13), Vector3(0.061, -0.046, 0.061), 0.005)


func _span(joint: Node3D, start: Vector3, end: Vector3, rest_length: float) -> void:
	var y := (start - end).normalized()
	var x := y.cross(Vector3.BACK)
	if x.length_squared() < 0.001:
		x = Vector3.RIGHT
	x = x.normalized()
	var z := x.cross(y).normalized()
	joint.transform = Transform3D(Basis(x, y * start.distance_to(end) / rest_length, z), start)


func _surface(parent: Node3D, key: String) -> SurfaceTool:
	if not _surfaces.has(parent):
		_surfaces[parent] = {}
	if not _surfaces[parent].has(key):
		var surface := SurfaceTool.new()
		surface.begin(Mesh.PRIMITIVE_TRIANGLES)
		surface.set_material(_materials[key])
		_surfaces[parent][key] = surface
	return _surfaces[parent][key]


func _poly(parent: Node3D, key: String, corners: Array, normal: Vector3) -> void:
	var points := corners.duplicate()
	if ((points[1] - points[0]) as Vector3).cross(points[2] - points[0]).dot(normal) < 0:
		points.reverse()
	var surface := _surface(parent, key)
	for i in range(1, points.size() - 1):
		for p: Vector3 in [points[0], points[i], points[i + 1]]:
			surface.set_normal(normal.normalized())
			surface.set_uv(Vector2(p.x, p.y))
			surface.add_vertex(p)


func _loft(parent: Node3D, key: String, rings: Array, sides: int = 12) -> void:
	for row in rings.size() - 1:
		var a: Vector4 = rings[row]
		var b: Vector4 = rings[row + 1]
		for i in sides:
			var angle := TAU * float(i) / sides
			var next := TAU * float(i + 1) / sides
			var points := [Vector3(sin(angle) * a.y, a.x, -cos(angle) * a.z + a.w),
				Vector3(sin(next) * a.y, a.x, -cos(next) * a.z + a.w),
				Vector3(sin(next) * b.y, b.x, -cos(next) * b.z + b.w),
				Vector3(sin(angle) * b.y, b.x, -cos(angle) * b.z + b.w)]
			var normal: Vector3 = (points[1] - points[0]).cross(points[2] - points[0]).normalized()
			var outward := Vector3(sin((angle + next) * 0.5), 0, -cos((angle + next) * 0.5))
			if normal.dot(outward) < 0:
				normal = -normal
			_poly(parent, key, points, normal)
	for cap in [0, rings.size() - 1]:
		var ring: Vector4 = rings[cap]
		var corners: Array[Vector3] = []
		for i in sides:
			var angle := TAU * float(i) / sides
			corners.append(Vector3(sin(angle) * ring.y, ring.x, -cos(angle) * ring.z + ring.w))
		var up: float = signf(rings[rings.size() - 1].x - rings[0].x)
		_poly(parent, key, corners, Vector3.UP * (up if cap > 0 else -up))


func _box(parent: Node3D, key: String, size: Vector3, at: Vector3,
		bevel: float = 0.005, basis_value: Basis = Basis.IDENTITY) -> void:
	var half := size * 0.5
	var inset := half - Vector3.ONE * minf(bevel, minf(half.x, minf(half.y, half.z)) * 0.7)
	var pose_value := Transform3D(basis_value, at)
	for axis in 3:
		var a := (axis + 1) % 3
		var b := (axis + 2) % 3
		for s in [-1.0, 1.0]:
			var normal := Vector3.ZERO
			normal[axis] = s
			var points: Array[Vector3] = []
			for pair in [Vector2(-1, -1), Vector2(1, -1), Vector2(1, 1), Vector2(-1, 1)]:
				var v := Vector3.ZERO
				v[axis] = half[axis] * s
				v[a] = inset[a] * pair.x
				v[b] = inset[b] * pair.y
				points.append(pose_value * v)
			_poly(parent, key, points, basis_value * normal)
	for axis in 3:
		var a := (axis + 1) % 3
		var b := (axis + 2) % 3
		for sa in [-1.0, 1.0]:
			for sb in [-1.0, 1.0]:
				var points: Array[Vector3] = []
				for pair in [Vector2(-1, 0), Vector2(1, 0), Vector2(1, 1), Vector2(-1, 1)]:
					var v := Vector3.ZERO
					v[axis] = inset[axis] * pair.x
					v[a] = (half[a] if pair.y == 0 else inset[a]) * sa
					v[b] = (inset[b] if pair.y == 0 else half[b]) * sb
					points.append(pose_value * v)
				var normal := Vector3.ZERO
				normal[a] = sa
				normal[b] = sb
				_poly(parent, key, points, basis_value * normal.normalized())
	for sx in [-1.0, 1.0]:
		for sy in [-1.0, 1.0]:
			for sz in [-1.0, 1.0]:
				_poly(parent, key, [pose_value * Vector3(half.x * sx, inset.y * sy, inset.z * sz),
					pose_value * Vector3(inset.x * sx, half.y * sy, inset.z * sz),
					pose_value * Vector3(inset.x * sx, inset.y * sy, half.z * sz)],
					basis_value * Vector3(sx, sy, sz).normalized())


func _line(parent: Node3D, key: String, start: Vector3, end: Vector3, width: float) -> void:
	var y := (end - start).normalized()
	var x := y.cross(Vector3.BACK)
	if x.length_squared() < 0.001:
		x = y.cross(Vector3.RIGHT)
	x = x.normalized()
	var z := x.cross(y).normalized()
	_box(parent, key, Vector3(width, start.distance_to(end) + width * 0.5, width * 0.4),
		(start + end) * 0.5, width * 0.15, Basis(x, y, z))


func _band(parent: Node3D, key: String, y: float, height: float,
		width: float, depth: float, thickness: float) -> void:
	var count := 16
	for i in count:
		var a := TAU * float(i) / count
		var b := TAU * float(i + 1) / count
		var normal := Vector3(sin((a + b) * 0.5), 0, -cos((a + b) * 0.5))
		var outer_a := Vector3(sin(a) * width, y, -cos(a) * depth)
		var outer_b := Vector3(sin(b) * width, y, -cos(b) * depth)
		var inner_a := Vector3(sin(a) * (width - thickness), y, -cos(a) * (depth - thickness))
		var inner_b := Vector3(sin(b) * (width - thickness), y, -cos(b) * (depth - thickness))
		var top := Vector3.UP * height * 0.5
		_poly(parent, key, [outer_a - top, outer_b - top, outer_b + top, outer_a + top], normal)
		_poly(parent, key, [outer_a + top, outer_b + top, inner_b + top, inner_a + top], Vector3.UP)
		_poly(parent, key, [outer_a - top, outer_b - top, inner_b - top, inner_a - top], Vector3.DOWN)


func _loop(parent: Node3D, key: String, center: Vector3,
		radius: Vector2, thickness: float, count: int = 16) -> void:
	# Open-centred oval tube in the XY plane; use real metal/rope silhouettes.
	for i in count:
		var a := TAU * float(i) / count
		var b := TAU * float(i + 1) / count
		var p := center + Vector3(sin(a) * radius.x, cos(a) * radius.y, 0)
		var q := center + Vector3(sin(b) * radius.x, cos(b) * radius.y, 0)
		var normal_a := Vector3(sin(a), cos(a), 0)
		var normal_b := Vector3(sin(b), cos(b), 0)
		for j in 8:
			var u := TAU * float(j) / 8.0
			var v := TAU * float(j + 1) / 8.0
			var corners := [p + (normal_a * cos(u) + Vector3.BACK * sin(u)) * thickness,
				q + (normal_b * cos(u) + Vector3.BACK * sin(u)) * thickness,
				q + (normal_b * cos(v) + Vector3.BACK * sin(v)) * thickness,
				p + (normal_a * cos(v) + Vector3.BACK * sin(v)) * thickness]
			var normal := ((normal_a + normal_b).normalized() * cos((u + v) * 0.5)
				+ Vector3.BACK * sin((u + v) * 0.5)).normalized()
			_poly(parent, key, corners, normal)


func _foot(parent: Node3D, key: String, sections: Array, bevel: float) -> void:
	# z, half width, bottom y, top y. Eight corners bevel the shoe profile.
	var rings: Array = []
	for section: Vector4 in sections:
		rings.append([Vector3(-section.y + bevel, section.z, section.x),
			Vector3(section.y - bevel, section.z, section.x),
			Vector3(section.y, section.z + bevel, section.x),
			Vector3(section.y, section.w - bevel, section.x),
			Vector3(section.y - bevel, section.w, section.x),
			Vector3(-section.y + bevel, section.w, section.x),
			Vector3(-section.y, section.w - bevel, section.x),
			Vector3(-section.y, section.z + bevel, section.x)])
	for row in rings.size() - 1:
		for i in 8:
			var points := [rings[row][i], rings[row][(i + 1) % 8],
				rings[row + 1][(i + 1) % 8], rings[row + 1][i]]
			var normal: Vector3 = -(points[1] - points[0]).cross(points[2] - points[0]).normalized()
			_poly(parent, key, points, normal)
	_poly(parent, key, rings[0], Vector3.BACK)
	_poly(parent, key, rings[rings.size() - 1], Vector3.FORWARD)


func _finish_batches() -> void:
	for parent: Node3D in _surfaces:
		var mesh := ArrayMesh.new()
		for key: String in _surfaces[parent]:
			var surface: SurfaceTool = _surfaces[parent][key]
			surface.index()
			surface.commit(mesh)
		var instance := MeshInstance3D.new()
		instance.name = parent.name + "Surfaces"
		instance.mesh = mesh
		parent.add_child(instance)
	_surfaces.clear()
