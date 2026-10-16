extends SceneTree

var failed := false
var frame_count := 0
var finished := false
var loading_captured := false

func check(ok: bool, message: String) -> void:
	if not ok:
		failed = true
		push_error("STARTUP_MAIN FAIL " + message)

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	root.size = Vector2i(1080,928)
	root.content_scale_size = Vector2i(2160,1856)
	root.content_scale_mode = Window.CONTENT_SCALE_MODE_CANVAS_ITEMS
	var main: Node = load("res://main.tscn").instantiate()
	main.prepare_initial_world = true
	main._settings_persistent = false
	main.play_ready.connect(func() -> void:
		main.set_process(false)
		finished = true
	)
	root.add_child(main)
	check(main._native == null and main._startup_preparation != null,
		"normal Main with preparation holds native out of gameplay")
	check(not main._router.gameplay_active and root.disable_3d,
		"input and expensive3D rendering wait for preparation")
	var started_usec := Time.get_ticks_usec()
	while not finished and Time.get_ticks_usec()-started_usec < 120000000:
		await process_frame
		frame_count += 1
		if not loading_captured:
			await RenderingServer.frame_post_draw
			var path := OS.get_environment("SCRAPERX_STARTUP_CAPTURE_PATH")
			if path != "":
				check(root.get_texture().get_image().save_png(path) == OK,"loading view captured")
			loading_captured = true
		if main._startup_preparation != null and main._startup_preparation._finished and not finished:
			check(false,"real preparation cannot fail its readiness path")
			break
	check(finished,"real normal Main reaches play_ready within bounded wall time")
	check(frame_count > 5,"loading draws and processes frames while native prepares")
	if not finished:
		main.free()
		quit(1)
		return
	check(main._startup_preparation == null and not root.disable_3d,
		"joined worker and3D viewport return to normal gameplay")
	check(main._router.gameplay_active and main._hud.visible,
		"ordinary input and HUD become available")
	var native: Object = main._native
	check(native.is_initial_world_settled(),"actual whole catch assembly slept and player earned firm footing")
	var before := int(native.get_tick_index())
	check(before > 0 and before <= 900,"all preparation used finite actual native ticks")
	main._process(0.15)
	check(native.get_tick_index() == before,"first control frame discards pre-control construction debt")
	main._process(native.get_fixed_step_seconds())
	check(native.get_tick_index() == before+1,"normal fixed cadence resumes immediately after handoff")
	var jump := InputEventKey.new()
	jump.physical_keycode = KEY_SPACE
	jump.keycode = KEY_SPACE
	jump.pressed = true
	Input.parse_input_event(jump)
	await process_frame
	main._process(native.get_fixed_step_seconds())
	jump.pressed = false
	Input.parse_input_event(jump)
	check(native.get_player_linear_velocity().y > 0.1,"fresh ordinary keyboard Jump reaches live native player")
	print("STARTUP_MAIN ticks="+str(before)+" responsive_ui_frames="+str(frame_count)+" failures="+str(int(failed)))
	main.free()
	await process_frame
	quit(int(failed))
