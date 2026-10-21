extends SceneTree

const TouchControls := preload("res://presentation/ui/touch_controls.gd")
const InputRouter := preload("res://presentation/ui/input_router.gd")
var failures: Array[String] = []
var controls: Control
var router: Node

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, message: String) -> void:
	if not ok:
		failures.append(message)
		push_error("TOOLBELT_TOUCH FAIL " + message)

func context() -> Dictionary:
	return {"hanging": false, "jump_ok": true, "action": {"id": &"climb", "icon": &"climb", "label": "CLIMB"},
		"chute_ok": true, "chute": false, "danger": 0.0, "operating": &"", "slung": false,
		"crouched": false, "grapple_draw_available": true, "grapple_equipped": false,
		"traversal_line_available": true, "rigging_available": true}

func event(id: StringName, pressed: bool, cancelled: bool = false) -> void:
	var input := InputEventScreenTouch.new()
	input.index = 20
	input.position = controls.button_center(id)
	input.pressed = pressed
	input.canceled = cancelled
	router._input(input)

func tap(id: StringName) -> Array:
	event(id, true)
	event(id, false)
	return router.frame(0.016)["verbs"]

func open_tools() -> void:
	if not controls._tool_tray_open:
		check(tap(&"tools").is_empty(), "selector emits no native verb")

func _run() -> void:
	root.size = Vector2i(2160, 1856)
	controls = TouchControls.new()
	router = InputRouter.new()
	root.add_child(controls)
	root.add_child(router)
	router.capture_mouse = false
	router.touch = controls
	controls.router = router
	var ctx := context()
	controls.update_context(ctx, 1.0)
	if "--prototype" in OS.get_cmdline_user_args():
		await _prototype()
		return
	var action_home: Vector2 = controls.button_center(&"action")
	check(controls.is_button_shown(&"tools") and not controls.is_button_shown(&"grapple"), "compact selector replaces scattered equipment")
	for entry in [[&"grapple", &"toggle_grapple"], [&"toggle_traversal_line", &"toggle_traversal_line"], [&"toggle_rigging", &"toggle_rigging"]]:
		open_tools()
		check(controls.is_button_shown(entry[0]), "named choice visible")
		check(tap(entry[0]) == [entry[1]], "choice delivers existing native verb")
		check(not controls._tool_tray_open and controls._tool_name == "NONE", "selection closes and waits for acknowledgement")
	ctx["operating"] = &"grappling_gun"
	ctx["grapple_equipped"] = true
	ctx["grapple_line_attached"] = false
	controls.update_context(ctx, 1.0)
	check(not controls.is_button_shown(&"pendant_up") and not controls.is_button_shown(&"pendant_down"), "no reel controls before a line exists")
	ctx["grapple_line_attached"] = true
	ctx["grapple_state"] = 1
	controls.update_context(ctx, 1.0)
	check(not controls.is_button_shown(&"pendant_up") and not controls.is_button_shown(&"pendant_down"), "flying free-spool hook offers Lock Line before reel controls")
	ctx["grapple_state"] = 2
	controls.update_context(ctx, 1.0)
	check(controls.is_button_shown(&"pendant_up") and controls.is_button_shown(&"pendant_down"), "locked native line exposes its actual reel controls")
	ctx["grapple_line_attached"] = false
	controls.update_context(ctx, 1.0)
	check(controls.is_button_shown(&"grapple_range") and controls.button_label(&"grapple_range") == "LONG SHOT", "equipped unshot gun offers acknowledged default long range")
	event(&"grapple_range", true)
	check(router.frame(0.016)["verbs"] == [&"grapple_range"], "range press emits one semantic verb through real router")
	for frame in range(4):
		controls.update_context(ctx, 0.016)
		check(router.frame(0.016)["verbs"].is_empty(), "held range press never repeats")
	check(controls.button_label(&"grapple_range") == "LONG SHOT", "range press waits for native acknowledgment")
	ctx["grapple_shot_profile"] = 0
	controls.update_context(ctx, 1.0)
	check(controls.button_label(&"grapple_range") == "NEAR SHOT", "native near profile updates label")
	check(controls.button_center(&"action") == action_home, "range selection preserves primary Action home")
	ctx["grapple_line_attached"] = true
	controls.update_context(ctx, 1.0)
	check(not controls.is_button_shown(&"grapple_range") and controls._buttons[&"grapple_range"].index == -1, "attached line hides range and releases held capture")
	event(&"grapple_range", false)
	check(router.frame(0.016)["verbs"].is_empty(), "hidden range release emits no extra toggle")
	ctx["grapple_line_attached"] = false
	controls.update_context(ctx, 1.0)
	open_tools()
	check(not controls.is_button_shown(&"grapple_range"), "open tray hides range operation")
	tap(&"tools")
	ctx["operating"] = &""
	ctx["grapple_equipped"] = false
	controls.update_context(ctx, 1.0)
	open_tools()
	event(&"tools", false, true)
	check(not controls._tool_tray_open, "OS cancel closes tray")
	open_tools()
	check(tap(&"tools").is_empty() and not controls._tool_tray_open, "selector cancels locally")
	open_tools()
	var outside := InputEventScreenTouch.new()
	outside.index = 22
	outside.position = action_home
	outside.pressed = true
	router._input(outside)
	check(not controls._tool_tray_open and router.frame(0.016)["verbs"].is_empty(), "outside press cancels without activating Action")
	outside.pressed = false
	router._input(outside)
	open_tools()
	router.clear_held()
	check(not controls._tool_tray_open and controls._buttons[&"tools"].index == -1, "capture reset closes tray")
	open_tools()
	check(tap(&"pause") == [&"pause"] and not controls._tool_tray_open, "pause remains immediate with tray open")
	open_tools()
	ctx["operating"] = &"forklift"
	controls.update_context(ctx, 1.0)
	check(not controls._tool_tray_open and not controls.is_button_shown(&"tools"), "machine context closes portable tray")
	ctx = context()
	ctx["grapple_equipped"] = true
	ctx["grapple_line_attached"] = true
	ctx["drop_ok"] = true
	ctx["operating"] = &"grappling_gun"
	controls.update_context(ctx, 1.0)
	check(controls._tool_name == "GRAPPLE" and controls.button_label(&"drop") == "RELEASE", "native selected tool and explicit release")
	check(not controls.is_button_shown(&"auto_reel"), "automatic reel has only primary Action")
	check(controls.button_center(&"action") == action_home, "right Action stays stable")
	var layouts := 0
	for viewport in [Vector2i(968, 2376), Vector2i(2376, 968), Vector2i(1856, 2160), Vector2i(2160, 1856), Vector2i(1920,1080), Vector2i(2400,1080)]:
		root.size = viewport
		for scale in [0.8, 1.0, 1.3]:
			controls.set_touch_scale(scale)
			for tool in ["grapple", "grapple_attached", "line", "rigging"]:
				ctx = context()
				ctx["operating"] = {"grapple": &"grappling_gun", "grapple_attached": &"grappling_gun", "line": &"traversal_line", "rigging": &"rigging"}[tool]
				ctx["grapple_equipped"] = tool.begins_with("grapple")
				ctx["grapple_line_attached"] = tool == "grapple_attached"
				ctx["traversal_line_equipped"] = tool == "line"
				ctx["traversal_line_can_recover"] = true
				ctx["rigging_equipped"] = tool == "rigging"
				ctx["rigging_draft"] = true
				ctx["rigging_selected"] = 4
				ctx["rigging_power_operable"] = true
				controls.update_context(ctx, 1.0)
				controls._layout_in_safe_rect(Rect2(Vector2(24, 32), Vector2(viewport) - Vector2(48, 64)))
				check(controls.is_button_shown(&"grapple_range") == (tool == "grapple"), "range belongs only to unshot grapple")
				check(controls.is_button_shown(&"rigging_cut") == (tool == "rigging"), "cut belongs only to rigging")
				check(controls.is_button_shown(&"pack_traversal_line") == (tool == "line"), "pack belongs only to line")
				for tray in [false, true]:
					if tray:
						open_tools()
					for button in controls._buttons.values():
						if not button.shown:
							continue
						var hit: float = button.radius * controls.HIT_SLOP
						check(controls._safe.encloses(Rect2(button.center - Vector2.ONE * hit, Vector2.ONE * hit * 2.0)), "safe hit bounds " + str(button.id))
						for other in controls._buttons.values():
							if other == button or not other.shown:
								continue
							check(button.center.distance_to(other.center) > (button.radius + other.radius) * controls.HIT_SLOP, "hit circles separated " + str(button.id) + "/" + str(other.id))
					layouts += 1
	if failures.is_empty():
		print("TOOLBELT_TOUCH PASS real touch/router selection, acknowledgement, cancel, context, capture, active-tool operations; ", layouts, " layouts")
	quit(0 if failures.is_empty() else 1)

func _prototype() -> void:
	# Wait for the real window's resize notification before opening the tray.
	for frame in range(5):
		await process_frame
	controls.update_context(context(), 1.0)
	open_tools()
	controls.update_context(context(), 1.0)
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture="):
			await process_frame
			await RenderingServer.frame_post_draw
			check(controls._tool_tray_open, "prototype captures the open tray")
			var error := root.get_texture().get_image().save_png(arg.trim_prefix("--capture="))
			check(error == OK, "prototype screenshot saved")
			quit(0 if error == OK and failures.is_empty() else 1)
