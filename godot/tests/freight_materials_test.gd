extends SceneTree

var failures := 0
func check(ok: bool, why: String) -> void:
	if not ok:
		failures += 1
		push_error("FREIGHT_MATERIALS " + why)

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var path := "res://presentation/materials/freight_materials.gd"
	check(ResourceLoader.exists(path), "freight finish resource exists")
	if failures:
		quit(1)
		return
	var finishes = load(path).new()
	var palette: Array[Material] = []
	for color in [Color("687174"), Color("86472d"), Color("4a3420"), Color("4e4841"), Color("a04d16"), Color("879394"), Color("5b544b"), Color("c19a2a")]:
		var material := StandardMaterial3D.new()
		material.albedo_color = color
		palette.append(material)
	for entity in [2440, 2441, 2442, 2443, 2444, 2445, 2446, 2447, 1450, 2450, 2451, 2452, 2453]:
		for index in [0, 1, 5, 7]:
			var material: Material = finishes.for_part(entity, index, palette[index], true)
			check(material is ShaderMaterial, "native freight finish %d/%d" % [entity, index])
			check(material == finishes.for_part(entity, index, palette[index], true), "reuse same resource")
			check(material.next_pass == null, "single pass")
			check(material.get_shader_parameter("base_color") == (palette[index] as StandardMaterial3D).albedo_color, "original color role")
			var roughness: float = material.get_shader_parameter("base_roughness")
			check(is_finite(roughness) and roughness >= .3 and roughness <= .95, "finite roughness")
	for entity in [1935, 1954, 2320, 2963, 2448, 2439]:
		check(finishes.for_part(entity, 7, palette[7], true) == palette[7], "unrelated surface preserved")
	for index in [2, 3, 4, 6]:
		check(finishes.for_part(2444, index, palette[index], true) == palette[index], "timber/concrete/hazard/rubble preserved")
	root.size = Vector2i(960, 540)
	var app = load("res://main.tscn").instantiate()
	app._settings_persistent = false
	root.add_child(app)
	app.set_process(false)
	root.size = Vector2i(640, 360)
	app._settings.render_scale = 1.0
	app._settings.msaa = 0
	app._apply_settings()
	var kit: Node3D = app.get_node("KitPresentation")
	for static_body in kit.static_render_roots():
		for child in static_body.get_children():
			if child is MeshInstance3D and child.mesh != null:
				var finish: Material = child.get_active_material(0)
				check(not (finish is ShaderMaterial and finish.shader == load("res://presentation/materials/freight_surface.gdshader")), "actual static freight remains eligible for shared world/shadow batches")
	var selected := 0
	var mesh_count := 0
	var vertices := 0
	for body in kit.get_children():
		for child in body.get_children():
			if child is MeshInstance3D and child.mesh != null:
				mesh_count += 1
				for surface in child.mesh.get_surface_count():
					vertices += child.mesh.surface_get_arrays(surface)[Mesh.ARRAY_VERTEX].size()
					if child.mesh.surface_get_material(surface) is ShaderMaterial and child.mesh.surface_get_material(surface).shader == load("res://presentation/materials/freight_surface.gdshader"):
						selected += 1
	check(selected >= 15, "actual native freight batches use finish")
	print("FREIGHT_TOPOLOGY meshes=", mesh_count, " vertices=", vertices, " selected=", selected)
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--capture=") or arg.begins_with("--capture-cab="):
			var camera: Camera3D = app._camera
			var frame: Transform3D = kit.get_node("KitBody2440").global_transform
			var cab := arg.begins_with("--capture-cab=")
			camera.global_position = frame * (Vector3(.2, 1.28, -1.65) if cab else Vector3(4, 2.6, 4))
			camera.look_at(frame * (Vector3(0, .8, 1) if cab else Vector3(0, .6, -.5)))
			await RenderingServer.frame_post_draw
			await RenderingServer.frame_post_draw
			check(root.get_texture().get_image().save_png(arg.get_slice("=", 1)) == OK, "near truck capture")
	app.free()
	print("FREIGHT_MATERIALS failures=", failures)
	quit(1 if failures else 0)
