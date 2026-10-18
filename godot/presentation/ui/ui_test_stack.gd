extends "res://presentation/ui/ui_test_service.gd"

# UI scenarios: checkpoints, the title, the teeter, the roll, the trapeze,
# saves, and the whole Stack on touch. (Part of the ui_test_driver.gd chain.)

func _checkpoint_continuation(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var initial_deaths := int(_native().get_death_count())
	var at0 := _position()
	if at0.y < 44.0:
		return _fail("checkpoint scenario did not start at Deck 4 (y %.2f)" % at0.y)
	await _walk_to(device, Vector2(10.0, -125.5), 0.1, 4.0)
	await _seconds(0.5)
	var cp_pos: Vector3 = _native().get_checkpoint_position()
	if cp_pos.y < 44.0:
		return _fail("committed checkpoint altitude too low (cp_y %.2f)" % cp_pos.y)
	await _face(Vector2(0.0, 1.0))
	_move(device, 1.0)
	var died: bool = await _wait_until(
		func() -> bool: return int(_native().get_death_count()) > initial_deaths, 7.0)
	_move(device, 0.0)
	if not died:
		return _fail("fall off deck was not fatal")
	await _seconds(0.5)
	var after_restore: Vector3 = _position()
	if after_restore.y < 44.0 or not bool(_native().is_player_grounded()):
		return _fail("checkpoint restore failed to return to deck (y %.2f, grounded %s)" % [
			after_restore.y, str(_native().is_player_grounded())])
	if absf(after_restore.y - cp_pos.y) > 0.5:
		return _fail("restored y %.2f != checkpoint y %.2f" % [after_restore.y, cp_pos.y])
	if not await _stack_s2(device):
		return false
	_detail = "restored_y=%.2f continued_deck6_y=%.2f deaths=%d" % [
		cp_pos.y, _position().y, int(_native().get_death_count())]
	return true

func _title() -> bool:
	if not await _wait_until(func() -> bool: return bool(_main._title_open), 1.0):
		return _fail("title: not shown at launch")
	var title: Control = _main._title
	if not title.visible or title.has_continue():
		return _fail("title: visible %s, CONTINUE offered %s with no save" % [str(title.visible), str(title.has_continue())])
	var tick := int(_native().get_tick_index())
	await _seconds(0.6)
	if int(_native().get_tick_index()) != tick:
		return _fail("title: the simulation ran behind the title (%d -> %d ticks)" % [tick, int(_native().get_tick_index())])
	await _pose("title")
	# The drift's other end: up the tower into the cloud layer.
	title._clock = title.DRIFT_PERIOD_SECONDS
	await _frames(2)
	await _pose("title_high")
	if title.button_text(&"new") != "NEW CLIMB":
		return _fail("title: the first entry reads '%s'" % title.button_text(&"new"))
	_click(title.button_center(&"new"))
	if not await _wait_until(func() -> bool: return not bool(_main._title_open), 0.5):
		return _fail("title: NEW CLIMB did not start the climb")
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var at := _position()
	_stick_push(Vector2(0.0, -1.0))
	await _seconds(0.8)
	_touch(0, _main._touch.stick_home(), false)
	var moved := Vector2(_position().x - at.x, _position().z - at.z).length()
	if moved < 1.0:
		return _fail("title: the climb did not take input after NEW CLIMB (moved %.2f m)" % moved)
	_detail = "frozen_ticks=%d moved_m=%.2f" % [tick, moved]
	return true

func _teeter() -> bool:
	var device := InputRouter.Device.TOUCH
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	# The deck's south edge, as the native suite measures it (run_teeter).
	var edge_z := -123.87
	if not await _go(device, Vector2(11.0, edge_z - 7.0), 0.2, 8.0):
		return _fail("teeter: the walk back stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not _stick_sprint():
		_touch(0, _main._touch.stick_home(), false)
		return _fail("teeter: the stick pushed past its ring did not latch sprint")
	if not await _wait_until(func() -> bool: return _position().z > edge_z - 2.0, 4.0):
		_touch(0, _main._touch.stick_home(), false)
		return _fail("teeter: the sprint never reached the release point (at %s)" % str(_position()))
	var speed := Vector2(_velocity().x, _velocity().z).length()
	_touch(0, _main._touch.stick_home(), false)
	var teetered: bool = await _wait_until(func() -> bool:
			return bool(_native().get_landing_state()["teetering"]), 0.5)
	if teetered:
		await _pose("teeter_edge")
	await _seconds(2.5)
	if not teetered:
		return _fail("teeter: the native never teetered (speed %.2f)" % speed)
	if _position().y > 44.0:
		return _fail("teeter: a sprint let go 2 m from the edge stopped on the deck (z %.2f)" % _position().z)
	if _felt(&"teeter") < 1:
		return _fail("teeter: the hands felt nothing at the edge (haptics %s)" % str(_main.haptic_counts))
	_detail = "sprint_mps=%.2f teeters=%d fell_to_y=%.2f" % [speed,
		int(_native().get_landing_state()["teeters"]), _position().y]
	return true

func _roll(reduced := false) -> bool:
	var device := InputRouter.Device.TOUCH
	if reduced:
		_main._settings.reduced_motion = true
		_main._apply_settings()
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not (await _go(device, Vector2(6.0, -70.0), 0.2, 20.0) and \
			await _go(device, Vector2(-13.4, -86.0), 0.2, 20.0) and \
			await _go(device, Vector2(-13.4, -103.0), 0.15, 8.0)):
		return _fail("roll: the walk onto the platform stalled at %s" % str(_position()))
	var top := _position().y
	await _face(Vector2(-1.0, 0.0))
	var before := int(_native().get_landing_state()["count"])
	_stick_push(Vector2(0.0, -1.0))
	if not await _wait_until(func() -> bool: return not bool(_ctx()["grounded"]), 3.0):
		_touch(0, _main._touch.stick_home(), false)
		return _fail("roll: never left the platform (at %s)" % str(_position()))
	_press(&"crouch", 1)
	if not await _wait_until(func() -> bool:
			return int(_native().get_landing_state()["count"]) > before, 3.0):
		_touch(0, _main._touch.stick_home(), false)
		return _fail("roll: never landed (at %s)" % str(_position()))
	var landing: Dictionary = _native().get_landing_state()
	_touch(0, _main._touch.stick_home(), false)
	if int(landing["response"]) != 3:
		return _fail("roll: the landing was response %d, not a roll (%s)" % [int(landing["response"]), str(landing)])
	var pitch_min := 0.0
	var turned := false
	var t := 0.0
	while t < 0.7:
		await get_tree().process_frame
		t += get_process_delta_time()
		pitch_min = minf(pitch_min, _main._camera.rotation.x - _main._pitch)
		if not turned and float(_native().get_landing_state()["roll_progress"]) > 0.35:
			turned = true
			await _pose("roll_mid")
	turned = pitch_min < -2.5
	if reduced:
		# Reduced motion: the roll is the native's, the view stays upright.
		turned = pitch_min > -0.3
	landing = _native().get_landing_state()
	if float(landing["roll_travel"]) < 1.5:
		return _fail("roll: travelled %.2f m" % float(landing["roll_travel"]))
	if not turned:
		return _fail("roll: the view %s (min pitch offset %.2f)" % [
			"turned with REDUCED MOTION on" if reduced else "did not turn over", pitch_min])
	if int(_native().get_death_count()) != 0:
		return _fail("roll: died")
	if _felt(&"roll") < 1:
		return _fail("roll: the hands felt no roll (haptics %s)" % str(_main.haptic_counts))
	# Up again: the button's toggle let go; swiped, the down let go on landing.
	if not _gestures or bool(_native().is_player_crouched()):
		_press(&"crouch", 1)
	await _seconds(0.5)
	_detail = "drop_m=%.2f normal_mps=%.2f forward_mps=%.2f balance_after=%.2f roll_travel_m=%.2f" % [
		top - _position().y, float(landing["normal_speed"]), float(landing["tangent_speed"]),
		1.0 - float(landing["loss"]), float(landing["roll_travel"])]
	return true

func _trapeze() -> bool:
	var device := InputRouter.Device.TOUCH
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not (await _go(device, Vector2(14.0, -28.0), 0.2, 20.0) and \
			await _go(device, Vector2(27.6, -28.0), 0.15, 12.0)):
		return _fail("trapeze: the walk up onto the launch stand stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	_stick_push(Vector2(0.0, -1.0))
	if not await _wait_until(func() -> bool: return _position().x > 30.5, 3.0):
		_touch(0, _main._touch.stick_home(), false)
		return _fail("trapeze: the run never reached the stand's edge (at %s)" % str(_position()))
	_press(&"jump", 1)
	await _frames(2)
	_touch(0, _main._touch.stick_home(), false)
	if not await _wait_until(func() -> bool: return str(_ctx()["action"]["label"]) == "GRAB THE BAR", 1.0):
		return _fail("trapeze: GRAB THE BAR was never offered in the air (at %s)" % str(_position()))
	_press(&"action", 1)
	if not await _wait_until(func() -> bool: return bool(_native().get_trapeze_state()["harnessed"]), 1.0):
		return _fail("trapeze: the hands never closed on the bar (at %s)" % str(_position()))
	await _pose("trapeze_caught")
	# Pump: Crouch pulls up as the bar passes plumb, and lets down at the ends;
	# swiped, a swipe down kept on the glass pulls, and lifting it lets down.
	var pull_index := -1
	var t := 0.0
	var released := false
	var peak := 0.0
	while t < 40.0 and not released:
		await get_tree().process_frame
		t += get_process_delta_time()
		var trap: Dictionary = _native().get_trapeze_state()
		if not bool(trap["harnessed"]):
			return _fail("trapeze: the hands came off the bar (%s)" % str(trap))
		var angle := float(trap["angle_rad"])
		var w := float(trap["angular_speed"])
		peak = maxf(peak, angle)
		if absf(angle) < 0.12 and not bool(trap["pulled"]):
			if _gestures:
				pull_index = _swipe(1.0, true)
			else:
				_press(&"crouch", 1)
			await _frames(2)
		elif absf(w) < 0.25 and bool(trap["pulled"]):
			if _gestures:
				_lift(pull_index)
			else:
				_press(&"crouch", 1)
			await _frames(2)
		if angle > 0.82 and w > 0.0:
			if peak > 0.82:
				await _pose("trapeze_high")
			if _gestures and pull_index != -1:
				_lift(pull_index)
			_press(&"action", 1)
			released = true
	if not released:
		return _fail("trapeze: the swing never grew to 0.82 rad in 40 s (peak %.2f)" % peak)
	if not await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 4.0):
		return _fail("trapeze: never came down (at %s)" % str(_position()))
	await _seconds(0.5)
	var at := _position()
	if int(_native().get_death_count()) != 0 or at.y < 5.8 or at.x < 39.5:
		return _fail("trapeze: did not land on the landing stand (at %s, deaths %d)" % [str(at),
			int(_native().get_death_count())])
	var trap_end: Dictionary = _native().get_trapeze_state()
	_detail = "landed=(%.2f,%.2f) pumps=%d arm_work_J=%.0f catch_mps=%.2f" % [at.x, at.y,
		int(trap_end["pumps"]), float(trap_end["arm_work_j"]), float(trap_end["catch_speed_mps"])]
	return true

# Continue, through main.gd's own save path (redirected to a test file): the
# save is written while playing with nothing pressed for it, loading it puts
# the player back on the saved checkpoint, a cut-short save is refused and set
# aside as .bad, and NEW CLIMB's discard removes the save.
func _save(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var path := "user://uitest_climb.save"
	for leftover in [path, path + ".tmp", path + ".bad"]:
		if FileAccess.file_exists(leftover):
			DirAccess.remove_absolute(leftover)
	_main._save_path = path
	_main._save_enabled = true
	var at0 := _position()
	if at0.y < 44.0:
		return _fail("save scenario did not start at Deck 4 (y %.2f)" % at0.y)
	await _seconds(2.5)
	if not FileAccess.file_exists(path):
		return _fail("no save written while playing")
	_main._save_climb(true)
	var saved: Vector3 = _native().get_checkpoint_position()
	var bytes := FileAccess.get_file_as_bytes(path)
	var deaths := int(_native().get_death_count())
	_main._save_enabled = false
	await _walk_to(device, Vector2(at0.x - 4.0, at0.z), 0.2, 4.0)
	_move(device, 0.0)
	if Vector2(_position().x - saved.x, _position().z - saved.z).length() < 2.0:
		return _fail("did not walk away from the saved checkpoint")
	if not _main.load_saved_climb():
		return _fail("the save was not loaded")
	var loaded := _position()
	if loaded.distance_to(saved) > 0.05 or int(_native().get_death_count()) != deaths:
		return _fail("loaded at (%.2f, %.2f, %.2f), deaths %d; saved (%.2f, %.2f, %.2f), deaths %d" % [
			loaded.x, loaded.y, loaded.z, int(_native().get_death_count()), saved.x, saved.y, saved.z, deaths])
	await _seconds(0.5)
	if not bool(_native().is_player_grounded()) or _position().y < 44.0:
		return _fail("not standing on deck 4 after the load (y %.2f)" % _position().y)
	var cut := FileAccess.open(path, FileAccess.WRITE)
	cut.store_buffer(bytes.slice(0, bytes.size() >> 1))
	cut.close()
	var before_cut := _position()
	if _main.load_saved_climb():
		return _fail("a cut-short save was accepted")
	if FileAccess.file_exists(path) or not FileAccess.file_exists(path + ".bad"):
		return _fail("the cut-short save was not set aside as .bad")
	if _position().distance_to(before_cut) > 0.05:
		return _fail("a refused save moved the player")
	_main._save_enabled = true
	_main._save_climb(true)
	if not FileAccess.file_exists(path):
		return _fail("no save written after the refusal")
	_main.discard_saved_climb()
	if FileAccess.file_exists(path):
		return _fail("NEW CLIMB's discard left the save")
	DirAccess.remove_absolute(path + ".bad")
	_detail = "bytes=%d saved_y=%.2f" % [bytes.size(), saved.y]
	return true

# The Stack from the game's own start at grade to the 220 m ring, on the
# device under test. Across the yard into S1's cage and onto the scale plate
# in its floor, nothing pressed: the rider's weight works the valve, the
# water runs into the bucket until it outweighs the cage and the rider, and
# the cage carries them 21.8 m to deck 2. Off over the gangway, and up C1 to
# deck 4. Then S2's walking-beam cage to deck 6, the C2 climb to deck 8, S3's
# counterweight carriage to deck 12, the C3 climb to deck 14 (154 m), and the
# well's stages A, B and C to the 220 m ring. Every verb is the one the HUD
# offers at that moment, pressed on the device; every leg is proven by the
# native state it changed.
func _stack(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var body := _watch_body()
	var started := int(_native().get_tick_index())
	# S1, the water-balance hoist.
	if not await _go(device, Vector2(10.0, -117.3), 0.3, 40.0):
		return _fail("the walk across the yard to S1 stalled at %s" % str(_position()))
	if not await _go(device, Vector2(10.0, -120.8), 0.1, 6.0):
		return _fail("the step into S1's cage and onto its plate stalled at %s" % str(_position()))
	var opened: bool = await _wait_until(func() -> bool:
		return bool(_native().is_stack_s1_valve_pawled()) and \
			not bool(_native().is_stack_s1_catch_latched()), 1.5)
	if not opened:
		return _fail("standing on S1's plate did not pawl its valve open (lever %.2f rad, on %d)" % [
			float(_native().get_stack_s1_valve_angle()), int(_native().get_support_entity_id())])
	var counting: bool = await _wait_until(func() -> bool:
		return _action_label() == "FILLING" and \
			String(_ctx()["action"]["detail"]).begins_with("BUCKET") and \
			float(_native().get_stack_s1_bucket_water_kg()) > 120.0, 2.0)
	if not counting:
		return _fail("on S1's plate, the HUD read '%s %s', not the bucket filling" % [
			_action_label(), String(_ctx()["action"]["detail"])])
	await _pose("stack_filling")
	var lifted: bool = await _wait_until(
		func() -> bool: return float(_native().get_stack_s1_cage_travel()) > 3.0, 8.0)
	if not lifted:
		return _fail("on its plate, S1's cage never left the yard (bucket %.0f kg)" %
			float(_native().get_stack_s1_bucket_water_kg()))
	await _pose("stack_ride")
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_native().get_stack_s1_cage_travel()) >= 21.79, 20.0)
	if not arrived:
		return _fail("S1's cage never reached deck 2 (travel %.2f m)" %
			float(_native().get_stack_s1_cage_travel()))
	await _face(Vector2(0.0, -1.0))
	if not (await _walk_to(device, Vector2(10.0, -123.2), 0.2) and \
			await _walk_to(device, Vector2(10.0, -126.5), 0.2)):
		return _fail("the walk off S1 over the gangway stalled at %s" % str(_position()))
	await _seconds(0.3)
	if int(_native().get_support_entity_id()) != TOWER_ENTITY or _position().y < 22.5:
		return _fail("off S1, not standing on deck 2 (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	var at_deck2 := _position().y
	await _pose("stack_deck2")
	# C1, the facade: out onto the landing, round the cabinet, up onto it.
	if not await _go(device, Vector2(21.2, -124.4), 0.15, 20.0) or \
			not await _go(device, Vector2(21.2, -121.3), 0.15, 6.0) or \
			not await _go(device, Vector2(20.0, -121.3), 0.08, 6.0):
		return _fail("the walk to C1's cabinet stalled at %s" % str(_position()))
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("Action read '%s' facing the cabinet, not CLIMB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return _standing_above(24.5), 2.0):
		return _fail("CLIMB did not mantle onto the cabinet (y %.2f)" % _position().y)
	# A jump from the cabinet to hang from the duct's lip, and up onto it.
	if not await _walk_to(device, Vector2(20.0, -122.10), 0.05):
		return _fail("the step to the cabinet's north edge stalled at %s" % str(_position()))
	await _seconds(0.3)
	_jump(device)
	_move(device, 0.4)
	var hung: bool = await _wait_until(
		func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_HANGING, 2.0)
	_move(device, 0.0)
	if not hung:
		return _fail("the jump from the cabinet did not hang from the duct (at %s)" % str(_position()))
	await _pose("stack_hang")
	if not await _climb_up(device):
		return _fail("CLIMB UP was not offered hanging from the duct (Action '%s', touch jump '%s')" % [
			_action_label(), _main._touch.button_label(&"jump")])
	if not await _wait_until(func() -> bool: return _standing_above(28.0), 2.5):
		return _fail("CLIMB UP did not bring the climber up onto the duct (y %.2f)" % _position().y)
	# Along the duct to the vent stack, and up it over deck 3's edge.
	if not await _go(device, Vector2(22.5, -123.05), 0.1, 6.0) or \
			not await _go(device, Vector2(24.0, -123.0), 0.06, 6.0):
		return _fail("the walk along the duct stalled at %s" % str(_position()))
	await _face(Vector2(0.0, -1.0))
	if not await _offered(&"climb", "CLIMB", "HOLD"):
		return _fail("Action read '%s %s' facing the vent stack, not CLIMB HOLD" % [
			_action_label(), String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("CLIMB did not take hold of the vent stack")
	_move(device, 1.0)
	var on_deck3: bool = await _wait_until(func() -> bool: return _standing_above(33.5), 20.0)
	_move(device, 0.0)
	if not on_deck3:
		return _fail("climbing the vent stack did not top out onto deck 3 (y %.2f)" % _position().y)
	# Along deck 3 and out along the monorail under the davit's ladder.
	if not await _go(device, Vector2(22.0, -125.2), 0.15, 6.0) or \
			not await _go(device, Vector2(12.5, -125.2), 0.08, 12.0) or \
			not await _go(device, Vector2(12.5, -119.65), 0.06, 12.0):
		return _fail("the walk out along the monorail stalled at %s" % str(_position()))
	await _seconds(0.3)
	if not bool(_native().is_player_grounded()) or _position().y < 34.0:
		return _fail("not standing at the monorail's end (y %.2f)" % _position().y)
	# Turn to the ladder and leap for it; up it onto the davit's arm.
	await _face(Vector2(0.0, -1.0))
	await _seconds(0.3)
	await _pose("stack_monorail")
	_jump(device)
	_move(device, 0.5)
	var caught: bool = await _wait_until(
		func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 2.0)
	if not caught:
		_move(device, 0.0)
		return _fail("the leap from the monorail did not catch the davit's ladder (at %s)" % str(_position()))
	_move(device, 1.0)
	var on_arm: bool = await _wait_until(func() -> bool: return _standing_above(44.5), 20.0)
	_move(device, 0.0)
	if not on_arm:
		return _fail("up the ladder did not top out onto the davit's arm (y %.2f)" % _position().y)
	await _face(Vector2(0.0, 1.0))
	if not await _walk_to(device, Vector2(12.5, -125.0), 0.1, 10.0) or \
			not await _go(device, Vector2(11.2, -125.3), 0.1, 4.0):
		return _fail("the walk back along the arm onto deck 4 stalled at %s" % str(_position()))
	await _seconds(0.5)
	if not bool(_native().is_player_grounded()) or int(_native().get_support_entity_id()) != TOWER_ENTITY or \
			_position().y < 44.5:
		return _fail("not standing on deck 4 (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	await _pose("stack_deck4")
	var at_deck4 := _position().y

	# Continue upward through the full Stack sequence to Deck 14, then the AS-006 well to Ring 220.
	if not await _stack_s2(device):
		return false
	var at_deck6 := _position().y

	if not await _stack_c2(device):
		return false
	var at_deck8 := _position().y

	if not await _stack_s3(device):
		return false
	var at_deck12 := _position().y

	if not await _stack_c3(device):
		return false
	var at_deck14 := _position().y

	if not await _well_stage_a(device):
		return false
	var at_ring176 := _position().y

	if not await _well_stage_b(device):
		return false
	var at_ring198 := _position().y

	if not await _well_stage_c(device):
		return false
	var at_ring220 := _position().y

	# Along the 220 ring to AS-007, and up its three machines to TP-340.
	if not (await _go(device, Vector2(2.5, -129.6), 0.15, 8.0) and \
			await _go(device, Vector2(4.0, -129.2), 0.15, 6.0)):
		return _fail("the walk along the 220 ring to AS-007 stalled at %s" % str(_position()))
	if not await _wet_d(device):
		return false
	var at_wet_d := _position().y
	if not await _wet_e(device):
		return false
	var at_wet_e := _position().y
	if not await _wet_f(device):
		return false
	var at_tp340 := _position().y

	# West across TP-340 to AS-008, up its three machines and the ladder to
	# the 484 ring.
	if not await _shop_g(device):
		return false
	var at_shop_g := _position().y
	if not await _shop_h(device):
		return false
	var at_shop_h := _position().y
	if not await _shop_i(device):
		return false
	if not await _shop_ladder(device):
		return false
	var at_ring484 := _position().y

	# Round the 484 ring to AS-009, up its three machines into TP-640.
	if not await _crane_j(device):
		return false
	var at_crane_j := _position().y
	if not await _crane_k(device):
		return false
	var at_crane_k := _position().y
	if not await _crane_l(device):
		return false
	var at_tp640 := _position().y

	# AS-010's service lift from TP-640 to the 662 deck.
	if not await _service_m(device):
		return false
	var at_deck662 := _position().y

	# AS-010's C4, the service gantry, to the 684 deck, stage N, the granular
	# discharge hoist, to the 706 deck, C5, the cooling plant, to the 728 deck,
	# and stage O, the gravel wheel, to the 750 deck: on touch, the device the game is played on (the owner, 2026-09-24:
	# no keyboard or gamepad needed); the pad and keyboard runs end on the 662
	# deck.
	var reached := ""
	if device == InputRouter.Device.TOUCH:
		if not await _service_c4(device):
			return false
		reached = " deck684_y=%.2f" % _position().y
		# AS-010's stage N, the granular discharge hoist, to the 706 deck.
		if not await _service_n(device):
			return false
		reached += " deck706_y=%.2f" % _position().y
		# AS-010's C5, the cooling plant, to the 728 deck.
		if not await _service_c5(device):
			return false
		reached += " deck728_y=%.2f" % _position().y
		# AS-010's stage O, the gravel wheel, to the 750 deck.
		if not await _service_o(device):
			return false
		reached += " deck750_y=%.2f" % _position().y
		# Over the 750 deck's east parapet, the crown gondola and the luffing
		# derrick to the top of the route (802.64 m): grade to the top in one run.
		if not await _high_legs(device):
			return false
		if not _standing_above(803.3):
			return _fail("stack: not standing on the derrick's upper receiver (y %.2f)" % _position().y)
		reached += " crown_y=%.2f top_y=%.2f" % [_high_crown_y, _position().y]

	# A frame here is one or two native ticks: 0.25 m is over 11 m/s sideways,
	# faster than a sprint; only a snap moves the view that far.
	var worst_step := _stop_watch(body)
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step,
			str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died %d times on the way" % int(_native().get_death_count()))
	_detail = "deck2_y=%.2f deck4_y=%.2f deck6_y=%.2f deck8_y=%.2f deck12_y=%.2f deck14_y=%.2f ring176_y=%.2f ring198_y=%.2f ring220_y=%.2f wet_d_y=%.2f wet_e_y=%.2f tp340_y=%.2f shop_g_y=%.2f shop_h_y=%.2f ring484_y=%.2f crane_j_y=%.2f crane_k_y=%.2f tp640_y=%.2f deck662_y=%.2f%s seconds=%.1f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		at_deck2, at_deck4, at_deck6, at_deck8, at_deck12, at_deck14, at_ring176, at_ring198, at_ring220,
		at_wet_d, at_wet_e, at_tp340, at_shop_g, at_shop_h, at_ring484, at_crane_j, at_crane_k, at_tp640, at_deck662,
		reached, float(int(_native().get_tick_index()) - started) / 90.0, worst_step, float(body["lift"])]
	return true
