extends RefCounted

# Isolated native cabinet/duct/vent geometry, with the shipping palette.
# No player stepping, route completion or whole-world performance claim.
func execute(tree: SceneTree) -> Dictionary:
	var failures: Array[String] = []
	var finishes = load("res://presentation/materials/freight_materials.gd").new()
	var fixture := Node3D.new()
	fixture.name = "ServiceFinishFixture"
	tree.root.add_child(fixture)
	tree.root.size = Vector2i(960, 720)
	tree.root.disable_3d = false
	var palette: Array[Material] = []
	for color in ["687174", "86472d", "4a3420", "4e4841", "a04d16", "879394", "5b544b", "c19a2a"]:
		var material := StandardMaterial3D.new()
		material.albedo_color = Color(color)
		material.roughness = 0.78
		palette.append(material)
	var native: Object = ClassDB.instantiate("ScraperXSimulation")
	var instances: Array[MeshInstance3D] = []
	var slots: Array[Vector2i] = []
	var entities: Array[int] = []
	for body in int(native.get_kit_body_count()):
		var entity := int(native.get_kit_body_entity_id(body))
		if entity not in [2560, 2561, 2562]:
			continue
		entities.append(entity)
		var owner := Node3D.new()
		fixture.add_child(owner)
		owner.transform = native.get_kit_body_transform(body)
		owner.position -= Vector3(20, 11, -123)
		var parts: PackedFloat32Array = native.get_kit_body_parts(body)
		var groups: Dictionary = {}
		for p in range(0, parts.size(), 13):
			var index := int(parts[p + 10])
			if int(parts[p + 11]) != 0:
				failures.append("unexpected non-box native service part")
			var box := BoxMesh.new()
			box.size = Vector3(parts[p], parts[p + 1], parts[p + 2]) * 2.0
			var local := Transform3D(Basis(Quaternion(parts[p + 6], parts[p + 7], parts[p + 8], parts[p + 9])),
				Vector3(parts[p + 3], parts[p + 4], parts[p + 5]))
			if not groups.has(index):
				var surface := SurfaceTool.new()
				surface.begin(Mesh.PRIMITIVE_TRIANGLES)
				groups[index] = surface
			groups[index].append_from(box, 0, local)
		for index in groups:
			var instance := MeshInstance3D.new()
			instance.mesh = groups[index].commit()
			instance.material_override = palette[index]
			owner.add_child(instance)
			instances.append(instance)
			slots.append(Vector2i(entity, index))
	native = null
	if entities != [2560, 2561, 2562]:
		failures.append("missing actual native service body")
	var environment := WorldEnvironment.new()
	environment.environment = Environment.new()
	var sky := Sky.new()
	sky.sky_material = ProceduralSkyMaterial.new()
	environment.environment.sky = sky
	environment.environment.background_mode = Environment.BG_SKY
	environment.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	environment.environment.ambient_light_color = Color("c0c9d0")
	environment.environment.ambient_light_energy = 0.65
	fixture.add_child(environment)
	var light := DirectionalLight3D.new()
	light.rotation_degrees = Vector3(-38, -35, 0)
	light.light_energy = 1.4
	light.shadow_enabled = true
	fixture.add_child(light)
	var camera := Camera3D.new()
	fixture.add_child(camera)
	camera.position = Vector3(9, 5, 17)
	camera.look_at(Vector3(1, 5.3, 0))
	camera.current = true
	await RenderingServer.frame_post_draw
	await RenderingServer.frame_post_draw
	var before := tree.root.get_texture().get_image()
	before.save_png("user://service_finish_before.png")
	for i in instances.size():
		var slot := slots[i]
		var source := palette[slot.y] as StandardMaterial3D
		var finish: Material = finishes.for_part(slot.x, slot.y, source, true)
		instances[i].material_override = finish
		if slot.y in [0, 5]:
			if finish == source or not finish is StandardMaterial3D:
				failures.append("service finish not applied")
			elif not finish.normal_enabled or finish.normal_texture == null:
				failures.append("service finish lacks real normal texture")
			if finish != finishes.for_part(slot.x, slot.y, source, true):
				failures.append("service finish not cached")
		elif finish != source:
			failures.append("unrelated service material changed")
	for index in [0, 1, 5, 7]:
		var finish: Material = finishes.for_part(2440, index, palette[index], true)
		if not finish is ShaderMaterial or finish.get_shader_parameter("base_color") != palette[index].albedo_color:
			failures.append("original freight finish selection changed")
	if finishes.for_part(1935, 0, palette[0], true) != palette[0]:
		failures.append("unrelated machine changed")
	# Existing source bump/projection remains intact when one is supplied.
	var detailed := palette[0].duplicate() as StandardMaterial3D
	detailed.normal_enabled = true
	detailed.normal_texture = ImageTexture.create_from_image(Image.create(8, 8, false, Image.FORMAT_RGB8))
	detailed.uv1_scale = Vector3(2, 3, 4)
	var inherited: StandardMaterial3D = finishes.for_part(2560, 0, detailed, true)
	if inherited.normal_texture != detailed.normal_texture or inherited.uv1_scale != detailed.uv1_scale:
		failures.append("source bump/projection lost")
	await tree.process_frame
	await RenderingServer.frame_post_draw
	await RenderingServer.frame_post_draw
	var after := tree.root.get_texture().get_image()
	after.save_png("user://service_finish_after.png")
	if before.get_data() == after.get_data():
		failures.append("actual rendered pixels unchanged")
	return {"failures": failures, "entities": entities, "native_material_groups": instances.size(),
		"draw_groups_unchanged": true, "rendered": true, "gameplay": false,
		"before": ProjectSettings.globalize_path("user://service_finish_before.png"),
		"after": ProjectSettings.globalize_path("user://service_finish_after.png")}
