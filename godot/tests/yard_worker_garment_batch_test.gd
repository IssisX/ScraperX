extends SceneTree

# Presentation-only pairs use the same posed skeleton, camera and shadow light.
# No native contact/controller result is claimed by these supplied pose states.
class Probe extends RefCounted:
	const Worker := preload("res://presentation/life/yard_worker_view.gd")
	const CHANNEL_TOLERANCE := 2.0 / 255.0
	var checks := 0
	var failures: Array[String] = []

	func check(ok: bool, label: String) -> void:
		checks += 1
		if not ok:
			failures.append(label)
			push_error("YARD_WORKER_GARMENT_BATCH FAIL " + label)

	func frame(viewport: SubViewport) -> Image:
		await RenderingServer.frame_post_draw
		await RenderingServer.frame_post_draw
		return viewport.get_texture().get_image()

	func draws(viewport: SubViewport) -> int:
		return RenderingServer.viewport_get_render_info(viewport.get_viewport_rid(),
			RenderingServer.VIEWPORT_RENDER_INFO_TYPE_VISIBLE,
			RenderingServer.VIEWPORT_RENDER_INFO_DRAW_CALLS_IN_FRAME)

	func check_group(batch: MeshInstance3D, sources: Array) -> void:
		var vertices := PackedVector3Array()
		var bones := PackedInt32Array()
		var weights := PackedFloat32Array()
		var indices := PackedInt32Array()
		for source in sources:
			check(source.skin == batch.skin and source.skeleton == batch.skeleton and source.get_parent() == batch.get_parent(), "joined sources share real Skin, skeleton path and parent")
			check(source.transform.is_equal_approx(Transform3D.IDENTITY) and batch.transform.is_equal_approx(Transform3D.IDENTITY), "joined transforms stay identity")
			check(source.material_override == batch.material_override and source.cast_shadow == batch.cast_shadow and source.layers == batch.layers, "joined material, shadow mode and render layers preserved")
			check(source.mesh.surface_get_format(0) == batch.mesh.surface_get_format(0), "joined vertex format preserved")
			var arrays: Array = source.mesh.surface_get_arrays(0)
			var offset := vertices.size()
			vertices.append_array(arrays[Mesh.ARRAY_VERTEX])
			bones.append_array(arrays[Mesh.ARRAY_BONES])
			weights.append_array(arrays[Mesh.ARRAY_WEIGHTS])
			for index in arrays[Mesh.ARRAY_INDEX]:
				indices.append(index + offset)
		var actual := batch.mesh.surface_get_arrays(0)
		check(actual[Mesh.ARRAY_VERTEX] == vertices, "joined vertex values/order/count unchanged")
		check(actual[Mesh.ARRAY_BONES] == bones and actual[Mesh.ARRAY_WEIGHTS] == weights, "joined bone indices and weights unchanged")
		check(actual[Mesh.ARRAY_INDEX] == indices, "joined triangle indices preserved with exact vertex offsets")

	func run(tree: SceneTree, capture_dir := "") -> Dictionary:
		var viewport := SubViewport.new()
		viewport.size = Vector2i(384, 384)
		viewport.own_world_3d = true
		viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
		tree.root.add_child(viewport)
		var host := Node3D.new()
		viewport.add_child(host)
		var worker = Worker.new()
		host.add_child(worker)
		var state := {"present": true, "supported": true, "position": Vector3(0, 0.9, 0),
			"velocity": Vector3.ZERO, "heading_radians": 0.0, "episode": 1, "native_tick": 100,
			"activity": 0, "work_point": Vector3(-0.08, 1.08, -0.48),
			"guide_point": Vector3(-3, 2, -5), "look_target": Vector3(-1, 1.5, -2)}
		worker.present(state, 1.0 / 90.0)
		var batches := worker.find_children("GarmentBatch*", "MeshInstance3D", true, false)
		var grouped: Array = []
		var grouped_names: Array[String] = []
		for source in worker._body._meshes:
			if not source.visible:
				grouped.append(source)
				grouped_names.append(String(source.name))
		grouped_names.sort()
		check(worker._body._meshes.size() == 8, "all eight original garment sources retained")
		check(batches.size() == 2 and grouped_names == ["jacket", "jacket_inner", "soles", "trousers", "zip"], "only the two expected compatible groups joined")
		for batch in batches:
			check(batch.visible and batch.mesh.get_surface_count() == 1, "each joined group draws one surface")
			var sources: Array = []
			for source in grouped:
				if source.material_override == batch.material_override:
					sources.append(source)
			check(sources.size() >= 2, "batch has multiple matching original sources")
			check_group(batch, sources)
		var floor := MeshInstance3D.new()
		var floor_mesh := BoxMesh.new()
		floor_mesh.size = Vector3(5, 0.1, 5)
		floor.mesh = floor_mesh
		var floor_material := StandardMaterial3D.new()
		floor_material.albedo_color = Color("777d85")
		floor.material_override = floor_material
		host.add_child(floor)
		floor.position.y = -0.05
		var light := DirectionalLight3D.new()
		host.add_child(light)
		light.rotation_degrees = Vector3(-45, -35, 0)
		light.shadow_enabled = true
		light.directional_shadow_mode = DirectionalLight3D.SHADOW_ORTHOGONAL
		light.directional_shadow_max_distance = 8.0
		var environment := WorldEnvironment.new()
		environment.environment = Environment.new()
		environment.environment.background_mode = Environment.BG_COLOR
		environment.environment.background_color = Color("20262c")
		environment.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
		environment.environment.ambient_light_color = Color.WHITE
		environment.environment.ambient_light_energy = 0.4
		host.add_child(environment)
		var camera := Camera3D.new()
		host.add_child(camera)
		camera.projection = Camera3D.PROJECTION_ORTHOGONAL
		camera.size = 2.25
		camera.near = 0.05
		camera.far = 12.0
		camera.position = Vector3(2.1, 1.8, -3.2)
		camera.look_at(Vector3(0, 0.85, 0))
		camera.current = true
		var poses: Array = []
		for activity in [0, 2, 3]:
			var label: String = {0: "working", 2: "pointing", 3: "moving"}[activity]
			state.activity = activity
			state.velocity = Vector3(0.8, 0, -0.6) if activity == 3 else Vector3.ZERO
			state.heading_radians = 0.35 if activity == 2 else 0.0
			for tick in 40:
				state.native_tick += 1
				worker.present(state, 1.0 / 90.0)
			worker.visible = false
			var empty: Image = await frame(viewport)
			worker.visible = true
			for source in grouped:
				source.visible = true
			for batch in batches:
				batch.visible = false
			var before: Image = await frame(viewport)
			var before_draws := draws(viewport)
			for source in grouped:
				source.visible = false
			for batch in batches:
				batch.visible = true
			var after: Image = await frame(viewport)
			var after_draws := draws(viewport)
			var maximum := 0.0
			var changed := 0
			var visible_pixels := 0
			for y in before.get_height():
				for x in before.get_width():
					var a := before.get_pixel(x, y)
					var b := after.get_pixel(x, y)
					var e := empty.get_pixel(x, y)
					var error := maxf(absf(a.r-b.r), maxf(absf(a.g-b.g), absf(a.b-b.b)))
					maximum = maxf(maximum, error)
					if error > CHANNEL_TOLERANCE:
						changed += 1
					if maxf(absf(a.r-e.r), maxf(absf(a.g-e.g), absf(a.b-e.b))) > 8.0 / 255.0:
						visible_pixels += 1
			check(before_draws > after_draws and after_draws > 0, label + " actual draw submissions decrease")
			check(visible_pixels > 2000, label + " substantial visible worker and cast shadow pixels")
			check(changed == 0 and maximum <= CHANNEL_TOLERANCE, label + " image and shadows match within two 8-bit levels per channel")
			poses.append({"pose": label, "draws_before": before_draws, "draws_after": after_draws,
				"visible_pixels": visible_pixels, "changed_pixels": changed, "maximum_channel_error": maximum})
			if not capture_dir.is_empty():
				before.save_png(capture_dir.path_join("worker-garment-%s-original.png" % label))
				after.save_png(capture_dir.path_join("worker-garment-%s-batched.png" % label))
		var result := {"checks": checks, "failures": failures, "poses": poses, "channel_tolerance": CHANNEL_TOLERANCE}
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
	print("SCRAPERX_YARD_WORKER_GARMENT_BATCH ", JSON.stringify(result))
	print("SCRAPERX_YARD_WORKER_GARMENT_BATCH failures=%d" % result.failures.size())
	quit(0 if result.failures.is_empty() else 1)
