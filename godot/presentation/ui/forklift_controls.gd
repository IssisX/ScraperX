extends RefCounted

# Operator intent only. Native owns traction, steering geometry and hydraulic ratings.
const RELEASED_THROTTLE := 0.02
const REVERSAL_SPEED_MPS := 0.25
var active := false
var gear := 0
var pending_direction := 0
var parking := true
var horn := false
var inch := 0.0
var _last_direction := 0
var _throttle := 0.0
var _park_release_requested := false

func enter() -> void:
	leave()
	active = true

func leave() -> void:
	active = false
	gear = 0
	pending_direction = 0
	parking = true
	horn = false
	inch = 0.0
	_last_direction = 0
	_throttle = 0.0
	_park_release_requested = false

func handle_verb(verb: StringName) -> void:
	if not active:
		return
	match verb:
		&"forklift_forward", &"forklift_reverse":
			pending_direction = 1 if verb == &"forklift_forward" else -1
		&"forklift_neutral":
			gear = 0
			pending_direction = 0
		&"forklift_park":
			if parking:
				_park_release_requested = true
			else:
				parking = true
				pending_direction = 0
				_park_release_requested = false

func _axis(input: Dictionary, name: String, low: float, high: float) -> float:
	var value := float(input.get(name, 0.0))
	return clampf(value, low, high) if is_finite(value) else 0.0

# [drive, steer, lift, tilt (positive back), service brake].
func sample(input: Dictionary, actual_forward_speed_mps := 0.0) -> Array:
	if not active or input.is_empty() or not is_finite(actual_forward_speed_mps):
		horn = false
		return [0.0, 0.0, 0.0, 0.0, 1.0]
	for name in ["throttle", "brake", "steer", "lift", "tilt", "inch"]:
		if not input.has(name) or not (input[name] is float or input[name] is int) or not is_finite(float(input[name])):
			horn = false
			return [0.0, 0.0, 0.0, 0.0, 1.0]
	_throttle = _axis(input, "throttle", 0.0, 1.0)
	var released := _throttle <= RELEASED_THROTTLE
	if _park_release_requested and released:
		parking = false
		_park_release_requested = false
	if pending_direction != 0:
		var reversing := _last_direction != 0 and pending_direction != _last_direction
		if reversing:
			gear = 0
		if released and (not reversing or absf(actual_forward_speed_mps) <= REVERSAL_SPEED_MPS):
			gear = pending_direction
			_last_direction = gear
			pending_direction = 0
			parking = false
			_park_release_requested = false
	horn = bool(input.get("horn", false))
	inch = _axis(input, "inch", 0.0, 1.0)
	var demand := _throttle * lerpf(1.0, 0.25, inch)
	var brake := maxf(_axis(input, "brake", 0.0, 1.0), inch * (1.0 - _throttle) * 0.65)
	return [0.0 if parking else demand * gear, _axis(input, "steer", -1.0, 1.0),
		_axis(input, "lift", -1.0, 1.0), _axis(input, "tilt", -1.0, 1.0),
		1.0 if parking else brake]

func ui_state() -> Dictionary:
	return {"active": active, "gear": gear, "pending_direction": pending_direction,
		"parking": parking, "horn": horn, "inch": inch,
		"throttle_release_required": pending_direction != 0 and _throttle > RELEASED_THROTTLE}
