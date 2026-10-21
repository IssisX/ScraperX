extends "res://tests/grapple_toolbelt_input_case.gd"

# Normal shipping arrival, touch events and Main processing. No actor/player
# placement or contact/activity injection is used in this complete interaction.
var phase := 0
var phase_ticks := 0
var initial := Vector3.ZERO
var saw_attention := false
var saw_point := false
var saw_yield := false
var onward_corner := false
var last_state: Dictionary = {}

func step(count: int = 1) -> void:
	for index in count:
		main._process(STEP)
		phase_ticks += 1
		last_state = main._native.get_yard_worker_state()
		saw_attention = saw_attention or int(last_state.activity) == 1
		saw_point = saw_point or int(last_state.activity) == 2
		saw_yield = saw_yield or int(last_state.activity) == 4

func begin(shipping_main: Node) -> Dictionary:
	main = shipping_main
	main.set_process(false)
	main._router.capture_mouse = false
	main._router.gameplay_active = true
	initial = main._native.get_player_position()
	check(initial.distance_to(Vector3(-94,1.2,-44)) < 0.01, "ordinary shipping arrival is unchanged")
	return result()

func walk_chunk(at: Vector3) -> bool:
	for index in 24:
		var position: Vector3 = main._native.get_player_position()
		var offset := Vector2(at.x-position.x, at.z-position.z)
		if offset.length() < 0.16:
			stick(Vector2.ZERO)
			step(12)
			return true
		var forward := Vector2(-sin(main._yaw),-cos(main._yaw))
		var right := Vector2(cos(main._yaw),-sin(main._yaw))
		var move := offset.normalized()*minf(0.28,offset.length()*0.6)
		stick(Vector2(move.dot(right),move.dot(forward)))
		step()
	return false

func next_phase() -> void:
	phase += 1
	phase_ticks = 0

func advance() -> Dictionary:
	match phase:
		0:
			step(30)
			if phase_ticks >= 90:
				check(bool(last_state.present) and bool(last_state.supported) and int(last_state.activity) == 0,
					"real worker settles into supported welding at arrival")
				check(main._yard_worker_view != null and main._yard_worker_view.visible,
					"shipping Main presents the imported worker")
				check(main._yard_worker_view.welding_active() and main._yard_worker_audio._arc.playing,
					"actual work pose starts its spatial arc and sound together")
				check(main._yard_worker_audio._arc.global_position.distance_to(last_state.work_point) < 0.001,
					"welding sound stays at the bench seam")
				next_phase()
		1:
			if walk_chunk(Vector3(-93.15,0,-45.6)):
				check(saw_attention, "touch approach makes native worker notice the player")
				next_phase()
			elif phase_ticks > 360:
				check(false, "touch approach reaches the worker without staging")
		2:
			step(12)
			if int(last_state.activity) == 2:
				check(saw_point and last_state.look_target == last_state.guide_point,
					"native pointing directs the player toward the crane yard")
				check(not main._yard_worker_view.welding_active() and not main._yard_worker_audio._arc.playing,
					"looking up and pointing stop the electrode effect and sound")
				main._open_pause()
				check(not main._yard_worker_audio._arc.playing and not main._yard_worker_audio._step.playing,
					"actual pause immediately silences the worker")
				main._resume()
				main.set_process(false)
				next_phase()
			elif phase_ticks > 270:
				check(false, "noticed player receives a finite pointing gesture")
		3:
			if not onward_corner:
				onward_corner = walk_chunk(Vector3(-99,0,-45.6))
			elif walk_chunk(Vector3(-99,0,-43.5)):
				check(main._native.get_player_position().distance_to(initial) > 4.5,
					"player continues toward the yard through ordinary touch movement")
				next_phase()
			elif phase_ticks > 600:
				check(false, "worker interaction leaves a usable onward route")
		4:
			step(30)
			if int(last_state.activity) == 0 and bool(last_state.supported):
				check(main._yard_worker_view.welding_active() and main._yard_worker_audio._arc.playing,
					"worker returns to supported work and audible arc after player passes")
				check(main._native.get_death_count() == 0, "entire normal touch encounter uses no hidden recovery")
				stick(Vector2.ZERO)
				next_phase()
			elif phase_ticks > 630:
				check(false, "worker resumes work after the finite greeting")
	return result()

func result() -> Dictionary:
	return {"done": phase >= 5, "checks": checks, "failures": failures, "phase": phase,
		"native": true, "touch": true, "staged": false, "state": last_state}
