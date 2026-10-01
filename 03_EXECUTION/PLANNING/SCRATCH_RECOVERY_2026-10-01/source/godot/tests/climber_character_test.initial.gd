extends SceneTree

const Character := preload("res://presentation/climber_character.gd")

var _failed := false


func _initialize() -> void:
	_run.call_deferred()


func _check(condition: bool, message: String) -> void:
	if not condition:
		_failed = true
		push_error("SCRAPERX_CLIMBER_CHARACTER FAIL " + message)


func _run() -> void:
	var stage := Node3D.new()
	root.add_child(stage)
	var rider := Character.new()
	stage.add_child(rider)
	rider.build()
	var joint_count := rider.get_child_count()
	rider.build()
	_check(rider.get_child_count() == joint_count, "building twice does not duplicate body")
	var meshes := rider.find_children("*Surfaces", "MeshInstance3D", true, false)
	var vertices := 0
	var surfaces := 0
	var bounds := AABB()
	var first := true
	for instance: MeshInstance3D in meshes:
		_check(instance.mesh is ArrayMesh, "visible body consists of authored meshes")
		var part_bounds := instance.global_transform * instance.mesh.get_aabb()
		bounds = part_bounds if first else bounds.merge(part_bounds)
		first = false
		for surface in instance.mesh.get_surface_count():
			surfaces += 1
			vertices += instance.mesh.surface_get_arrays(surface)[Mesh.ARRAY_VERTEX].size()
	_check(vertices > 5000, "body includes face, layered equipment and finger geometry")
	_check(surfaces <= 160, "moving parts stay within the launch shot mobile surface budget")
	_check(bounds.position.y < -0.89 and bounds.end.y > 0.95 and bounds.size.y < 1.91,
		"standing anatomy fits the 1.8m capsule midpoint convention")
	_check(rider.find_children("*", "CollisionObject3D", true, false).is_empty(),
		"presentation never adds collision or another simulated body")
	_check(rider.get_node("ExpressiveHead/ExpressiveHeadSurfaces").mesh.get_surface_count() >= 10,
		"face has distinct skin, eyes, pupils, hair, lips and helmet details")
	var hand := rider.get_node("LeftGlovedHand")
	_check(hand.get_node("Finger1/DistalJoint") != null and hand.get_node("OpposedThumb") != null,
		"gloves have jointed fingers and opposing thumb")
	var native_position := Vector3(11.0, 25.0, -7.0)
	var native_velocity := Vector3(0, 21, -19)
	rider.update_pose(native_position, native_velocity, Vector3.LEFT, 0.016)
	_check(rider.position == native_position and native_velocity == Vector3(0, 21, -19),
		"body follows native position and never writes velocity")
	_check((rider.basis * Vector3.FORWARD).is_equal_approx(Vector3.LEFT), "body faces launch axis")
	_check(rider.get_node("LeftLacedBoot").position.y > -0.78,
		"fast flight bends the knees and tucks the feet")
	rider.position = Vector3.ZERO
	rider.rotation = Vector3.ZERO
	rider.pose(0)
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--capture="):
			await _capture(stage, rider, argument.trim_prefix("--capture="))
	print("SCRAPERX_CLIMBER_CHARACTER %s meshes=%d surfaces=%d vertices=%d" % [
		"FAIL" if _failed else "PASS", meshes.size(), surfaces, vertices])
	stage.queue_free()
	await process_frame
	quit(42 if _failed else 0)


func _capture(stage: Node3D, rider: Node3D, directory: String) -> void:
	DirAccess.make_dir_recursive_absolute(directory)
	root.size = Vector2i(1200, 1000)
	var environment := Environment.new()
	environment.background_mode = Environment.BG_COLOR
	environment.background_color = Color("2e3835")
	environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.ambient_light_color = Color("c6d4c4")
	environment.ambient_light_energy = 0.65
	environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	var world := WorldEnvironment.new()
	world.environment = environment
	stage.add_child(world)
	var key := DirectionalLight3D.new()
	key.rotation_degrees = Vector3(-35, -33, 0)
	key.light_color = Color("ffe2b5")
	key.light_energy = 1.6
	stage.add_child(key)
	var rim := DirectionalLight3D.new()
	rim.rotation_degrees = Vector3(-20, 155, 0)
	rim.light_color = Color("c9e5ed")
	rim.light_energy = 0.8
	stage.add_child(rim)
	var camera := Camera3D.new()
	camera.fov = 40.0
	stage.add_child(camera)
	camera.make_current()
	var floor_mesh := MeshInstance3D.new()
	var plane := PlaneMesh.new()
	plane.size = Vector2(10, 10)
	floor_mesh.mesh = plane
	floor_mesh.position.y = -0.913
	var floor_material := StandardMaterial3D.new()
	floor_material.albedo_color = Color("404a43")
	floor_mesh.material_override = floor_material
	stage.add_child(floor_mesh)
	for shot in ["full", "face", "flight", "back"]:
		rider.pose(25.0 if shot == "flight" else 0.0, 0.7, 0.0)
		if shot == "face":
			camera.position = Vector3(0.35, 0.76, -0.85)
			camera.look_at(Vector3(0, 0.76, -0.005))
			camera.fov = 32.0
		elif shot == "back":
			camera.position = Vector3(-2.1, 1.00, 3.45)
			camera.look_at(Vector3(0, 0.02, 0))
			camera.fov = 34.0
		else:
			camera.position = Vector3(2.2, 1.12, -3.75)
			camera.look_at(Vector3(0, 0.10, 0))
			camera.fov = 34.0
		for frame in 4:
			await process_frame
		await RenderingServer.frame_post_draw
		var result := root.get_texture().get_image()
		result.save_png(directory.path_join("climber_" + shot + ".png"))
