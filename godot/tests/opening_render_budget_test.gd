extends SceneTree
# Real shipping Main, default quality, actual native arrival. Elevated camera
# positions are rendering stress probes, never evidence of native route play.
var failures := 0
func check(ok: bool, label: String) -> void:
	if not ok:
		failures += 1
		push_error("OPENING_RENDER_BUDGET FAIL " + label)
func _initialize() -> void:
	_run.call_deferred()
func _run() -> void:
	root.content_scale_mode = Window.CONTENT_SCALE_MODE_DISABLED
	root.size = Vector2i(160, 90)
	var app = load("res://main.tscn").instantiate()
	app._settings_persistent = false
	root.add_child(app)
	app.set_process(false)
	app._settings.msaa = 0
	app._settings.render_scale = 1
	app._apply_settings()
	for i in 45: app._process(1.0 / 90.0)
	app._sky_cycle.day_minutes = 0
	var camera: Camera3D = app._camera
	var start := camera.global_transform
	var native_start: Vector3 = app._native.get_player_position()
	check(app._settings.shadow_quality == 3 and is_equal_approx(app._settings.view_distance, 2000), "shipping high shadows and whole-tower visibility retained")
	for mode in ["opening", "ground_to_crown", "crown_to_ground"]:
		camera.global_transform = start
		if mode == "ground_to_crown":
			camera.look_at(Vector3(0, 1500, -150), Vector3.UP)
		elif mode == "crown_to_ground":
			camera.global_position = Vector3(35, 1590, -110)
			camera.look_at(Vector3(-20, 0, -150), Vector3.UP)
		await RenderingServer.frame_post_draw
		await RenderingServer.frame_post_draw
		var draws := Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME)
		check(app._sky_cycle._sun.shadow_enabled, mode + " retains actual sun shadows")
		check(draws > 0 and draws < 1000, mode + " stays below 1000 draw calls")
		check(app._native.get_player_position() == native_start, "render camera never moves native player")
		print("OPENING_RENDER_BUDGET ", JSON.stringify({"mode":mode,"draws":draws,"primitives":Performance.get_monitor(Performance.RENDER_TOTAL_PRIMITIVES_IN_FRAME),"renderer":RenderingServer.get_video_adapter_name(),"video_bytes":Performance.get_monitor(Performance.RENDER_VIDEO_MEM_USED),"buffer_bytes":Performance.get_monitor(Performance.RENDER_BUFFER_MEM_USED),"scope":"render stress view; not AndroidFPS or route completion"}))
	app.free()
	print("OPENING_RENDER_BUDGET failures=", failures)
	quit(1 if failures else 0)
