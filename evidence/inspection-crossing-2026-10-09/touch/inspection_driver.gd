# Private diagnostic only: the normal world remains authoritative. Sole
# debug_restart_at is the real entry396 footing; every later command is touch.
var _inspection_phase := "entry396"
var _inspection_last_sample := -90
var _inspection_timber_contact := false
var _inspection_captured := {}

func _inspection_sample(event: String) -> void:
	var state: Dictionary = _native().get_landing_state()
	print("INSPECTION_TOUCH phase=%s event=%s tick=%d position=%s velocity=%s grounded=%s support=%d traversal=%d traversal_support=%d hands=%d swing=%s grip=%s gravity=%.3f deaths=%d jump_work=%.6f hand_work=%.6f firm=%s yaw=%.6f touch_move=%s action=%s" % [
		_inspection_phase, event, _native().get_tick_index(), _position(), _velocity(),
		_native().is_player_grounded(), _native().get_support_entity_id(), _native().get_traversal_state(),
		_native().get_traversal_support_entity_id(), state["traversal_hand_constraint_count"],
		state["player_swinging"], _native().is_grip_available(), state["player_gravity_factor"],
		_native().get_death_count(), state["landing_jump_work_j"], state["hand_actuator_positive_work_j"],
		state["checkpoint_footing_valid"], _main._yaw, _main._touch.move_vector, _ctx()["action"]])

func _inspection_frame() -> bool:
	await get_tree().process_frame
	var tick := int(_native().get_tick_index())
	if tick - _inspection_last_sample >= 90:
		_inspection_last_sample = tick
		_inspection_sample("tick")
	var owner := int(_native().get_support_entity_id())
	_inspection_timber_contact = _inspection_timber_contact or (owner >= 2880 and owner <= 2891 and bool(_native().is_player_grounded()))
	var state: Dictionary = _native().get_landing_state()
	if not _position().is_finite() or not _velocity().is_finite() or float(state["player_gravity_factor"]) != 1.0 or int(_native().get_death_count()) != 0 or bool(_native().is_parachute_deployed()):
		_inspection_sample("invariant_failure")
		return _fail("finite gravity-on same-life touch route lost native invariants")
	return true

func _inspection_phase_start(label: String) -> void:
	_inspection_phase = label
	_inspection_sample("begin")

# Invert the actual touch deadzone, then project desired ordinary world
# effort into the current camera axes. No native input or body writes.
func _inspection_world_stick(effort: Vector2) -> void:
	var amount := minf(1.0, effort.length())
	if amount < 0.0001:
		_move_dir(InputRouter.Device.TOUCH, Vector2.ZERO)
		return
	var yaw := float(_main._yaw)
	var forward := Vector2(-sin(yaw), -cos(yaw))
	var right := Vector2(cos(yaw), -sin(yaw))
	var direction := Vector2(effort.dot(right), effort.dot(forward)).normalized()
	_move_dir(InputRouter.Device.TOUCH, direction * (TouchControls.STICK_DEADZONE + (1.0 - TouchControls.STICK_DEADZONE) * amount))

func _inspection_steer(target: Vector2, extra: Vector2 = Vector2.ZERO) -> void:
	var at := _position()
	var velocity := _velocity()
	_inspection_world_stick((1.8 * (target - Vector2(at.x, at.z)) - 0.28 * Vector2(velocity.x, velocity.z) + extra).limit_length(1.0))

func _inspection_footing(owner: int, standing_y: float) -> bool:
	var state: Dictionary = _native().get_landing_state()
	return bool(_native().is_player_grounded()) and int(_native().get_support_entity_id()) == owner and not bool(_native().is_player_crouched()) and absf(_position().y - standing_y) < 0.15 and int(_native().get_traversal_state()) == 0 and int(state["traversal_hand_constraint_count"]) == 0

func _inspection_walk(target: Vector2, label: String, budget: int = 2700) -> bool:
	_inspection_phase_start(label)
	# Release the old screen-relative command before turning the view.
	_inspection_world_stick(Vector2.ZERO)
	var at := _position()
	var facing := target - Vector2(at.x, at.z)
	if facing.length() > 0.75 and not await _touch_face(facing.normalized()):
		return _fail("ordinary look touch did not face walk phase " + label)
	var effort := Vector2.ZERO
	var start := int(_native().get_tick_index())
	while int(_native().get_tick_index()) - start < budget:
		at = _position()
		var to := target - Vector2(at.x, at.z)
		var velocity := _velocity()
		if bool(_native().is_player_grounded()) and to.length() < 0.10 and Vector2(velocity.x, velocity.z).length() < 0.14:
			_inspection_world_stick(Vector2.ZERO)
			_inspection_sample("arrived")
			return true
		if to.length() > 0.75:
			effort = Vector2.ZERO
		else:
			effort = (effort + to * get_process_delta_time() * 0.4).limit_length(0.35)
		_inspection_steer(target, effort)
		if not await _inspection_frame():
			return false
	_inspection_world_stick(Vector2.ZERO)
	_inspection_sample("walk_timeout")
	return _fail("bounded ordinary touch walking did not reach " + label)

func _inspection_stable(owner: int, standing_y: float) -> bool:
	_inspection_world_stick(Vector2.ZERO)
	var start := int(_native().get_tick_index())
	while int(_native().get_tick_index()) - start < 90:
		if not await _inspection_frame():
			return false
		if not _inspection_footing(owner, standing_y):
			_inspection_sample("lost_receiving_contact")
			return _fail("receiving contact did not retain neutral native footing")
	if not bool(_native().get_landing_state()["checkpoint_footing_valid"]):
		return _fail("receiving contact lacks actual native resting support patch")
	_inspection_sample("stable")
	return true

func _inspection_inward_jump(target: Vector2, owner: int, standing_y: float, label: String) -> bool:
	_inspection_phase_start(label)
	if not bool(_native().is_player_grounded()):
		return _fail("inward Jump lacks actual brace contact")
	var at := _position()
	if not await _touch_face((target - Vector2(at.x, at.z)).normalized()):
		return _fail("inward Jump facing failed")
	_inspection_world_stick(Vector2.ZERO)
	var work_before := float(_native().get_landing_state()["landing_jump_work_j"])
	_tap(1, _center(&"jump"))
	var airborne := false
	var landed := false
	var start := int(_native().get_tick_index())
	while int(_native().get_tick_index()) - start < 360:
		_inspection_steer(target)
		if not await _inspection_frame():
			return false
		airborne = airborne or (not bool(_native().is_player_grounded()) and int(_native().get_support_entity_id()) == 0)
		if int(_native().get_traversal_state()) != 0:
			return _fail("inward Jump acquired assisted traversal")
		if airborne and _inspection_footing(owner, standing_y):
			landed = true
			break
	if not airborne or not landed or float(_native().get_landing_state()["landing_jump_work_j"]) <= work_before:
		_inspection_sample("jump_failed")
		return _fail("finite touch Jump did not reach actual inward receiver")
	return await _inspection_walk(target, label + "_braking") and await _inspection_stable(owner, standing_y)

func _inspection_hanger(direction: float = 1.0) -> bool:
	var runup := -16.65 if direction > 0 else -10.95
	var receiver := -10.8 if direction > 0 else -16.8
	if not await _inspection_walk(Vector2(runup, -128.2), "hanger_runup") or not _inspection_footing(1935, 407.9):
		return _fail("hanger run-up lacks actual platform contact")
	if direction > 0 and not _inspection_captured.has("inspection_left407"):
		if not await _touch_face(Vector2.RIGHT):
			return _fail("supported run-up look touch failed")
		await _inspection_capture("inspection_left407")
	_inspection_phase_start("jump_catch2935")
	if not await _touch_face(Vector2(direction, 0)):
		return _fail("hanger facing failed")
	_inspection_world_stick(Vector2(direction * 0.55, 0))
	_tap(1, _center(&"jump"))
	var caught := false
	var action_sent := false
	var start := int(_native().get_tick_index())
	while int(_native().get_tick_index()) - start < 110:
		if not await _inspection_frame():
			return false
		var state: Dictionary = _native().get_landing_state()
		if bool(state["player_swinging"]) and int(_native().get_traversal_support_entity_id()) == 2935 and int(state["traversal_hand_constraint_count"]) == 2:
			caught = true
			break
		if not action_sent and not bool(_native().is_player_grounded()) and bool(_native().is_grip_available()) and _ctx()["action"]["id"] == &"catch":
			_inspection_sample("action_catch")
			_act(InputRouter.Device.TOUCH)
			action_sent = true
	if not caught:
		_inspection_sample("catch_failed")
		return _fail("rising touch Jump/Action did not acquire two actual hanger hands")
	_inspection_sample("caught")
	print("INSPECTION_TOUCH_BUTTONS jump=%s drop=%s action=%s action_shown=%s" % [_main._touch.button_label(&"jump"), _main._touch.button_label(&"drop"), _action_label(), _main._touch.is_button_shown(&"action")])
	if _main._touch.button_label(&"jump") != "LET GO" or _main._touch.button_label(&"drop") != "LET GO" or _action_label() != "LET GO" or _main._touch.is_button_shown(&"action"):
		return _fail("actual swing did not present coherent passive-release touch labels")
	# Direct non-touch HUD regression consumes this actual caught context.
	# Only a presentation mode flag is temporarily changed; no input/state
	# or physics tick is written or advanced during the check.
	var old_touch_active: bool = _main._hud.touch_active
	_main._hud.touch_active = false
	var prompts: Array = _main._hud._build_prompts(_ctx())
	_main._hud.touch_active = old_touch_active
	if prompts.size() != 2 or prompts[0]["verb"] != &"jump" or prompts[0]["text"] != "LET GO" or prompts[0]["detail"] != "MOVE TO LEAN" or prompts[1]["verb"] != &"drop" or prompts[1]["text"] != "LET GO":
		return _fail("actual swinging context did not produce non-touch passive-release HUD prompts")
	print("INSPECTION_SWING_HUD PASS actual_caught_context=1 jump=LET_GO drop=LET_GO detail=MOVE_TO_LEAN")
	_inspection_phase_start("pump_and_release2935")
	var released := false
	var lean_tick := -1000
	var previous_vx := _velocity().x
	var previous_direction := signf(previous_vx)
	var early_turn := true
	var lean_updates := 0
	print("INSPECTION_COARSE_LEAN held_sample_ms=122 nominal_turn_lead_ms=300 alternating_jitter_ms=150 mode=visible_tangent")
	start = int(_native().get_tick_index())
	while int(_native().get_tick_index()) - start < 5400:
		var state: Dictionary = _native().get_landing_state()
		if not bool(state["player_swinging"]) or int(_native().get_traversal_support_entity_id()) != 2935 or int(state["traversal_hand_constraint_count"]) != 2:
			return _fail("short hanger lost actual physical hands")
		var now_tick := int(_native().get_tick_index())
		if now_tick - lean_tick >= 11:
			var vx := _velocity().x
			var travel := signf(vx)
			var hand: Vector3 = _native().get_traversal_left_hand()
			if absf(vx) < 0.15:
				travel = -signf(hand.x + 14.0)
			if absf(vx) >= 0.15 and travel != previous_direction:
				early_turn = not early_turn
				previous_direction = travel
			var acceleration := 0.0 if lean_tick < 0 else (vx - previous_vx) / (float(now_tick - lean_tick) / 90.0)
			var lead := 0.45 if early_turn else 0.15
			var time_to_turn := -vx / acceleration if vx * acceleration < -0.01 else 100.0
			var effort := -travel if time_to_turn >= 0.0 and time_to_turn < lead else travel
			_inspection_world_stick(Vector2(effort, 0))
			previous_vx = vx
			lean_tick = now_tick
			lean_updates += 1
		if not await _inspection_frame():
			return false
		if (_position().x + 14.0) * direction > 1.3 and _velocity().x * direction > 1.5 and _velocity().y > 0.3:
			_inspection_world_stick(Vector2.ZERO)
			_tap(1, _center(&"drop"))
			if not await _inspection_frame():
				return false
			state = _native().get_landing_state()
			if bool(state["player_swinging"]) or int(state["traversal_hand_constraint_count"]) != 0:
				return _fail("touch Drop did not detach actual hanger hands")
			released = true
			_inspection_sample("released")
			print("INSPECTION_COARSE_LEAN released=1 updates=%d elapsed_ticks=%d direction=%.0f" % [lean_updates, int(_native().get_tick_index()) - start, direction])
			break
	if not released:
		return _fail("hanger did not reach observed rising release window")
	_inspection_phase_start("407_receiver")
	var landed := false
	start = int(_native().get_tick_index())
	while int(_native().get_tick_index()) - start < 540:
		_inspection_steer(Vector2(receiver, -128.2))
		if not await _inspection_frame():
			return false
		if _inspection_footing(1935, 407.9) and (_position().x + 14.0) * direction > 2.4:
			landed = true
			break
	if not landed:
		return _fail("earned touch swing release missed actual407 receiver")
	return await _inspection_walk(Vector2(receiver, -128.2), "407_receiver_braking") and await _inspection_stable(1935, 407.9)

func _inspection_miss_recover() -> bool:
	if not await _inspection_walk(Vector2(-16.65, -128.2), "miss_runup"):
		return false
	_inspection_phase_start("deliberate_gap_walkoff")
	if not await _touch_face(Vector2.RIGHT):
		return _fail("miss facing failed")
	_inspection_world_stick(Vector2.RIGHT)
	var unsupported := false
	var landed := false
	var start := int(_native().get_tick_index())
	while int(_native().get_tick_index()) - start < 540:
		if not await _inspection_frame():
			return false
		if int(_native().get_traversal_state()) != 0:
			return _fail("miss acquired unrequested assisted traversal")
		unsupported = unsupported or not bool(_native().is_player_grounded())
		if _position().x > -15.25:
			_inspection_world_stick(Vector2.ZERO)
		if unsupported and _inspection_footing(1936, 404.4):
			landed = true
			break
	if not landed or float(_native().get_last_impact_speed_mps()) <= 4.0:
		return _fail("deliberate no-Jump miss lacked nontrivial actual recovery-tray impact")
	_inspection_sample("tray_impact")
	if not await _inspection_walk(Vector2(-12.4, -129.3), "tray_landing_recovery"):
		return false
	await _seconds(2.0)
	if not await _inspection_stable(1936, 404.4):
		return false
	if not await _touch_face(Vector2(0, 1)):
		return _fail("tray look touch failed")
	await _inspection_capture("inspection_tray")
	for entry in [[Vector2(-20, -129.65), "tray_return_toe"], [Vector2(-12.4, -129.65), "return_brace_ascent"], [Vector2(-10.8, -129.65), "return407_receiver"], [Vector2(-10.8, -128.2), "return407_braking"]]:
		if not await _inspection_walk(entry[0], entry[1]):
			return false
	if not await _inspection_stable(1935, 407.9):
		return false
	# Real reverse crossing returns to the same left run-up before forward
	# retry; it is an unverified diagnostic candidate, never a staging hook.
	return await _inspection_hanger(-1.0)

func _touch_inspection_junction() -> bool:
	if not _campaign_world_intact() or not bool(_native().debug_restart_at(Vector3(-22.5, 397.15, -142))):
		return _fail("sole actual396 entry staging failed")
	_inspection_world_stick(Vector2.ZERO)
	await _seconds(0.5)
	if not await _inspection_stable(51, 397.15):
		return false
	for entry in [[Vector2(-22.5, -128.3), "north396_band"], [Vector2(0, -127.2), "north396_toe_approach"], [Vector2(0, -125.82), "lower_brace_toe396"], [Vector2(-22.5, -125.82), "lower_brace_ascent"]]:
		if not await _inspection_walk(entry[0], entry[1], 4500):
			return false
	if not bool(_native().is_player_grounded()) or int(_native().get_support_entity_id()) != 51 or _position().y <= 407.0:
		return _fail("lower diagonal did not earn actual height")
	if not await _inspection_inward_jump(Vector2(-22.5, -127.3), 1935, 407.9, "left407_inward_jump"):
		return false

	if "--inspection-miss" in OS.get_cmdline_user_args() and not await _inspection_miss_recover():
		return false
	if not await _inspection_hanger():
		return false
	await _inspection_capture("inspection_right407")
	if not await _inspection_walk(Vector2(0, -128.2), "right407_timber_to_upper_toe"):
		return false
	if not _inspection_timber_contact:
		return _fail("connected crossing never contacted an actual native timber segment")
	if not await _inspection_walk(Vector2(0, -126.275), "upper_brace_toe407") or not await _inspection_walk(Vector2(-21.5, -126.275), "upper_brace_ascent", 4500):
		return false
	if not await _inspection_inward_jump(Vector2(-21.5, -128.9), 51, 419.15, "actual418_floor_jump"):
		return false
	if not await _inspection_walk(Vector2(-21, -142), "onward418_west_roof") or not await _inspection_stable(51, 419.15):
		return false
	_inspection_sample("final418_receipt")
	_detail = "ordinary_viewport_touch=1 sole_entry396_staging=1 lower_brace=1 hanger2935=1 native_timber=1 upper_brace=1 actual418_25_receiver=1 onward_walk=1 gravity=1 deaths=0 miss_recovery_retry=%s" % ("1" if "--inspection-miss" in OS.get_cmdline_user_args() else "not_exercised")
	return true

func _inspection_capture(label: String) -> void:
	if _inspection_captured.has(label):
		return
	_inspection_captured[label] = true
	await _pose(label)
