extends RefCounted

# Isolated presentation fixture. No native actor/controller is simulated here.
func execute(scene_tree: SceneTree) -> Variant:
	var root := Node3D.new()
	root.name = "WorkerPresentationFixture"
	scene_tree.root.add_child(root)
	scene_tree.root.get_viewport().disable_3d = false
	var worker = load("res://presentation/life/yard_worker_view.gd").new()
	root.add_child(worker)
	var state := {"present": true, "supported": true, "position": Vector3(0, 0.9, 0),
		"velocity": Vector3.ZERO, "heading_radians": 0.0, "episode": 1, "native_tick": 100,
		"activity": 0, "activity_seconds": 1.0, "work_point": Vector3(-0.08, 1.08, -0.48),
		"guide_point": Vector3(-3, 2, -5), "look_target": Vector3(0, 1.5, -1)}
	for index in 30:
		state.native_tick = 100 + index
		worker.present(state, 1.0 / 90.0)
	var checks := {"rig_loaded": worker._body.ready_to_pose(), "welding": worker.welding_active(),
		"tip_at_seam": worker.weld_tip_world().is_equal_approx(state.work_point),
		"masked_head": worker._hood.visible, "bounded_sparks": worker._sparks.multimesh.instance_count == 10,
		"active_sparks_visible": worker._sparks.visible}
	var paused_spark: Transform3D = worker._sparks.multimesh.get_instance_transform(0)
	state["paused"] = true
	worker.present(state, 0.0)
	checks["pause_retains_frozen_sparks"] = worker._sparks.visible and worker._sparks.multimesh.get_instance_transform(0).is_equal_approx(paused_spark)
	state["paused"] = false
	for side in 2:
		for joint in ["sh", "el", "wr", "hp", "kn", "an"]:
			checks["finite_%s_%d" % [joint, side]] = worker._body.joint_world(side, joint).is_finite()
	for activity in [1, 2, 3, 4, 5]:
		state.activity = activity
		worker.present(state, 1.0 / 90.0)
		checks["activity_%d_stops_arc" % activity] = not worker.welding_active() and not worker._arc.visible and not worker._sparks.visible
	state.activity = 0
	state.supported = false
	state.velocity = Vector3(1, -3, 0)
	var serial: int = worker.footstep_serial()
	for index in 30:
		worker.present(state, 1.0 / 90.0)
	checks["fall_stops_arc"] = not worker.welding_active() and not worker._sparks.visible
	checks["fall_no_footsteps"] = serial == worker.footstep_serial()
	state.supported = true
	state.activity = 3
	state.velocity = Vector3(1, 0, 0)
	for index in 100:
		worker.present(state, 1.0 / 90.0)
	checks["walk_has_steps"] = worker.footstep_serial() > serial
	state.present = false
	worker.present(state, 1.0 / 90.0)
	checks["hide_clears"] = not worker.visible and not worker.welding_active() and not worker._arc.visible and not worker._sparks.visible
	state.present = true
	state.velocity = Vector3.ZERO
	state.activity = 0
	state.episode = 2
	state.native_tick = 0
	worker.present(state, 0.12)
	checks["episode_clears_steps"] = worker.footstep_serial() == 0
	state.position = Vector3(INF, 0, 0)
	worker.present(state, 0.1)
	checks["invalid_hidden"] = not worker.visible and not worker.welding_active() and not worker._sparks.visible
	state.position = Vector3(0, 0.9, 0)
	for index in 30:
		state.native_tick = index
		worker.present(state, 1.0 / 90.0)
	worker.reset()
	checks["reset_hides_sparks"] = not worker._sparks.visible and not worker.welding_active()
	worker.present(state, 1.0 / 90.0)
	# A plain work surface/floor provides scale without supplying fictional footing.
	worker._box(root, "FixtureBench", Vector3(1.3, 0.12, 0.6), Vector3(0, 1.0, -0.65), worker._material(Color("44494c"), 0.7))
	worker._box(root, "FixtureFloor", Vector3(12, 0.1, 12), Vector3(0, -0.05, 0), worker._material(Color("555657")))
	for x in [-0.5, 0.5]:
		worker._box(root, "FixtureBenchLeg", Vector3(0.07, 0.95, 0.07), Vector3(x, 0.47, -0.65), worker._material(Color("35393a"), 0.7))
	var camera := Camera3D.new()
	root.add_child(camera)
	camera.position = Vector3(1.3, 1.55, -2.0)
	camera.look_at(Vector3(0, 1.1, -0.1))
	camera.current = true
	var light := DirectionalLight3D.new()
	root.add_child(light)
	light.rotation_degrees = Vector3(-45, -25, 0)
	light.light_energy = 1.5
	var environment := WorldEnvironment.new()
	environment.environment = Environment.new()
	environment.environment.background_mode = Environment.BG_COLOR
	environment.environment.background_color = Color("24292c")
	environment.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.environment.ambient_light_color = Color("a3b0b5")
	environment.environment.ambient_light_energy = 0.65
	root.add_child(environment)
	var failed: Array[String] = []
	for label in checks:
		if not checks[label]:
			failed.append(label)
	return {"checks": checks.size(), "failed": failed, "welding": worker.welding_active(), "hood": worker._hood.global_position, "hand": worker._body.joint_world(0, "wr"), "tip": worker.weld_tip_world()}
