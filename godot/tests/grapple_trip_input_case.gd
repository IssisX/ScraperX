extends "res://tests/mounted_winch_crossing_input_case.gd"

# One initial roof stage, then ordinary input only. No capture flag, injected
# anchor, player transform write or changed native physics is used in the trip.
const STEP := 1.0 / 90.0
var phase := "settle"
var phase_ticks := 0
var ticks := 0
var done := false
var walk_target := Vector3.ZERO
var next_phase := ""
var loaded_ticks := 0
var paid_before := 0.0
var initial_shots := 0
var before_exit := Vector3.ZERO

func begin(shipping_main: Node) -> Dictionary:
	main = shipping_main
	main.set_process(false)
	main._router.capture_mouse = false
	main._router.gameplay_active = true
	main._ctx = main._read_context()
	main._touch.update_context(main._ctx, 1.0)
	check(bool(main._native.get_grapple_gun_state().acquired), "gun starts carried without visiting high rack")
	check(bool(main._ctx.grapple_draw_available), "starting touch draw is offered")
	tap(&"grapple")
	step(2)
	check(bool(main._native.get_grapple_gun_state().equipped), "touch draws carried gun at ordinary spawn")
	tap(&"grapple")
	step(2)
	check(not bool(main._native.get_grapple_gun_state().equipped), "touch stows at spawn")
	check(main._native.debug_restart_at(Vector3(10, 485.15, -132.8)), "single roof start, no later relocation")
	transition("settle")
	return result()

func result() -> Dictionary:
	return {"checks": checks, "failures": failures, "done": done, "phase": phase, "ticks": ticks,
		"position": main._native.get_player_position() if main != null else Vector3.ZERO}

func transition(value: String) -> void:
	phase = value
	phase_ticks = 0

func body_point(entity: int, local: Vector3 = Vector3.ZERO) -> Vector3:
	var index: int = main._native.get_kit_body_index(entity)
	return main._native.get_kit_body_transform(index) * local

func walk(at: Vector3, following: String) -> void:
	walk_target = at
	next_phase = following
	transition("walk")

func move_world(value: Vector2) -> void:
	var forward := Vector2(-sin(main._yaw), -cos(main._yaw))
	var right := Vector2(cos(main._yaw), -sin(main._yaw))
	stick(Vector2(value.dot(right), value.dot(forward)))

func steer_to(at: Vector3) -> void:
	var position: Vector3 = main._native.get_player_position()
	var velocity: Vector3 = main._native.get_player_linear_velocity()
	var delta := Vector2(at.x - position.x, at.z - position.z)
	var value := delta * 1.8 - Vector2(velocity.x, velocity.z) * .28
	move_world(value.limit_length(1.0))

func step(count: int = 1) -> void:
	for i in count:
		var intent: Dictionary = main._router.frame(STEP)
		var forward := Vector2(-sin(main._yaw), -cos(main._yaw))
		var right := Vector2(cos(main._yaw), -sin(main._yaw))
		var movement: Vector2 = right * intent.move.x + forward * intent.move.y
		main._native.set_move_input(movement.x, movement.y)
		main._native.set_facing(forward.x, forward.y)
		main._native.set_grapple_aim(-sin(main._yaw) * cos(main._pitch), sin(main._pitch), -cos(main._yaw) * cos(main._pitch))
		main._dispatch(intent.verbs, STEP)
		main._deliver_tool_pendant_input(intent.pendant)
		var previous: int = main._native.get_tick_index()
		main._native.advance_frame(STEP)
		main._ctx = main._read_context()
		main._touch.update_context(main._ctx, 1.0)
		check(main._native.get_tick_index() == previous + 1, "one actual native tick")
		var landing: Dictionary = main._native.get_landing_state()
		check(main._native.get_death_count() == 0 and main._native.get_traversal_state() == 0 \
			and float(landing.player_gravity_factor) == 1.0 and main._native.get_player_position().is_finite(),
			"same life, ordinary gravity, no hand-assisted or transformed crossing")
		ticks += 1
		phase_ticks += 1

func shoot() -> void:
	var position: Vector3 = main._native.get_player_position()
	var target := body_point(1966)
	var delta := target - position
	var distance := Vector2(delta.x, delta.z).length()
	# Same physical launch model as the native public route: 2kg head,
	# 180J cartridge, reciprocal85kg recoil, finite muzzle/head offset.
	var speed := sqrt(360.0 / (1.0 / 85.0 + 1.0 / 2.0)) / 2.0
	var low := 1.1
	var high := 1.49
	for i in 48:
		var angle := (low + high) * .5
		var time := (distance - .95 * cos(angle)) / (speed * cos(angle))
		var height := .95 * sin(angle) + speed * sin(angle) * time - 4.905 * time * time
		if height > delta.y - .35:
			low = angle
		else:
			high = angle
	main._yaw = atan2(-delta.x, -delta.z)
	main._pitch = (low + high) * .5
	initial_shots = int(main._native.get_grapple_gun_state().cartridges_remaining)
	tap(&"action")
	transition("flight")

func advance(budget: int = 90) -> Dictionary:
	for i in budget:
		if done or not failures.is_empty():
			break
		var position: Vector3 = main._native.get_player_position()
		var velocity: Vector3 = main._native.get_player_linear_velocity()
		var gun: Dictionary = main._native.get_grapple_gun_state()
		if ticks > 0 and phase in ["flight", "lip", "takeup", "passive", "reel"]:
			check(bool(gun.line_attached), "physical rope survives " + phase + " at tick " + str(ticks))
			if not failures.is_empty():
				break
		if phase_ticks > 1500:
			check(false, "bounded phase did not complete: " + phase)
			break
		match phase:
			"settle":
				if phase_ticks >= 135:
					walk(Vector3(12, 485.15, -132.8), "bay")
			"walk":
				steer_to(walk_target)
				if main._native.is_player_grounded() and Vector2(position.x - walk_target.x, position.z - walk_target.z).length() < .1 \
					and Vector2(velocity.x, velocity.z).length() < .14:
					stick(Vector2.ZERO)
					transition(next_phase)
			"bay":
				walk(body_point(1965), "departure")
			"departure":
				if phase_ticks >= 90:
					check(main._native.get_support_entity_id() == 1965, "actual departure footing")
					tap(&"grapple")
					transition("draw")
			"draw":
				if phase_ticks >= 1:
					check(bool(gun.equipped), "touch draw acknowledged before shot")
					shoot()
			"flight":
				if phase_ticks >= 450:
					check(int(gun.cartridges_remaining) == initial_shots - 1, "shot spends exactly one finite cartridge")
					tap(&"action")
					transition("lip")
			"lip":
				var lip := body_point(1965, Vector3(.95, 0, 0))
				steer_to(lip)
				if Vector2(position.x - lip.x, position.z - lip.z).length() < .1 and Vector2(velocity.x, velocity.z).length() < .14:
					stick(Vector2.ZERO)
					check(not bool(gun.action_recovers_hook) and main._ctx.action.label == "REEL IN", "locked distant hook offers travel, not recovery")
					tap(&"action")
					transition("takeup")
			"takeup":
				if float(gun.paid_out_length) <= (gun.rope_end as Vector3).distance_to(gun.rope_start) and float(gun.tension_n) >= 300.0:
					check(bool(gun.auto_reel) and main._ctx.action.label == "STOP REEL", "touch automatic takeup and truthful stop label")
					tap(&"action")
					move_world(Vector2(.1, 0))
					transition("passive")
			"passive":
				if not main._native.is_player_grounded():
					stick(Vector2.ZERO)
					loaded_ticks = loaded_ticks + 1 if float(gun.tension_n) > 250.0 else 0
				if loaded_ticks >= 12:
					check(not bool(gun.auto_reel), "real rope holds airborne player with reel stopped")
					paid_before = float(gun.reel_work_j)
					loaded_ticks = 0
					tap(&"action")
					transition("reel")
			"reel":
				steer_to(body_point(1968))
				if not main._native.is_player_grounded() and float(gun.tension_n) > 250.0:
					loaded_ticks += 1
				var receiver := body_point(1968, Vector3(0, .9, 0))
				var rise := position.y - receiver.y
				var discriminant := velocity.y * velocity.y + 19.62 * rise
				if rise > 0.0 and discriminant >= 0.0 and loaded_ticks >= 12 and float(gun.reel_work_j) - paid_before > 100.0:
					var flight := (velocity.y + sqrt(discriminant)) / 9.81
					var body: int = main._native.get_kit_body_index(1968)
					var frame: Transform3D = main._native.get_kit_body_transform(body)
					var predicted := frame.affine_inverse() * (position + velocity * flight + Vector3(0, -4.905 * flight * flight, 0))
					if flight > .05 and predicted.x > -1.0 and predicted.x < 1.0 and predicted.z > -1.0 and predicted.z < 1.6 and velocity.x > .25:
						tap(&"drop")
						transition("landing")
			"landing":
				steer_to(body_point(1968))
				if main._native.is_player_grounded() and main._native.get_support_entity_id() == 1968:
					check(not bool(gun.line_attached), "touch Drop removes rope before real receiver contact")
					walk(body_point(1968), "receiver")
			"receiver":
				if phase_ticks >= 90:
					check(main._native.is_player_grounded() and main._native.get_support_entity_id() == 1968, "stable receiver footing for90ticks")
					before_exit = position
					walk(Vector3(20, 485.15, -132.8), "exit")
			"exit":
				if phase_ticks >= 90:
					check(main._native.is_player_grounded() and main._native.get_support_entity_id() == 51 and position.distance_to(before_exit) > 2.0,
						"onward walking reaches existing roof after real grapple trip")
					done = true
					break
		step()
	return result()
