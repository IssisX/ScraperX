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
	_exercise_loaded_body(rider)
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


func _exercise_loaded_body(rider: Node3D) -> void:
	var actual_position := Vector3(11.0, 25.0, -7.0)
	var seat := actual_position + Vector3(0, -0.90, 0)
	var native_state := {"seated": true, "drawing": true, "released": false,
		"draw_m": 9.0, "max_draw_m": 10.0, "seat_surface_position": seat,
		"simulation_time_seconds": 1.0, "rider_specific_acceleration": Vector3(0, 9.81, 0)}
	var untouched := native_state.duplicate(true)
	for frame in 24:
		native_state.simulation_time_seconds = 1.0 + float(frame) / 90.0
		rider.reaction(native_state, Vector3.ZERO, -1.0)
		rider.update_pose(actual_position, Vector3.ZERO, Vector3.FORWARD, 1.0 / 60.0)
	var pelvis: Node3D = rider.get_node("HarnessPelvis")
	_check((pelvis.global_transform * Vector3(0, -0.19, 0)).distance_to(seat) < 0.002,
		"visible pelvis underside is cradled on the measured leather basin")
	_check(rider.get_node("LeftLacedBoot").position.y > pelvis.position.y - 0.16,
		"cradled knees fold forward instead of standing down through the pouch")
	untouched.simulation_time_seconds = native_state.simulation_time_seconds
	_check(native_state == untouched and rider.position == actual_position,
		"load response never writes native state or mass-root position")
	native_state.seated = false
	native_state.released = true
	var rising := Vector3(0.0, 45.0, -90.0)
	var torso: Node3D = rider.get_node("WorkJacket")
	var before: Transform3D = torso.transform
	# Equal speed and wall time cannot manufacture response without native time.
	native_state.rider_specific_acceleration = Vector3(0, 600, -160)
	rider.reaction(native_state, rising, 0.8)
	rider.update_pose(actual_position, rising, Vector3.FORWARD, 0.5)
	_check(torso.transform.is_equal_approx(before), "zero native elapsed time freezes inertial pose despite a new wall-clock phase")
	for frame in 9:
		native_state.simulation_time_seconds += 1.0 / 90.0
		rider.reaction(native_state, rising, 0.8)
		rider.update_pose(actual_position, rising, Vector3.FORWARD, 1.0 / 60.0)
	_check(torso.transform.basis.get_rotation_quaternion().angle_to(before.basis.get_rotation_quaternion()) > 0.08,
		"measured launch load creates perceptible torso recoil at the same speed")
	_check(rider.get_node("ExpressiveHead").basis.get_rotation_quaternion().angle_to(torso.basis.get_rotation_quaternion()) > 0.02,
		"head inertia lags the torso instead of moving as one rigid mannequin")
	var shoulder: Node3D = rider.get_node("LeftUpperArm")
	var elbow: Node3D = rider.get_node("LeftForearm")
	var hand: Node3D = rider.get_node("LeftGlovedHand")
	_check(absf(shoulder.position.distance_to(elbow.position) - 0.29) < 0.0001 \
		and absf(elbow.position.distance_to(hand.position) - 0.27) < 0.0001,
		"inertial arm targets preserve both anatomical segment lengths")
	var thigh: Node3D = rider.get_node("LeftThigh")
	var shin: Node3D = rider.get_node("LeftShin")
	var boot: Node3D = rider.get_node("LeftLacedBoot")
	_check(absf(thigh.position.distance_to(shin.position) - 0.38) < 0.0001 \
		and absf(shin.position.distance_to(boot.position) - 0.34) < 0.0001,
		"folded and recoiling legs preserve both anatomical segment lengths")
	for part in [pelvis, torso, rider.get_node("ExpressiveHead"), shoulder, elbow, thigh, shin]:
		_check(part.transform.is_finite() and absf(part.basis.orthonormalized().determinant() - 1.0) < 0.001,
			"loaded anatomy retains finite right-handed frames")
	var left := Character.new()
	var right := Character.new()
	root.add_child(left)
	root.add_child(right)
	for frame in 13:
		for pair in [[left, -1.0], [right, 1.0]]:
			var load := {"seated": false, "released": true, "simulation_time_seconds": float(frame) / 90.0,
				"rider_specific_acceleration": Vector3(float(pair[1]) * 120.0, 0, 0)}
			pair[0].reaction(load, rising, 0.5)
			pair[0].update_pose(Vector3.ZERO, rising, Vector3.FORWARD, 1.0 / 60.0)
	var left_roll: float = left.get_node("WorkJacket").rotation.z
	var right_roll: float = right.get_node("WorkJacket").rotation.z
	_check(left_roll * right_roll < -0.005, "opposite physical accelerations cause opposite body recoil at identical flight speed")
	# Explicit relocation/restart may keep native time monotonic. The next
	# baseline must not inherit a previous launch's joint angular momentum.
	rider.reset_response()
	native_state.simulation_time_seconds = 10.0
	native_state.rider_specific_acceleration = Vector3.ZERO
	rider.reaction(native_state, rising, 0.5)
	rider.update_pose(actual_position, rising, Vector3.FORWARD, 0.0)
	_check(torso.rotation.is_zero_approx() and rider.get_node("ExpressiveHead").rotation.is_zero_approx(),
		"explicit restart clears old load response even when native tick time stays monotonic")
	left.queue_free()
	right.queue_free()


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
