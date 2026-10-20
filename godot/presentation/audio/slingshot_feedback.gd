extends RefCounted
## Only observes native travel and launch receipts; never advances the mechanism.
## Native ratchet travel is continuous. 0.12 m is an audible spacing hypothesis,
## not simulated tooth geometry. Crossed teeth coalesce to one click per sample.
const CLICK_TRAVEL_M := 0.12
const CREAK_TRAVEL_M := 0.7

var _seen := false
var _launches := 0
var _ratchet := 0.0
var _draw := 0.0
var _creak_draw := 0.0
var _tick := -1


func reset() -> void:
	_seen = false


func sample(state: Dictionary, enabled: bool) -> Array[StringName]:
	var cues: Array[StringName] = []
	for key in ["draw_m", "max_draw_m", "ratchet_draw_m"]:
		if not (state.get(key) is float or state.get(key) is int) or not is_finite(float(state[key])):
			reset()
			return cues
	if not state.get("launch_count") is int or int(state.launch_count) < 0:
		reset()
		return cues
	if not state.get("seated") is bool or not state.get("released") is bool or not state.get("tick_index") is int or int(state.tick_index) < 0:
		reset()
		return cues
	var draw := float(state.draw_m)
	var ratchet := float(state.ratchet_draw_m)
	var maximum := float(state.max_draw_m)
	if draw < 0.0 or ratchet < 0.0 or maximum <= 0.0:
		reset()
		return cues
	var launches := int(state.launch_count)
	var tick := int(state.tick_index)
	var baseline := not _seen or launches < _launches or tick < _tick
	baseline = baseline or (ratchet < _ratchet and launches == _launches)
	if baseline or not enabled:
		_creak_draw = draw
	elif tick != _tick:
		if launches > _launches:
			cues.append(&"sling_release")
		elif bool(state.get("seated", false)) and not bool(state.get("released", false)):
			if ratchet > _ratchet and floori(ratchet / CLICK_TRAVEL_M) > floori(_ratchet / CLICK_TRAVEL_M):
				cues.append(&"sling_ratchet")
			if draw > _draw and draw - _creak_draw >= CREAK_TRAVEL_M:
				_creak_draw = draw
				cues.append(&"band_stretch")
	if draw < _creak_draw:
		_creak_draw = draw
	_seen = true
	_launches = launches
	_ratchet = ratchet
	_draw = draw
	_tick = tick
	return cues
