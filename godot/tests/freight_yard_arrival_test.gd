extends SceneTree

# Actual shipping Main/bridge spawn, camera and exported yard. No debug staging.
var failures := 0
func check(ok: bool, message: String) -> void:
	if not ok:
		failures += 1
		push_error("FREIGHT_ARRIVAL FAIL " + message)
func _initialize() -> void:
	_run.call_deferred()
func _run() -> void:
	root.content_scale_mode = Window.CONTENT_SCALE_MODE_DISABLED
	var full_resolution := "--full-resolution" in OS.get_cmdline_user_args()
	root.size = Vector2i(1280, 720) if full_resolution else Vector2i(640, 360)
	var app = load("res://main.tscn").instantiate()
	app._settings_persistent = false
	root.add_child(app)
	app.set_process(false)
	app._settings.render_scale = 1.0 if full_resolution else 0.5
	app._settings.msaa = 0
	app._apply_settings()
	check(app._native != null, "real native bridge")
	var spawn: Vector3 = app._native.get_player_position()
	check(spawn.distance_to(Vector3(-94, 1.2, -44)) < 0.02, "ordinary bridge arrival without staging")
	for i in 45:
		app._process(1.0 / 90.0)
	check(app._native.is_player_grounded(), "actual grade support")
	check(app._native.get_forklift_state().available, "native forklift exists")
	check(app._native.get_yard_crane_state().available, "native crane exists")
	var camera: Camera3D = app._camera
	check(camera.is_position_in_frustum(Vector3(-100, 1.0, -56)), "real truck in initial camera")
	check(camera.is_position_in_frustum(Vector3(-66.5, 18, -150)), "real crane in initial camera")
	check(app.find_child("FreightArrival", true, false) != null, "receiving canopy built")
	check(app._yard_ambience._positions[0].distance_to(spawn) < 15.0, "sheltered ambience belongs to the actual arrival")
	check(app.find_child("FreightActivity", true, false) != null, "actual native machinery activity layer is present")
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture="):
			await RenderingServer.frame_post_draw
			await RenderingServer.frame_post_draw
			var path := arg.trim_prefix("--capture=")
			check(root.get_texture().get_image().save_png(path) == OK, "viewport image saved")
			print("FREIGHT_ARRIVAL_RENDER draws=", Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME), " renderer=", RenderingServer.get_video_adapter_name())
	app.free()
	print("FREIGHT_ARRIVAL failures=", failures)
	quit(1 if failures else 0)
