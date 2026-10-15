extends RefCounted
## The slingshot's launch shot. The push is never slowed or taken from the
## eyes: the shot starts on the tick the pouch lets the rider go (the native's
## slingshot `flight` turning true), when the bands have done all their work.
## From there, for a moment, time runs slow and the eyes turn back to see the
## bands snap home and the yard drop away, then forward to the tower rushing
## up; by SECONDS it is all at full speed and the view is the player's own
## again.
##
## Slow time is fewer native ticks per second of the wall clock, nothing else:
## every tick is the same 90 Hz step, so the flight is the flight it would
## have been (the initial state determines the final state). Presentation only.

const SECONDS := 1.2            # wall time from the rider leaving the bands to all normal
const SLOWEST := 0.3            # time's rate at the slowest
const EASE_IN := 0.12           # to the slowest
const HOLD_UNTIL := 0.55        # slowest until here
const LOOK_TURN := 0.18         # the eyes on the bands by here
const LOOK_HOLD_UNTIL := 0.5    # and held there until here

var _clock := -1.0              # wall seconds since the rider left the bands; < 0 idle
var _was_flying := false
var _look_back := Vector3.ZERO  # where the eyes turn: the bands' frame


func active() -> bool:
	return _clock >= 0.0 and _clock < SECONDS


# Once per frame, with the wall delta and the native's slingshot state.
func update(wall_delta: float, sling: Dictionary) -> void:
	var flying := bool(sling.get("flight", false))
	if flying and not _was_flying:
		_clock = 0.0
		var left: Vector3 = sling.get("anchor_left", Vector3.ZERO)
		var right: Vector3 = sling.get("anchor_right", Vector3.ZERO)
		_look_back = 0.5 * (left + right)
	elif _clock >= 0.0:
		_clock += wall_delta
		if _clock >= SECONDS:
			_clock = -1.0
	_was_flying = flying


# The rate the native's time runs at now: 1 outside the shot.
func time_scale() -> float:
	if not active():
		return 1.0
	if _clock < EASE_IN:
		return lerpf(1.0, SLOWEST, smoothstep(0.0, EASE_IN, _clock))
	if _clock < HOLD_UNTIL:
		return SLOWEST
	return lerpf(SLOWEST, 1.0, smoothstep(HOLD_UNTIL, SECONDS, _clock))


# How far the eyes are turned to the bands, 0..1.
func look_weight() -> float:
	if not active():
		return 0.0
	if _clock < LOOK_TURN:
		return smoothstep(0.0, LOOK_TURN, _clock)
	if _clock < LOOK_HOLD_UNTIL:
		return 1.0
	return 1.0 - smoothstep(LOOK_HOLD_UNTIL, SECONDS, _clock)


# The camera's orientation for this frame: the player's own `view`, turned
# toward the bands by look_weight(). Upright: no roll is added.
func orient(view: Basis, eye: Vector3) -> Basis:
	var weight := look_weight()
	if weight <= 0.0:
		return view
	var to_bands := _look_back - eye
	if to_bands.length() < 0.5:
		return view
	var back := Basis.looking_at(to_bands.normalized(), Vector3.UP)
	return Basis(view.get_rotation_quaternion().slerp(back.get_rotation_quaternion(), weight))
