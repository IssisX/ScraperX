extends "res://tests/yard_worker_input_case.gd"

# One uninterrupted shipping life, ordinary touch input and native contacts.
# The caller may disable viewport 3D for software-rendered interaction checks;
# this fixture never disables physics or replaces Main's processing path.
const GRADE := [Vector3(-94, 0, -75), Vector3(-69, 0, -106.5),
	Vector3(20, 0, -106.5), Vector3(20, 0, -114)]
const RING := [Vector3(18.5, 0, -120.5), Vector3(18.5, 0, -126)]
var waypoint := 0
var cues: Dictionary = {}
var radio_cues: Dictionary = {}
var story: Dictionary = {}
var landing: Dictionary = {}
var transfer_pressed := false
var gravity_intact := true
var hud_hidden := true
var suspended_hand_ticks := 0
var receiver_ticks := 0
var stall_ticks := 0
var stall_position := Vector3.ZERO
var ring_position := Vector3.ZERO
var blocker: Dictionary = {}

func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures.append(why)
		push_error("OPENING_JOURNEY " + why)

func step(count: int = 1) -> void:
	for index in count:
		if not failures.is_empty():
			return
		main._process(STEP)
		phase_ticks += 1
		landing = main._native.get_landing_state()
		if phase == 4:
			if bool(main._ctx.grounded) and bool(landing.checkpoint_footing_valid) and int(main._ctx.support_entity) == 2952:
				receiver_ticks += 1
			else:
				receiver_ticks = 0
		gravity_intact = gravity_intact and float(landing.player_gravity_factor) == 1.0
		hud_hidden = hud_hidden and not main._story_presenter._objective.visible
		story = main._story_guidance.sample(main._ctx, int(main._native.get_tick_index()))
		cues[story.get("transmission_key", &"")] = true
		var radio: Dictionary = main._story_radio.readback()
		if bool(radio.playing):
			radio_cues[radio.active_key] = true
		if not bool(main._ctx.grounded) and int(landing.traversal_hand_constraint_count) == 2:
			suspended_hand_ticks += 1
		if not gravity_intact or int(main._native.get_death_count()) != 0:
			check(false, "continuous life retains gravity and never dies or restarts")
			return

func begin(shipping_main: Node) -> Dictionary:
	main = shipping_main
	main.set_process(false)
	main._router.capture_mouse = false
	main._router.gameplay_active = true
	initial = main._native.get_player_position()
	check(initial.distance_to(Vector3(-94, 1.2, -44)) < .01,
		"ordinary FreightYardArrival begins the complete journey")
	check(not main._ci_mode and main._uitest_scenario.is_empty(), "actual shipping Main owns the journey")
	stall_position = initial
	return result()

func face_mesh() -> void:
	# Send the same screen drag consumed by shipping look; never write facing.
	var error := wrapf(main._yaw, -PI, PI)
	if absf(error) < .002:
		return
	var home: Vector2 = main.get_viewport().get_visible_rect().size * Vector2(.45, .4)
	var touch := InputEventScreenTouch.new()
	touch.index = 43
	touch.position = home
	touch.pressed = true
	main._router._input(touch)
	var drag := InputEventScreenDrag.new()
	drag.index = 43
	drag.relative = Vector2(clampf(error, -.4, .4) / (main._router.LOOK_RADIANS_PER_UNIT * main._router.look_sensitivity), 0)
	drag.position = home + drag.relative
	main._router._input(drag)
	touch.pressed = false
	main._router._input(touch)
	step()

func hold_climb() -> void:
	# Walking helper caps speed for footing; a climb uses full ordinary stick.
	stick(Vector2(0, .55))
	var drag := InputEventScreenDrag.new()
	drag.index = 42
	drag.position = main._touch.stick_home() - Vector2(0, 150.0 * float(main._touch._u))
	main._router._input(drag)

func walk_route(points: Array) -> bool:
	if walk_chunk(points[waypoint]):
		waypoint += 1
		stall_ticks = 0
		stall_position = main._native.get_player_position()
		return waypoint == points.size()
	stall_ticks += 24
	if stall_ticks >= 180:
		var position: Vector3 = main._native.get_player_position()
		check(position.distance_to(stall_position) > .1,
			"open route makes physical progress toward " + str(points[waypoint]))
		stall_ticks = 0
		stall_position = position
	check(phase_ticks < 20000, "touch route reaches its bounded waypoint budget")
	return false

func advance() -> Dictionary:
	if not failures.is_empty() or phase >= 8:
		return result()
	match phase:
		0:
			step(30)
			if phase_ticks >= 90:
				check(bool(main._ctx.grounded) and bool(landing.checkpoint_footing_valid), "arrival settles on actual grade")
				check(cues.has(&"arrival_iona_empty_record") and radio_cues.has(&"arrival_iona_empty_record"), "ordinary worker arrival starts its packaged radio")
				next_phase()
		1:
			if walk_route(GRADE):
				check(bool(main._ctx.grounded), "continuous arrival approach reaches actual mesh grade")
				check(cues.has(&"arrival_iona_personnel_way"), "actual approach earns personnel-way guidance")
				next_phase()
		2:
			face_mesh()
			if absf(wrapf(main._yaw, -PI, PI)) < .002:
				stick(Vector2(0, .25))
				for index in 24:
					if main._native.is_grip_available():
						stick(Vector2.ZERO)
						check(bool(main._ctx.grounded), "mesh grip is reached from real grade")
						tap(&"action")
						step()
						check(int(main._native.get_traversal_state()) == 4 and int(main._native.get_traversal_support_entity_id()) == 1953
							and int(landing.traversal_hand_constraint_count) == 2, "touch Action takes mesh1953 with two physical hands")
						hold_climb()
						next_phase()
						break
					step()
				check(phase != 2 or phase_ticks < 420, "grade approach offers reachable cargo mesh")
		3:
			for index in 24:
				if not transfer_pressed and int(main._native.get_traversal_support_entity_id()) == 2952 and int(main._native.get_traversal_state()) == 4:
					tap(&"action")
					transfer_pressed = true
				step()
				if bool(main._ctx.grounded) and (main._native.get_player_position() as Vector3).y > 11.7:
					stick(Vector2.ZERO)
					check(transfer_pressed and suspended_hand_ticks > 90, "real hand climb reaches fresh-Action receiving transfer")
					next_phase()
					break
			check(phase != 3 or phase_ticks < 2000, "held touch climb reaches the real11m receiver")
		4:
			step(24)
			var position: Vector3 = main._native.get_player_position()
			if receiver_ticks >= 90:
				check(position.x >= 17.5 and position.x <= 22.5 and position.z >= -120.8 and position.z <= -118 and absf(position.y - 11.9) < .2,
					"released hands stand firmly on actual2952 receiver at11m")
				check(int(landing.traversal_hand_constraint_count) == 0, "firm receiver footing releases physical hands")
				check(cues.has(&"arrival_iona_mesh_lip"), "actual suspended climb earns mesh-lip guidance")
				waypoint = 0
				stall_position = position
				next_phase()
			check(phase != 4 or phase_ticks < 360, "actual receiver retains ninety firm ticks")
		5:
			if walk_route(RING):
				check(bool(main._ctx.grounded) and bool(landing.checkpoint_footing_valid) and int(main._ctx.support_entity) == 11,
					"touch walking connects receiver to actual first tower ring11")
				check(cues.has(&"arrival_iona_first_ring") and radio_cues.has(&"arrival_iona_first_ring"), "real first-ring arrival plays its packaged radio")
				check(story.objective_id == &"crown" and String(story.detail).contains("+22 M"), "firm first ring points onward to existing22m service route")
				ring_position = main._native.get_player_position()
				waypoint = 0
				next_phase()
		6:
			if walk_route([Vector3(17, 0, -126)]):
				check((main._native.get_player_position() as Vector3).distance_to(ring_position) > 1 and bool(main._ctx.grounded)
					and int(main._ctx.support_entity) == 11, "ordinary onward touch movement works after first-ring radio")
				next_phase()
		7:
			stick(Vector2.ZERO)
			check(gravity_intact and hud_hidden and not bool(story.objective_visible), "whole journey keeps gravity and hides permanent mission HUD")
			check(main._story_radio.readback().missing_keys.is_empty(), "journey has no missing packaged recording")
			check(int(main._native.get_death_count()) == 0, "whole continuous journey has no death or hidden recovery")
			next_phase()
	return result()

func result() -> Dictionary:
	if main != null and not failures.is_empty() and blocker.is_empty():
		stick(Vector2.ZERO)
		blocker = {"phase": phase, "waypoint": waypoint, "position": main._native.get_player_position(),
			"velocity": main._native.get_player_linear_velocity(), "grip_available": main._native.is_grip_available(),
			"grip_point": main._native.get_grip_point(), "support": main._native.get_support_entity_id(), "traversal": main._native.get_traversal_state(),
			"traversal_support": main._native.get_traversal_support_entity_id(), "landing": landing.duplicate()}
	return {"done": phase >= 8 or not failures.is_empty(), "checks": checks, "failures": failures,
		"phase": phase, "phase_ticks": phase_ticks, "waypoint": waypoint, "blocker": blocker,
		"native": true, "touch": true, "staged": false, "position": main._native.get_player_position() if main != null else Vector3.ZERO,
		"tick": main._native.get_tick_index() if main != null else -1, "gravity_intact": gravity_intact,
		"mission_hud_hidden": hud_hidden, "cues": cues.keys(), "radio_cues": radio_cues.keys(),
		"viewport_3d_disabled": main.get_viewport().disable_3d if main != null else false}
