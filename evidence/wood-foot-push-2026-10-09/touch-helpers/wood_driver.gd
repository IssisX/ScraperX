# Private acceptance driver, actual normal-world mounted timber.
# Only the initial supported left-steel stage uses a development command.
var _wood_phase := "entry_steel407"
var _wood_last_sample := -90

func _wood_sample(event: String) -> void:
	var state: Dictionary = _native().get_landing_state()
	print("WOOD_FOOT_PUSH_TOUCH phase=%s event=%s tick=%d position=%s velocity=%s grounded=%s support=%d gravity=%s deaths=%d foot_active=%s foot_support=%s starts=%s stop=%s work_j=%s stroke_m=%s elapsed_s=%s peak_n=%s impulse_ns=%s fracture=%s" % [_wood_phase, event, _native().get_tick_index(), _position(), _velocity(), _native().is_player_grounded(), _native().get_support_entity_id(), state["player_gravity_factor"], _native().get_death_count(), state["foot_push_active"], state["foot_push_support_entity_id"], state["foot_push_start_count"], state["foot_push_stop_reason"], state["foot_push_command_work_bound_j"], state["foot_push_stroke_m"], state["foot_push_elapsed_seconds"], state["foot_push_peak_load_n"], state["foot_push_last_impulse_ns"], state["plank_broken_joint_mask"]])

func _wood_frame() -> bool:
	await get_tree().process_frame
	var state: Dictionary = _native().get_landing_state()
	if not _position().is_finite() or not _velocity().is_finite() or float(state["player_gravity_factor"]) != 1 or int(_native().get_death_count()) != 0 or bool(_native().is_parachute_deployed()) or int(_native().get_traversal_state()) != 0 or int(state["traversal_hand_constraint_count"]) != 0:
		_wood_sample("invariant_failure")
		return _fail("ordinary gravity-on wood input lost actual native invariants")
	if float(state["foot_push_command_work_bound_j"]) > 1285.62501 or float(state["foot_push_stroke_m"]) > 0.300001 or float(state["foot_push_peak_load_n"]) > 3500.01 or float(state["foot_push_elapsed_seconds"]) > 0.261112:
		return _fail("actual finite foot command exceeded force/stroke/time/work limits")
	if int(_native().get_tick_index()) - _wood_last_sample >= 90:
		_wood_last_sample = int(_native().get_tick_index())
		_wood_sample("tick")
	return true

func _wood_phase_start(label: String) -> void:
	_wood_phase = label
	_wood_sample("begin")

func _wood_wait(ticks: int) -> bool:
	var start := int(_native().get_tick_index())
	while int(_native().get_tick_index()) - start < ticks:
		if not await _wood_frame(): return false
	return true

func _wood_supported() -> bool:
	var owner := int(_native().get_support_entity_id())
	return bool(_native().is_player_grounded()) and owner >= 2880 and owner <= 2891

func _wood_world_stick(effort: Vector2) -> void:
	var amount := minf(1.0, effort.length())
	if amount < 0.0001:
		_move_dir(InputRouter.Device.TOUCH, Vector2.ZERO)
		return
	var yaw := float(_main._yaw)
	var forward := Vector2(-sin(yaw), -cos(yaw))
	var right := Vector2(cos(yaw), -sin(yaw))
	var direction := Vector2(effort.dot(right), effort.dot(forward)).normalized()
	_move_dir(InputRouter.Device.TOUCH, direction * (TouchControls.STICK_DEADZONE + (1.0 - TouchControls.STICK_DEADZONE) * amount))

func _wood_steer(target: Vector2, extra: Vector2 = Vector2.ZERO) -> void:
	var at := _position()
	var velocity := _velocity()
	_wood_world_stick((1.8 * (target - Vector2(at.x, at.z)) - 0.28 * Vector2(velocity.x, velocity.z) + extra).limit_length(1.0))

func _wood_walk(target: Vector2, label: String, budget: int = 2700) -> bool:
	_wood_phase_start(label)
	# Release the old screen-relative command before turning the view.
	_wood_world_stick(Vector2.ZERO)
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
			_wood_world_stick(Vector2.ZERO)
			_wood_sample("arrived")
			return true
		if to.length() > 0.75:
			effort = Vector2.ZERO
		else:
			effort = (effort + to * get_process_delta_time() * 0.4).limit_length(0.35)
		_wood_steer(target, effort)
		if not await _wood_frame():
			return false
	_wood_world_stick(Vector2.ZERO)
	_wood_sample("walk_timeout")
	return _fail("bounded ordinary touch walking did not reach " + label)

func _wood_settle_landing() -> bool:
	_wood_phase_start("actual_wood_landing_settle")
	_wood_world_stick(Vector2.ZERO)
	var start := int(_native().get_tick_index())
	var quiet_ticks := 0
	var previous_tick := start
	while int(_native().get_tick_index()) - start < 360:
		if not await _wood_frame(): return false
		var state: Dictionary = _native().get_landing_state()
		if int(state["plank_broken_joint_mask"]) != 0:
			_wood_sample("landing_fracture")
			return _fail("actual timber fractured during neutral landing settle")
		var relative: Vector3 = _velocity() - _native().get_support_point_linear_velocity()
		var now := int(_native().get_tick_index())
		if _wood_supported() and absf(relative.y) < 0.1 and Vector2(relative.x, relative.z).length() < 0.1:
			quiet_ticks += now - previous_tick
		else:
			quiet_ticks = 0
		previous_tick = now
		if quiet_ticks >= 30:
			_wood_sample("actual_settled_wood")
			return true
	return _fail("actual intact supported wood did not settle before ordinary departure")

func _touch_wood_foot_push() -> bool:
	if not bool(_native().debug_restart_at(Vector3(-8.8, 407.9, -128.2))):
		return _fail("sole actual steel407 entry stage rejected")
	_wood_world_stick(Vector2.ZERO)
	if not await _wood_wait(360): return false
	if not bool(_native().is_player_grounded()) or int(_native().get_support_entity_id()) != 1935:
		return _fail("actual starting steel did not support native player")
	if not await _wood_walk(Vector2(-6.8, -128.2), "actual_steel_to_timber") or not await _wood_wait(360): return false
	if not _wood_supported(): return _fail("real85kg centre load lacks actual timber contact")
	_wood_phase_start("ordinary_wood_jump")
	var before: Dictionary = _native().get_landing_state()
	var before_y := float(_position().y)
	var before_landings := int(before["landing_count"])
	var before_starts := int(before["foot_push_start_count"])
	_wood_world_stick(Vector2.ZERO)
	_tap(1, _center(&"jump"))
	var active_observed := false
	var airborne := false
	var landed := false
	var peak_vy := float(_velocity().y)
	var apex := before_y
	var start := int(_native().get_tick_index())
	while int(_native().get_tick_index()) - start < 540:
		_wood_steer(Vector2(-6.8, -128.2))
		if not await _wood_frame(): return false
		var state: Dictionary = _native().get_landing_state()
		if bool(state["foot_push_active"]):
			active_observed = true
			if int(state["foot_push_support_entity_id"]) < 2880 or int(state["foot_push_support_entity_id"]) > 2891:
				return _fail("active foot constraint lacks actual timber material owner")
		if int(state["foot_push_start_count"]) > before_starts + 1 or (int(_native().get_tick_index()) - start > 9 and int(state["foot_push_start_count"]) != before_starts + 1):
			return _fail("one ordinary touch Jump did not start exactly one finite leg")
		peak_vy = maxf(peak_vy, float(_velocity().y))
		apex = maxf(apex, float(_position().y))
		airborne = airborne or (not bool(_native().is_player_grounded()) and int(_native().get_support_entity_id()) == 0)
		if airborne and _wood_supported() and int(state["landing_count"]) > before_landings:
			landed = true
			break
	_wood_sample("actual_wood_jump_landing")
	print("WOOD_FOOT_PUSH_TOUCH_TAKEOFF peak_vy_mps=%f apex_rise_m=%f baseline_vy_mps=.197759" % [peak_vy, apex-before_y])
	var final: Dictionary = _native().get_landing_state()
	if not active_observed or not airborne or not landed or peak_vy <= 0.8 or apex-before_y <= 0.05 or bool(final["foot_push_active"]) or float(final["foot_push_command_work_bound_j"]) <= 0 or int(final["plank_broken_joint_mask"]) != 0:
		return _fail("finite paid wood stroke lacks meaningful actual takeoff/wood landing")
	if not await _wood_settle_landing(): return false
	if not await _wood_walk(Vector2(-4.9, -128.2), "ordinary_onward_steel") or not await _wood_wait(180): return false
	if not bool(_native().is_player_grounded()) or int(_native().get_support_entity_id()) != 1935 or not bool(_native().get_landing_state()["checkpoint_footing_valid"]):
		return _fail("actual onward steel lacks firm native footing")
	_wood_sample("final_real_steel_exit")
	print("SCRAPERX_UITEST PASS touch_wood_foot_push ordinary_viewport_touch=1 sole_steel_staging=1 actual_finite_stroke=1 actual_wood_landing=1 stable_steel_exit=1 gravity=1 deaths=0")
	return true
