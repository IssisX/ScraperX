extends SceneTree

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var capture_dir := ""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--portable-crossing-capture-dir="):
			capture_dir = arg.trim_prefix("--portable-crossing-capture-dir=")
	root.size = Vector2i(1280, 900) if not capture_dir.is_empty() else Vector2i(2160, 1856)
	root.disable_3d = true
	var main: Node = load("res://main.tscn").instantiate()
	main.prepare_initial_world = false
	main._settings_persistent = false
	main._force_touch = true
	root.add_child(main)
	var test: RefCounted = load("res://tests/portable_line_crossing_input_case.gd").new()
	var result: Dictionary = test.run(main)
	var deadline := Time.get_ticks_msec() + 10000
	while not main._audio._bank_ready and Time.get_ticks_msec() < deadline:
		await process_frame
	if not capture_dir.is_empty() and result.failures.is_empty():
		main._native.request_traversal_line(2, 0)
		test.step(2)
		test.check(bool(main._native.get_traversal_line_state().gripping), "regrip installed crossing for native view")
		main._yaw = PI
		main._pitch = 0.35
		main._render_snapshot()
		main._hud.update_hud(main._ctx, 0.0)
		DirAccess.make_dir_recursive_absolute(capture_dir)
		root.disable_3d = false
		await process_frame
		await RenderingServer.frame_post_draw
		var image := root.get_texture().get_image()
		test.check(image != null and image.save_png(capture_dir.path_join("portable-line-crossing.png")) == OK,
			"actual two-anchor native line and hands captured")
		result = {"checks": test.checks, "failures": test.failures}
	print("SCRAPERX_PORTABLE_CROSSING_INPUT %s checks=%d fixture_start_only=1" %
		["PASS" if result.failures.is_empty() else "FAIL", result.checks])
	main.queue_free()
	await process_frame
	quit(0 if result.failures.is_empty() else 1)
