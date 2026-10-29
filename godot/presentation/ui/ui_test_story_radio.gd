extends "res://presentation/ui/ui_test_identity_radio.gd"

# Real grade walk, S1 weight-operated hoist, and supported deck-2 arrival.
# The story recording gate stays closed; readback proves the watcher actually
# offered two distinct matching manifest cues to the radio player.
func _touch_story_radio_triggers() -> bool:
	var device := InputRouter.Device.TOUCH
	if not await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0):
		return _fail("player did not land at grade")
	if not await _go(device, Vector2(10.0, -117.3), 0.3, 40.0):
		return _fail("the walk to the yard approach stalled at %s" % str(_position()))
	await _frames(3)
	var watcher: Node = _main._story_triggers
	var radio: Node = _main._story_radio
	var first: Dictionary = watcher.readback()
	if not String(first["load_error"]).is_empty():
		return _fail("radio trigger rules did not load: %s" % String(first["load_error"]))
	if not first["attempted"].has("receiving_iona_freight_one"):
		return _fail("approaching the real yard did not present the Freight One cue")
	if String(first["attempted"]["receiving_iona_freight_one"]["status"]) != "voice_unapproved":
		return _fail("yard cue was not blocked by the recording approval gate")
	if not await _go(device, Vector2(10.0, -120.8), 0.1, 6.0):
		return _fail("the step onto S1's scale plate stalled")
	if not await _wait_until(func() -> bool:
		return bool(_native().is_stack_s1_valve_pawled()) and \
			not bool(_native().is_stack_s1_catch_latched()), 1.5):
		return _fail("S1's native weight-operated valve did not open")
	if not await _wait_until(func() -> bool:
		return float(_native().get_stack_s1_cage_travel()) >= 21.79, 25.0):
		return _fail("the native S1 hoist did not reach deck 2")
	await _face(Vector2(0.0, -1.0))
	if not (await _walk_to(device, Vector2(10.0, -123.2), 0.2) and \
			await _walk_to(device, Vector2(10.0, -126.5), 0.2)):
		return _fail("the walk to supported deck 2 stalled at %s" % str(_position()))
	if not await _wait_until(func() -> bool:
		return bool(_native().is_player_grounded()) and \
			int(_native().get_support_entity_id()) == TOWER_ENTITY and _position().y >= 22.5, 2.0):
		return _fail("the player never stood on real tower support at deck 2")
	await _frames(20)
	var report: Dictionary = watcher.readback()
	if not report["attempted"].has("receiving_iona_keep_the_height"):
		return _fail("first real deck-2 reach did not present the height cue")
	if report["attempted"].size() != 2:
		return _fail("radio triggers unexpectedly presented %d cues, not two" % report["attempted"].size())
	for key in ["receiving_iona_freight_one", "receiving_iona_keep_the_height"]:
		var cue: Dictionary = radio.cue_metadata(StringName(key))
		if cue.is_empty() or String(report["attempted"][key]["status"]) != "voice_unapproved":
			return _fail("manifest cue %s was absent or bypassed the voice gate" % key)
	var voice: Dictionary = radio.readback()
	if bool(voice["playing"]) or int(voice["scheduled_plays"]) != 0:
		return _fail("unapproved voice played or was scheduled in the real walk")
	_detail = "native_y=%.2f native_support=%d attempted=2 blocked=2 plays=0" % [
		_position().y, int(_native().get_support_entity_id())]
	return true
