extends RefCounted
## Two ground-bay bearings, observed from native state only.
## Presentation hypotheses: metres of COM travel and native 90 Hz ticks.
const ENTITIES := [2480, 2481]
const BAY := Vector3(-48.45, 4.2, -59.0)
const NEAR_DISTANCE_M := 28.0
const MIN_SPEED_MPS := 0.12
const TRAVEL_PER_CUE_M := 0.4
const MIN_CUE_TICKS := 54
const MAX_SAMPLE_TICKS := 45

var _tick := -1
var _deaths := -1
var _bodies := PackedInt32Array([-1, -1])
var _travel := PackedFloat64Array([0.0, 0.0])
var _last_cue := PackedInt64Array([0, 0])


func reset() -> void:
	_tick = -1
	_deaths = -1
	_bodies.fill(-1)
	_travel.fill(0.0)
	_last_cue.fill(0)


func sample(native: Object, player: Vector3, tick: int, deaths: int, enabled: bool) -> Array[Dictionary]:
	var cues: Array[Dictionary] = []
	if native == null or not player.is_finite() or tick < 0 or deaths < 0:
		reset()
		return cues
	if player.distance_squared_to(BAY) > NEAR_DISTANCE_M * NEAR_DISTANCE_M:
		reset()
		return cues
	var elapsed := tick - _tick
	var baseline := _tick < 0 or elapsed < 0 or elapsed > MAX_SAMPLE_TICKS or deaths != _deaths
	if baseline:
		reset()
	_tick = tick
	_deaths = deaths
	if baseline or not enabled:
		_travel.fill(0.0)
		_last_cue.fill(tick)
		return cues
	if elapsed == 0:
		return cues
	for hand_bar in ENTITIES.size():
		if _bodies[hand_bar] == -1:
			var resolved := int(native.get_kit_body_index(ENTITIES[hand_bar]))
			_bodies[hand_bar] = resolved if resolved >= 0 else -2
		var body := _bodies[hand_bar]
		if body < 0:
			continue
		var velocity: Vector3 = native.get_kit_body_linear_velocity(body)
		var pose: Transform3D = native.get_kit_body_transform(body)
		if not velocity.is_finite() or not pose.origin.is_finite():
			_travel[hand_bar] = 0.0
			_last_cue[hand_bar] = tick
			continue
		var speed := velocity.length()
		if speed < MIN_SPEED_MPS:
			_travel[hand_bar] = 0.0
			continue
		_travel[hand_bar] = minf(_travel[hand_bar] + speed * float(elapsed) / 90.0,
			2.0 * TRAVEL_PER_CUE_M)
		if _travel[hand_bar] >= TRAVEL_PER_CUE_M and tick - _last_cue[hand_bar] >= MIN_CUE_TICKS:
			_travel[hand_bar] = 0.0
			_last_cue[hand_bar] = tick
			cues.append({"at": pose.origin, "strength": clampf(speed / 2.0, 0.15, 1.0)})
	return cues
