# Private diagnostic only: the normal world remains authoritative. Sole
# debug_restart_at is the real entry418 footing; every later command is touch.
var _north_phase := "entry418"
var _north_last_sample := -90

func _north_sample(event: String) -> void:
	var state: Dictionary = _native().get_landing_state()
	print("NORTH_TRANSFER_TOUCH phase=%s event=%s tick=%d position=%s velocity=%s grounded=%s support=%d traversal=%d traversal_support=%d hands=%d swing=%s grip=%s gravity=%.3f deaths=%d jump_work=%.6f hand_work=%.6f firm=%s yaw=%.6f touch_move=%s action=%s left=%s right=%s transfer=%s transfer_support=%s" % [
		_north_phase, event, _native().get_tick_index(), _position(), _velocity(),
		_native().is_player_grounded(), _native().get_support_entity_id(), _native().get_traversal_state(),
		_native().get_traversal_support_entity_id(), state["traversal_hand_constraint_count"],
		state["player_swinging"], _native().is_grip_available(), state["player_gravity_factor"],
		_native().get_death_count(), state["landing_jump_work_j"], state["hand_actuator_positive_work_j"],
		state["checkpoint_footing_valid"], _main._yaw, _main._touch.move_vector, _ctx()["action"], _native().get_traversal_left_hand(), _native().get_traversal_right_hand(), state["foot_transfer_count"], state["foot_transfer_support_entity_id"]])

func _north_frame() -> bool:
	await get_tree().process_frame
	var tick := int(_native().get_tick_index())
	if tick - _north_last_sample >= 90:
		_north_last_sample = tick
		_north_sample("tick")
	var state: Dictionary = _native().get_landing_state()
	if not _position().is_finite() or not _velocity().is_finite() or float(state["player_gravity_factor"]) != 1.0 or int(_native().get_death_count()) != 0 or bool(_native().is_parachute_deployed()):
		_north_sample("invariant_failure")
		return _fail("finite gravity-on same-life touch route lost native invariants")
	return true

func _north_phase_start(label: String) -> void:
	_north_phase = label
	_north_sample("begin")

# Invert the actual touch deadzone, then project desired ordinary world
# effort into the current camera axes. No native input or body writes.
func _north_world_stick(effort: Vector2) -> void:
	var amount := minf(1.0, effort.length())
	if amount < 0.0001:
		_move_dir(InputRouter.Device.TOUCH, Vector2.ZERO)
		return
	var yaw := float(_main._yaw)
	var forward := Vector2(-sin(yaw), -cos(yaw))
	var right := Vector2(cos(yaw), -sin(yaw))
	var direction := Vector2(effort.dot(right), effort.dot(forward)).normalized()
	_move_dir(InputRouter.Device.TOUCH, direction * (TouchControls.STICK_DEADZONE + (1.0 - TouchControls.STICK_DEADZONE) * amount))

func _north_steer(target: Vector2, extra: Vector2 = Vector2.ZERO) -> void:
	var at := _position()
	var velocity := _velocity()
	_north_world_stick((1.8 * (target - Vector2(at.x, at.z)) - 0.28 * Vector2(velocity.x, velocity.z) + extra).limit_length(1.0))

func _north_footing(owner: int, standing_y: float) -> bool:
	var state: Dictionary = _native().get_landing_state()
	return bool(_native().is_player_grounded()) and int(_native().get_support_entity_id()) == owner and not bool(_native().is_player_crouched()) and absf(_position().y - standing_y) < 0.15 and int(_native().get_traversal_state()) == 0 and int(state["traversal_hand_constraint_count"]) == 0

func _north_walk(target: Vector2, label: String, budget: int = 2700) -> bool:
	_north_phase_start(label)
	# Release the old screen-relative command before turning the view.
	_north_world_stick(Vector2.ZERO)
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
			_north_world_stick(Vector2.ZERO)
			_north_sample("arrived")
			return true
		if to.length() > 0.75:
			effort = Vector2.ZERO
		else:
			effort = (effort + to * get_process_delta_time() * 0.4).limit_length(0.35)
		_north_steer(target, effort)
		if not await _north_frame():
			return false
	_north_world_stick(Vector2.ZERO)
	_north_sample("walk_timeout")
	return _fail("bounded ordinary touch walking did not reach " + label)

func _north_stable(owner: int, standing_y: float) -> bool:
	_north_world_stick(Vector2.ZERO)
	var start := int(_native().get_tick_index())
	while int(_native().get_tick_index()) - start < 90:
		if not await _north_frame():
			return false
		if not _north_footing(owner, standing_y):
			_north_sample("lost_receiving_contact")
			return _fail("receiving contact did not retain neutral native footing")
	if not bool(_native().get_landing_state()["checkpoint_footing_valid"]):
		return _fail("receiving contact lacks actual native resting support patch")
	_north_sample("stable")
	return true

func _north_inward_jump(target: Vector2, owner: int, standing_y: float, label: String) -> bool:
	_north_phase_start(label)
	if not bool(_native().is_player_grounded()):
		return _fail("inward Jump lacks actual brace contact")
	var at := _position()
	if not await _touch_face((target - Vector2(at.x, at.z)).normalized()):
		return _fail("inward Jump facing failed")
	_north_world_stick(Vector2.ZERO)
	var work_before := float(_native().get_landing_state()["landing_jump_work_j"])
	_tap(1, _center(&"jump"))
	var airborne := false
	var landed := false
	var start := int(_native().get_tick_index())
	while int(_native().get_tick_index()) - start < 360:
		_north_steer(target)
		if not await _north_frame():
			return false
		airborne = airborne or (not bool(_native().is_player_grounded()) and int(_native().get_support_entity_id()) == 0)
		if int(_native().get_traversal_state()) != 0:
			return _fail("inward Jump acquired assisted traversal")
		if airborne and _north_footing(owner, standing_y):
			landed = true
			break
	if not airborne or not landed or float(_native().get_landing_state()["landing_jump_work_j"]) <= work_before:
		_north_sample("jump_failed")
		return _fail("finite touch Jump did not reach actual inward receiver")
	return await _north_walk(target, label + "_braking") and await _north_stable(owner, standing_y)

func _north_approach() -> bool:
	for entry in [[Vector2(-21, -128.5), "north418_band"], [Vector2(0, -128.0), "north418_toe_approach"], [Vector2(0, -126.73), "lower_brace_toe418"], [Vector2(-21.5, -126.73), "lower_brace_ascent429"]]:
		if not await _north_walk(entry[0], entry[1], 4500):
			return false
	if not bool(_native().is_player_grounded()) or int(_native().get_support_entity_id()) != 51 or _position().y <= 428.8:
		return _fail("actual lower brace did not earn429 entry height")
	return await _north_inward_jump(Vector2(-21.5, -128.3), 1938, 429.9, "entry429_inward_jump")

func _north_handrail() -> bool:
	if not await _north_walk(Vector2(-19.6, -128.2), "rail_jump_runup") or not _north_footing(1938, 429.9):
		return _fail("rail run-up lacks actual1938 resting contact")
	_north_world_stick(Vector2.ZERO)
	if not await _touch_face(Vector2(0, 1)):
		return _fail("touch look did not face actual rail")
	_north_phase_start("jump_action_catch1938")
	var landing_before := int(_native().get_landing_state()["landing_count"])
	_north_world_stick(Vector2(0.5, 0))
	_tap(1, _center(&"jump"))
	var start := int(_native().get_tick_index())
	var airborne := false
	var action := false
	var caught := false
	while int(_native().get_tick_index()) - start < 135:
		if not await _north_frame():
			return false
		airborne = airborne or not bool(_native().is_player_grounded())
		if int(_native().get_traversal_state()) == 4 and int(_native().get_traversal_support_entity_id()) == 1938 and int(_native().get_landing_state()["traversal_hand_constraint_count"]) == 2:
			caught = true
			break
		if not action and not bool(_native().is_player_grounded()) and bool(_native().is_grip_available()) and absf(_native().get_grip_point().y - 431.1) < 0.1 and absf(_native().get_grip_point().z + 127.7) < 0.1:
			_act(InputRouter.Device.TOUCH)
			action = true
			_north_sample("action_requested")
	if not airborne or not action or not caught:
		return _fail("ordinary touch Jump/Action did not catch actual1938 rail")
	_north_sample("caught")
	var first_left: Vector3 = _native().get_traversal_left_hand()
	var work_before := float(_native().get_landing_state()["hand_actuator_positive_work_j"])
	_north_phase_start("finite_rail_regrip")
	start = int(_native().get_tick_index())
	var end_reached := false
	while int(_native().get_tick_index()) - start < 1800:
		var state: Dictionary = _native().get_landing_state()
		if int(_native().get_traversal_state()) != 4 or int(_native().get_traversal_support_entity_id()) != 1938 or int(state["traversal_hand_constraint_count"]) != 2 or bool(state["player_swinging"]):
			return _fail("static rail lost actual finite native hands")
		var left: Vector3 = _native().get_traversal_left_hand()
		var right: Vector3 = _native().get_traversal_right_hand()
		if 0.5 * (left.x + right.x) > -15.95 and _position().x >= -15.85:
			end_reached = true
			break
		_north_world_stick(Vector2(0.99, 0))
		if not await _north_frame():
			return false
	if not end_reached or _native().get_traversal_left_hand().distance_to(first_left) < 1.5 or float(_native().get_landing_state()["hand_actuator_positive_work_j"]) <= work_before:
		return _fail("actual hand anchors and finite work did not traverse rail")
	_north_sample("rail_end")
	if _position().x <= -16.2 or _position().x >= -14.2 or _position().z <= -129.2 or _position().z >= -127.1:
		return _fail("actual loaded hands have not carried body above receiving rest")
	_north_phase_start("rail_drop_to_real_endrest")
	_north_world_stick(Vector2.ZERO)
	_north_sample("before_drop")
	_tap(1, _center(&"drop"))
	if not await _north_frame():
		return false
	if int(_native().get_traversal_state()) != 0 or int(_native().get_landing_state()["traversal_hand_constraint_count"]) != 0:
		return _fail("ordinary viewport Drop did not detach real hands")
	_north_sample("drop_first_native_tick")
	start = int(_native().get_tick_index())
	var landed := false
	while int(_native().get_tick_index()) - start < 360:
		_north_steer(Vector2(-15.2, -128.4))
		if not await _north_frame():
			return false
		if int(_native().get_traversal_state()) != 0 or int(_native().get_landing_state()["traversal_hand_constraint_count"]) != 0:
			return _fail("released transfer acquired assisted traversal")
		if _north_footing(1938, 429.9) and _position().x > -16.2:
			landed = true
			break
	if not landed or int(_native().get_landing_state()["landing_count"]) <= landing_before:
		return _fail("ordinary rail Drop did not earn actual end-rest foot contact")
	return await _north_walk(Vector2(-15, -128.4), "endrest_braking") and await _north_stable(1938, 429.9)

func _north_balance_pocket() -> bool:
	_north_phase_start("narrow_balance")
	var start := int(_native().get_tick_index())
	var balancing := false
	while int(_native().get_tick_index()) - start < 1200:
		_north_steer(Vector2(-10, -128.4))
		if not await _north_frame():
			return false
		balancing = balancing or (bool(_native().is_player_balancing()) and bool(_native().is_player_grounded()) and int(_native().get_support_entity_id()) == 1938)
		if bool(_native().is_player_grounded()) and Vector2(_position().x + 10, _position().z + 128.4).length() < 0.12 and Vector2(_velocity().x, _velocity().z).length() < 0.15:
			break
	if not balancing or not await _north_walk(Vector2(-9.5, -128.4), "pocket_approach"):
		return _fail("actual beam did not produce balancing contact")
	_tap(1, _center(&"crouch"))
	if not await _north_walk(Vector2(-9.5, -129.5), "sheltered_crouch_pocket") or not bool(_native().is_player_crouched()) or not bool(_native().is_player_grounded()) or int(_native().get_support_entity_id()) != 1938:
		return _fail("actual short capsule did not enter sheltered pocket")
	_tap(1, _center(&"crouch"))
	await _seconds(0.4)
	if not bool(_native().is_player_crouched()) or not bool(_native().is_player_grounded()):
		return _fail("actual pocket header did not block standing")
	_north_sample("blocked_stand")
	# STAND is still requested; native clearance keeps the short capsule until
	# the exit is clear. Another press here also requests STAND, not a toggle.
	if not await _north_walk(Vector2(-9.5, -128.4), "pocket_exit"):
		return false
	await _seconds(0.5)
	return _north_footing(1938, 429.9) or _fail("real beam clearance did not permit standing")

func _north_gap_jump() -> bool:
	if not await _north_walk(Vector2(-11, -128.4), "offset_gap_runup"):
		return false
	_north_world_stick(Vector2.ZERO)
	if not await _touch_face(Vector2(1, 0)):
		return _fail("ordinary gap run-up look failed")
	_north_phase_start("supported_run_jump")
	_north_world_stick(Vector2.RIGHT)
	var start := int(_native().get_tick_index())
	var launched := false
	while int(_native().get_tick_index()) - start < 270:
		if not await _north_frame():
			return false
		if not bool(_native().is_player_grounded()) or int(_native().get_support_entity_id()) != 1938:
			return _fail("run-up left actual beam before Jump")
		if _position().x >= -8.8:
			if _position().x >= -8.4 or _velocity().x <= 2:
				return _fail("gap takeoff lacks actual supported run-up speed")
			# No camera turn while a screen-relative run command is held.
			_tap(1, _center(&"jump"))
			launched = true
			break
	if not launched:
		return _fail("ordinary run-up did not reach gap takeoff")
	var work_before := float(_native().get_landing_state()["landing_jump_work_j"])
	_north_phase_start("offset_gap_free_flight")
	start = int(_native().get_tick_index())
	var airborne := false
	var landed := false
	while int(_native().get_tick_index()) - start < 360:
		_north_steer(Vector2(-4.4, -129.15))
		if not await _north_frame():
			return false
		if int(_native().get_traversal_state()) != 0 or int(_native().get_landing_state()["traversal_hand_constraint_count"]) != 0:
			return _fail("offset gap acquired assisted traversal")
		airborne = airborne or (not bool(_native().is_player_grounded()) and int(_native().get_support_entity_id()) == 0)
		if airborne and bool(_native().is_player_grounded()):
			if not _north_footing(1938, 429.9) or _position().x < -5.2 or _position().z < -129.65 or _position().z > -128.65:
				return _fail("first gap landing missed actual offset receiver")
			landed = true
			break
	if not airborne or not landed or float(_native().get_landing_state()["landing_jump_work_j"]) <= work_before:
		return _fail("ordinary supported Jump did not cross actual gap")
	return await _north_walk(Vector2(-4.4, -129.15), "offset_receiver_braking") and await _north_stable(1938, 429.9)

func _north_miss_retry() -> bool:
	if not await _north_walk(Vector2(-12, -128.4), "earned_beam_miss_start") or not _north_footing(1938, 429.9):
		return _fail("miss lacks earned beam contact")
	_north_world_stick(Vector2.ZERO)
	if not await _touch_face(Vector2(0, -1)):
		return false
	_north_phase_start("deliberate_beam_walkoff")
	var count_before := int(_native().get_landing_state()["landing_count"])
	_north_world_stick(Vector2(0, -1))
	var start := int(_native().get_tick_index())
	var falling := false
	var landed := false
	while int(_native().get_tick_index()) - start < 720:
		if not await _north_frame():
			return false
		if int(_native().get_traversal_state()) != 0:
			return _fail("deliberate miss acquired assisted traversal")
		falling = falling or (not bool(_native().is_player_grounded()) and int(_native().get_support_entity_id()) == 0)
		if _position().z < -129.1:
			_north_world_stick(Vector2.ZERO)
		if falling and bool(_native().is_player_grounded()) and int(_native().get_support_entity_id()) == 51 and absf(_position().y - 419.15) < 0.25:
			landed = true
			break
	if not falling or not landed or int(_native().get_landing_state()["landing_count"]) <= count_before:
		return _fail("real beam miss did not recover onto actual418floor")
	_north_sample("actual418_recovery_impact")
	if not await _north_walk(Vector2(-12, -129.3), "recovery_braking"):
		return false
	await _seconds(2)
	if not await _north_stable(51, 419.15) or not await _north_walk(Vector2(-21, -128.5), "recovery_north418_reset_approach"):
		return false
	return await _north_approach() and await _north_handrail()

func _touch_north_transfer() -> bool:
	if not _campaign_world_intact() or not bool(_native().debug_restart_at(Vector3(-21, 419.15, -142))):
		return _fail("sole actual418roof staging failed")
	_north_world_stick(Vector2.ZERO)
	await _seconds(0.5)
	if not await _north_stable(51, 419.15) or not await _north_approach():
		return false
	if "--north-approach" in OS.get_cmdline_user_args():
		_detail = "ordinary_viewport_touch=1 sole_entry418_staging=1 actual429rest=1 full_route=0"
		return true
	if not await _north_handrail():
		return false
	if "--north-miss" in OS.get_cmdline_user_args() and not await _north_miss_retry():
		return false
	if not await _north_balance_pocket() or not await _north_gap_jump():
		return false
	for entry in [[Vector2(0, -129.15), "429_onward_to_upper_toe"], [Vector2(0, -127.185), "upper_brace_toe429"], [Vector2(-20, -127.185), "upper_brace_ascent440"]]:
		if not await _north_walk(entry[0], entry[1], 4500):
			return false
	if not await _north_inward_jump(Vector2(-20, -129.0), 51, 441.15, "actual440_floor_jump") or not await _north_walk(Vector2(-20, -142), "onward440_west_roof") or not await _north_stable(51, 441.15):
		return false
	_north_sample("final440_receipt")
	_detail = "ordinary_viewport_touch=1 sole_entry418_staging=1 actual_handrail=1 real_hand_to_foot=1 balance=1 pocket=1 offset_gap=1 actual440_25_receiver=1 onward_walk=1 gravity=1 deaths=0 miss_retry=%s" % ("1" if "--north-miss" in OS.get_cmdline_user_args() else "not_exercised")
	return true
