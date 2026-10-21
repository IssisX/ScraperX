extends "res://tests/service_ascent_input_case.gd"

# Exactly one declared22m placement, settled by native contacts. The rail,
# jump-only ladder catch, ascent and onward walk use shipping touch input.
const RAIL_APPROACH := [Vector3(12.5, 0, -125.2), Vector3(12.5, 0, -119.65)]
const DAVIT_EXIT := [Vector3(12.5, 0, -125), Vector3(11.2, 0, -125.3)]
var rail_balancing_ticks := 0
var tower_firm_ticks := 0
var rail_end: Dictionary = {}
var ladder_catch: Dictionary = {}
var davit_footing: Dictionary = {}
var tower_footing: Dictionary = {}
var ladder_unavailable_standing := false

func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures.append(why)
		push_error("INSPECTION_RAIL " + why)

func step(count: int = 1) -> void:
	for index in count:
		if not failures.is_empty():
			return
		super.step(1)
		if bool(main._ctx.grounded) and int(main._ctx.support_entity) == 2563 and main._native.is_player_balancing():
			rail_balancing_ticks += 1
		if phase == 7:
			if standing_on(11, 33.7) and bool(landing.checkpoint_footing_valid):
				tower_firm_ticks += 1
			else:
				tower_firm_ticks = 0

func begin(shipping_main: Node) -> Dictionary:
	main = shipping_main
	main.set_process(false)
	main._router.capture_mouse = false
	main._router.gameplay_active = true
	check(not main._ci_mode and main._uitest_scenario.is_empty(), "shipping Main owns the staged-start inspection rail slice")
	check(main._native.debug_restart_at(Vector3(22.0435, 22.95, -125.18)), "single declared initial22m placement is accepted")
	initial_stage_count = 1
	initial = main._native.get_player_position()
	stall_position = initial
	return result()

func physical_state() -> Dictionary:
	return {"tick": main._native.get_tick_index(), "position": main._native.get_player_position(),
		"velocity": main._native.get_player_linear_velocity(), "grounded": main._native.is_player_grounded(),
		"support": main._native.get_support_entity_id(), "balancing": main._native.is_player_balancing(),
		"firm": landing.get("checkpoint_footing_valid", false), "grip_available": main._native.is_grip_available(),
		"traversal": main._native.get_traversal_state(), "hand_support": main._native.get_traversal_support_entity_id(),
		"hands": landing.get("traversal_hand_constraint_count", 0), "gravity": landing.get("player_gravity_factor", 0),
		"deaths": main._native.get_death_count(), "hand_positive_work_j": landing.get("hand_actuator_positive_work_j", 0),
		"hand_absorbed_work_j": landing.get("hand_actuator_absorbed_work_j", 0),
		"foot_transfer_count": landing.get("foot_transfer_count", 0), "foot_transfer_tick": landing.get("foot_transfer_tick", 0),
		"foot_transfer_support": landing.get("foot_transfer_support_entity_id", 0),
		"shared_solve_tick": landing.get("hand_foot_last_solve_tick", 0),
		"shared_hand_load_n": landing.get("hand_foot_last_hand_load_n", 0), "shared_foot_load_n": landing.get("hand_foot_last_foot_load_n", 0)}

func advance() -> Dictionary:
	if not failures.is_empty() or phase >= 10:
		return result()
	match phase:
		0:
			step(24)
			if receiver_ticks >= 90:
				check(absf((main._native.get_player_position() as Vector3).y - 22.9) < .15, "declared initial22m footing settles on actual tower11 contact")
				check(radio_cues.has(&"service_iona_inspection_rail"), "actual22m footing plays original packaged inspection-rail direction")
				next_phase()
			check(phase != 0 or phase_ticks < 360, "initial22m tower support earns ninety firm native ticks")
		1:
			if walk_route(RAIL_APPROACH):
				step(27)
				rail_end = physical_state()
				check(bool(rail_end.grounded) and int(rail_end.support) == 2563 and bool(rail_end.balancing)
					and (rail_end.position as Vector3).y > 23 and rail_balancing_ticks > 90,
					"ordinary touch walking reaches the real narrow2563 rail end while balancing")
				next_phase()
		2:
			face_mesh()
			if absf(wrapf(main._yaw, -PI, PI)) < .002:
				step(27)
				ladder_unavailable_standing = not main._native.is_grip_available()
				check(ladder_unavailable_standing and bool(main._ctx.grounded), "ladder2566 remains out of reach while standing on rail")
				tap(&"jump")
				stick(Vector2(0, .3))
				next_phase()
		3:
			for index in 24:
				step()
				if int(main._native.get_traversal_state()) == 4:
					ladder_catch = physical_state()
					check(int(ladder_catch.hand_support) == 2566 and int(ladder_catch.hands) == 2
						and not bool(ladder_catch.grounded), "fresh touch Jump catches actual2566 ladder with two physical hands")
					hold_climb()
					next_phase()
					break
			check(phase != 3 or phase_ticks < 180, "rail jump reaches the actual hanging ladder")
		4:
			for index in 24:
				step()
				if standing_on(2565, 33.5):
					stick(Vector2.ZERO)
					davit_footing = physical_state()
					check(int(davit_footing.hands) == 0 and int(hand_ticks.get(2566, 0)) > 90,
						"held touch climb releases hands on actual33.3m davit2565 footing")
					next_phase()
					break
			check(phase != 4 or phase_ticks < 1800, "ladder ascent reaches real33m davit support")
		5:
			step(24)
			if phase_ticks >= 90:
				check(standing_on(2565, 33.5), "released touch retains actual davit footing")
				reset_walk()
				next_phase()
		6:
			if walk_route(DAVIT_EXIT):
				check(standing_on(11, 33.7), "ordinary touch walks back along davit and onto actual33m tower11")
				next_phase()
		7:
			step(24)
			if tower_firm_ticks >= 90:
				tower_footing = physical_state()
				check(absf((tower_footing.position as Vector3).y - 33.9) < .15 and int(tower_footing.hands) == 0,
					"actual33m tower receiver retains ninety firm ticks with hands released")
				check(radio_cues.has(&"service_iona_swing_stair"), "actual33m tower receiver plays the packaged stair and release-chain direction")
				reset_walk()
				next_phase()
			check(phase != 7 or phase_ticks < 360, "33m tower support earns continuous checkpoint footing")
		8:
			if walk_route([Vector3(10, 0, -125.3)]):
				check(standing_on(11, 33.7) and bool(landing.checkpoint_footing_valid)
					and (main._native.get_player_position() as Vector3).distance_to(tower_footing.position) > 1,
					"supported onward touch walking remains available on33m ring")
				next_phase()
		9:
			stick(Vector2.ZERO)
			check(ladder_unavailable_standing and not ladder_catch.is_empty() and not davit_footing.is_empty(), "rail-to-ladder route requires an actual jump and complete hand-to-foot transfer")
			check(gravity_intact and hud_hidden and int(main._native.get_death_count()) == 0 and initial_stage_count == 1,
				"inspection route retains gravity, hidden mission HUD and one uninterrupted staged-start life")
			check(main._story_radio.readback().missing_keys.is_empty(), "inspection route has no missing packaged recording")
			next_phase()
	return result()

func result() -> Dictionary:
	if main != null and not failures.is_empty() and blocker.is_empty():
		stick(Vector2.ZERO)
		blocker = physical_state()
		blocker["phase"] = phase
		blocker["waypoint"] = waypoint
		blocker["landing"] = landing.duplicate()
	return {"done": phase >= 10 or not failures.is_empty(), "checks": checks, "failures": failures,
		"phase": phase, "phase_ticks": phase_ticks, "waypoint": waypoint, "blocker": blocker,
		"native": true, "touch": true, "initial_22m_stage_only": true, "initial_stage_count": initial_stage_count,
		"position": main._native.get_player_position() if main != null else Vector3.ZERO,
		"tick": main._native.get_tick_index() if main != null else -1, "gravity_intact": gravity_intact,
		"mission_hud_hidden": hud_hidden, "rail_balancing_ticks": rail_balancing_ticks,
		"hand_ticks": hand_ticks, "support_ticks": support_ticks, "radio_cues": radio_cues.keys(),
		"rail_end": rail_end, "ladder_catch": ladder_catch, "davit_footing": davit_footing, "tower_footing": tower_footing,
		"viewport_3d_disabled": main.get_viewport().disable_3d if main != null else false}
