extends SceneTree

const Fit := preload("res://presentation/identity/portrait_face_fit.gd")
var failed := false

func check(condition: bool, message: String) -> void:
	if not condition:
		failed = true
		push_error("PORTRAIT_FACE_FIT FAIL " + message)

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var fit := Fit.new()
	var image := Image.create(32, 32, false, Image.FORMAT_RGB8)
	image.fill(Color(0.6, 0.3, 0.2))
	var photo := ImageTexture.create_from_image(image)
	var points := PackedVector2Array([Vector2(0.3, 0.3), Vector2(0.7, 0.35), Vector2(0.53, 0.7)])
	var anchors := [Fit.LEFT_EYE, Fit.RIGHT_EYE, Fit.MOUTH]
	for index in 3:
		check(fit.photo_uv(anchors[index], points).is_equal_approx(points[index]), "affine anchor %d" % index)
	# Independent evidence from GLB vertex/UV inspection: these inner lip
	# vertices border the atlas mouth opening, U=.905022/.913715,V=.51684.
	# The photo mouth must land here, rather than at an oris bone above it.
	var seam := (Vector3(0, 1.547971, 0.151440) + Vector3(0, 1.547965, 0.154638)) * 0.5
	check(fit.photo_uv(seam, points).distance_to(points[2]) < 0.00001, "photo mouth lands on independently measured template lip seam")
	var mesh := ArrayMesh.new()
	mesh.add_blend_shape("smile")
	mesh.blend_shape_mode = Mesh.BLEND_SHAPE_MODE_NORMALIZED
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = PackedVector3Array([Fit.LEFT_EYE, Fit.RIGHT_EYE, Fit.MOUTH, Vector3(0, 1.61, -0.03), Vector3(0.09, 1.61, 0.13), Vector3(0, 1.2, 0.13)])
	arrays[Mesh.ARRAY_NORMAL] = PackedVector3Array([Vector3(0,0,1), Vector3(0,0,1), Vector3(0,0,1), Vector3(0,0,-1), Vector3(0,0,1), Vector3(0,0,1)])
	arrays[Mesh.ARRAY_TANGENT] = PackedFloat32Array([1,0,0,1, 1,0,0,1, 1,0,0,1, 1,0,0,1, 1,0,0,1, 1,0,0,1])
	arrays[Mesh.ARRAY_TEX_UV] = PackedVector2Array([Vector2.ZERO, Vector2.RIGHT, Vector2.DOWN, Vector2.ONE, Vector2(0.2,0.2), Vector2(0.4,0.4)])
	arrays[Mesh.ARRAY_BONES] = PackedInt32Array([0,1,2,3, 0,1,2,3, 0,1,2,3, 0,1,2,3, 0,1,2,3, 0,1,2,3])
	arrays[Mesh.ARRAY_WEIGHTS] = PackedFloat32Array([0.5,0.25,0.125,0.125, 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1, 1,0,0,0])
	arrays[Mesh.ARRAY_INDEX] = PackedInt32Array([0,1,2,3,4,5])
	var morph := []
	morph.resize(Mesh.ARRAY_MAX)
	morph[Mesh.ARRAY_VERTEX] = arrays[Mesh.ARRAY_VERTEX].duplicate()
	morph[Mesh.ARRAY_VERTEX][2] += Vector3(0,0.002,0)
	morph[Mesh.ARRAY_NORMAL] = arrays[Mesh.ARRAY_NORMAL].duplicate()
	morph[Mesh.ARRAY_TANGENT] = arrays[Mesh.ARRAY_TANGENT].duplicate()
	var shapes: Array[Array] = [morph]
	var lods := {0.2: PackedInt32Array([0,1,2])}
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, shapes, lods)
	var skin := StandardMaterial3D.new()
	skin.resource_name = Fit.SKIN_MATERIAL
	skin.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	skin.cull_mode = BaseMaterial3D.CULL_DISABLED
	skin.albedo_texture = photo
	skin.normal_enabled = true
	skin.normal_texture = photo
	skin.normal_scale = 0.7
	skin.roughness_texture = photo
	skin.roughness_texture_channel = BaseMaterial3D.TEXTURE_CHANNEL_GREEN
	mesh.surface_set_material(0, skin)
	mesh.surface_set_name(0, "skin")
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, shapes, lods)
	var cloth := StandardMaterial3D.new()
	cloth.resource_name = "cloth"
	mesh.surface_set_material(1, cloth)
	var before := mesh.surface_get_arrays(0)
	var before_cloth := mesh.surface_get_arrays(1)
	var before_shapes := mesh.surface_get_blend_shape_arrays(0)
	var before_lods := fit._surface_lods(mesh, 0)
	var fitted := fit.fit_mesh(mesh, photo, points)
	check(fitted != null, "valid mesh accepted: " + fit.last_error)
	if fitted != null:
		var after := fitted.surface_get_arrays(0)
		for slot in [Mesh.ARRAY_VERTEX, Mesh.ARRAY_NORMAL, Mesh.ARRAY_TANGENT, Mesh.ARRAY_TEX_UV, Mesh.ARRAY_BONES, Mesh.ARRAY_WEIGHTS, Mesh.ARRAY_INDEX]:
			check(after[slot] == before[slot], "preserved data slot %d" % slot)
		check(fitted.surface_get_blend_shape_arrays(0) == before_shapes, "blend shape data unchanged")
		check(fitted.get_blend_shape_name(0) == "smile" and fitted.blend_shape_mode == mesh.blend_shape_mode, "blend shape contract unchanged")
		check(fit._surface_lods(fitted, 0) == before_lods and not before_lods.is_empty(), "LOD indices preserved")
		check(fitted.surface_get_arrays(1) == before_cloth and fitted.surface_get_material(1) == cloth, "cloth untouched")
		check(mesh.surface_get_arrays(0) == before and mesh.surface_get_material(0) == skin, "source untouched")
		for index in 3:
			check(after[Mesh.ARRAY_TEX_UV2][index].distance_to(points[index]) < 0.00001, "baked anchor %d" % index)
			check(after[Mesh.ARRAY_COLOR][index].a > 0.5, "front face receives portrait %d" % index)
		for index in range(3,6):
			check(after[Mesh.ARRAY_COLOR][index].a == 0, "back/ear/body excluded %d" % index)
		var material: ShaderMaterial = fitted.surface_get_material(0)
		check(not material.shader.code.contains("depth_prepass_alpha"), "ordinary alpha preserves absence of depth prepass")
		check(material.get_shader_parameter("portrait_texture") == photo, "photo bound")
		check(material.get_shader_parameter("skin_albedo") == skin.albedo_texture and material.get_shader_parameter("skin_normal") == skin.normal_texture, "skin textures retained")
		check(is_equal_approx(material.get_shader_parameter("normal_scale"), 0.7), "normal scale retained")
		check(material.get_shader_parameter("roughness_channel") == Vector4(0,1,0,0), "roughness channel retained")
		check(material.get_shader_parameter("skin_tint").is_equal_approx(Vector3.ONE), "matching photo and source skin retain original tone")
		var dark_image := Image.create(32, 32, false, Image.FORMAT_RGB8)
		dark_image.fill(Color(0.3, 0.15, 0.1))
		var dark_photo := ImageTexture.create_from_image(dark_image)
		var tinted := fit.skin_material(skin, dark_photo, points)
		var tint: Vector3 = tinted.get_shader_parameter("skin_tint")
		check(tint.x > 0.15 and tint.x < 0.5 and tint.y > 0.15 and tint.y < 0.5 and tint.z > 0.15 and tint.z < 0.5, "darker photo shades exposed template skin through multiplicative tint")
		check(tinted.get_shader_parameter("skin_albedo") == photo, "tint keeps original skin detail texture")

	for invalid in [PackedVector2Array(), PackedVector2Array([Vector2.ZERO, Vector2.ZERO, Vector2.ONE]), PackedVector2Array([Vector2(0.3,0.3), Vector2(0.7,0.3), Vector2(0.5,0.3)]), PackedVector2Array([Vector2(NAN,0.3),Vector2(0.7,0.3),Vector2(0.5,0.7)]), PackedVector2Array([Vector2(-0.1,0.3),Vector2(0.7,0.3),Vector2(0.5,0.7)])]:
		check(fit.fit_mesh(mesh, photo, invalid) == null and not fit.last_error.is_empty(), "invalid landmarks rejected")
	check(fit.fit_mesh(null, photo, points) == null and not fit.last_error.is_empty(), "missing source rejected")
	check(fit.fit_mesh(mesh, null, points) == null, "missing photo rejected")
	skin.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_DEPTH_PRE_PASS
	var prepass_fit := fit.fit_mesh(mesh, photo, points)
	check(prepass_fit != null, "actual imported alpha depth prepass mode accepted: " + fit.last_error)
	if prepass_fit != null:
		var prepass_material: ShaderMaterial = prepass_fit.surface_get_material(0)
		check(prepass_material.shader.code.contains("depth_prepass_alpha"), "imported alpha depth prepass retained")
	check(mesh.surface_get_material(0) == skin and skin.transparency == BaseMaterial3D.TRANSPARENCY_ALPHA_DEPTH_PRE_PASS, "fitting preserves source material and transparency")
	skin.transparency = BaseMaterial3D.TRANSPARENCY_DISABLED
	check(fit.fit_mesh(mesh, photo, points) == null and not fit.last_error.is_empty(), "incompatible transparency explicitly rejected")
	skin.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	skin.cull_mode = BaseMaterial3D.CULL_BACK
	check(fit.fit_mesh(mesh, photo, points) == null, "incompatible culling explicitly rejected")
	skin.cull_mode = BaseMaterial3D.CULL_DISABLED
	skin.resource_name = "unapproved"
	check(fit.fit_mesh(mesh, photo, points) == null, "unapproved skin rejected")
	print("PORTRAIT_FACE_FIT ", "FAIL" if failed else "PASS")
	quit(1 if failed else 0)
