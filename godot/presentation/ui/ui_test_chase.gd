extends "res://presentation/ui/ui_test_stack.gd"
# The Matron (src/sim/matron.hpp), played and looked at on the 154 m deck.
#
# touch_matron: a sprint past her locker wakes her; the gauge climbs and the
# bolts pop while the player walks quietly on to the catwalk's east end; she
# bursts out and hunts; the player sprints over the rotten catwalk and she
# follows onto a plate that gives way under her: the one thing that kills her.
# look_matron_*: renders only (not in CI): the locker straining, her bursting
# out, her hunt, from the player's eyes.

const MATRON_STATE_STRAINING := 1
const MATRON_STATE_HUNTING := 3
const MATRON_STATE_DEAD := 6
const MATRON_BAND_X := 21.5
const MATRON_NORTH_Z := -128.5
const MATRON_MID_Z := -150.0
const MATRON_DECK_Y := 154.0


func _matron_state() -> Dictionary:
	return _native().get_matron_state()


func _crouch_on(device: int, on: bool) -> bool:
	if bool(_native().is_player_crouched()) == on:
		return true
	_tap(1, _center(&"crouch"))
	return await _wait_until(func() -> bool: return bool(_native().is_player_crouched()) == on, 1.5)


# A sprint toward the locker for a second or two (the touch stick pushed past
# its ring), then let go.
func _wake_matron(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	await _face(Vector2(10.0, -128.5) - Vector2(_position().x, _position().z))
	if not _stick_sprint():
		return _fail("the stick pushed past its ring did not latch sprint")
	var woke: bool = await _wait_until(func() -> bool: return int(_matron_state()["state"]) == MATRON_STATE_STRAINING, 6.0)
	_move(device, 0.0)
	if not woke:
		return _fail("a sprint beside her locker did not wake her (at %s)" % str(_position()))
	return true


func _matron_touch(device: int) -> bool:
	if not await _wake_matron(device):
		return false
	await _seconds(0.6)
	await _tilt(0.0)
	_main._yaw = atan2(-(11.0 - _position().x), -(-126.2 - _position().z))
	await _frames(3)
	await _pose("matron_straining")
	# Quietly on to the catwalk's east end: crouched, the locker rattling behind.
	if not await _crouch_on(device, true):
		return _fail("CROUCH did not crouch the climber")
	if not (await _go(device, Vector2(MATRON_BAND_X, MATRON_NORTH_Z), 0.5, 40.0) and \
			await _go(device, Vector2(MATRON_BAND_X, MATRON_MID_Z), 0.5, 40.0)):
		return _fail("the crouched walk to the catwalk's east end stalled at %s" % str(_position()))
	var burst: bool = await _wait_until(func() -> bool: return bool(_matron_state()["released"]), 25.0)
	if not burst:
		return _fail("she never burst out of her locker (strain %.2f)" % float(_matron_state()["strain"]))
	if not (_main._matron_view.door_tore_off > 0 and _main._matron_view.bolts_shown_popped >= 3):
		return _fail("the locker's bolts and door did not show the burst (popped %d, door off %d)" % [
			_main._matron_view.bolts_shown_popped, _main._matron_view.door_tore_off])
	var coming: bool = await _wait_until(func() -> bool:
		var m := _matron_state()
		var at: Vector3 = m["position"]
		return int(m["state"]) == MATRON_STATE_HUNTING and Vector2(at.x, at.z).distance_to(Vector2(_position().x, _position().z)) < 16.0, 40.0)
	if not coming:
		return _fail("she did not come for the player at the catwalk's east end")
	await _face(Vector2(-1.0, 0.0))
	await _pose("matron_hunting")
	# The sprint over the catwalk, west; she follows on to it.
	await _crouch_on(device, false)
	await _face(Vector2(-1.0, 0.0))
	if not _stick_sprint():
		return _fail("the stick pushed past its ring did not latch sprint")
	# Across and off the catwalk onto the west band, then still: she is on the
	# catwalk behind, or she is not.
	var across: bool = await _wait_until(func() -> bool: return _position().x < -19.5, 8.0)
	_move(device, 0.0)
	if not across:
		return _fail("the sprint over the catwalk never reached the west band (at %s)" % str(_position()))
	var dead: bool = await _wait_until(func() -> bool: return int(_matron_state()["state"]) == MATRON_STATE_DEAD, 14.0)
	var m := _matron_state()
	if not dead:
		return _fail("she did not follow over the catwalk and fall (state %d, plates down %d, she at %s, at %s)" % [
			int(m["state"]), int(m["planks_fallen"]), str(m["position"]), str(_position())])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died in the chase")
	if int(m["planks_fallen"]) < 1:
		return _fail("she died without a plate giving way (she was last at %s, at %s)" % [str(m["position"]), str(_position())])
	_detail = "plates_down=%d deaths=%d steps_heard=%d" % [int(_matron_state()["planks_fallen"]),
		int(_native().get_death_count()), int(_main._audio.matron_cues.get(&"matron_step", 0))]
	return true


# Renders only: the locker straining at three points of the climb, then her
# bursting out and the hunt, from where the player stands.
func _look_matron(stage: String) -> bool:
	var device := InputRouter.Device.TOUCH
	_main._sky_cycle.set_hour(20.2)
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if stage == "catwalk":
		await _go(device, Vector2(MATRON_BAND_X, MATRON_NORTH_Z), 0.5, 40.0)
		await _go(device, Vector2(19.0, MATRON_MID_Z), 0.5, 20.0)
		await _look_at_point(Vector3(0.0, MATRON_DECK_Y, MATRON_MID_Z))
		await _pose("matron_catwalk")
		return true
	# Wake her from a few metres off, then look at the locker.
	await _go(device, Vector2(8.0, -131.0), 0.4, 20.0)
	await _face(Vector2(10.5, -126.2) - Vector2(_position().x, _position().z))
	if not _stick_sprint():
		return _fail("no sprint latched")
	await _wait_until(func() -> bool: return int(_matron_state()["state"]) == MATRON_STATE_STRAINING, 6.0)
	_move(device, 0.0)
	await _seconds(0.3)
	await _go(device, Vector2(7.6, -130.5), 0.3, 10.0)
	for mark in [[0.25, "a"], [0.6, "b"], [0.92, "c"]]:
		await _wait_until(func() -> bool: return float(_matron_state()["strain"]) >= float(mark[0]), 20.0)
		await _look_at_point(Vector3(10.6, MATRON_DECK_Y + 1.3, -126.4))
		await _pose("matron_locker_%s" % mark[1])
	if stage == "locker":
		return true
	await _wait_until(func() -> bool: return bool(_matron_state()["released"]), 8.0)
	await _seconds(0.25)
	await _look_at_point(Vector3(11.0, MATRON_DECK_Y + 1.2, -126.8))
	await _pose("matron_burst")
	await _seconds(0.9)
	await _look_at_point(_main._matron_view.head_world_position())
	await _pose("matron_emerging")
	await _wait_until(func() -> bool: return int(_matron_state()["state"]) == MATRON_STATE_HUNTING, 6.0)
	# Back away so she is seen coming over the deck.
	await _go(device, Vector2(2.0, -146.0), 0.5, 10.0)
	await _seconds(1.2)
	await _look_at_point(_main._matron_view.head_world_position())
	await _pose("matron_hunt_a")
	await _seconds(0.5)
	await _look_at_point(_main._matron_view.head_world_position())
	await _pose("matron_hunt_b")
	return true


# Renders only: her figure in each of its poses, a few metres in front of the
# player, from made-up data (the native's frames are ignored meanwhile).
func _look_matron_gallery() -> bool:
	_main._sky_cycle.set_hour(20.2)
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var view: Node3D = _main._matron_view
	view.previewing = true
	var here := _position()
	var forward := Vector3(-sin(float(_main._yaw)), 0.0, -cos(float(_main._yaw)))
	var spot := here + forward * 3.6
	var base: Dictionary = _native().get_matron_state()
	var yaw := atan2(-forward.x, -forward.z)
	var poses := [
		["shuffle", {"state": 5, "speed": 1.0, "stride": 0.6, "turn": 0}],
		["search", {"state": 4, "speed": 0.0, "stride": 0.0, "turn": 0}],
		["hunt_a", {"state": 3, "speed": 6.8, "stride": 0.5, "turn": 0, "sees_player": true}],
		["hunt_b", {"state": 3, "speed": 6.8, "stride": 1.2, "turn": 0, "sees_player": false}],
		["turn_windup", {"state": 3, "speed": 0.0, "stride": 0.0, "turn": 1}],
		["turn_lunge_a", {"state": 3, "speed": 9.0, "stride": 0.3, "turn": 2}],
		["turn_lunge_b", {"state": 3, "speed": 9.0, "stride": 0.9, "turn": 2}],
		["emerge", {"state": 2, "speed": 0.0, "stride": 0.0, "turn": 0}],
	]
	var t := 0.0
	for entry in poses:
		var data: Dictionary = base.duplicate()
		data["alive"] = true
		data["released"] = true
		data["position"] = Vector3(spot.x, here.y - 0.9 + 1.05, spot.z)
		data["yaw"] = yaw
		for key in entry[1]:
			data[key] = entry[1][key]
		# Settle the pose a few frames (the state's own clock runs).
		for k in 6:
			t += 0.04 + 0.013 * float(k)
			view.preview(data, 0.04 + 0.013 * float(k), _main._camera.global_position, here)
			await _frames(1)
		await _look_at_point(view.head_world_position() - Vector3(0.0, 0.5, 0.0))
		await _pose("matron_pose_%s" % entry[0])
		if entry[0] in ["shuffle", "hunt_a", "turn_windup", "turn_lunge_a"]:
			# Close on the face: she stands 1.3 m from the player's eyes.
			var near := here + forward * 1.3
			data["position"] = Vector3(near.x, here.y - 0.9 + 1.05, near.z)
			for k in 4:
				view.preview(data, 0.04, _main._camera.global_position, here)
				await _frames(1)
			await _look_at_point(view.head_world_position())
			await _pose("matron_face_%s" % entry[0])
	view.previewing = false
	return true
