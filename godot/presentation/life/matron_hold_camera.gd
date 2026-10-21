extends RefCounted
## The head while she has the body (matron-grab-design.md §2) and the
## fatality's shots. Read-only on the native's get_matron_hold_state(): the
## sim decides everything; this only turns its body quaternion and its beat
## edges into a camera.
##
## Held / dangled: head basis = Basis(body_quat) * yaw * pitch. No look_at.
## Slerped toward with 1 - exp(-12 dt); the roll of the result is clamped to
## 240 deg/s. The free look is limited to yaw +-70, pitch +-60 about the body.
## A spinning body (|w| > 3 rad/s) narrows the view: vignette 0.35, FOV -6.
## Reduce motion keeps the horizon level (the roll is taken out).
##
## Fatality (Matron::Fatal): each beat's edge cuts to a framing over the head
## basis; the framings ease in, never snap, and Black fades to black.

const SLERP_RATE := 12.0
const ROLL_RATE_MAX := deg_to_rad(240.0)
const LOOK_YAW_MAX := deg_to_rad(70.0)
const LOOK_PITCH_MAX := deg_to_rad(60.0)
const SPIN_RAD_S := 3.0
const SPIN_VIGNETTE := 0.35
const SPIN_FOV := -6.0
const FATAL_EASE := 3.0

# Fatal beats: Lay 0, Pin 1, Leg0 2, Leg1 3, Drag 4, Kick 5, Haul 6, Fall 7,
# Wipe 8, Black 9. Per beat: extra pitch (look down at the legs, up at the
# hoist), extra yaw, and an FOV offset.
const FATAL_FRAMES := [
	[0.0, 0.0, 0.0],
	[-0.55, 0.0, -4.0],
	[-0.75, -0.35, -6.0],
	[-0.75, 0.35, -6.0],
	[-0.2, 0.0, 0.0],
	[0.25, 0.4, -2.0],
	[0.9, 0.0, 4.0],
	[1.1, 0.0, 10.0],
	[0.3, 0.0, -8.0],
	[0.3, 0.0, -8.0],
]

var _q := Quaternion.IDENTITY
var _active := false
var _roll := 0.0
var _fov := 0.0
var _vignette := 0.0
var _black := 0.0
var _fatal_beat := -1
var _fatal_edges := PackedInt64Array()
var _fatal_pitch := 0.0
var _fatal_yaw := 0.0
var _fatal_fov := 0.0
var beat_cuts := 0  # edges seen: a hook for tests and the sound


func active() -> bool:
	return _active


func fov_offset() -> float:
	return _fov


func vignette() -> float:
	return _vignette


func black() -> float:
	return _black


static func clamp_look(look_yaw: float, look_pitch: float) -> Vector2:
	return Vector2(clampf(wrapf(look_yaw, -PI, PI), -LOOK_YAW_MAX, LOOK_YAW_MAX),
		clampf(look_pitch, -LOOK_PITCH_MAX, LOOK_PITCH_MAX))


## Returns the camera basis to use this frame (or `fallback` when she has
## nothing). `look_yaw`/`look_pitch` are the player's free look relative to
## the body.
func orient(hold: Dictionary, fallback: Basis, look_yaw: float, look_pitch: float,
		delta: float, reduced_motion: bool) -> Basis:
	var held := bool(hold.get("held", false)) and bool(hold.get("free_rotation", false))
	var claimed := bool(hold.get("claimed", false))
	if not held and not claimed:
		if _active:
			_active = false
			_fatal_beat = -1
		_fov = lerpf(_fov, 0.0, 1.0 - exp(-SLERP_RATE * delta))
		_vignette = lerpf(_vignette, 0.0, 1.0 - exp(-SLERP_RATE * delta))
		_black = 0.0
		return fallback
	if not _active:
		_active = true
		_q = fallback.get_rotation_quaternion()
		_roll = 0.0
	var body_q: Quaternion = hold.get("body_rotation", Quaternion.IDENTITY)
	if not body_q.is_normalized():
		body_q = body_q.normalized() if body_q.length() > 1.0e-6 else Quaternion.IDENTITY
	var look := clamp_look(look_yaw, look_pitch)
	var fatal_fov := 0.0
	if claimed:
		_step_fatality(hold, delta)
		look += Vector2(_fatal_yaw, _fatal_pitch)
		fatal_fov = _fatal_fov
	var target := Basis(body_q) * Basis(Vector3.UP, look.x) * Basis(Vector3.RIGHT, look.y)
	var target_q := target.get_rotation_quaternion()
	var k := 1.0 - exp(-SLERP_RATE * maxf(delta, 0.0))
	_q = _q.slerp(target_q, k).normalized()
	var basis := Basis(_q)
	# The roll (about the view axis) against world up: rate-clamped, or taken
	# out entirely with reduce motion.
	var fwd := -basis.z
	var flat_right := fwd.cross(Vector3.UP)
	var roll := 0.0
	if flat_right.length() > 1.0e-4:
		flat_right = flat_right.normalized()
		roll = flat_right.signed_angle_to(basis.x, fwd)
	var wanted_roll := 0.0 if reduced_motion else roll
	var step := clampf(angle_difference(_roll, wanted_roll), -ROLL_RATE_MAX * delta, ROLL_RATE_MAX * delta)
	_roll = wrapf(_roll + step, -PI, PI)
	if flat_right.length() > 1.0e-4:
		basis = Basis(fwd.normalized(), _roll - roll) * basis
	var spin: Vector3 = hold.get("body_angular_velocity", Vector3.ZERO)
	var spinning := spin.length() > SPIN_RAD_S
	_vignette = lerpf(_vignette, SPIN_VIGNETTE if spinning else 0.0, k)
	_fov = lerpf(_fov, (SPIN_FOV if spinning else 0.0) + fatal_fov, k)
	return basis.orthonormalized()


func _step_fatality(hold: Dictionary, delta: float) -> void:
	var beat := int(hold.get("fatality_beat", -1))
	var edges: PackedInt64Array = hold.get("fatality_beat_edges", PackedInt64Array())
	if _fatal_edges.size() == edges.size():
		for i in edges.size():
			if edges[i] != _fatal_edges[i]:
				beat_cuts += 1
	_fatal_edges = edges.duplicate()
	_fatal_beat = beat
	var frame: Array = FATAL_FRAMES[clampi(beat, 0, FATAL_FRAMES.size() - 1)] if beat >= 0 else [0.0, 0.0, 0.0]
	var e := 1.0 - exp(-FATAL_EASE * delta)
	_fatal_pitch = lerpf(_fatal_pitch, float(frame[0]), e)
	_fatal_yaw = lerpf(_fatal_yaw, float(frame[1]), e)
	_fatal_fov = lerpf(_fatal_fov, float(frame[2]), e)
	_black = move_toward(_black, 1.0 if beat == 9 else 0.0, delta / 1.5)
