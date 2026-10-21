extends RefCounted

# Staged integration only. The native public-route test separately proves the
# normal approach, receiver, same-life miss/retry and second-structure reuse.
var main: Node
var checks := 0
var failures: Array[String] = []

func check(value: bool, message: String) -> void:
	checks += 1
	if not value:
		failures.append(message)
		push_error("PORTABLE_LINE_NATIVE_INPUT " + message)

func step(count: int = 1) -> void:
	for i in count:
		var intent: Dictionary = main._router.frame(1.0 / 90.0)
		main._dispatch(intent.verbs, 1.0 / 90.0)
		main._native.set_traversal_line_reel_input(intent.pendant.y)
		main._native.advance_frame(1.0 / 90.0)
		main._ctx = main._read_context()
		main._touch.update_context(main._ctx, 1.0)
		main._portable_line_view.update_state(main._native.get_traversal_line_render_state(), main._audio)

func tap(control: StringName) -> void:
	if control in [&"grapple", &"toggle_traversal_line", &"toggle_rigging"] and not main._touch.is_button_shown(control):
		tap(&"tools")
	check(main._touch.is_button_shown(control), "touch target visible: " + str(control))
	for down in [true, false]:
		var event := InputEventScreenTouch.new()
		event.index = 41
		event.position = main._touch.button_center(control)
		event.pressed = down
		main._router._input(event)

func run(shipping_main: Node) -> Dictionary:
	main = shipping_main
	var native: Object = main._native
	main.set_process(false)
	main._router.capture_mouse = false
	main._router.gameplay_active = true
	step()
	tap(&"toggle_traversal_line")
	step()
	check(bool(native.get_traversal_line_state().equipped), "actual touch equips carried line")
	check(main._portable_line_view._carried.visible, "available stock displays carried spool")
	# Actual moving rail, with no manufactured grip/anchor or force.
	check(native.debug_restart_at(Vector3(-50, 2, -59)), "clear staged integration position")
	native.request_traversal_line(0, 0)
	var rail: Transform3D = native.get_kit_body_transform(native.get_kit_body_index(2480))
	var target: Vector3 = rail * Vector3(0, -1.2, 0)
	var direction: Vector3 = target - native.get_player_position() - Vector3(0, 0.6, 0)
	check(native.set_rigging_aim(direction.x, direction.y, direction.z), "actual ordinary rail aim")
	step()
	check(bool(native.get_traversal_line_state().target_valid), "native preview admits actual broad rail")
	tap(&"action")
	step()
	var line: Dictionary = native.get_traversal_line_state()
	check(bool(line.installed) and bool(line.gripping), "shipping Action installs and grips actual native fitting")
	check(int(line.available_spools) == 0, "installation consumes single carried stock")
	check(not main._portable_line_view._carried.visible, "installed hardware removes carried duplicate")
	step(30)
	line = native.get_traversal_line_state()
	var landing: Dictionary = native.get_landing_state()
	check(int(native.get_traversal_state()) == 6 and int(landing.traversal_hand_constraint_count) == 2,
		"line swing uses existing two physical hands")
	check(float(landing.player_gravity_factor) == 1.0 and float(line.tension_n) > 100.0,
		"real gravity and measured load support player")
	check(bool(line.path_valid) and line.path_points.size() >= 2 and float(line.path_length_m) > 0.0,
		"actual cable geometry reaches native bridge")
	check(main._portable_line_view._line.get_surface_count() == 1, "view draws authoritative native route")
	check(main._touch.is_button_shown(&"pendant_up") and main._touch.is_button_shown(&"drop"),
		"held line exposes touch reel and ordinary Drop")
	tap(&"drop")
	step()
	line = native.get_traversal_line_state()
	check(bool(line.installed) and not bool(line.gripping) and int(native.get_landing_state().traversal_hand_constraint_count) == 0,
		"ordinary touch Drop removes hands and preserves installed line")
	tap(&"toggle_traversal_line")
	step()
	line = native.get_traversal_line_state()
	check(not bool(line.equipped) and bool(line.installed), "touch stow retains physical world installation")
	return {"checks": checks, "failures": failures, "fixture_start_only": true,
		"native_tick": native.get_tick_index(), "installed": line.installed}
