# Private diagnostic only: the normal world remains authoritative. Sole
# debug_restart_at is the real entry440 footing; every later command is touch.
var _pipe_phase := "entry440"
var _pipe_last_sample := -90

func _pipe_sample(event: String) -> void:
	var state: Dictionary = _native().get_landing_state()
	print("PIPE_SERVICE_TOUCH phase=%s event=%s tick=%d position=%s velocity=%s grounded=%s support=%d traversal=%d traversal_support=%d hands=%d swing=%s grip=%s gravity=%.3f deaths=%d jump_work=%.6f hand_work=%.6f firm=%s yaw=%.6f touch_move=%s action=%s left=%s right=%s transfer=%s transfer_support=%s" % [
		_pipe_phase, event, _native().get_tick_index(), _position(), _velocity(),
		_native().is_player_grounded(), _native().get_support_entity_id(), _native().get_traversal_state(),
		_native().get_traversal_support_entity_id(), state["traversal_hand_constraint_count"],
		state["player_swinging"], _native().is_grip_available(), state["player_gravity_factor"],
		_native().get_death_count(), state["landing_jump_work_j"], state["hand_actuator_positive_work_j"],
		state["checkpoint_footing_valid"], _main._yaw, _main._touch.move_vector, _ctx()["action"], _native().get_traversal_left_hand(), _native().get_traversal_right_hand(), state["foot_transfer_count"], state["foot_transfer_support_entity_id"]])

func _pipe_frame() -> bool:
	await get_tree().process_frame
	var tick := int(_native().get_tick_index())
	if tick - _pipe_last_sample >= 90:
		_pipe_last_sample = tick
		_pipe_sample("tick")
	var state: Dictionary = _native().get_landing_state()
	if not _position().is_finite() or not _velocity().is_finite() or float(state["player_gravity_factor"]) != 1.0 or int(_native().get_death_count()) != 0 or bool(_native().is_parachute_deployed()) or int(_native().get_traversal_state()) != 0 or int(state["traversal_hand_constraint_count"]) != 0 or int(_native().get_accepted_traversal_count()) != 0:
		_pipe_sample("invariant_failure")
		return _fail("finite gravity-on same-life touch route lost native invariants")
	return true

func _pipe_phase_start(label: String) -> void:
	_pipe_phase = label
	_pipe_sample("begin")

# Invert the actual touch deadzone, then project desired ordinary world
# effort into the current camera axes. No native input or body writes.
func _pipe_world_stick(effort: Vector2) -> void:
	var amount := minf(1.0, effort.length())
	if amount < 0.0001:
		_move_dir(InputRouter.Device.TOUCH, Vector2.ZERO)
		return
	var yaw := float(_main._yaw)
	var forward := Vector2(-sin(yaw), -cos(yaw))
	var right := Vector2(cos(yaw), -sin(yaw))
	var direction := Vector2(effort.dot(right), effort.dot(forward)).normalized()
	_move_dir(InputRouter.Device.TOUCH, direction * (TouchControls.STICK_DEADZONE + (1.0 - TouchControls.STICK_DEADZONE) * amount))

func _pipe_steer(target: Vector2, extra: Vector2 = Vector2.ZERO) -> void:
	var at := _position()
	var velocity := _velocity()
	_pipe_world_stick((1.8 * (target - Vector2(at.x, at.z)) - 0.28 * Vector2(velocity.x, velocity.z) + extra).limit_length(1.0))

func _pipe_footing(owner: int, standing_y: float) -> bool:
	var state: Dictionary = _native().get_landing_state()
	return bool(_native().is_player_grounded()) and int(_native().get_support_entity_id()) == owner and not bool(_native().is_player_crouched()) and absf(_position().y - standing_y) < 0.15 and int(_native().get_traversal_state()) == 0 and int(state["traversal_hand_constraint_count"]) == 0

func _pipe_walk(target: Vector2, label: String, budget: int = 2700) -> bool:
	_pipe_phase_start(label)
	# Release the old screen-relative command before turning the view.
	_pipe_world_stick(Vector2.ZERO)
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
			_pipe_world_stick(Vector2.ZERO)
			_pipe_sample("arrived")
			return true
		if to.length() > 0.75:
			effort = Vector2.ZERO
		else:
			effort = (effort + to * get_process_delta_time() * 0.4).limit_length(0.35)
		_pipe_steer(target, effort)
		if not await _pipe_frame():
			return false
	_pipe_world_stick(Vector2.ZERO)
	_pipe_sample("walk_timeout")
	return _fail("bounded ordinary touch walking did not reach " + label)

func _pipe_stable(owner: int, standing_y: float) -> bool:
	_pipe_world_stick(Vector2.ZERO)
	var start := int(_native().get_tick_index())
	while int(_native().get_tick_index()) - start < 90:
		if not await _pipe_frame():
			return false
		if not _pipe_footing(owner, standing_y):
			_pipe_sample("lost_receiving_contact")
			return _fail("receiving contact did not retain neutral native footing")
	if not bool(_native().get_landing_state()["checkpoint_footing_valid"]):
		return _fail("receiving contact lacks actual native resting support patch")
	_pipe_sample("stable")
	return true

func _pipe_inward_jump(target: Vector2, owner: int, standing_y: float, label: String) -> bool:
	_pipe_phase_start(label)
	if not bool(_native().is_player_grounded()):
		return _fail("inward Jump lacks actual brace contact")
	var at := _position()
	if not await _touch_face((target - Vector2(at.x, at.z)).normalized()):
		return _fail("inward Jump facing failed")
	_pipe_world_stick(Vector2.ZERO)
	var work_before := float(_native().get_landing_state()["landing_jump_work_j"])
	_tap(1, _center(&"jump"))
	var airborne := false
	var landed := false
	var start := int(_native().get_tick_index())
	while int(_native().get_tick_index()) - start < 360:
		_pipe_steer(target)
		if not await _pipe_frame():
			return false
		airborne = airborne or (not bool(_native().is_player_grounded()) and int(_native().get_support_entity_id()) == 0)
		if int(_native().get_traversal_state()) != 0:
			return _fail("inward Jump acquired assisted traversal")
		if airborne and _pipe_footing(owner, standing_y):
			landed = true
			break
	if not airborne or not landed or float(_native().get_landing_state()["landing_jump_work_j"]) <= work_before:
		_pipe_sample("jump_failed")
		return _fail("finite touch Jump did not reach actual inward receiver")
	return await _pipe_walk(target, label + "_braking") and await _pipe_stable(owner, standing_y)

func _pipe_approach() -> bool:
	for entry in [[Vector2(-20, -130.0), "north440_band"], [Vector2(0, -129.7), "north440_toe_approach"], [Vector2(0, -127.64), "lower_brace_toe440"], [Vector2(-20.7, -127.64), "lower_brace_ascent451"]]:
		if not await _pipe_walk(entry[0], entry[1], 4500):
			return false
	if not bool(_native().is_player_grounded()) or int(_native().get_support_entity_id()) != 51 or _position().y <= 450.8:
		return _fail("actual lower brace did not earn451 entry height")
	return await _pipe_inward_jump(Vector2(-20.7, -129.375), 1939, 451.9, "entry451_inward_jump")

func _pipe_quick() -> bool:
	if not await _pipe_walk(Vector2(-18.5, -129.375), "quick_pipe_runup") or not _pipe_footing(1939, 451.9):
		return _fail("quick run-up lacks actual service floor")
	_pipe_world_stick(Vector2.ZERO)
	if not await _touch_face(Vector2.RIGHT):
		return _fail("ordinary touch look did not face pipe")
	_pipe_phase_start("ordinary_walk_pipe_collision_probe")
	_pipe_world_stick(Vector2.RIGHT)
	var start := int(_native().get_tick_index())
	var furthest_x := _position().x
	while int(_native().get_tick_index()) - start < 180:
		if not await _pipe_frame():
			return false
		furthest_x = maxf(furthest_x, _position().x)
	_pipe_sample("walk_only_contact_result")
	print("PIPE_WALK_TOUCH furthest_x=%.6f grounded=%s support=%d accepted_traversals=%d" % [furthest_x, _native().is_player_grounded(), _native().get_support_entity_id(), _native().get_accepted_traversal_count()])
	if furthest_x <= -16.3 or furthest_x >= -14.15:
		return _fail("walk-only attempt did not meet actual pipe or crossed it without Jump")
	if not await _pipe_walk(Vector2(-18.5, -129.375), "ordinary_backtrack_to_quick_runup") or not await _pipe_stable(1939, 451.9):
		return false
	_pipe_world_stick(Vector2.ZERO)
	if not await _touch_face(Vector2.RIGHT):
		return false
	_pipe_phase_start("fresh_supported_pipe_jump")
	_pipe_world_stick(Vector2.RIGHT)
	start = int(_native().get_tick_index())
	var requested := false
	var work_before := 0.0
	var accepted_before := int(_native().get_accepted_traversal_count())
	while int(_native().get_tick_index()) - start < 180:
		if not await _pipe_frame():
			return false
		if not _pipe_footing(1939, 451.9):
			return _fail("fresh pipe Jump lacks actual supported floor")
		if _position().x >= -16.7:
			if _position().x >= -16.35 or _velocity().x <= 3:
				return _fail("quick pipe takeoff lacks earned supported run-up speed")
			work_before = float(_native().get_landing_state()["landing_jump_work_j"])
			_tap(1, _center(&"jump"))
			requested = true
			_pipe_sample("jump_requested")
			break
	if not requested:
		return _fail("ordinary quick run-up did not reach actual pipe takeoff")
	_pipe_phase_start("pipe_ballistic_crossing")
	start = int(_native().get_tick_index())
	var airborne := false
	var cleared := false
	var landed := false
	while int(_native().get_tick_index()) - start < 360:
		_pipe_steer(Vector2(-12.5, -129.375))
		if not await _pipe_frame():
			return false
		if int(_native().get_accepted_traversal_count()) != accepted_before:
			return _fail("pipe Jump accepted assisted traversal instead of ordinary flight")
		airborne = airborne or (not bool(_native().is_player_grounded()) and int(_native().get_support_entity_id()) == 0)
		if absf(_position().x + 15) < 0.12:
			if not airborne or bool(_native().is_player_grounded()) or _position().y - 0.9 <= 452.03:
				return _fail("actual airborne soles did not clear real pipe/flange crest")
			cleared = true
			_pipe_sample("actual_pipe_crest_clearance")
		if airborne and bool(_native().is_player_grounded()):
			if not _pipe_footing(1939, 451.9) or _position().x <= -14.15:
				return _fail("first receiving contact did not land beyond actual pipe")
			landed = true
			break
	if not airborne or not cleared or not landed or float(_native().get_landing_state()["landing_jump_work_j"]) <= work_before:
		return _fail("single supported touch Jump did not cross real pipe and earn landing")
	return await _pipe_walk(Vector2(-12.5, -129.375), "quick_receiving_braking") and await _pipe_stable(1939, 451.9)

func _pipe_sheltered() -> bool:
	if not await _pipe_walk(Vector2(-18, -129.375), "shelter_choice_approach") or not await _pipe_walk(Vector2(-18, -130.65), "shelter_entry_alignment"):
		return false
	_tap(1, _center(&"crouch"))
	if not await _pipe_walk(Vector2(-16.2, -130.65), "actual_crouched_shelter_entry"):
		return false
	if not bool(_native().is_player_crouched()) or not bool(_native().is_player_grounded()) or int(_native().get_support_entity_id()) != 1939:
		return _fail("short native capsule did not enter actual sheltered passage")
	_tap(1, _center(&"crouch"))
	await _seconds(0.4)
	if not bool(_native().is_player_crouched()) or not bool(_native().is_player_grounded()) or int(_native().get_support_entity_id()) != 1939:
		return _fail("actual1.45m roof did not reject standing capsule")
	_pipe_sample("real_shelter_blocked_stand")
	# STAND remains requested; real clearance keeps the short capsule until
	# it clears the open exit. Another native-state button press also says STAND.
	if not await _pipe_walk(Vector2(-8, -130.65), "long_crouched_clearance_passage"):
		return false
	if not bool(_native().is_player_crouched()) or not bool(_native().is_player_grounded()) or int(_native().get_support_entity_id()) != 1939:
		return _fail("long shelter crossing lacks actual crouched receiving contact")
	if not await _pipe_walk(Vector2(-6.5, -130.65), "shelter_real_exit"):
		return false
	await _seconds(0.5)
	if not _pipe_footing(1939, 451.9):
		return _fail("real open shelter exit did not permit native standing")
	return await _pipe_stable(1939, 451.9)

func _pipe_miss_retry() -> bool:
	for entry in [[Vector2(-6.5, -129.375), "miss_approach_past_shelter_end"], [Vector2(-6.5, -130.5), "miss_toe_alignment"], [Vector2(0.25, -130.5), "earned_toe_miss_start"]]:
		if not await _pipe_walk(entry[0], entry[1]):
			return false
	if not _pipe_footing(1939, 451.9):
		return _fail("miss lacks earned actual receiving toe contact")
	_pipe_world_stick(Vector2.ZERO)
	if not await _touch_face(Vector2.RIGHT):
		return false
	_pipe_phase_start("actual_toe_walkoff")
	var before_landings := int(_native().get_landing_state()["landing_count"])
	_pipe_world_stick(Vector2.RIGHT)
	var start := int(_native().get_tick_index())
	var falling := false
	var corrected := false
	var landed := false
	var loss_tick := -1
	while int(_native().get_tick_index()) - start < 720:
		if not await _pipe_frame():
			return false
		if not falling and not bool(_native().is_player_grounded()) and int(_native().get_support_entity_id()) == 0:
			falling = true
			loss_tick = int(_native().get_tick_index())
			_pipe_sample("actual_toe_support_loss")
		if falling and int(_native().get_tick_index()) - loss_tick >= 23:
			_pipe_steer(Vector2(1.6, -130.5))
			if not corrected:
				_pipe_sample("delayed_ordinary_air_braking")
			corrected = true
		elif _position().x > 1.5:
			_pipe_world_stick(Vector2.ZERO)
		if falling and bool(_native().is_player_grounded()) and int(_native().get_support_entity_id()) == 51 and absf(_position().y - 441.15) < 0.25:
			landed = true
			break
	if not falling or not corrected or not landed or int(_native().get_landing_state()["landing_count"]) <= before_landings or float(_native().get_last_impact_speed_mps()) <= 8:
		return _fail("actual toe miss did not land on real440roof on same life")
	_pipe_sample("actual440_recovery_impact")
	if not await _pipe_walk(Vector2(1.6, -130.0), "actual440_recovery_braking"):
		return false
	await _seconds(2)
	if not await _pipe_stable(51, 441.15):
		return false
	return await _pipe_approach() and await _pipe_sheltered()

func _pipe_onward() -> bool:
	for entry in [[Vector2(0, -129.375), "451_onward_to_upper_toe"], [Vector2(0, -128.095), "upper_brace_toe451"], [Vector2(-20.3, -128.095), "upper_brace_ascent462"]]:
		if not await _pipe_walk(entry[0], entry[1], 4500):
			return false
	if not await _pipe_inward_jump(Vector2(-20.3, -130.0), 51, 463.15, "actual462_floor_jump"):
		return false
	return await _pipe_walk(Vector2(-19.5, -142), "onward462_west_roof") and await _pipe_stable(51, 463.15)

func _touch_pipe_service() -> bool:
	if not _campaign_world_intact() or not bool(_native().debug_restart_at(Vector3(-20, 441.15, -142))):
		return _fail("sole actual440roof staging failed")
	_pipe_world_stick(Vector2.ZERO)
	await _seconds(0.5)
	if not await _pipe_stable(51, 441.15) or not await _pipe_approach():
		return false
	var mode := "sheltered" if "--pipe-sheltered" in OS.get_cmdline_user_args() else ("miss-retry" if "--pipe-miss" in OS.get_cmdline_user_args() else "quick")
	if mode == "sheltered":
		if not await _pipe_sheltered():
			return false
	elif not await _pipe_quick():
		return false
	if mode == "miss-retry" and not await _pipe_miss_retry():
		return false
	if not await _pipe_onward():
		return false
	_pipe_sample("final462_receipt")
	_detail = "mode=%s ordinary_viewport_touch=1 sole_entry440_staging=1 actual_pipe_jump=%s actual_sheltered_clearance=%s actual462_25_receiver=1 stable_onward=1 gravity=1 deaths=0 assisted_traversals=0 miss_retry=%s" % [mode, mode != "sheltered", mode != "quick", mode == "miss-retry"]
	return true
