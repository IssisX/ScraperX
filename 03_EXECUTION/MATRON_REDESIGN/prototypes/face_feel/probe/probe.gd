extends Node3D
func _ready() -> void:
	var mesh: Mesh = load("res://head.obj")
	var mat := ShaderMaterial.new()
	mat.shader = load("res://skin.gdshader")
	mat.set_shader_parameter("skin_lut", load("res://skin_lut.png"))
	var e := float(OS.get_environment("EXPO")) if OS.get_environment("EXPO") != "" else 0.0
	mat.set_shader_parameter("exposure", e)
	var head := MeshInstance3D.new()
	head.mesh = mesh
	head.material_override = mat
	head.rotation.y = float(OS.get_environment("YAW")) if OS.get_environment("YAW") != "" else 0.0
	add_child(head)
	var env := WorldEnvironment.new()
	env.environment = Environment.new()
	env.environment.background_mode = Environment.BG_COLOR
	env.environment.background_color = Color(0.02, 0.02, 0.025)
	env.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.environment.ambient_light_color = Color(0.05, 0.055, 0.07)
	env.environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	add_child(env)
	# The caged work bulb: warm, above and to her left, a little behind.
	var bulb := OmniLight3D.new()
	bulb.light_color = Color("ffd49a")
	bulb.light_energy = 1.6
	bulb.omni_range = 6.0
	bulb.position = Vector3(-0.6, 0.45, -0.25)
	add_child(bulb)
	# A cool, dim key from in front-right (the sky through the steel).
	var key := OmniLight3D.new()
	key.light_color = Color("b9c8dc")
	key.light_energy = 0.5
	key.omni_range = 8.0
	key.position = Vector3(0.9, 0.3, 1.4)
	add_child(key)
	var cam := Camera3D.new()
	cam.fov = 40.0
	cam.position = Vector3(0.0, -0.02, 0.62)
	add_child(cam)
	cam.current = true
	for i in 4:
		await get_tree().process_frame
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png(OS.get_environment("OUT"))
	get_tree().quit()
