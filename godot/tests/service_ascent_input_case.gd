extends "res://tests/opening_journey_input_case.gd"

# Only the initial11m footing is staged and then solved by native physics.
# Every later movement, jump and hand transfer uses shipping touch input.
const CABINET_APPROACH := [Vector3(21.2, 0, -124.4), Vector3(21.2, 0, -121.3), Vector3(20, 0, -121.3)]
const DUCT_APPROACH := [Vector3(22.5, 0, -123.05), Vector3(24, 0, -123)]
var hand_ticks: Dictionary = {}
var support_ticks: Dictionary = {}
var initial_stage_count := 0
var cabinet_stood := false
var duct_hung := false
var duct_stood := false
var vent_climbed := false
var high_position := Vector3.ZERO
# Diagnostic opt-in only; default route keeps the original no-tuck input.
var explicit_tuck_before_duct_topout := false
var duct_tuck_before: Dictionary = {}
var duct_topout_before_jump: Dictionary = {}
var duct_topout_after_jump: Dictionary = {}

func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures.append(why)
		push_error("SERVICE_ASCENT " + why)

func step(count: int = 1) -> void:
	for index in count:
		if not failures.is_empty():
			return
		main._process(STEP)
		phase_ticks += 1
		landing = main._native.get_landing_state()
		var support := int(main._ctx.support_entity)
		var hands := int(landing.traversal_hand_constraint_count)
		var traversal := int(main._native.get_traversal_state())
		var hand_support := int(main._native.get_traversal_support_entity_id())
		if bool(main._ctx.grounded):
			support_ticks[support] = int(support_ticks.get(support, 0)) + 1
		if hands > 0:
			hand_ticks[hand_support] = int(hand_ticks.get(hand_support, 0)) + 1
		gravity_intact = gravity_intact and float(landing.player_gravity_factor) == 1.0
		hud_hidden = hud_hidden and not main._story_presenter._objective.visible
		story = main._story_guidance.sample(main._ctx, int(main._native.get_tick_index()))
		cues[story.get("transmission_key", &"")] = true
		var radio: Dictionary = main._story_radio.readback()
		if bool(radio.playing):
			radio_cues[radio.active_key] = true
		if phase == 0 or phase == 9:
			var expected := support == 11 if phase == 0 else support in [11, 2562]
			if expected and bool(main._ctx.grounded) and bool(landing.checkpoint_footing_valid):
				receiver_ticks += 1
			else:
				receiver_ticks = 0
		if not gravity_intact or int(main._native.get_death_count()) != 0:
			check(false, "service ascent retains gravity and has no death or hidden retry")
		if traversal in [1, 4] and hands == 0:
			check(false, "native hanging and climbing retain actual hand attachments")

func begin(shipping_main: Node) -> Dictionary:
	main = shipping_main
	main.set_process(false)
	main._router.capture_mouse = false
	main._router.gameplay_active = true
	check(not main._ci_mode and main._uitest_scenario.is_empty(), "shipping Main owns the staged-start service slice")
	check(main._native.debug_restart_at(Vector3(18.5, 11.95, -126)), "single declared initial11m placement is accepted")
	initial_stage_count = 1
	initial = main._native.get_player_position()
	stall_position = initial
	return result()

func walk_chunk(at: Vector3) -> bool:
	# Tight cabinet/duct arrivals retain the inherited touch stick and Main
	# processing, with a smaller tolerance than the wide grade corridor.
	for index in 24:
		var position: Vector3 = main._native.get_player_position()
		var offset := Vector2(at.x - position.x, at.z - position.z)
		if offset.length() < .05:
			stick(Vector2.ZERO)
			step(12)
			return true
		var forward := Vector2(-sin(main._yaw), -cos(main._yaw))
		var right := Vector2(cos(main._yaw), -sin(main._yaw))
		var move := offset.normalized() * minf(.28, offset.length() * .6)
		stick(Vector2(move.dot(right), move.dot(forward)))
		step()
	return false

func standing_on(entity: int, minimum_y: float) -> bool:
	return bool(main._ctx.grounded) and int(main._ctx.support_entity) == entity \
		and int(main._native.get_traversal_state()) == 0 \
		and (main._native.get_player_position() as Vector3).y > minimum_y

func reset_walk() -> void:
	waypoint = 0
	stall_ticks = 0
	stall_position = main._native.get_player_position()

func duct_posture() -> Dictionary:
	var state: Dictionary = main._native.get_landing_state()
	var position: Vector3 = main._native.get_player_position()
	var lip: Vector3 = main._native.get_traversal_ledge_point()
	return {"tick": main._native.get_tick_index(), "position": position,
		"velocity": main._native.get_player_linear_velocity(), "ledge": lip,
		"shoulder_to_lip_m": (lip - position - Vector3(0, .45, 0)).length(),
		"hands": state.traversal_hand_constraint_count, "hand_support": main._native.get_traversal_support_entity_id(),
		"tuck_available": state.swing_tuck_available, "tuck_requested": state.swing_tuck_requested,
		"commanded_span_m": state.hanging_commanded_span_m, "actual_span_m": state.hanging_actual_span_m,
		"hand_positive_work_j": state.hand_actuator_positive_work_j, "hand_absorbed_work_j": state.hand_actuator_absorbed_work_j,
		"hand_load_n": state.actual_hand_load_n, "left_force_n": state.left_hand_force_n,
		"right_force_n": state.right_hand_force_n, "gravity": state.player_gravity_factor,
		"accepted": main._native.get_accepted_traversal_count(), "rejected": main._native.get_rejected_traversal_count()}

func advance() -> Dictionary:
	if not failures.is_empty() or phase >= 12:
		return result()
	match phase:
		0:
			step(24)
			if receiver_ticks >= 90:
				check(absf((main._native.get_player_position() as Vector3).y - 11.9) < .15, "initial11m tower footing settles through actual native contact")
				next_phase()
			check(phase != 0 or phase_ticks < 360, "staged initial footing earns ninety firm native ticks")
		1:
			if walk_route(CABINET_APPROACH):
				next_phase()
		2:
			face_mesh()
			if absf(wrapf(main._yaw, -PI, PI)) < .002:
				step(36)
				check(main._native.is_ledge_available(), "ordinary touch approach offers cabinet ledge")
				tap(&"action")
				next_phase()
		3:
			step(24)
			if standing_on(2560, 13.5):
				cabinet_stood = true
				check(hand_ticks.has(2560), "touch Action climbs cabinet2560 through physical hand load")
				reset_walk()
				next_phase()
			check(phase != 3 or phase_ticks < 360, "cabinet Action reaches actual12.7m cabinet top")
		4:
			if walk_route([Vector3(20, 0, -122.10)]):
				face_mesh()
				step(27)
				tap(&"jump")
				stick(Vector2(0, .4))
				next_phase()
		5:
			for index in 24:
				step()
				if int(main._native.get_traversal_state()) == 1:
					stick(Vector2.ZERO)
					duct_hung = true
					check(int(main._native.get_traversal_support_entity_id()) == 2561 and int(landing.traversal_hand_constraint_count) == 2,
						"real jump and forward touch catch duct2561 with two hands")
					step(27)
					if explicit_tuck_before_duct_topout:
						duct_tuck_before = duct_posture()
						check(bool(duct_tuck_before.tuck_available), "actual duct hang offers existing touch tuck")
						tap(&"crouch")
						# One second lets the existing finite hand actuator settle;
						# no state/energy or shoulder-reach limit is changed.
						step(90)
					duct_topout_before_jump = duct_posture()
					tap(&"jump")
					next_phase()
					break
			check(phase != 5 or phase_ticks < 180, "cabinet jump reaches actual duct lip")
		6:
			step(24)
			if duct_topout_after_jump.is_empty():
				duct_topout_after_jump = duct_posture()
			if standing_on(2561, 17):
				duct_stood = true
				check(int(landing.traversal_hand_constraint_count) == 0, "fresh touch Jump tops out onto real16.3m duct footing")
				reset_walk()
				next_phase()
			check(phase != 6 or phase_ticks < 360, "duct topout reaches real standing support")
		7:
			if walk_route(DUCT_APPROACH):
				face_mesh()
				step(36)
				tap(&"action")
				hold_climb()
				step(18)
				check(int(main._native.get_traversal_state()) == 4 and int(main._native.get_traversal_support_entity_id()) == 2562
					and int(landing.traversal_hand_constraint_count) == 2, "touch Action takes actual vent2562 with two physical hands")
				vent_climbed = failures.is_empty()
				next_phase()
		8:
			for index in 24:
				step()
				if bool(main._ctx.grounded) and int(main._native.get_traversal_state()) == 0 \
						and (main._native.get_player_position() as Vector3).y > 22.5:
					stick(Vector2.ZERO)
					receiver_ticks = 0
					next_phase()
					break
			check(phase != 8 or phase_ticks < 1800, "held touch vent climb reaches actual22m receiver")
		9:
			step(24)
			if receiver_ticks >= 90:
				high_position = main._native.get_player_position()
				check(high_position.y > 22.7 and high_position.y < 23.2 and int(landing.traversal_hand_constraint_count) == 0,
					"real22m receiver retains ninety firm ticks with hands released")
				reset_walk()
				next_phase()
			check(phase != 9 or phase_ticks < 360, "vent receiving settles on real22m support")
		10:
			if walk_route([Vector3(22, 0, -125.2)]):
				check(standing_on(11, 22.7) and bool(landing.checkpoint_footing_valid)
					and (main._native.get_player_position() as Vector3).distance_to(high_position) > 1,
					"ordinary onward touch walking reaches supported22m tower ring")
				next_phase()
		11:
			stick(Vector2.ZERO)
			check(cabinet_stood and duct_hung and duct_stood and vent_climbed, "complete service ascent uses cabinet, duct and vent in one life")
			check(radio_cues.has(&"service_iona_inspection_rail"), "actual22m receiver plays packaged onward rail and ladder direction")
			check(gravity_intact and hud_hidden and int(main._native.get_death_count()) == 0, "whole staged-start slice retains gravity, hidden mission HUD and zero deaths")
			check(main._story_radio.readback().missing_keys.is_empty(), "service ascent has no missing packaged radio cue")
			next_phase()
	return result()

func result() -> Dictionary:
	if main != null and not failures.is_empty() and blocker.is_empty():
		stick(Vector2.ZERO)
		blocker = {"phase": phase, "waypoint": waypoint, "position": main._native.get_player_position(),
			"velocity": main._native.get_player_linear_velocity(), "support": main._native.get_support_entity_id(),
			"traversal": main._native.get_traversal_state(), "traversal_support": main._native.get_traversal_support_entity_id(),
			"grip_available": main._native.is_grip_available(), "ledge_available": main._native.is_ledge_available(), "landing": landing.duplicate()}
	return {"done": phase >= 12 or not failures.is_empty(), "checks": checks, "failures": failures,
		"phase": phase, "phase_ticks": phase_ticks, "waypoint": waypoint, "blocker": blocker,
		"native": true, "touch": true, "initial_11m_stage_only": true, "initial_stage_count": initial_stage_count,
		"position": main._native.get_player_position() if main != null else Vector3.ZERO,
		"tick": main._native.get_tick_index() if main != null else -1, "gravity_intact": gravity_intact,
		"mission_hud_hidden": hud_hidden, "hand_ticks": hand_ticks, "support_ticks": support_ticks,
		"explicit_tuck_before_duct_topout": explicit_tuck_before_duct_topout,
		"duct_tuck_before": duct_tuck_before, "duct_topout_before_jump": duct_topout_before_jump,
		"duct_topout_after_jump": duct_topout_after_jump,
		"cues": cues.keys(), "radio_cues": radio_cues.keys(),
		"viewport_3d_disabled": main.get_viewport().disable_3d if main != null else false}
