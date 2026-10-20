extends SceneTree

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var capture_dir := ""
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--mounted-crossing-capture-dir="):
			capture_dir = arg.trim_prefix("--mounted-crossing-capture-dir=")
	root.size = Vector2i(1280, 900)
	root.disable_3d = true
	var main: Node = load("res://main.tscn").instantiate()
	main.prepare_initial_world = false
	main._settings_persistent = false
	main._force_touch = true
	root.add_child(main)
	main.set_process(false)
	var deadline := Time.get_ticks_msec() + 10000
	while not main._audio._bank_ready and Time.get_ticks_msec() < deadline:
		await process_frame
	var test: RefCounted = load("res://tests/mounted_winch_crossing_input_case.gd").new()
	test.check(main._audio._bank_ready, "owned audio setup completes")
	var result: Dictionary = {"checks": test.checks, "failures": test.failures}
	if result.failures.is_empty():
		result = test.run(main)
	if not capture_dir.is_empty() and result.failures.is_empty():
		test.aim(Vector3(-108, 1.6, -35.5))
		main._render_snapshot()
		main._hud.update_hud(main._ctx, 0.0)
		DirAccess.make_dir_recursive_absolute(capture_dir)
		root.disable_3d = false
		await process_frame
		await RenderingServer.frame_post_draw
		var image := root.get_texture().get_image()
		test.check(image != null and image.save_png(capture_dir.path_join("mounted-winch-crossing.png")) == OK,
			"actual two-cable beam and touch control view captured")
		result = {"checks": test.checks, "failures": test.failures}
	print("SCRAPERX_MOUNTED_WINCH_CROSSING_INPUT %s checks=%d fixture_start_only=1" %
		["PASS" if result.failures.is_empty() else "FAIL", result.checks])
	main.queue_free()
	await process_frame
	quit(0 if result.failures.is_empty() else 1)
