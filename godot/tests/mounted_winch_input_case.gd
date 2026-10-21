extends RefCounted

# One staged start beside the winch. The native public route separately
# exercises normal arrival, leaving range, returning and onward movement.
var main: Node
var checks := 0
var failures: Array[String] = []

func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures.append(why)
		push_error("MOUNTED_WINCH_INPUT " + why)

func step(count: int = 1) -> void:
	for i in count:
		var intent: Dictionary = main._router.frame(1.0 / 90.0)
		main._native.set_rigging_aim(-sin(main._yaw) * cos(main._pitch), sin(main._pitch), -cos(main._yaw) * cos(main._pitch))
		main._dispatch(intent.verbs, 1.0 / 90.0)
		main._deliver_tool_pendant_input(intent.pendant)
		main._native.advance_frame(1.0 / 90.0)
		main._ctx = main._read_context()
		main._router.set_rigging_mode(bool(main._ctx.rigging_equipped))
		main._touch.update_context(main._ctx, 1.0)
		main._rigging_view.update_state(main._native.get_rigging_state(), main._audio)

func finger(control: StringName, pressed: bool, canceled: bool = false) -> void:
	var event := InputEventScreenTouch.new()
	event.index = 41
	event.position = main._touch.button_center(control)
	event.pressed = pressed
	event.canceled = canceled
	main._router._input(event)

func tap(control: StringName) -> void:
	if control in [&"grapple", &"toggle_traversal_line", &"toggle_rigging"] and not main._touch.is_button_shown(control):
		tap(&"tools")
	check(main._touch.is_button_shown(control), "visible touch control " + str(control))
	finger(control, true)
	finger(control, false)

func aim(at: Vector3) -> void:
	var direction: Vector3 = at - main._native.get_player_position() - Vector3(0, .62, 0)
	main._yaw = atan2(-direction.x, -direction.z)
	main._pitch = atan2(direction.y, Vector2(direction.x, direction.z).length())
	step(3)

func beam_eye() -> Vector3:
	var body: int = main._native.get_kit_body_index(2499)
	var transform: Transform3D = main._native.get_kit_body_transform(body)
	return transform * Vector3(-1.8, .17, 0)

func run(shipping_main: Node) -> Dictionary:
	main = shipping_main
	main.set_process(false)
	main._router.capture_mouse = false
	main._router.gameplay_active = true
	check(main._native.debug_restart_at(Vector3(-109.1, .9, -36.85)), "single start beside normal yard bay")
	step(10)
	tap(&"toggle_rigging")
	step()
	aim(Vector3(-109.8, 1.08, -37.33))
	check(int(main._native.get_rigging_state().target_kind) == 2 and int(main._native.get_rigging_state().target) != 0, "native mounted source offered")
	check(main._ctx.action.label == "CLIP WINCH", "source has useful installation label")
	tap(&"action")
	step()
	aim(beam_eye())
	tap(&"action")
	step(45)
	var state: Dictionary = main._native.get_rigging_state()
	check(bool(state.power_operable) and int(state.installed_count) == 1, "two touch clips install real powered cable")
	check(main._touch.is_button_shown(&"pendant_up") and main._touch.is_button_shown(&"pendant_down"), "loaded winch exposes lift and lower")
	check(not main._touch._movement_blocked, "winch controls preserve walking and looking")
	var initial_y := beam_eye().y
	var initial_energy := float(state.energy_j)
	finger(&"pendant_up", true)
	step(270)
	check(main._rigging_view._winch.playing, "native powered operation schedules spatial gearbox loop")
	check(main._rigging_view._winch.global_position.distance_to(Vector3(-109.8, 3.70, -36)) < .01, "motor sound belongs to actual source")
	finger(&"pendant_up", false)
	step(90)
	state = main._native.get_rigging_state()
	var raised_y := beam_eye().y
	check(raised_y > initial_y + .4 and float(state.motor_work_j) > 0.0 and float(state.energy_j) < initial_energy, "touch lifts loose beam through finite work")
	check(bool(state.braking) and is_zero_approx(float(state.target_speed_mps)), "release holds native brake")
	check(not main._rigging_view._winch.playing, "brake hold stops motor sound")
	tap(&"rigging_unclip")
	step()
	check(int(main._native.get_rigging_state().failure) == 8 and int(main._native.get_rigging_state().installed_count) == 1, "loaded touch unclip rejects safely")
	tap(&"toggle_rigging")
	step(60)
	check(int(main._native.get_rigging_state().installed_count) == 1 and absf(beam_eye().y-raised_y) < .08, "stow keeps installed loaded cable")
	tap(&"toggle_rigging")
	step()
	finger(&"pendant_down", true)
	step(270)
	finger(&"pendant_down", false, true)
	step(90)
	check(beam_eye().y < raised_y - .3, "touch payout lowers actual beam")
	check(bool(main._native.get_rigging_state().braking) and is_zero_approx(float(main._native.get_rigging_state().target_speed_mps)), "canceled finger stops payout")
	check(main._station_panel().title == "MOUNTED WINCH", "native power readback selects winch panel")
	return {"checks": checks, "failures": failures, "fixture_start_only": true}
