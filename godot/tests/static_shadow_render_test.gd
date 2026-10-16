extends SceneTree
const Batch := preload("res://presentation/static_world_batch.gd")
const Geometry := preload("res://presentation/world_geometry.gd")
var failures := 0
func check(ok: bool, message: String) -> void:
	if not ok:
		failures += 1
		push_error("STATIC_SHADOW_RENDER FAIL " + message)
func _initialize() -> void:
	_run.call_deferred()
func frame_image() -> Image:
	await RenderingServer.frame_post_draw
	await RenderingServer.frame_post_draw
	return root.get_texture().get_image()
func _run() -> void:
	root.content_scale_mode = Window.CONTENT_SCALE_MODE_DISABLED
	root.size = Vector2i(256, 256)
	var host := Node3D.new()
	root.add_child(host)
	var g := Geometry.new(host)
	var red := g.material(Color("a53a27"), .4, .3)
	var blue := g.material(Color("254b6b"), .2, .85)
	g.add_box("YardStructure", Vector3(2, 3, 2), Vector3(-2, 1.5, 0), red)
	var second := g.add_box("YardStructure", Vector3(2, 2, 3), Vector3(2, 1, 0), blue)
	second.rotation.y = .4
	var ground := g.add_box("YardStructure", Vector3(28, .2, 28), Vector3(0, -.1, 0), g.material(Color("c2bfa9"), 0, 1))
	ground.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-45, -35, 0)
	light.shadow_enabled = true
	light.directional_shadow_mode = DirectionalLight3D.SHADOW_ORTHOGONAL
	light.directional_shadow_max_distance = 60
	host.add_child(light)
	var world := WorldEnvironment.new()
	var env := Environment.new()
	env.background_mode = Environment.BG_COLOR
	env.background_color = Color("888a8f")
	env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.ambient_light_color = Color.WHITE
	env.ambient_light_energy = .3
	world.environment = env
	host.add_child(world)
	var camera := Camera3D.new()
	host.add_child(camera)
	camera.position = Vector3(10, 9, 13)
	camera.look_at(Vector3(0, 1, 0), Vector3.UP)
	camera.far = 60
	camera.make_current()
	var before: Image = await frame_image()
	var before_draws := Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME)
	var stats := Batch.new().build_tower(host)
	var after: Image = await frame_image()
	var after_draws := Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME)
	var changed := 0
	var maximum := 0.0
	var total := 0.0
	for y in before.get_height():
		for x in before.get_width():
			var a := before.get_pixel(x, y)
			var b := after.get_pixel(x, y)
			var error := maxf(absf(a.r-b.r), maxf(absf(a.g-b.g), absf(a.b-b.b)))
			maximum = maxf(maximum, error)
			total += error
			if error > 2.0/255.0: changed += 1
	check(stats.shadow_sources == 2, "opaque casters consolidated")
	check(after_draws < before_draws, "actual shadow submissions reduced")
	check(changed < 16 and maximum <= .04, "paint, geometry and actual cast shadows remain equivalent")
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture-dir="):
			var path := arg.trim_prefix("--capture-dir=")
			before.save_png(path.path_join("shadow-before.png"))
			after.save_png(path.path_join("shadow-after.png"))
	print("STATIC_SHADOW_RENDER draws_before=", before_draws, " draws_after=", after_draws, " changed_pixels=", changed, " maximum_error=", maximum, " mean_error=", total/(before.get_width()*before.get_height()), " failures=", failures)
	host.free()
	quit(1 if failures else 0)
