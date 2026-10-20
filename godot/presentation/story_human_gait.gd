extends RefCounted

# Visual foot placement only. Native position, support, and tick are inputs;
# this helper never moves the actor or creates a contact/controller.
const CYCLE_DISTANCE_M := .32
const STANCE_FRACTION := .60
const FOOT_LIFT_M := .035
const START_DISTANCE_M := .035
const SETTLE_SECONDS := .16
const NATIVE_HZ := 90.0

var last_error := ""
var _skeleton: Skeleton3D
var _legs: Array[Dictionary] = []
var _last_position := Vector3.ZERO
var _last_tick := -1
var _episode := -1
var _phase := .30
var _weight := 0.0
var _direction := Vector3(0, 0, 1)

func configure(skeleton: Skeleton3D) -> bool:
	last_error = ""
	_legs.clear()
	_skeleton = null
	_last_tick = -1
	_weight = 0.0
	_phase = .30
	if not is_instance_valid(skeleton):
		last_error = "Story gait requires a Skeleton3D"
		return false
	for side in ["L", "R"]:
		var bones: Array[int] = []
		var rests: Array[Transform3D] = []
		for stem in ["upperleg01.", "upperleg02.", "lowerleg01.", "lowerleg02.", "foot."]:
			var bone := skeleton.find_bone(stem + side)
			if bone < 0 or (not bones.is_empty() and skeleton.get_bone_parent(bone) != bones.back()):
				last_error = "Story gait requires connected MPFB leg bone: " + stem + side
				_legs.clear()
				return false
			var rest := skeleton.get_bone_rest(bone)
			if not rest.is_finite():
				last_error = "Story gait received a nonfinite leg rest: " + stem + side
				_legs.clear()
				return false
			bones.append(bone)
			rests.append(rest)
		var hip := skeleton.get_bone_global_rest(bones[0])
		var knee := skeleton.get_bone_global_rest(bones[2])
		var ankle := skeleton.get_bone_global_rest(bones[4])
		var upper := knee.origin - hip.origin
		var lower := ankle.origin - knee.origin
		if upper.length() < .05 or lower.length() < .05:
			last_error = "Story gait requires nonzero leg lengths: " + side
			_legs.clear()
			return false
		var axis := (ankle.origin - hip.origin).normalized()
		var bend := upper - axis * upper.dot(axis)
		if bend.length_squared() < .000001:
			bend = Vector3(0, 0, 1).slide(axis)
		_legs.append({"bones": bones, "rests": rests, "hip": hip, "knee": knee,
			"ankle": ankle, "upper": upper, "lower": lower, "bend": bend.normalized()})
	_skeleton = skeleton
	return true

func apply(world_position: Vector3, actor_basis: Basis, supported: bool, tick: int, episode: int) -> void:
	if not is_instance_valid(_skeleton):
		return
	if not world_position.is_finite() or not actor_basis.is_finite() or absf(actor_basis.determinant()) < .00001 or tick < 0:
		last_error = "Story gait requires a finite position, invertible actor basis, and native tick"
		return
	if tick == _last_tick and episode == _episode:
		return
	var elapsed_ticks := tick - _last_tick
	var displacement := world_position - _last_position
	displacement.y = 0.0
	# A rewind, new episode, missing half-second, or teleport is a new sample,
	# never hundreds of imagined footsteps through the omitted interval.
	var reset := _last_tick < 0 or episode != _episode or elapsed_ticks <= 0 or elapsed_ticks > 45
	if not reset:
		reset = displacement.length() > .5 or displacement.length() * NATIVE_HZ / float(elapsed_ticks) > 3.0
	_last_position = world_position
	_last_tick = tick
	_episode = episode
	if reset or not supported:
		_phase = .30
		_weight = 0.0
		_restore()
		return
	var distance := displacement.length()
	var elapsed := float(elapsed_ticks) / NATIVE_HZ
	if distance > .00001:
		var local_direction := actor_basis.orthonormalized().inverse() * displacement
		local_direction.y = 0.0
		if local_direction.length_squared() > .00000001:
			_direction = local_direction.normalized()
		_phase = fposmod(_phase + distance / CYCLE_DISTANCE_M, 1.0)
		_weight = minf(1.0, _weight + distance / START_DISTANCE_M)
	else:
		# Settling closes the last partial step without advancing gait phase.
		_weight = maxf(0.0, _weight - elapsed / SETTLE_SECONDS)
	_restore()
	if _weight <= 0.0:
		_phase = .30
		return
	for side in _legs.size():
		var phase := fposmod(_phase + float(side) * .5, 1.0)
		var fore_aft: float
		var lift := 0.0
		var stroke := CYCLE_DISTANCE_M * STANCE_FRACTION
		if phase < STANCE_FRACTION:
			# Derivative is -1 meter per measured meter: a stance foot stays
			# in place in the world while the native body passes over it.
			fore_aft = stroke * (.5 - phase / STANCE_FRACTION)
		else:
			var swing := (phase - STANCE_FRACTION) / (1.0 - STANCE_FRACTION)
			var smooth := swing * swing * (3.0 - 2.0 * swing)
			fore_aft = stroke * (smooth - .5)
			lift = FOOT_LIFT_M * pow(sin(PI * swing), 2.0)
		var leg: Dictionary = _legs[side]
		var target: Vector3 = leg.ankle.origin + (_direction * fore_aft + Vector3.UP * lift) * _weight
		_pose_leg(leg, target)

func _restore() -> void:
	for leg in _legs:
		for i in leg.bones.size():
			_skeleton.set_bone_pose(leg.bones[i], leg.rests[i])

func _pose_leg(leg: Dictionary, target: Vector3) -> void:
	# Solve the anatomical hip/knee/ankle, retaining both imported split
	# thigh/shin bones. Rotations use measured rest vectors, not assumed axes.
	var hip_pose := _skeleton.get_bone_global_pose(leg.bones[0])
	var upper: Vector3 = leg.upper
	var lower: Vector3 = leg.lower
	var a := upper.length()
	var b := lower.length()
	var toward := target - hip_pose.origin
	var length := clampf(toward.length(), absf(a - b) + .00001, a + b - .00001)
	var axis := toward.normalized()
	var bend: Vector3 = leg.bend
	bend = bend.slide(axis).normalized()
	var along := (a * a - b * b + length * length) / (2.0 * length)
	var height := sqrt(maxf(0.0, a * a - along * along))
	var knee := hip_pose.origin + axis * along + bend * height
	target = hip_pose.origin + axis * length
	hip_pose.basis = Basis(Quaternion(upper.normalized(), (knee - hip_pose.origin).normalized())) * leg.hip.basis
	_skeleton.set_bone_global_pose(leg.bones[0], hip_pose)
	var knee_pose := _skeleton.get_bone_global_pose(leg.bones[2])
	knee_pose.basis = Basis(Quaternion(lower.normalized(), (target - knee_pose.origin).normalized())) * leg.knee.basis
	_skeleton.set_bone_global_pose(leg.bones[2], knee_pose)
	# Keep the shoe's imported sole orientation while the leg bends above it.
	var foot_pose := _skeleton.get_bone_global_pose(leg.bones[4])
	foot_pose.basis = leg.ankle.basis
	_skeleton.set_bone_global_pose(leg.bones[4], foot_pose)
