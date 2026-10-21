extends SceneTree

# Actual production batch versus the previous ten separately submitted boxes.
# The inner probe also runs through MCP without creating another SceneTree.
class Probe extends RefCounted:
	const Worker := preload("res://presentation/life/yard_worker_view.gd")
	const PIXEL_TOLERANCE := 2.0 / 255.0
	var checks := 0
	var failures: Array[String] = []

	func check(ok: bool, label: String) -> void:
		checks += 1
		if not ok:
			failures.append(label)
			push_error("YARD_WORKER_SPARK_BATCH FAIL " + label)

	func frame(view: SubViewport) -> Image:
		await RenderingServer.frame_post_draw
		await RenderingServer.frame_post_draw
		return view.get_texture().get_image()

	func draws(view: SubViewport) -> int:
		return RenderingServer.viewport_get_render_info(view.get_viewport_rid(),
			RenderingServer.VIEWPORT_RENDER_INFO_TYPE_VISIBLE,
			RenderingServer.VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME)

	func run(tree: SceneTree, capture_dir := "") -> Dictionary:
		var viewport := SubViewport.new()
		viewport.size = Vector2i(256, 256)
		viewport.own_world_3d = true
		viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
		tree.root.add_child(viewport)
		var host := Node3D.new()
		viewport.add_child(host)
		var worker = Worker.new()
		host.add_child(worker)
		# The old boxes inherited this orientation but set their positions in world
		# space; a translated/rotated parent catches a mistaken local trajectory.
		worker.transform = Transform3D(Basis(Vector3.UP, 0.47), Vector3(0.06, 0.03, -0.02))
		var seam := Vector3(-0.08, 1.08, -0.48)
		const TICK := 129
		var state := {"present": true, "supported": true, "position": Vector3(0, 0.9, 0),
			"velocity": Vector3.ZERO, "heading_radians": 0.0, "episode": 1, "native_tick": TICK,
			"activity": 0, "work_point": seam, "look_target": seam}
		for index in 30:
			worker.present(state, 1.0 / 90.0)
		var batch: MultiMeshInstance3D = worker._sparks
		check(worker.welding_active() and batch.visible, "real work enables spark batch")
		check(batch.multimesh.instance_count == 10, "all ten sparks retained")
		check(batch.multimesh.mesh is BoxMesh and batch.multimesh.mesh.size.is_equal_approx(Vector3(0.003, 0.003, 0.018)), "exact original spark box dimensions")
		check(batch.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_ON, "default spark shadow casting retained")
		for child in worker.get_children():
			if child is Node3D and child != batch:
				child.visible = false
		var originals := Node3D.new()
		host.add_child(originals)
		var old_mesh := BoxMesh.new()
		old_mesh.size = Vector3(0.003, 0.003, 0.018)
		for index in 10:
			var age := fposmod(float(TICK) / 90.0 * 3.0 + index * 0.137, 1.0) * 0.16
			var angle := index * 2.399
			var position := seam + Vector3(cos(angle) * age * 0.8, age * 0.6 - 4.9 * age * age, sin(angle) * age * 0.8)
			var previous := MeshInstance3D.new()
			previous.mesh = old_mesh
			previous.material_override = batch.material_override
			originals.add_child(previous)
			previous.global_transform = Transform3D(worker.global_basis, position)
			var actual := batch.global_transform * batch.multimesh.get_instance_transform(index)
			check(actual.origin.distance_to(position) < 0.00001 and actual.basis.is_equal_approx(previous.global_basis), "spark %d preserves original world trajectory and orientation" % index)
		var floor_mesh := BoxMesh.new()
		floor_mesh.size = Vector3(0.6, 0.01, 0.6)
		var floor := MeshInstance3D.new()
		floor.mesh = floor_mesh
		var floor_material := StandardMaterial3D.new()
		floor_material.albedo_color = Color(0.16, 0.18, 0.2)
		floor.material_override = floor_material
		host.add_child(floor)
		floor.position = seam + Vector3(0, -0.05, 0)
		var light := DirectionalLight3D.new()
		host.add_child(light)
		light.rotation_degrees = Vector3(-50, -30, 0)
		light.shadow_enabled = true
		light.directional_shadow_mode = DirectionalLight3D.SHADOW_ORTHOGONAL
		light.directional_shadow_max_distance = 3.0
		var environment := WorldEnvironment.new()
		environment.environment = Environment.new()
		environment.environment.background_mode = Environment.BG_COLOR
		environment.environment.background_color = Color(0.025, 0.025, 0.025)
		environment.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
		environment.environment.ambient_light_color = Color.WHITE
		environment.environment.ambient_light_energy = 0.4
		host.add_child(environment)
		var camera := Camera3D.new()
		host.add_child(camera)
		camera.projection = Camera3D.PROJECTION_ORTHOGONAL
		camera.size = 0.34
		camera.near = 0.01
		camera.far = 3.0
		camera.position = seam + Vector3(0.19, 0.20, 0.3)
		camera.look_at(seam)
		camera.current = true
		batch.visible = false
		var before: Image = await frame(viewport)
		var before_draws := draws(viewport)
		originals.visible = false
		batch.visible = true
		var after: Image = await frame(viewport)
		var after_draws := draws(viewport)
		var maximum := 0.0
		var changed := 0
		var spark_pixels := 0
		for y in before.get_height():
			for x in before.get_width():
				var a := before.get_pixel(x, y)
				var b := after.get_pixel(x, y)
				var error := maxf(absf(a.r-b.r), maxf(absf(a.g-b.g), absf(a.b-b.b)))
				maximum = maxf(maximum, error)
				if error > PIXEL_TOLERANCE:
					changed += 1
				if b.r > 0.6 and b.g > 0.25 and b.r > b.b * 1.4:
					spark_pixels += 1
		check(before_draws > 0 and after_draws > 0 and after_draws < before_draws, "actual batch draw submissions decrease")
		check(spark_pixels > 10, "sparks occupy real visible orange pixels")
		check(changed == 0 and maximum <= PIXEL_TOLERANCE, "paired images match within two 8-bit levels per channel")
		if not capture_dir.is_empty():
			before.save_png(capture_dir.path_join("worker-sparks-individual.png"))
			after.save_png(capture_dir.path_join("worker-sparks-batch.png"))
		var result := {"checks": checks, "failures": failures, "draws_before": before_draws,
			"draws_after": after_draws, "spark_pixels": spark_pixels, "changed_pixels": changed,
			"maximum_channel_error": maximum, "channel_tolerance": PIXEL_TOLERANCE}
		viewport.queue_free()
		return result

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var capture_dir := ""
	for argument in OS.get_cmdline_user_args():
		if argument.begins_with("--capture-dir="):
			capture_dir = argument.trim_prefix("--capture-dir=")
	var result: Dictionary = await Probe.new().run(self, capture_dir)
	print("SCRAPERX_YARD_WORKER_SPARK_BATCH ", JSON.stringify(result))
	print("SCRAPERX_YARD_WORKER_SPARK_BATCH failures=%d" % result.failures.size())
	quit(0 if result.failures.is_empty() else 1)
