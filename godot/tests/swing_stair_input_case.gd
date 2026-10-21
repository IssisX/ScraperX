extends "res://tests/inspection_rail_input_case.gd"

# One declared33m start. Touch grabs/pulls/releases the real trip handle;
# native stair rotation and yielding receiver contact own the usable route.
const STAIR_APPROACH := [Vector3(-17.5, 0, -125.5), Vector3(-17.5, 0, -121.5)]
const TREADS := [Vector3(-16.4, 0, -122.1), Vector3(-15.2, 0, -122.1),
	Vector3(-14, 0, -122.1), Vector3(-12, 0, -122.1), Vector3(-10, 0, -122.1),
	Vector3(-8, 0, -122.1), Vector3(-5, 0, -122.1), Vector3(-2.7, 0, -122.1)]
var stair_body := -1
var pad_body := -1
var stair_angle := 0.0
var stair_angular_speed := 0.0
var armed_angle := 0.0
var receiver_front := 0.0
var peak_receiver_front := 0.0
var stable_receiver_ticks := 0
var stair_contact_ticks := 0
var firm_44m_ticks := 0
var exit_x := 0.0
var handle_approach: Dictionary = {}
var held_handle: Dictionary = {}
var pulled_handle: Dictionary = {}
var released_stair: Dictionary = {}
var settled_stair: Dictionary = {}
var loaded_stair: Dictionary = {}
var exit_contact: Dictionary = {}
var receiver_44m: Dictionary = {}

func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures.append(why)
		push_error("SWING_STAIR_INPUT " + why)

func stair_state() -> Dictionary:
	var transform: Transform3D = main._native.get_kit_body_transform(stair_body)
	var pad: Transform3D = main._native.get_kit_body_transform(pad_body)
	return {"tick": main._native.get_tick_index(), "angle_rad": atan2(transform.basis.x.y, transform.basis.x.x),
		"origin": transform.origin, "linear_velocity": main._native.get_kit_body_linear_velocity(stair_body),
		"receiver_front_m": 44.9 - .875 - pad.origin.y,
		"carrying": main._native.get_carrying_entity_id(), "carry_target": main._native.get_carry_target_entity_id(),
		"player": physical_state()}

func step(count: int = 1) -> void:
	for index in count:
		if not failures.is_empty():
			return
		super.step(1)
		var state := stair_state()
		var next_angle := float(state.angle_rad)
		stair_angular_speed = wrapf(next_angle - stair_angle, -PI, PI) / STEP
		stair_angle = next_angle
		receiver_front = float(state.receiver_front_m)
		peak_receiver_front = maxf(peak_receiver_front, receiver_front)
		if bool(main._ctx.grounded) and int(main._ctx.support_entity) == 2600:
			stair_contact_ticks += 1
			loaded_stair = state
		if phase == 4:
			if absf(stair_angle - armed_angle) > .05 and receiver_front > .05 and absf(stair_angular_speed) < .015:
				stable_receiver_ticks += 1
			else:
				stable_receiver_ticks = 0
		if phase == 10:
			if standing_on(11, 44.5) and bool(landing.checkpoint_footing_valid):
				firm_44m_ticks += 1
			else:
				firm_44m_ticks = 0

func begin(shipping_main: Node) -> Dictionary:
	main = shipping_main
	main.set_process(false)
	main._router.capture_mouse = false
	main._router.gameplay_active = true
	check(not main._ci_mode and main._uitest_scenario.is_empty(), "shipping Main owns the staged-start swing stair slice")
	check(main._native.debug_restart_at(Vector3(11.2, 33.95, -125.3)), "single declared initial33m placement is accepted")
	initial_stage_count = 1
	initial = main._native.get_player_position()
	stall_position = initial
	stair_body = main._native.get_kit_body_index(2600)
	pad_body = main._native.get_kit_body_index(2603)
	check(stair_body >= 0 and pad_body >= 0 and main._native.is_kit_body_dynamic(stair_body), "existing2600 stair and2603 receiver are real native bodies")
	if failures.is_empty():
		stair_angle = float(stair_state().angle_rad)
		armed_angle = stair_angle
	return result()

func face_plus_z() -> void:
	var error := wrapf(main._yaw - PI, -PI, PI)
	if absf(error) < .002:
		return
	var home: Vector2 = main.get_viewport().get_visible_rect().size * Vector2(.65, .35)
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

func walk_chunk(at: Vector3) -> bool:
	# Match the existing native route's ordinary support-relative walking law.
	# The precision rail helper remains unchanged; this stair has real risers.
	for index in 24:
		var position: Vector3 = main._native.get_player_position()
		var offset := Vector2(at.x - position.x, at.z - position.z)
		var velocity: Vector3 = main._native.get_player_linear_velocity() - main._native.get_support_point_linear_velocity()
		var relative := Vector2(velocity.x, velocity.z)
		# Native stair waypoints use0.1m; the handle approach stays precise.
		var tolerance := .1 if phase in [5, 7, 9, 11] else .05
		if offset.length() < tolerance and bool(main._ctx.grounded) and relative.length() < .15:
			stick(Vector2.ZERO)
			step(12)
			return true
		var move := offset * 1.8 - relative * .28
		move /= maxf(1.0, move.length())
		var forward := Vector2(-sin(main._yaw), -cos(main._yaw))
		var right := Vector2(cos(main._yaw), -sin(main._yaw))
		var value := Vector2(move.dot(right), move.dot(forward))
		stick(value)
		if not value.is_zero_approx():
			# Preserve full ordinary stick travel beyond the rail helper's cap.
			var drag := InputEventScreenDrag.new()
			drag.index = 42
			var pixels := value.normalized() * (.1 + .9 * value.length()) * 150.0 * float(main._touch._u)
			drag.position = main._touch.stick_home() + Vector2(pixels.x, -pixels.y)
			main._router._input(drag)
		step()
	return false

func advance() -> Dictionary:
	if not failures.is_empty() or phase >= 13:
		return result()
	match phase:
		0:
			step(24)
			if receiver_ticks >= 90:
				check(absf((main._native.get_player_position() as Vector3).y - 33.9) < .15, "initial33m stage settles on actual tower11 footing")
				check(radio_cues.has(&"service_iona_swing_stair"), "actual33m footing plays new packaged stair direction")
				next_phase()
			check(phase != 0 or phase_ticks < 360, "initial33m support earns ninety firm native ticks")
		1:
			if walk_route(STAIR_APPROACH):
				next_phase()
		2:
			face_plus_z()
			check(phase_ticks < 90, "right-half touch look turns toward the actual trip handle")
			if absf(wrapf(main._yaw - PI, -PI, PI)) < .002:
				step(45)
				handle_approach = {"before": physical_state()}
				# The native reach sphere owns acquisition; a horizontal waypoint
				# alone can leave the elevated handle a few centimetres too far.
				for approach_tick in 45:
					if main._native.get_carry_target_entity_id() == 2602:
						break
					check(bool(main._ctx.grounded) and bool(landing.checkpoint_footing_valid), "short handle approach retains actual firm footing")
					stick(Vector2(0, .15))
					step()
				stick(Vector2.ZERO)
				step(12)
				handle_approach["after"] = physical_state()
				check(main._native.get_carry_target_entity_id() == 2602 and main._ctx.action.id == &"pick_up", "actual trip handle2602 offers shipping touch GRAB")
				if not failures.is_empty():
					return result()
				tap(&"action")
				step(27)
				held_handle = stair_state()
				check(int(held_handle.carrying) == 2602 and main._ctx.action.id == &"set_down", "touch Action physically holds real handle and offers LET GO")
				if not failures.is_empty():
					return result()
				stick(Vector2(0, -.35))
				next_phase()
		3:
			step(18)
			if phase_ticks >= 54:
				stick(Vector2.ZERO)
				pulled_handle = stair_state()
				check(int(pulled_handle.carrying) == 2602, "ordinary0.6s backward touch pull retains the real handle")
				tap(&"action")
				step(5)
				released_stair = stair_state()
				check(int(released_stair.carrying) == 0, "shipping touch LET GO releases the physical trip handle")
				next_phase()
		4:
			step(24)
			if stable_receiver_ticks >= 90:
				settled_stair = stair_state()
				check(absf(stair_angle - armed_angle) > .05 and receiver_front > .05, "released stair rotates into its real yielding receiver")
				reset_walk()
				next_phase()
			check(phase != 4 or phase_ticks < 2700, "released stair earns stable native receiver contact within bounded physical travel")
		5:
			if walk_route(TREADS):
				check(stair_contact_ticks > 90 and not loaded_stair.is_empty(), "ordinary touch ascent loads actual stair2600 treads")
				stick(Vector2.ZERO)
				next_phase()
		6:
			step(27)
			var transform: Transform3D = main._native.get_kit_body_transform(stair_body)
			# Existing source route's final tread, transformed by actual rotation.
			var landing_x := .25 + 43 * .25 / tan(.70860367) + .45
			exit_x = transform.origin.x + landing_x * cos(stair_angle) - 11.3 * sin(stair_angle)
			reset_walk()
			next_phase()
		7:
			if walk_route([Vector3(exit_x, 0, -122.4)]):
				next_phase()
		8:
			step()
			if standing_on(2600, 44):
				exit_contact = stair_state()
				check(bool(main._ctx.grounded) and int(main._ctx.support_entity) == 2600, "final tread has actual departure contact before side-exit Jump")
				tap(&"jump")
				reset_walk()
				next_phase()
			check(phase != 8 or phase_ticks < 180, "side exit earns real final-tread contact")
		9:
			if walk_route([Vector3(exit_x, 0, -125.5)]):
				check(standing_on(11, 44.5), "fresh touch Jump and side movement reach actual44m tower11")
				next_phase()
		10:
			step(24)
			if firm_44m_ticks >= 90:
				receiver_44m = physical_state()
				check(absf((receiver_44m.position as Vector3).y - 44.9) < .15 and int(receiver_44m.hands) == 0, "44m arrival requires ninety firm tower ticks and released hands")
				check(radio_cues.has(&"freight_one_iona_crown"), "actual44m stair receiver plays the recorded campaign purpose")
				reset_walk()
				next_phase()
			check(phase != 10 or phase_ticks < 360, "actual44m tower receiver retains firm checkpoint footing")
		11:
			if walk_route([Vector3(exit_x + 1.5, 0, -125.5)]):
				check(standing_on(11, 44.5) and bool(landing.checkpoint_footing_valid)
					and (main._native.get_player_position() as Vector3).distance_to(receiver_44m.position) > 1,
					"supported onward touch walking works after44m stair exit")
				next_phase()
		12:
			stick(Vector2.ZERO)
			check(gravity_intact and hud_hidden and int(main._native.get_death_count()) == 0 and initial_stage_count == 1, "whole stair slice retains gravity and one declared staged-start life")
			check(main._story_radio.readback().missing_keys.is_empty(), "stair route has no missing packaged recording")
			next_phase()
	return result()

func result() -> Dictionary:
	if main != null and not failures.is_empty() and blocker.is_empty():
		stick(Vector2.ZERO)
		blocker = {"phase": phase, "waypoint": waypoint, "stair": stair_state() if stair_body >= 0 and pad_body >= 0 else {},
			"player": physical_state(), "landing": landing.duplicate()}
	return {"done": phase >= 13 or not failures.is_empty(), "checks": checks, "failures": failures,
		"phase": phase, "phase_ticks": phase_ticks, "waypoint": waypoint, "blocker": blocker,
		"native": true, "touch": true, "initial_33m_stage_only": true, "initial_stage_count": initial_stage_count,
		"tick": main._native.get_tick_index() if main != null else -1, "position": main._native.get_player_position() if main != null else Vector3.ZERO,
		"gravity_intact": gravity_intact, "mission_hud_hidden": hud_hidden, "stair_contact_ticks": stair_contact_ticks,
		"stable_receiver_ticks": stable_receiver_ticks, "firm_44m_ticks": firm_44m_ticks, "peak_receiver_front_m": peak_receiver_front,
		"handle_approach": handle_approach, "held_handle": held_handle, "pulled_handle": pulled_handle, "released_stair": released_stair, "settled_stair": settled_stair,
		"loaded_stair": loaded_stair, "exit_contact": exit_contact, "receiver_44m": receiver_44m,
		"radio_cues": radio_cues.keys(), "viewport_3d_disabled": main.get_viewport().disable_3d if main != null else false}
