extends RefCounted

# One staged start on existing yard geometry. The native public route owns
# arrival, airborne crossing, receiving footing, miss/retry and onward proof.
var main: Node
var checks := 0
var failures: Array[String] = []
var stick_down := false

func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures.append(why)
		push_error("PORTABLE_CROSSING_INPUT " + why)

func step(count: int = 1) -> void:
	for i in count:
		var intent: Dictionary = main._router.frame(1.0 / 90.0)
		# Same unchanged yaw-zero movement mapping used by shipping Main.
		main._native.set_move_input(intent.move.x, -intent.move.y)
		main._dispatch(intent.verbs, 1.0 / 90.0)
		main._native.set_traversal_line_reel_input(intent.pendant.y)
		main._native.advance_frame(1.0 / 90.0)
		main._ctx = main._read_context()
		main._touch.update_context(main._ctx, 1.0)
		main._portable_line_view.update_state(main._native.get_traversal_line_render_state(), main._audio)

func finger(at: Vector2, down: bool, index: int = 41) -> void:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = at
	event.pressed = down
	main._router._input(event)

func tap(control: StringName) -> void:
	if control in [&"grapple", &"toggle_traversal_line", &"toggle_rigging"] and not main._touch.is_button_shown(control):
		tap(&"tools")
	check(main._touch.is_button_shown(control), "visible touch control: " + str(control))
	finger(main._touch.button_center(control), true)
	finger(main._touch.button_center(control), false)

func stick(value: float) -> void:
	if is_zero_approx(value):
		if stick_down:
			finger(main._touch.stick_home(), false, 42)
		stick_down = false
		return
	if not stick_down:
		finger(main._touch.stick_home(), true, 42)
		stick_down = true
	var drag := InputEventScreenDrag.new()
	drag.index = 42
	drag.position = main._touch.stick_home() + Vector2(signf(value) * (0.1 + 0.9 * absf(value)) * 150.0 * main._touch._u, 0)
	main._router._input(drag)

func aim(at: Vector3) -> void:
	var direction: Vector3 = at - main._native.get_player_position() - Vector3(0, 0.6, 0)
	check(main._native.set_rigging_aim(direction.x, direction.y, direction.z), "ordinary aim delivered")
	step(3)

func run(shipping_main: Node) -> Dictionary:
	main = shipping_main
	main.set_process(false)
	main._router.capture_mouse = false
	main._router.gameplay_active = true
	main._operating = &""
	check(main._native.debug_restart_at(Vector3(-89.5, 0.9, -46.6)), "single staged start beside existing trestle")
	step(2)
	check(not bool(main._native.get_traversal_line_state().installed), "fresh game has carried stock")
	if not failures.is_empty():
		return {"checks": checks, "failures": failures, "fixture_start_only": true}
	tap(&"toggle_traversal_line")
	step()
	aim(Vector3(-89.5, 2.3, -45.15))
	tap(&"action")
	step(30)
	check(bool(main._native.get_traversal_line_state().gripping), "first real end gripped")
	finger(main._touch.button_center(&"pendant_down"), true)
	step(90)
	finger(main._touch.button_center(&"pendant_down"), false)
	for i in 240:
		var distance: float = -88.55 - main._native.get_player_position().x
		if absf(distance) < 0.06:
			break
		stick(clampf(distance, -0.3, 0.3))
		step()
	stick(0)
	step(30)
	aim(Vector3(-88.55, 2.3, -45.15))
	check(bool(main._native.get_traversal_line_state().can_pin_end), "actual unloaded second clamp admitted")
	check(main._ctx.action.label == "CLAMP OTHER END", "context names second-end operation")
	if not failures.is_empty():
		return {"checks": checks, "failures": failures, "line": main._native.get_traversal_line_state(), "action": main._ctx.action}
	tap(&"action")
	step(2)
	var line: Dictionary = main._native.get_traversal_line_state()
	check(bool(line.two_ended) and bool(line.path_valid) and not bool(line.gripping), "touch installs both real ends and releases carried fitting")
	tap(&"action")
	step(2)
	check(bool(main._native.get_traversal_line_state().gripping), "touch grabs material span")
	check(main._touch.button_label(&"jump") == "LET GO", "jump tells player it releases a line grip")
	check(not main._touch.is_button_shown(&"pendant_up") and not main._touch.is_button_shown(&"pendant_down"), "fixed crossing hides reel buttons")
	check(not main._portable_line_view._fitting.visible and main._portable_line_view._line.get_surface_count() == 1, "crossing draws actual line without floating handle")
	var start: Vector3 = main._native.get_player_position()
	stick(-0.5)
	step(120)
	stick(0)
	step()
	line = main._native.get_traversal_line_state()
	check(int(line.regrip_count) >= 1, "touch stick causes actual native regrip")
	check(main._native.get_player_position().x < start.x - 0.1, "touch stick moves player along material line")
	tap(&"drop")
	step()
	line = main._native.get_traversal_line_state()
	check(bool(line.two_ended) and not bool(line.gripping), "touch Drop retains two-anchor installation")
	return {"checks": checks, "failures": failures, "fixture_start_only": true,
		"regrips": int(line.regrip_count), "native_tick": main._native.get_tick_index()}
