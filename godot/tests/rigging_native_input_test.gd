extends SceneTree

var main: Node
var failures := 0
var checks := 0
var capture_dir := ""

func _initialize() -> void:
	_run.call_deferred()

func check(value: bool, message: String) -> void:
	checks += 1
	if not value:
		failures += 1
		push_error("RIGGING_NATIVE_INPUT " + message)

func step() -> void:
	main._dispatch(main._router.frame(1.0 / 90.0).verbs, 1.0 / 90.0)
	main._native.advance_frame(1.0 / 90.0)
	main._ctx = main._read_context()
	main._router.set_rigging_mode(bool(main._ctx.rigging_equipped))
	main._touch.update_context(main._ctx, 1.0)
	main._rigging_view.update_state(main._native.get_rigging_state(), main._audio)

func tap(control: StringName) -> void:
	check(main._touch.is_button_shown(control), "touch target visible: " + str(control))
	var event := InputEventScreenTouch.new()
	event.index = 41
	event.position = main._touch.button_center(control)
	event.pressed = true
	main._router._input(event)
	event.pressed = false
	main._router._input(event)

func _run() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--rigging-capture-dir="):
			capture_dir = arg.trim_prefix("--rigging-capture-dir=")
	root.size = Vector2i(1280, 900) if not capture_dir.is_empty() else Vector2i(2160, 1856)
	root.content_scale_size = root.size
	root.disable_3d = true
	main = load("res://main.tscn").instantiate()
	main.prepare_initial_world = false
	main._settings_persistent = false
	main._force_touch = true
	root.add_child(main)
	main.set_process(false)
	main._router.capture_mouse = false
	main._router.gameplay_active = true
	var native: Object = main._native
	check(native != null, "shipping Main owns matching native Simulation")
	if native == null:
		quit(1)
		return
	var bodies := int(native.get_kit_body_count())
	check(bool(native.get_rigging_state().available), "normal world contains actual rigging owner")
	step()
	tap(&"toggle_rigging")
	step()
	check(bool(native.get_rigging_state().equipped), "actual touch target equips native tool")
	check(main._ctx.operating == &"rigging", "native acknowledgment selects rigging station")
	check(main._touch.is_button_shown(&"toggle_rigging"), "phone equipment control stays visible")
	check(not main._touch.is_button_shown(&"pendant_up"), "tool does not expose machine pendant")
	check(int(native.get_rigging_state().installed_count) == 0 and int(native.get_kit_body_count()) == bodies,
		"equipping does not create support or hardware")
	tap(&"toggle_rigging")
	step()
	check(not bool(native.get_rigging_state().equipped), "actual phone control stows native tool")
	tap(&"toggle_rigging")
	step()
	check(bool(native.get_rigging_state().equipped), "actual phone control re-equips same native tool")
	check(not native.set_rigging_aim(NAN, 0.0, 1.0), "native input boundary rejects invalid aim")
	check(not native.request_rigging(99, 0), "bridge rejects unknown semantic command")
	main._dispatch([&"rigging_cut_stop"], 0.0)
	step()
	check(float(native.get_rigging_state().cut_progress) == 0.0, "release leaves no queued cut")
	var deadline := Time.get_ticks_msec() + 10000
	while not main._audio._bank_ready and Time.get_ticks_msec() < deadline:
		await process_frame
	check(main._audio._bank_ready, "owned audio setup completes before teardown")
	if not capture_dir.is_empty():
		DirAccess.make_dir_recursive_absolute(capture_dir)
		# Presentation fixture only. The separate public route proves arrival.
		check(native.debug_restart_at(Vector3(-124.8, 0.9, -40.8)), "clear grade for bay presentation")
		main._yaw = -PI * 0.75
		main._pitch = 0.04
		native.request_rigging(0, 0)
		step()
		main._render_snapshot()
		main._hud.update_hud(main._ctx, 0.0)
		root.disable_3d = false
		await process_frame
		await RenderingServer.frame_post_draw
		var image := root.get_texture().get_image()
		check(image != null and image.save_png(capture_dir.path_join("rigging-bay.png")) == OK,
			"actual native bay and rigging controls captured")
	print("SCRAPERX_RIGGING_NATIVE_INPUT %s checks=%d" % ["PASS" if failures == 0 else "FAIL", checks])
	main.queue_free()
	await process_frame
	quit(0 if failures == 0 else 1)
