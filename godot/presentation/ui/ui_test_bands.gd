extends "res://presentation/ui/ui_test_moves.gd"

# UI scenarios: AS-006's well stages, Wet Isolation, the Plate Shop and the
# Facade Crane. (Part of the ui_test_driver.gd chain.)

const TRAVERSAL_CLIMBING := 4

func _well_stage_a(device: int) -> bool:
	for leg in [Vector2(-10.0, -129.2), Vector2(-10.2, -130.6), Vector2(-11.35, -131.95)]:
		if not await _walk_to(device, leg, 0.2):
			return _fail("the walk into the cage stalled at (%.2f, %.2f)" % [
				_position().x, _position().z])
	await _face(Vector2(-0.7, -0.7))
	if not await _offered(&"unhook", "UNHOOK"):
		return _fail("Action read '%s' facing the bollard, not UNHOOK" % _action_label())
	_act(device)
	var holding_shackle: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 2002, 0.5)
	if not holding_shackle:
		return _fail("UNHOOK did not put the rope's shackle in the hands")
	if not await _walk_to(device, Vector2(-11.35, -131.4), 0.2):
		return _fail("the step to the cage's eye stalled")
	await _face(Vector2(-1.0, 0.0))
	if not await _offered(&"hook", "HOOK", "ONTO CAGE EYE"):
		return _fail("Action read '%s %s' at the cage's eye, not HOOK ONTO CAGE EYE" % [
			_action_label(), String(_ctx()["action"]["detail"])])
	await _pose("rig_shackle")
	_act(device)
	var hooked: bool = await _wait_until(
		func() -> bool: return int(_native().get_well_a_rope_end_entity_id()) == 2000, 0.5)
	if not hooked:
		return _fail("HOOK did not put the rope's end on the cage's eye (end %d, at %s, carrying %d)" % [
			int(_native().get_well_a_rope_end_entity_id()), str(_position()),
			int(_native().get_carrying_entity_id())])
	if not await _walk_to(device, Vector2(-11.05, -130.35), 0.2):
		return _fail("the step to the trip handle stalled")
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"pick_up", "GRAB"):
		return _fail("Action read '%s' facing the trip handle, not GRAB" % _action_label())
	_act(device)
	var holding_handle: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 2004, 0.5)
	if not holding_handle:
		return _fail("GRAB did not put the trip handle in the hands")
	await _seconds(0.5)
	if _main._arms.hand_poses() != [8, 8]:
		return _fail("hands not on the trip handle (poses %s)" % str(_main._arms.hand_poses()))
	if _action_label() != "LET GO":
		return _fail("Action read '%s' holding the handle, not LET GO" % _action_label())
	await _pose("rig_handle")
	_move(device, -0.6)
	var tripped: bool = await _wait_until(
		func() -> bool: return not bool(_native().is_well_a_catch_latched()), 3.0)
	_move(device, 0.0)
	if not tripped:
		return _fail("stepping back with the handle never opened the catch")
	_act(device)
	var let_go: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 0, 0.5)
	if not let_go:
		return _fail("LET GO did not take the handle out of the hands")
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_native().get_well_a_cage_travel()) >= 21.95, 15.0)
	if not arrived:
		return _fail("the cage never reached the top (travel %.2f m)" %
			float(_native().get_well_a_cage_travel()))
	await _seconds(0.5)
	await _pose("rig_top")
	if int(_native().get_support_entity_id()) != 2000 or _position().y < 177.0:
		return _fail("the rider is not standing in the cage at the top (y %.2f)" % _position().y)
	return true

func _rig(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var watch := _watch_wrists()
	if not await _well_stage_a(device):
		return false
	var worst_jump := _stop_watch(watch)
	if worst_jump > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_jump,
			str(watch.get("at", ""))])
	_detail = "top_y=%.2f worst_wrist_step_m=%.3f" % [_position().y, worst_jump]
	return true

func _well_stage_b(device: int) -> bool:
	if _position().x < -8.5:
		if not (await _walk_to(device, Vector2(-9.4, -131.2), 0.2, 4.0) and \
				await _walk_to(device, Vector2(-7.6, -131.2), 0.2, 4.0)):
			return _fail("step across from A's parked cage into B's stalled")
	if not await _walk_to(device, Vector2(-6.3, -130.35), 0.2, 4.0):
		return _fail("the step to Stage B cleat stalled")
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"unhook", "UNHOOK"):
		return _fail("Action read '%s' facing Stage B cleat, not UNHOOK" % _action_label())
	_act(device)
	var holding_shackle: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 2012, 0.5)
	if not holding_shackle:
		return _fail("UNHOOK did not put Stage B line's shackle in hands")
	if not await _walk_to(device, Vector2(-6.35, -131.40), 0.2, 4.0):
		return _fail("the step to Stage B cage eye stalled")
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"hook", "HOOK", "ONTO CAGE EYE"):
		return _fail("Action read '%s %s' at Stage B cage eye, not HOOK ONTO CAGE EYE" % [
			_action_label(), String(_ctx()["action"]["detail"])])
	await _pose("boom_shackle")
	_act(device)
	var hooked: bool = await _wait_until(
		func() -> bool: return int(_native().get_well_b_rope_end_entity_id()) == 2010, 0.5)
	if not hooked:
		return _fail("HOOK did not put Stage B line on cage eye")
	if not await _walk_to(device, Vector2(-8.0, -130.25), 0.2, 4.0):
		return _fail("the step to Stage B trip handle stalled")
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"pick_up", "GRAB"):
		return _fail("Action read '%s' facing Stage B trip handle, not GRAB" % _action_label())
	_act(device)
	var holding_handle: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 2014, 0.5)
	if not holding_handle:
		return _fail("GRAB did not put Stage B trip handle in hands")
	await _pose("boom_handle")
	_move(device, -0.6)
	var tripped: bool = await _wait_until(
		func() -> bool: return not bool(_native().is_well_b_catch_latched()), 3.0)
	_move(device, 0.0)
	if not tripped:
		return _fail("stepping back with Stage B handle never opened catch")
	_act(device)
	var let_go: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 0, 0.5)
	if not let_go:
		return _fail("LET GO did not drop Stage B handle")
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_native().get_well_b_cage_travel()) >= 21.9, 16.0)
	if not arrived:
		return _fail("Stage B cage never reached top (travel %.2f m)" %
			float(_native().get_well_b_cage_travel()))
	await _seconds(0.5)
	await _pose("boom_top")
	if int(_native().get_support_entity_id()) != 2010 or _position().y < 198.5:
		return _fail("rider not standing in Stage B cage at top (y %.2f)" % _position().y)
	return true

func _boom(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var watch := _watch_wrists()
	if not await _well_stage_b(device):
		return false
	var worst_jump := _stop_watch(watch)
	if worst_jump > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_jump,
			str(watch.get("at", ""))])
	_detail = "top_y=%.2f worst_wrist_step_m=%.3f" % [_position().y, worst_jump]
	return true

func _well_stage_c(device: int) -> bool:
	if _position().x < -5.0:
		if not (await _walk_to(device, Vector2(-6.2, -131.9), 0.2, 4.0) and \
				await _walk_to(device, Vector2(-4.4, -131.9), 0.2, 4.0)):
			return _fail("step across onto Stage C platform stalled")
	if not await _walk_to(device, Vector2(-4.4, -131.6), 0.2, 4.0):
		return _fail("the step to the rebar's handle stalled")
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"pick_up", "GRAB", "REBAR LINE"):
		return _fail("Action read '%s %s' facing the rebar's handle, not GRAB REBAR LINE" % [
			_action_label(), String(_ctx()["action"]["detail"])])
	_act(device)
	var holding_line: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 2024, 0.5)
	if not holding_line:
		return _fail("GRAB did not put the rebar's handle in the hands")
	_move(device, -0.6)
	var thrown: bool = await _wait_until(
		func() -> bool: return float(_native().get_well_c_rebar_angle()) > 1.7, 3.0)
	_move(device, 0.0)
	if not thrown:
		return _fail("stepping back never threw the rebar over")
	if not await _let_go_if_held(device):
		return _fail("LET GO did not take the rebar's handle out of the hands")
	await _face(Vector2(-1.0, 0.2))
	await _wait_until(func() -> bool: return float(_native().get_well_c_dumpster_kg()) > 300.0, 4.0)
	await _pose("debris_pour")
	var filled: bool = await _wait_until(
		func() -> bool: return float(_native().get_well_c_dumpster_kg()) >= 899.0, 8.0)
	if not filled:
		return _fail("the dumpster never filled (%.0f kg)" % float(_native().get_well_c_dumpster_kg()))
	if float(_native().get_well_c_platform_travel()) > 0.05:
		return _fail("latched, the platform moved %.2f m" % float(_native().get_well_c_platform_travel()))
	if not await _walk_to(device, Vector2(-3.0, -131.6), 0.2, 4.0):
		return _fail("the step to the latch's handle stalled")
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"pick_up", "GRAB", "LATCH HANDLE"):
		return _fail("Action read '%s %s' facing the latch's handle, not GRAB LATCH HANDLE" % [
			_action_label(), String(_ctx()["action"]["detail"])])
	_act(device)
	var holding_latch: bool = await _wait_until(
		func() -> bool: return int(_native().get_carrying_entity_id()) == 2026, 0.5)
	if not holding_latch:
		return _fail("GRAB did not put the latch's handle in the hands")
	_move(device, -0.6)
	var freed: bool = await _wait_until(
		func() -> bool: return not bool(_native().is_well_c_catch_latched()), 3.0)
	_move(device, 0.0)
	if not freed:
		return _fail("stepping back with the latch's handle never freed the platform")
	if not await _let_go_if_held(device):
		return _fail("LET GO did not take the latch's handle out of the hands")
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_native().get_well_c_platform_travel()) >= 21.9, 15.0)
	if not arrived:
		return _fail("the platform never reached the top (travel %.2f m)" %
			float(_native().get_well_c_platform_travel()))
	await _seconds(0.5)
	if not await _walk_to(device, Vector2(-3.0, -129.0), 0.2, 4.0):
		return _fail("the step off Stage C onto Ring 220 stalled")
	await _seconds(0.5)
	await _pose("debris_top")
	if not bool(_native().is_player_grounded()) or _position().y < 221.0 or \
			int(_native().get_support_entity_id()) == 2020:
		return _fail("not standing on Ring 220 receiver slab (y %.2f, support %d)" % [
			_position().y, int(_native().get_support_entity_id())])
	return true

func _debris(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var watch := _watch_wrists()
	if not await _well_stage_c(device):
		return false
	var worst_jump := _stop_watch(watch)
	if worst_jump > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_jump,
			str(watch.get("at", ""))])
	_detail = "top_y=%.2f worst_wrist_step_m=%.3f" % [_position().y, worst_jump]
	return true

# AS-007, Wet Isolation, the way its native band test runs it: from the 220
# ring, D's spool carried into its gap and its fill valve thrown, a ride on
# the float's platform to 256 m; E's door drawn shut and its chiller tripped,
# a ride in the cab on the chiller's air to 298 m; F's hose hooked onto the
# ram's inlet and its stop valve thrown, a ride on the accumulator's platform,
# and off onto TP-340.
func _wet(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var wrists := _watch_wrists()
	var body := _watch_body()
	if not (await _wet_d(device) and await _wet_e(device) and await _wet_f(device)):
		return false
	var worst_wrist := _stop_watch(wrists)
	var worst_step := _stop_watch(body)
	if worst_wrist > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_wrist,
			str(wrists.get("at", ""))])
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died %d times on the way" % int(_native().get_death_count()))
	if not _heard_region(&"wet"):
		return false
	_detail = "plate_y=%.2f seconds=%.1f worst_wrist_step_m=%.3f worst_body_step_m=%.3f" % [
		_position().y, float(int(_native().get_tick_index()) - started) / 90.0, worst_wrist, worst_step]
	return true

func _wet_state() -> Dictionary:
	return _native().get_wet_state()

# Faces the handle ahead at (x, z) along `facing`, GRABs `name` (entity
# `handle`) and steps back with it until `done` holds, then lets go.
func _pull_handle(device: int, at: Vector2, facing: Vector2, handle: int, name: String,
		done: Callable) -> bool:
	if not await _walk_to(device, at, 0.08, 6.0):
		return _fail("the step to %s stalled at %s" % [name, str(_position())])
	await _face(facing)
	if not await _offered(&"pick_up", "GRAB", name):
		return _fail("Action read '%s %s' facing %s, not GRAB" % [_action_label(),
			String(_ctx()["action"]["detail"]), name])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == handle, 0.5):
		return _fail("GRAB did not take %s" % name)
	_move(device, -0.35)
	var thrown: bool = await _wait_until(done, 4.0)
	_move(device, 0.0)
	if not thrown:
		return _fail("stepping back with %s never threw it" % name)
	if not await _let_go_if_held(device):
		return _fail("LET GO did not take %s out of the hands" % name)
	return true

func _wet_d(device: int) -> bool:
	if not await _go(device, Vector2(9.5, -130.6), 0.08, 12.0):
		return _fail("the walk to D's spool stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"pick_up", "PICK UP", "PIPE SPOOL"):
		return _fail("Action read '%s %s' facing D's spool, not PICK UP PIPE SPOOL" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2031, 0.5):
		return _fail("PICK UP did not lift D's spool")
	# Carried north into the gap as the native test carries it, at 0.35 of
	# full throw to z -128.25 and a coast; a key, with no throw, walks it
	# there and stops.
	var carried := false
	if device == InputRouter.Device.KEYBOARD_MOUSE:
		carried = await _walk_to(device, Vector2(9.5, -128.17), 0.05, 6.0)
	else:
		_move(device, 0.35)
		carried = await _wait_until(func() -> bool: return _position().z >= -128.25, 6.0)
		_move(device, 0.0)
	if not carried:
		return _fail("carrying D's spool to its gap stalled at %s" % str(_position()))
	# Steadied before it is set down, as a player does with a swinging load.
	await _seconds(0.6)
	var spool := int(_native().get_kit_body_index(2031))
	var spool_last := (_native().get_kit_body_transform(spool) as Transform3D).origin
	for frame in 180:
		await get_tree().process_frame
		var spool_now := (_native().get_kit_body_transform(spool) as Transform3D).origin
		var swing := (spool_now - spool_last).length() / get_process_delta_time()
		spool_last = spool_now
		if swing < 0.05:
			break
	if not await _let_go_if_held(device):
		return _fail("LET GO did not set D's spool down")
	await _seconds(1.5)
	if not bool(_wet_state()["d_pipe_whole"]):
		return _fail("set down, D's spool did not close the fill line's gap")
	if not (await _go(device, Vector2(13.4, -129.6), 0.15, 8.0) and \
			await _go(device, Vector2(13.4, -132.7), 0.15, 6.0)):
		return _fail("the walk onto D's platform stalled at %s" % str(_position()))
	if not await _pull_handle(device, Vector2(12.3, -132.7), Vector2(0.0, 1.0), 2033, "FILL VALVE",
			func() -> bool: return float(_wet_state()["d_valve_angle"]) > 1.8):
		return false
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_wet_state()["d_platform_travel"]) >= 35.95, 120.0)
	if not arrived:
		return _fail("D's platform never reached 256 m (travel %.2f m)" % float(_wet_state()["d_platform_travel"]))
	await _seconds(1.0)
	await _pose("wet_d_top")
	if int(_native().get_support_entity_id()) != 2030:
		return _fail("the rider is not on D's platform at the top (y %.2f)" % _position().y)
	return true

func _wet_e(device: int) -> bool:
	if not (await _go(device, Vector2(13.2, -134.5), 0.15, 6.0) and \
			await _walk_to(device, Vector2(13.2, -135.8), 0.15, 6.0) and \
			await _walk_to(device, Vector2(13.05, -136.4), 0.08, 6.0)):
		return _fail("the walk through E's door into its cab stalled at %s" % str(_position()))
	var leaf := int(_native().get_kit_body_index(2042))
	var last := (_native().get_kit_body_transform(leaf) as Transform3D).origin
	for frame in 240:
		await get_tree().process_frame
		var now := (_native().get_kit_body_transform(leaf) as Transform3D).origin
		var moved := Vector2(now.x - last.x, now.z - last.z).length() / get_process_delta_time()
		last = now
		if moved < 0.2:
			break
	var at := _position()
	await _face(Vector2(last.x - at.x, last.z - at.z))
	if not await _offered(&"pick_up", "GRAB", "DOOR"):
		return _fail("Action read '%s %s' facing E's door, not GRAB DOOR" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2042, 0.5):
		return _fail("GRAB did not take E's door")
	_move_world(device, Vector2(0.4, 0.0))
	var latched: bool = await _wait_until(func() -> bool: return bool(_wet_state()["e_door_latched"]), 6.0)
	_move_dir(device, Vector2.ZERO)
	if not latched:
		return _fail("drawing E's door east never latched it")
	if not await _let_go_if_held(device):
		return _fail("LET GO did not take E's door out of the hands")
	if not await _pull_handle(device, Vector2(13.4, -136.65), Vector2(1.0, 0.0), 2045, "CHILLER TRIP",
			func() -> bool: return not bool(_wet_state()["e_catch_latched"])):
		return false
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_wet_state()["e_cab_travel"]) >= 41.95, 90.0)
	if not arrived:
		return _fail("E's cab never reached 298 m (travel %.2f m)" % float(_wet_state()["e_cab_travel"]))
	await _seconds(1.0)
	await _pose("wet_e_top")
	if int(_native().get_support_entity_id()) != 2040:
		return _fail("the rider is not in E's cab at the top (y %.2f)" % _position().y)
	return true

func _wet_f(device: int) -> bool:
	if not (await _go(device, Vector2(13.4, -137.2), 0.15, 4.0) and \
			await _walk_to(device, Vector2(13.4, -139.9), 0.15, 6.0) and \
			await _walk_to(device, Vector2(12.6, -139.95), 0.08, 6.0)):
		return _fail("the walk from E's cab onto F's platform stalled at %s" % str(_position()))
	await _face(Vector2(-0.3, -0.95))
	if not await _offered(&"pick_up", "TAKE", "HOSE"):
		return _fail("Action read '%s %s' facing F's hose, not TAKE HOSE" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2052, 0.5):
		return _fail("TAKE did not put F's hose in the hands")
	if not await _walk_to(device, Vector2(12.4, -139.55), 0.08, 4.0):
		return _fail("the step to the ram's inlet stalled at %s" % str(_position()))
	await _face(Vector2(-0.6, 0.8))
	if not await _offered(&"hook", "HOOK", "ONTO RAM INLET"):
		return _fail("Action read '%s %s' at the ram's inlet, not HOOK ONTO RAM INLET" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_wet_state()["f_hose_coupled"]), 0.5):
		return _fail("HOOK did not couple F's hose to the ram")
	if not await _pull_handle(device, Vector2(12.25, -140.7), Vector2(0.0, -1.0), 2054, "STOP VALVE",
			func() -> bool: return not bool(_wet_state()["f_catch_latched"])):
		return false
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_wet_state()["f_platform_travel"]) >= 41.95, 90.0)
	if not arrived:
		var f := _wet_state()
		return _fail("F's platform never reached TP-340 (travel %.2f m, hose coupled %s, catch latched %s, accumulator %.2f m)" % [
			float(f["f_platform_travel"]), str(f["f_hose_coupled"]), str(f["f_catch_latched"]),
			float(f["f_accumulator_travel"])])
	await _seconds(1.0)
	if not await _go(device, Vector2(13.0, -137.2), 0.15, 6.0):
		return _fail("the step off F's platform onto TP-340 stalled at %s" % str(_position()))
	await _seconds(0.5)
	await _pose("wet_plate")
	if not bool(_native().is_player_grounded()) or _position().y < 340.25 or \
			int(_native().get_support_entity_id()) != 1005:
		return _fail("not standing on TP-340 (y %.2f, support %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true

# AS-008, the Plate Shop, the way its native band test runs it: from TP-340,
# G's rope off its cleat and hooked on its platform's eye, its prop pin
# drawn, a ride to the 374 ring; the girder's tail pin carried out, over the
# gangway onto H, its chock yanked, a ride to the 418 ring; across onto I's
# cage, its rope hooked on, the domino's pin drawn, the cascade's ride to
# 462 m; up the ladder onto the 484 ring.
func _shop(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var wrists := _watch_wrists()
	var body := _watch_body()
	if not await _shop_g(device):
		return false
	var at_g := _position().y
	if not await _shop_h(device):
		return false
	var at_h := _position().y
	if not await _shop_i(device):
		return false
	# The hands are watched on the machines' ropes, pins and lanyards, as
	# _wet and _stack watch them; a ladder's top-out is traversal, whose
	# planted hands the climb scenarios prove.
	var worst_wrist := _stop_watch(wrists)
	if not await _shop_ladder(device):
		return false
	var worst_step := _stop_watch(body)
	if worst_wrist > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_wrist,
			str(wrists.get("at", ""))])
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died %d times on the way" % int(_native().get_death_count()))
	# The Plate Shop at work: across the well a welder's arc burns, and is heard.
	if int(_main._crew.welding_frames) == 0 or int(_main._audio.welds_heard) == 0:
		return _fail("no welder was seen at work across the well (arc frames %d, heard %d)" % [
			int(_main._crew.welding_frames), int(_main._audio.welds_heard)])
	if not _heard_region(&"shop"):
		return false
	_detail = "shop_g_y=%.2f shop_h_y=%.2f ring484_y=%.2f seconds=%.1f worst_wrist_step_m=%.3f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		at_g, at_h, _position().y, float(int(_native().get_tick_index()) - started) / 90.0, worst_wrist, worst_step,
		float(body["lift"])]
	return true

func _shop_state() -> Dictionary:
	return _native().get_shop_state()

func _shop_g(device: int) -> bool:
	# From TP-340 by F's hole, west past the header, to the south of G's cleat.
	if not (await _go(device, Vector2(11.3, -138.0), 0.15, 6.0) and \
			await _go(device, Vector2(1.0, -141.0), 0.15, 16.0) and \
			await _go(device, Vector2(-5.0, -153.5), 0.15, 12.0) and \
			await _go(device, Vector2(-9.7, -152.9), 0.08, 8.0)):
		return _fail("the walk to G's cleat stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"unhook", "UNHOOK"):
		return _fail("Action read '%s %s' facing G's cleat, not UNHOOK" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2074, 0.5):
		return _fail("UNHOOK did not put G's rope shackle in the hands")
	# Round the shaft's posts onto the platform, the rope in hand.
	if not (await _go(device, Vector2(-8.5, -152.9), 0.15, 4.0) and \
			await _go(device, Vector2(-8.5, -150.1), 0.15, 4.0) and \
			await _go(device, Vector2(-11.2, -150.0), 0.08, 6.0)):
		return _fail("carrying G's rope onto its platform stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"hook", "HOOK", "ONTO PLATFORM EYE"):
		return _fail("Action read '%s %s' at G's platform eye, not HOOK ONTO PLATFORM EYE" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_shop_state()["g_rope_on_eye"]), 0.5):
		return _fail("HOOK did not put G's rope on the platform's eye")
	if not await _pull_handle(device, Vector2(-11.3, -149.25), Vector2(0.0, 1.0), 2073, "LANYARD",
			func() -> bool: return not bool(_shop_state()["g_tower_latched"])):
		return false
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_shop_state()["g_platform_travel"]) >= 33.7, 60.0)
	if not arrived:
		return _fail("G's platform never reached the 374 ring (travel %.2f m)" %
			float(_shop_state()["g_platform_travel"]))
	await _seconds(1.0)
	await _pose("shop_g_top")
	if not _capture_prefix.is_empty():
		# Across the well on the 374 ring, the Plate Shop's welder at work;
		# then the view as it was. (Capture only: a headless run is unchanged.)
		var yaw_before := float(_main._yaw)
		var pitch_before := float(_main._pitch)
		await _face(Vector2(1.0, -0.02))
		await _tilt(-0.1)
		await _wait_until(func() -> bool: return int(_main._crew.burning_count()) > 0, 40.0)
		await _seconds(0.5)
		await _pose("shop_welder")
		_main._yaw = yaw_before
		_main._pitch = pitch_before
	if int(_native().get_support_entity_id()) != 2070:
		return _fail("the rider is not on G's platform at the top (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true

func _shop_h(device: int) -> bool:
	# Off G onto the 374 ring, round its west band to the north band and the
	# girder's tail pin.
	if not (await _go(device, Vector2(-14.5, -149.3), 0.15, 8.0) and \
			await _go(device, Vector2(-14.5, -134.3), 0.15, 16.0) and \
			await _go(device, Vector2(-2.8, -134.0), 0.15, 16.0) and \
			await _walk_to(device, Vector2(-3.6, -134.0), 0.08, 4.0)):
		return _fail("the walk round the 374 ring to the girder's tail pin stalled at %s" % str(_position()))
	await _face(Vector2(-1.0, 0.0))
	if not await _offered(&"pick_up", "PICK UP", "TAIL PIN"):
		return _fail("Action read '%s %s' facing the girder's tail pin, not PICK UP TAIL PIN" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2084, 0.5):
		return _fail("PICK UP did not lift the girder's tail pin")
	# Back a metre with it, out of its socket, and down.
	if not await _walk_to(device, Vector2(-2.6, -134.0), 0.15, 4.0):
		return _fail("stepping back with the tail pin stalled at %s" % str(_position()))
	if not await _let_go_if_held(device):
		return _fail("LET GO did not set the tail pin down")
	await _seconds(1.0)
	if bool(_shop_state()["h_girder_latched"]):
		return _fail("the tail pin out, the girder is still latched")
	# Round the ring to the gangway and over it onto H's platform.
	if not (await _go(device, Vector2(-14.5, -134.3), 0.15, 16.0) and \
			await _go(device, Vector2(-14.5, -145.6), 0.15, 16.0) and \
			await _go(device, Vector2(-9.6, -145.6), 0.15, 10.0)):
		return _fail("the walk over the gangway onto H stalled at %s" % str(_position()))
	if not await _pull_handle(device, Vector2(-9.4, -144.9), Vector2(0.0, 1.0), 2086, "LANYARD",
			func() -> bool: return not bool(_shop_state()["h_trolley_latched"])):
		return false
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_shop_state()["h_platform_travel"]) >= 43.9, 90.0)
	if not arrived:
		return _fail("H's platform never reached the 418 ring (travel %.2f m)" %
			float(_shop_state()["h_platform_travel"]))
	await _seconds(1.0)
	await _pose("shop_h_top")
	if int(_native().get_support_entity_id()) != 2080:
		return _fail("the rider is not on H's platform at the top (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true

func _shop_i(device: int) -> bool:
	# Across onto I's cage; the rope's shackle hangs a metre west of the eye.
	if not await _walk_to(device, Vector2(-7.3, -145.6), 0.08, 6.0):
		return _fail("the step across onto I's cage stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"pick_up", "TAKE", "ROPE SHACKLE"):
		return _fail("Action read '%s %s' facing I's rope shackle, not TAKE ROPE SHACKLE" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2096, 0.5):
		return _fail("TAKE did not put I's rope shackle in the hands")
	if not await _walk_to(device, Vector2(-6.3, -145.6), 0.08, 4.0):
		return _fail("the step to I's cage eye stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"hook", "HOOK", "ONTO CAGE EYE"):
		return _fail("Action read '%s %s' at I's cage eye, not HOOK ONTO CAGE EYE" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_shop_state()["i_rope_on_eye"]), 0.5):
		return _fail("HOOK did not put I's rope on the cage's eye")
	if not await _pull_handle(device, Vector2(-6.5, -144.9), Vector2(0.0, 1.0), 2095, "LANYARD",
			func() -> bool: return not bool(_shop_state()["i_domino_latched"])):
		return false
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_shop_state()["i_cage_travel"]) >= 43.9, 90.0)
	if not arrived:
		var i := _shop_state()
		return _fail("the cascade never hauled I's cage to 462 m (travel %.2f m, domino %.2f rad, trip %.2f rad, monolith %.2f rad)" % [
			float(i["i_cage_travel"]), float(i["i_domino_angle"]), float(i["i_trip_angle"]),
			float(i["i_monolith_angle"])])
	await _seconds(1.0)
	await _pose("shop_i_top")
	if int(_native().get_support_entity_id()) != 2090:
		return _fail("the rider is not in I's cage at the top (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true

# Up the ladder from I's cage onto the 484 ring.
func _shop_ladder(device: int) -> bool:
	if not await _walk_to(device, Vector2(-7.6, -145.6), 0.08, 6.0):
		return _fail("the step to the ladder in I's cage stalled at %s" % str(_position()))
	await _face(Vector2(-1.0, 0.0))
	if not await _offered(&"climb", "CLIMB", "HOLD"):
		return _fail("Action read '%s %s' facing the ladder from I's cage, not CLIMB HOLD" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("CLIMB did not take hold of the ladder from I's cage")
	_move(device, 1.0)
	var on_ring: bool = await _wait_until(func() -> bool: return _standing_above(484.5), 60.0)
	_move(device, 0.0)
	if not on_ring:
		return _fail("climbing the ladder did not top out onto the 484 ring (y %.2f)" % _position().y)
	await _seconds(0.5)
	await _pose("shop_ring484")
	return true

# AS-009, the Facade Crane Stack, the way its native band test runs it: from
# the 484 ring, J's rail joint carried onto its traveler and laid in the
# cradle, the wagon's chock pulled, a ride to the 528 ring; round the ring to
# K's cage, its rope hooked on, the jib's pendant pin drawn, a ride to the 572
# ring; round to L's cab, the winch's clutch thrown in and the drop weight's
# pin drawn, a ride into TP-640; off onto the plate.
func _crane(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var wrists := _watch_wrists()
	var body := _watch_body()
	if not await _crane_j(device):
		return false
	var at_j := _position().y
	if not await _crane_k(device):
		return false
	var at_k := _position().y
	if not await _crane_l(device):
		return false
	var worst_wrist := _stop_watch(wrists)
	var worst_step := _stop_watch(body)
	if worst_wrist > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_wrist,
			str(wrists.get("at", ""))])
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died %d times on the way" % int(_native().get_death_count()))
	if not _heard_region(&"crane"):
		return false
	_detail = "crane_j_y=%.2f crane_k_y=%.2f tp640_y=%.2f seconds=%.1f worst_wrist_step_m=%.3f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		at_j, at_k, _position().y, float(int(_native().get_tick_index()) - started) / 90.0, worst_wrist, worst_step,
		float(body["lift"])]
	return true

func _crane_state() -> Dictionary:
	return _native().get_crane_state()

func _crane_j(device: int) -> bool:
	# Off the ladder from I's cage, round to the 484 ring's north band.
	var here := _position()
	if here.x < -8.0 and not await _go(device, Vector2(here.x, -140.3), 0.15, 12.0):
		return _fail("the walk round to the 484 ring's north band stalled at %s" % str(_position()))
	if not (await _go(device, Vector2(-3.0, -140.3), 0.15, 12.0) and \
			await _go(device, Vector2(2.8, -139.0), 0.08, 12.0)):
		return _fail("the walk to J's rail joint stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"pick_up", "PICK UP", "RAIL JOINT"):
		return _fail("Action read '%s %s' facing J's rail joint, not PICK UP RAIL JOINT" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2102, 0.5):
		return _fail("PICK UP did not lift J's rail joint")
	# Onto J's traveler and to the cradle, the joint in hand.
	if not (await _go(device, Vector2(-0.3, -138.3), 0.15, 8.0) and \
			await _go(device, Vector2(-0.3, -136.2), 0.08, 6.0) and \
			await _go(device, Vector2(0.9, -136.2), 0.08, 6.0)):
		return _fail("carrying J's rail joint to its cradle stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	# Steadied before it is set down, as a player does with a swinging load.
	await _seconds(0.6)
	var joint := int(_native().get_kit_body_index(2102))
	var joint_last := (_native().get_kit_body_transform(joint) as Transform3D).origin
	for frame in 180:
		await get_tree().process_frame
		var joint_now := (_native().get_kit_body_transform(joint) as Transform3D).origin
		var swing := (joint_now - joint_last).length() / get_process_delta_time()
		joint_last = joint_now
		if swing < 0.05:
			break
	if not await _let_go_if_held(device):
		return _fail("LET GO did not set J's rail joint down")
	await _seconds(1.5)
	if not bool(_crane_state()["j_rail_whole"]):
		var rest := (_native().get_kit_body_transform(joint) as Transform3D).origin
		return _fail("set down, J's rail joint did not close the rail's gap (it lies at %s, its seat (1.6, 485.0, -136.2); the rider at %s)" % [
			str(rest), str(_position())])
	if not await _pull_handle(device, Vector2(0.0, -135.5), Vector2(0.0, 1.0), 2104, "LANYARD",
			func() -> bool: return not bool(_crane_state()["j_wagon_latched"])):
		return false
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_crane_state()["j_traveler_travel"]) >= 43.9, 60.0)
	if not arrived:
		return _fail("J's traveler never reached the 528 ring (travel %.2f m)" %
			float(_crane_state()["j_traveler_travel"]))
	await _seconds(1.0)
	await _pose("crane_j_top")
	if int(_native().get_support_entity_id()) != 2100:
		return _fail("the rider is not on J's traveler at the top (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true

func _crane_k(device: int) -> bool:
	# Off J round the 528 ring and over the board to K's cage.
	if not (await _go(device, Vector2(0.0, -138.6), 0.15, 8.0) and \
			await _go(device, Vector2(0.0, -141.5), 0.15, 8.0) and \
			await _go(device, Vector2(-8.5, -141.5), 0.15, 12.0) and \
			await _go(device, Vector2(-8.5, -155.0), 0.15, 16.0) and \
			await _go(device, Vector2(-11.6, -155.0), 0.15, 8.0)):
		return _fail("the walk round the 528 ring to K's cage stalled at %s" % str(_position()))
	# To the cage's north side, along it clear of the shackle on its long
	# rope, and round behind it.
	if not (await _go(device, Vector2(_position().x, -154.2), 0.15, 6.0) and \
			await _go(device, Vector2(-13.3, -154.2), 0.15, 10.0) and \
			await _go(device, Vector2(-13.3, -155.0), 0.08, 4.0)):
		return _fail("the walk round K's shackle stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"pick_up", "TAKE", "ROPE SHACKLE"):
		return _fail("Action read '%s %s' facing K's rope shackle, not TAKE ROPE SHACKLE" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2114, 0.5):
		return _fail("TAKE did not put K's rope shackle in the hands")
	if not await _walk_to(device, Vector2(-12.3, -155.0), 0.08, 4.0):
		return _fail("the step to K's cage eye stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"hook", "HOOK", "ONTO CAGE EYE"):
		return _fail("Action read '%s %s' at K's cage eye, not HOOK ONTO CAGE EYE" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_crane_state()["k_rope_on_eye"]), 0.5):
		return _fail("HOOK did not put K's rope on the cage's eye")
	if not await _pull_handle(device, Vector2(-12.6, -154.3), Vector2(0.0, 1.0), 2113, "LANYARD",
			func() -> bool: return not bool(_crane_state()["k_jib_latched"])):
		return false
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_crane_state()["k_cage_travel"]) >= 43.9, 90.0)
	if not arrived:
		return _fail("K's cage never reached the 572 ring (travel %.2f m)" %
			float(_crane_state()["k_cage_travel"]))
	await _seconds(1.0)
	await _pose("crane_k_top")
	if int(_native().get_support_entity_id()) != 2110:
		return _fail("the rider is not in K's cage at the top (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true

func _crane_l(device: int) -> bool:
	# Over the board and round the 572 ring to L's cab.
	if not (await _go(device, Vector2(-11.9, -155.0), 0.15, 6.0) and \
			await _go(device, Vector2(-6.5, -155.0), 0.15, 10.0) and \
			await _go(device, Vector2(-6.5, -156.7), 0.15, 6.0) and \
			await _go(device, Vector2(-3.0, -156.7), 0.15, 8.0) and \
			await _go(device, Vector2(-3.0, -159.6), 0.15, 6.0)):
		return _fail("the walk round the 572 ring into L's cab stalled at %s" % str(_position()))
	if not await _pull_handle(device, Vector2(-3.7, -159.4), Vector2(0.0, 1.0), 2127, "CLUTCH HANDLE",
			func() -> bool: return bool(_crane_state()["l_clutch_in"])):
		return false
	if not await _pull_handle(device, Vector2(-3.0, -160.8), Vector2(0.0, -1.0), 2125, "LANYARD",
			func() -> bool: return not bool(_crane_state()["l_weight_latched"])):
		return false
	var arrived: bool = await _wait_until(
		func() -> bool: return float(_crane_state()["l_cab_travel"]) >= 67.9, 90.0)
	if not arrived:
		var l := _crane_state()
		return _fail("L's cab never reached TP-640 (travel %.2f m, clutch in %s, weight latched %s, cart latched %s)" % [
			float(l["l_cab_travel"]), str(l["l_clutch_in"]), str(l["l_weight_latched"]), str(l["l_cart_latched"])])
	await _seconds(1.0)
	if not await _go(device, Vector2(-3.0, -156.5), 0.15, 8.0):
		return _fail("the step off L's cab onto TP-640 stalled at %s" % str(_position()))
	await _seconds(0.5)
	await _pose("crane_plate")
	if not _standing_above(640.2):
		return _fail("not standing on TP-640 (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true

# AS-010, Midstack Service, its first stage the way its native band test
# runs it: from TP-640 into the service cage, the rope's shackle hooked on
# the cage's eye and the reel's chock pulled by its lanyard; the reel falls
# paying out its cable and hauls the cage to the 662 deck, where its dogs
# hold it; off onto the deck.
func _service(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	var started := int(_native().get_tick_index())
	var wrists := _watch_wrists()
	var body := _watch_body()
	if not await _service_m(device):
		return false
	var worst_wrist := _stop_watch(wrists)
	var worst_step := _stop_watch(body)
	if worst_wrist > 0.10:
		return _fail("a hand jumped %.3f m in one frame between poses (%s)" % [worst_wrist,
			str(wrists.get("at", ""))])
	if worst_step > 0.25:
		return _fail("the body jumped %.3f m sideways in one frame (%s)" % [worst_step, str(body.get("at", ""))])
	if float(body["lift"]) > 0.10:
		return _fail("the view jumped %.3f m in one frame beyond the body's own motion (%s)" % [
			float(body["lift"]), str(body.get("lift_at", ""))])
	if int(_native().get_death_count()) != 0:
		return _fail("the climber died %d times on the way" % int(_native().get_death_count()))
	_detail = "deck662_y=%.2f held_m=%.2f seconds=%.1f worst_wrist_step_m=%.3f worst_body_step_m=%.3f worst_view_lift_m=%.3f" % [
		_position().y, float(_service_state()["m_cage_travel"]),
		float(int(_native().get_tick_index()) - started) / 90.0, worst_wrist, worst_step, float(body["lift"])]
	return true

func _service_state() -> Dictionary:
	return _native().get_service_state()

func _sling() -> Dictionary:
	return _native().get_slingshot_state()

func _service_m(device: int) -> bool:
	# From L's hole north across TP-640 and west into the service cage, round
	# the shackle hanging over its middle.
	if not (await _go(device, Vector2(-3.0, -144.3), 0.15, 12.0) and \
			await _go(device, Vector2(-8.7, -144.3), 0.15, 8.0) and \
			await _go(device, Vector2(-8.7, -143.5), 0.08, 4.0)):
		return _fail("the walk into the service cage stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"pick_up", "TAKE", "ROPE SHACKLE"):
		return _fail("Action read '%s %s' facing M's rope shackle, not TAKE ROPE SHACKLE" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) == 2134, 0.5):
		return _fail("TAKE did not put M's rope shackle in the hands")
	if not await _walk_to(device, Vector2(-7.7, -143.5), 0.08, 4.0):
		return _fail("the step to M's cage eye stalled at %s" % str(_position()))
	await _face(Vector2(1.0, 0.0))
	if not await _offered(&"hook", "HOOK", "ONTO CAGE EYE"):
		return _fail("Action read '%s %s' at M's cage eye, not HOOK ONTO CAGE EYE" % [_action_label(),
			String(_ctx()["action"]["detail"])])
	_act(device)
	if not await _wait_until(func() -> bool: return bool(_service_state()["m_rope_on_eye"]), 0.5):
		return _fail("HOOK did not put M's rope on the cage's eye")
	if not await _pull_handle(device, Vector2(-8.0, -142.8), Vector2(0.0, 1.0), 2133, "LANYARD",
			func() -> bool: return not bool(_service_state()["m_reel_latched"])):
		return false
	# The ride, until the cage stands still on its dogs for a second.
	var last := float(_service_state()["m_cage_travel"])
	var still := 0.0
	var waited := 0.0
	while waited < 40.0 and still < 1.0:
		await get_tree().process_frame
		var dt := get_process_delta_time()
		waited += dt
		var travel := float(_service_state()["m_cage_travel"])
		still = still + dt if travel > 1.0 and absf(travel - last) < 0.0005 else 0.0
		last = travel
	var held := float(_service_state()["m_cage_travel"])
	if still < 1.0 or held < 21.05:
		return _fail("M's cage never came to rest on its dogs at the 662 deck (travel %.2f m, reel %.2f m)" % [
			held, float(_service_state()["m_reel_travel"])])
	await _pose("service_m_top")
	if int(_native().get_support_entity_id()) != 2130:
		return _fail("the rider is not in M's cage at the top (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	# Off east onto the 662 deck.
	if not await _go(device, Vector2(-5.8, -143.5), 0.15, 6.0):
		return _fail("the step off M's cage onto the 662 deck stalled at %s (on %d, crouched %s, grounded %s, v %s, cage %.3f m)" % [
			str(_position()), int(_native().get_support_entity_id()), str(_native().is_player_crouched()),
			str(_ctx()["grounded"]), str(_velocity()), float(_service_state()["m_cage_travel"])])
	await _seconds(0.5)
	await _pose("service_deck662")
	if not _standing_above(662.5):
		return _fail("not standing on the 662 deck (y %.2f, on %d)" % [_position().y,
			int(_native().get_support_entity_id())])
	return true

# AS-010's C4 alone, from the 662 deck start, on the device under test.
# The band the climber passed through was heard as itself (audio_director
# REGIONS): at least one of its one-shots played there.
func _heard_region(region: StringName) -> bool:
	var heard := int(_main._audio.region_sounds.get(region, 0))
	if heard < 1:
		return _fail("the %s band made no sound of its own (heard %s)" % [region,
			str(_main._audio.region_sounds)])
	return true
