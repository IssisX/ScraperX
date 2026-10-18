extends RefCounted
## Short clothing cues from native hand work; never a stamina or force owner.
## Presentation tuning, in joules, watts and native 90 Hz ticks.
const WORK_PER_CUE_J := 60.0
const MIN_POWER_W := 80.0
const MIN_LOAD_N := 200.0
const MIN_CUE_TICKS := 50
const MAX_SAMPLE_TICKS := 45

var _tick := -1
var _deaths := -1
var _work := 0.0
var _pending_work := 0.0
var _last_cue_tick := -MIN_CUE_TICKS


func reset() -> void:
	_tick = -1
	_deaths = -1
	_work = 0.0
	_pending_work = 0.0
	_last_cue_tick = -MIN_CUE_TICKS


func sample(state: Dictionary, tick: int, traversal: int, deaths: int, enabled: bool) -> float:
	var work: Variant = state.get("hand_actuator_positive_work_j")
	var left: Variant = state.get("left_hand_force_n")
	var right: Variant = state.get("right_hand_force_n")
	var attached_left: Variant = state.get("left_hand_attached")
	var attached_right: Variant = state.get("right_hand_attached")
	if not (work is float or work is int) or not left is Vector3 or not right is Vector3 \
			or not attached_left is bool or not attached_right is bool:
		reset()
		return 0.0
	var supplied := float(work)
	var left_force: Vector3 = left
	var right_force: Vector3 = right
	if tick < 0 or deaths < 0 or not is_finite(supplied) or supplied < 0.0 \
			or not left_force.is_finite() or not right_force.is_finite():
		reset()
		return 0.0
	var elapsed := tick - _tick
	var baseline := _tick < 0 or elapsed < 0 or elapsed > MAX_SAMPLE_TICKS \
		or deaths != _deaths or supplied < _work
	var delta_work := supplied - _work
	_tick = tick
	_deaths = deaths
	_work = supplied
	if baseline or not enabled or traversal not in [1, 4] or not (attached_left or attached_right):
		_pending_work = 0.0
		_last_cue_tick = tick - MIN_CUE_TICKS
		return 0.0
	if elapsed == 0:
		return 0.0
	# Native time and support-relative actuator receipts keep rendering rate,
	# passive support motion and a steady hanging load out of the trigger.
	var power := delta_work * 90.0 / float(elapsed)
	var load := (left_force + right_force).length()
	if power < MIN_POWER_W or load < MIN_LOAD_N:
		_pending_work = 0.0
		return 0.0
	_pending_work = minf(_pending_work + delta_work, 2.0 * WORK_PER_CUE_J)
	if _pending_work < WORK_PER_CUE_J or tick - _last_cue_tick < MIN_CUE_TICKS:
		return 0.0
	_pending_work = 0.0
	_last_cue_tick = tick
	return clampf(power / 1800.0, 0.15, 1.0)
