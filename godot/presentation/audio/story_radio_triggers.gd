extends Node

# Only observes native positions, support and landing outcomes. The simulation
# owns every event. Rules are data; no transmission key has special handling.
const RULES_PATH := "res://audio/story_radio/triggers.json"

var _native: Object
var _radio: Node
var _rules: Array[Dictionary] = []
var _queued := {}
var _pending: Array[String] = []
var _attempted := {}
var _in_flight := false
var _flight_high_y := 0.0
var _last_deaths := -1
var _load_error := ""


func setup(native: Object, radio: Node) -> bool:
	_native = native
	_radio = radio
	var document: Variant = JSON.parse_string(FileAccess.get_file_as_string(RULES_PATH)) \
		if FileAccess.file_exists(RULES_PATH) else null
	if not document is Dictionary or document.get("schema_version") != 1 or not document.get("triggers") is Array:
		_load_error = "Story radio trigger rules: expected schema_version 1 and triggers array at " + RULES_PATH
		push_error(_load_error)
		return false
	for value: Variant in document["triggers"]:
		if not value is Dictionary:
			_load_error = "Story radio trigger rules: entry is not a dictionary"
			push_error(_load_error)
			return false
		var key := String(value.get("key", ""))
		var kind := String(value.get("kind", ""))
		if key.is_empty() or not kind in ["zone", "first_reach", "landing_drop"] or \
				_radio.cue_metadata(StringName(key)).is_empty() or _queued.has(key):
			_load_error = "Story radio trigger rules: invalid or duplicate cue " + key
			push_error(_load_error)
			return false
		_rules.append(value.duplicate(true))
		_queued[key] = false
	return true


func watch(delta: float) -> void:
	if _native == null or _radio == null or not _load_error.is_empty():
		return
	var at: Vector3 = _native.get_player_position()
	var grounded := bool(_native.is_player_grounded())
	var support := int(_native.get_support_entity_id())
	var deaths := int(_native.get_death_count())
	var landing_drop := 0.0
	# A landing is a transition in observed native grounding, not a timer or
	# a guessed fall from a scene height. Death and reset invalidate the flight.
	if deaths != _last_deaths:
		_in_flight = false
		_last_deaths = deaths
	elif not grounded:
		if not _in_flight:
			_flight_high_y = at.y
		_in_flight = true
		_flight_high_y = maxf(_flight_high_y, at.y)
	elif _in_flight:
		landing_drop = maxf(0.0, _flight_high_y - at.y)
		_in_flight = false

	for rule: Dictionary in _rules:
		var key := String(rule["key"])
		if not bool(_queued[key]) and _matches(rule, at, grounded, support, landing_drop):
			_queued[key] = true
			_pending.append(key)
	# A live approved voice retains the channel until it finishes; a blocked
	# unapproved cue is attempted once, then the next pending cue can proceed.
	if _pending.is_empty() or bool(_radio.readback().get("playing", false)):
		return
	var key: String = _pending.pop_front()
	var cue: Dictionary = _radio.cue_metadata(StringName(key))
	var id := 10000 + _rule_index(key)
	_radio.present({"transmission_id": id, "transmission_key": key,
		"speaker": String(cue["speaker"]), "transmission": String(cue["text"])}, delta)
	_attempted[key] = {"id": id, "status": String(_radio.readback()["status"])}


func _rule_index(key: String) -> int:
	for i in range(_rules.size()):
		if String(_rules[i]["key"]) == key:
			return i
	return -1


func _matches(rule: Dictionary, at: Vector3, grounded: bool, support: int, landing_drop: float) -> bool:
	match String(rule["kind"]):
		"zone":
			var center: Array = rule.get("center_xz", [])
			return center.size() == 2 and grounded and support == int(rule["support"]) and \
				at.y <= float(rule["max_y"]) and \
				Vector2(at.x, at.z).distance_to(Vector2(float(center[0]), float(center[1]))) <= float(rule["radius_m"])
		"first_reach":
			return grounded and support == int(rule["support"]) and at.y >= float(rule["min_y"])
		"landing_drop":
			return grounded and landing_drop >= float(rule["min_drop_m"])
	return false


func readback() -> Dictionary:
	return {"attempted": _attempted.duplicate(true), "pending": _pending.duplicate(),
		"load_error": _load_error}
