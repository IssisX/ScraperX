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
	_exercise_native_reactions(rider)
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


func _exercise_native_reactions(rider: Node3D) -> void:
	var native_state := {"seated": true, "drawing": true, "released": false,
		"draw_m": 9.0, "max_draw_m": 10.0}
	var untouched := native_state.duplicate(true)
	var actual_position := Vector3(11.0, 25.0, -7.0)
	for frame in 24:
		rider.reaction(native_state, Vector3.ZERO, -1.0)
		rider.update_pose(actual_position, Vector3.ZERO, Vector3.FORWARD, 1.0 / 60.0)
	_check(rider._comedy_charge > 0.85 and rider._comedy_launch == 0.0,
		"charged nerves require actual seated draw rather than a speed guess")
	_check(native_state == untouched and rider.position == actual_position,
		"reaction context never mutates native state or capsule position")
	native_state.seated = false
	native_state.released = true
	var rising := Vector3(0.0, 45.0, -90.0)
	rider.reaction(native_state, rising, 0.08)
	rider.update_pose(actual_position, rising, Vector3.FORWARD, 1.0 / 60.0)
	_check(rider._comedy_launch > 0.0 and rider._comedy_launch < 0.3,
		"release reaction eases in rather than changing the pose in one frame")
	for frame in 36:
		rider.reaction(native_state, rising, 0.08 + float(frame) / 60.0)
		rider.update_pose(actual_position, rising, Vector3.FORWARD, 1.0 / 60.0)
	_check(rider._comedy_launch > 0.99 and rider._comedy_windmill > 0.7,
		"native fast release progresses from tuck into the brief futile swim")
	_check(rider._comedy_fall == 0.0, "rising velocity never invents a falling reaction")
	var shoulder: Node3D = rider.get_node("LeftUpperArm")
	var elbow: Node3D = rider.get_node("LeftForearm")
	var hand: Node3D = rider.get_node("LeftGlovedHand")
	_check(absf(shoulder.position.distance_to(elbow.position) - 0.29) < 0.0001 \
		and absf(elbow.position.distance_to(hand.position) - 0.27) < 0.0001,
		"comic arm targets preserve both anatomical segment lengths")
	var swim_hand := hand.position
	var falling := Vector3(0, -22, -36)
	for frame in 24:
		rider.reaction(native_state, falling, 0.95)
		rider.update_pose(actual_position, falling, Vector3.FORWARD, 1.0 / 60.0)
	_check(rider._comedy_fall > 0.99 and hand.position.distance_to(swim_hand) > 0.05,
		"actual downward velocity opens the limbs into a distinct falling flail")
	var before_zero_delta: float = rider._comedy_fall
	rider.reaction({}, Vector3.ZERO, -1.0)
	rider.update_pose(actual_position, Vector3.ZERO, Vector3.FORWARD, 0.0)
	_check(rider._comedy_fall == before_zero_delta, "zero elapsed time never advances a reaction")
	native_state.reduced_motion = true
	for frame in 48:
		rider.reaction(native_state, falling, 0.95)
		rider.update_pose(actual_position, falling, Vector3.FORWARD, 1.0 / 60.0)
	_check(rider._comedy_windmill < 0.0001 and rider._comedy_fall < 0.0001,
		"reduced motion suppresses the animated swim and flail")
	for frame in 60:
		rider.reaction({}, Vector3.ZERO, -1.0)
		rider.update_pose(Vector3.ZERO, Vector3.ZERO, Vector3.FORWARD, 1.0 / 60.0)


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
	for shot in ["charged_brace", "release_tuck", "panic_swim", "downward_flail"]:
		for frame in 60:
			rider.reaction({}, Vector3.ZERO, -1.0)
			rider.update_pose(Vector3.ZERO, Vector3.ZERO, Vector3.FORWARD, 1.0 / 60.0)
		var charged: bool = shot == "charged_brace"
		var state := {"seated": charged, "drawing": charged, "released": not charged,
			"draw_m": 9.0 if charged else 0.0, "max_draw_m": 10.0}
		var actual_velocity := Vector3.ZERO if charged else Vector3(0, 45, -90)
		var launch_phase := -1.0 if charged else (0.10 if shot == "release_tuck" else 0.83)
		if shot == "downward_flail":
			actual_velocity = Vector3(0, -22, -36)
		for frame in 30:
			rider.reaction(state, actual_velocity, launch_phase)
			rider.update_pose(Vector3.ZERO, actual_velocity, Vector3.FORWARD, 1.0 / 60.0)
		camera.position = Vector3(2.2, 1.12, -3.75)
		camera.look_at(Vector3(0, 0.10, 0))
		camera.fov = 34.0
		for frame in 4:
			await process_frame
		await RenderingServer.frame_post_draw
		var result := root.get_texture().get_image()
		result.save_png(directory.path_join("climber_" + shot + ".png"))
