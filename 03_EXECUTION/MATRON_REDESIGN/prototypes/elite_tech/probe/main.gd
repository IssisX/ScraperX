extends Node3D
# Elite-technique probes for the Matron, Godot 4.7 gl_compatibility under Xvfb.
# PROBE=<name> DATA=<data dir> OUT=<render dir>; each probe saves PNGs and
# prints "METRIC ..." lines. Run through run.sh.

var DATA := OS.get_environment("DATA")
var OUT := OS.get_environment("OUT")
var PROBE := OS.get_environment("PROBE")
var meta := {}
var eyes_meta := {}
var _tex_cache := {}
const HEAD_LAYER := 1
const EYE_LAYER := 4
const HER_LAYER := 2

func _ready() -> void:
	print("ADAPTER ", RenderingServer.get_video_adapter_name(), " | ", RenderingServer.get_video_adapter_vendor(), " | ", RenderingServer.get_video_adapter_api_version())
	print("METHOD ", RenderingServer.get_current_rendering_method())
	meta = JSON.parse_string(FileAccess.get_file_as_string(DATA + "/head_meta.json"))
	eyes_meta = JSON.parse_string(FileAccess.get_file_as_string(DATA + "/eyes.json"))
	DirAccess.make_dir_recursive_absolute(OUT)
	RenderingServer.viewport_set_measure_render_time(get_viewport().get_viewport_rid(), true)
	match PROBE:
		"skin": await p_skin()
		"blend": await p_blend()
		"eyes": await p_eyes()
		"hair": await p_hair()
		"cloth": await p_cloth()
		"screen": await p_screen()
		"micro": await p_micro()
		"capsule": await p_capsule()
		"rig": await p_rig()
		"lod": await p_lod()
		"quality": await p_quality()
		"hero": await p_hero()
		"dbg": await p_dbg()
		"alpha": await p_alpha()
		"ssao": await p_ssao()
		"tf": await p_tf()
		_: push_error("unknown probe " + PROBE)
	get_tree().quit()

# ------------------------------------------------------------------ io
func f32(path: String) -> PackedFloat32Array:
	return FileAccess.get_file_as_bytes(path).to_float32_array()

func i32(path: String) -> PackedInt32Array:
	return FileAccess.get_file_as_bytes(path).to_int32_array()

func v3(a: PackedFloat32Array) -> PackedVector3Array:
	var out := PackedVector3Array()
	out.resize(a.size() / 3)
	for k in out.size():
		out[k] = Vector3(a[3 * k], a[3 * k + 1], a[3 * k + 2])
	return out

func v2(a: PackedFloat32Array) -> PackedVector2Array:
	var out := PackedVector2Array()
	out.resize(a.size() / 2)
	for k in out.size():
		out[k] = Vector2(a[2 * k], a[2 * k + 1])
	return out

func tex(name: String, mip := true) -> ImageTexture:
	var key := name + str(mip)
	if _tex_cache.has(key):
		return _tex_cache[key]
	var img := Image.load_from_file(DATA + "/tex/" + name)
	if mip:
		img.generate_mipmaps()
	var t := ImageTexture.create_from_image(img)
	_tex_cache[key] = t
	return t

func shader(name: String, defines := "") -> Shader:
	var s := Shader.new()
	var code := FileAccess.get_file_as_string(get_script().resource_path.get_base_dir() + "/shaders/" + name)
	s.code = code.replace("//@QUALITY@", defines)
	return s

# ------------------------------------------------------------------ the head
# Code-built ArrayMesh: positions, normals, tangents, uvs, colors (curvature,
# AO, thickness), CUSTOM0 (rest position + pulse mask), 3-bone skin, and
# RELATIVE blend shapes.
func head_mesh(lod: String, with_shapes := true) -> ArrayMesh:
	var d := DATA + "/" + lod
	var m: Dictionary = JSON.parse_string(FileAccess.get_file_as_string(d + "/meta.json"))
	var pos := v3(f32(d + "/pos.f32"))
	var n := pos.size()
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = pos
	arrays[Mesh.ARRAY_NORMAL] = v3(f32(d + "/nrm.f32"))
	arrays[Mesh.ARRAY_TEX_UV] = v2(f32(d + "/uv.f32"))
	var col := f32(d + "/col.f32")
	var colors := PackedColorArray()
	colors.resize(n)
	var curv := PackedFloat32Array()
	if FileAccess.file_exists(d + "/curv.f32"):
		curv = f32(d + "/curv.f32")
	for k in n:
		var c := clampf(curv[k] / 2.0, 0.0, 1.0) if curv.size() == n else clampf(col[4 * k] / 2.0, 0.0, 1.0)
		colors[k] = Color(c, col[4 * k + 1], col[4 * k + 2], col[4 * k + 3])
	arrays[Mesh.ARRAY_COLOR] = colors
	if FileAccess.file_exists(d + "/tan.f32"):
		arrays[Mesh.ARRAY_TANGENT] = f32(d + "/tan.f32")
	# CUSTOM0: rest position and the carotid mask
	var cust := PackedFloat32Array()
	cust.resize(n * 4)
	for k in n:
		var p := pos[k]
		var ax := absf(p.x)
		var mask := exp(-pow((ax - 0.03) / 0.011, 2.0) - pow((p.y - 0.535) / 0.022, 2.0)) * smoothstep(0.03, 0.07, p.z)
		cust[4 * k] = p.x
		cust[4 * k + 1] = p.y
		cust[4 * k + 2] = p.z
		cust[4 * k + 3] = mask
	arrays[Mesh.ARRAY_CUSTOM0] = cust
	if FileAccess.file_exists(d + "/bones.i32"):
		arrays[Mesh.ARRAY_BONES] = i32(d + "/bones.i32")
		arrays[Mesh.ARRAY_WEIGHTS] = f32(d + "/weights.f32")
	arrays[Mesh.ARRAY_INDEX] = i32(d + "/idx.i32")
	var mesh := ArrayMesh.new()
	var shapes := []
	if with_shapes:
		mesh.blend_shape_mode = Mesh.BLEND_SHAPE_MODE_RELATIVE
		# Found here: positions blend as deltas (RELATIVE), but normals and
		# tangents blend as absolute values, base*(1 - sum w) + sum(w * shape).
		# Zero normal "deltas" black the face out as weights rise, so each
		# shape carries full normals (its own, when baked, else the base's).
		var zero_n := PackedVector3Array()
		zero_n.resize(n)
		var zero_t := PackedFloat32Array()
		zero_t.resize(n * 4)
		var mode := OS.get_environment("BS_NORMALS")
		for s in m["shapes"]:
			mesh.add_blend_shape(s)
			var bs := []
			bs.resize(Mesh.ARRAY_MAX)
			bs[Mesh.ARRAY_VERTEX] = v3(f32(d + "/bs_%s.f32" % s))
			if mode == "zero":
				bs[Mesh.ARRAY_NORMAL] = zero_n
			elif FileAccess.file_exists(d + "/bsn_%s.f32" % s) and mode != "base":
				bs[Mesh.ARRAY_NORMAL] = v3(f32(d + "/bsn_%s.f32" % s))
			else:
				bs[Mesh.ARRAY_NORMAL] = arrays[Mesh.ARRAY_NORMAL]
			if arrays[Mesh.ARRAY_TANGENT] != null:
				bs[Mesh.ARRAY_TANGENT] = zero_t if mode == "zero" else arrays[Mesh.ARRAY_TANGENT]
			shapes.append(bs)
	var fmt := Mesh.ARRAY_CUSTOM_RGBA_FLOAT << Mesh.ARRAY_FORMAT_CUSTOM0_SHIFT
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays, shapes, {}, fmt)
	return mesh

func skin_material(quality := 2) -> ShaderMaterial:
	var mat := ShaderMaterial.new()
	mat.shader = shader("skin.gdshader", "#define QUALITY %d" % quality)
	mat.set_shader_parameter("albedo_tex", tex("head_albedo.png"))
	mat.set_shader_parameter("skin_lut", tex("skin_lut.png", false))
	mat.set_shader_parameter("pores_n", tex("pores_n.png"))
	return mat

# Skeleton with three bones (neck_lo, neck_hi, head) and the head under it.
func rig(parent: Node3D, mesh: ArrayMesh, mat: Material) -> Dictionary:
	var bones: Dictionary = meta["bones"]
	var sk := Skeleton3D.new()
	parent.add_child(sk)
	sk.add_bone("neck_lo")
	sk.add_bone("neck_hi")
	sk.add_bone("head")
	sk.set_bone_parent(1, 0)
	sk.set_bone_parent(2, 1)
	sk.set_bone_rest(0, Transform3D(Basis(), Vector3(0, bones["neck_lo"], 0.03)))
	sk.set_bone_rest(1, Transform3D(Basis(), Vector3(0, bones["neck_hi"] - bones["neck_lo"], 0.012)))
	sk.set_bone_rest(2, Transform3D(Basis(), Vector3(0, bones["head"] - bones["neck_hi"], 0.03)))
	sk.reset_bone_poses()
	var skin := Skin.new()
	for b in 3:
		skin.add_named_bind(sk.get_bone_name(b), sk.get_bone_global_rest(b).affine_inverse())
	var mi := MeshInstance3D.new()
	mi.mesh = mesh
	mi.material_override = mat
	sk.add_child(mi)
	mi.skin = skin
	mi.skeleton = NodePath("..")
	return {"skeleton": sk, "mesh": mi}

func shape(mi: MeshInstance3D, name: String, w: float) -> void:
	var idx := mi.find_blend_shape_by_name(name)
	if idx >= 0:
		mi.set_blend_shape_value(idx, w)

# ------------------------------------------------------------------ scene bits
func environment(ambient := Color(0.045, 0.05, 0.06), energy := 1.0) -> WorldEnvironment:
	var env := WorldEnvironment.new()
	env.environment = Environment.new()
	env.environment.background_mode = Environment.BG_COLOR
	env.environment.background_color = Color(0.02, 0.02, 0.024)
	env.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.environment.ambient_light_color = ambient
	env.environment.ambient_light_energy = energy
	env.environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	add_child(env)
	return env

# The locker's caged work bulb (warm, above and to her left), a cold fill
# from the open steel to her right.
func locker_lights(scale := 1.0) -> Array:
	var bulb := OmniLight3D.new()
	bulb.light_color = Color("ffd8a8")
	bulb.light_energy = 1.5 * scale
	bulb.omni_range = 5.0
	bulb.omni_attenuation = 1.2
	bulb.position = Vector3(-0.55, 1.15, 0.5)
	add_child(bulb)
	var fill := OmniLight3D.new()
	fill.light_color = Color("a9bdd6")
	fill.light_energy = 0.35 * scale
	fill.omni_range = 6.0
	fill.position = Vector3(1.2, 0.6, 0.9)
	add_child(fill)
	return [bulb, fill]

func camera(at: Vector3, look: Vector3, fov := 30.0) -> Camera3D:
	var cam := Camera3D.new()
	cam.fov = fov
	cam.near = 0.02
	add_child(cam)
	cam.look_at_from_position(at, look)
	cam.current = true
	return cam

func shot(name: String, frames := 3) -> void:
	for i in frames:
		await get_tree().process_frame
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png(OUT + "/" + name + ".png")
	print("SHOT ", name)

func clear_scene() -> void:
	for c in get_children():
		c.queue_free()
	await get_tree().process_frame

# frame timing over N frames: CPU wall and GPU (timer query) per frame
func timing(label: String, frames := 60) -> void:
	var rid := get_viewport().get_viewport_rid()
	for i in 5:
		await get_tree().process_frame
	var gpu := 0.0
	var cpu := 0.0
	var t0 := Time.get_ticks_usec()
	for i in frames:
		await RenderingServer.frame_post_draw
		gpu += RenderingServer.viewport_get_measured_render_time_gpu(rid)
		cpu += RenderingServer.viewport_get_measured_render_time_cpu(rid)
	var wall := float(Time.get_ticks_usec() - t0) / 1000.0 / frames
	print("METRIC %s wall_ms=%.2f gpu_ms=%.3f cpu_ms=%.3f draws=%d prims=%d objs=%d" % [label, wall, gpu / frames, cpu / frames,
		Performance.get_monitor(Performance.RENDER_TOTAL_DRAW_CALLS_IN_FRAME),
		Performance.get_monitor(Performance.RENDER_TOTAL_PRIMITIVES_IN_FRAME),
		Performance.get_monitor(Performance.RENDER_TOTAL_OBJECTS_IN_FRAME)])

# ------------------------------------------------------------------ probes
# Skin: the same head, the same lights; StandardMaterial3D vs the custom
# light() skin at three quality levels, front and back-lit.
func p_skin() -> void:
	var variants := ["standard", "q0", "q1", "q2"]
	for v in variants:
		await clear_scene()
		environment()
		locker_lights()
		var mat: Material
		if v == "standard":
			var sm := StandardMaterial3D.new()
			sm.albedo_texture = tex("head_albedo.png")
			sm.roughness = 0.6
			mat = sm
		else:
			mat = skin_material(int(v.substr(1)))
		var r := rig(self, head_mesh("lod0"), mat)
		camera(Vector3(0.22, 0.67, 0.62), Vector3(0.0, 0.645, 0.1), 28.0)
		await shot("skin_%s_front" % v)
		await timing("skin_" + v, 40)
		# back light: a bare bulb behind her right ear
		await clear_scene()
		environment(Color(0.02, 0.02, 0.025))
		var back := OmniLight3D.new()
		back.light_color = Color("ffe0b8")
		back.light_energy = 2.0
		back.omni_range = 3.0
		back.position = Vector3(-0.25, 0.7, -0.25)
		add_child(back)
		var weak := OmniLight3D.new()
		weak.light_energy = 0.15
		weak.position = Vector3(0.6, 0.6, 0.8)
		add_child(weak)
		if v != "standard":
			mat = skin_material(int(v.substr(1)))
		rig(self, head_mesh("lod0"), mat)
		camera(Vector3(-0.42, 0.67, 0.42), Vector3(0.0, 0.66, 0.06), 28.0)
		await shot("skin_%s_back" % v)

func p_blend() -> void:
	environment()
	locker_lights()
	var r := rig(self, head_mesh("lod0"), skin_material(2))
	var mi: MeshInstance3D = r["mesh"]
	var sk: Skeleton3D = r["skeleton"]
	camera(Vector3(0.0, 0.66, 0.75), Vector3(0.0, 0.64, 0.08), 32.0)
	await shot("blend_0_neutral")
	shape(mi, "blink_l", 1.0)
	shape(mi, "brow_r", 0.6)
	await shot("blend_1_wink_wrong")
	shape(mi, "blink_l", 0.0)
	shape(mi, "brow_r", 0.0)
	for s in ["jaw_open", "lips_back", "wide_l", "wide_r", "brow_l", "brow_r"]:
		shape(mi, s, 1.0)
	await shot("blend_2_shriek")
	for s in ["jaw_open", "lips_back", "wide_l", "wide_r", "brow_l", "brow_r"]:
		shape(mi, s, 0.0)
	shape(mi, "smile", 0.7)
	await shot("blend_3_smile_no_eyes")
	shape(mi, "smile", 0.0)
	# the Turn's look back: 2.6 rad shared over three bones, plus a roll
	sk.set_bone_pose_rotation(0, Quaternion(Vector3.UP, 0.7))
	sk.set_bone_pose_rotation(1, Quaternion(Vector3.UP, 0.9) * Quaternion(Vector3.FORWARD, 0.15))
	sk.set_bone_pose_rotation(2, Quaternion(Vector3.UP, 1.0) * Quaternion(Vector3.FORWARD, 0.35))
	await shot("blend_4_neck_twist")
	await timing("blend_skinned_lod0", 40)

# ------------------------------------------------------------------ parts on the head bone
func head_bone(sk: Skeleton3D) -> BoneAttachment3D:
	var ba := BoneAttachment3D.new()
	sk.add_child(ba)
	ba.bone_name = "head"
	return ba

func head_rest_origin(sk: Skeleton3D) -> Vector3:
	return sk.get_bone_global_rest(2).origin

func eye_material() -> ShaderMaterial:
	var m := ShaderMaterial.new()
	m.shader = shader("eye.gdshader")
	return m

func add_eyes(sk: Skeleton3D, look_at_pos: Vector3, mat_l: Material = null, mat_r: Material = null) -> Array:
	var ba := head_bone(sk)
	var o := head_rest_origin(sk)
	var out := []
	for side in ["l", "r"]:
		var e: Dictionary = eyes_meta[side]
		var c: Array = e["centre"]
		var r := float(e["radius"])
		var mi := MeshInstance3D.new()
		var sph := SphereMesh.new()
		sph.radius = r
		sph.height = 2.0 * r
		sph.radial_segments = 40
		sph.rings = 24
		mi.mesh = sph
		var m: Material = (mat_l if side == "l" else mat_r)
		if m == null:
			m = eye_material()
		mi.material_override = m
		mi.layers = EYE_LAYER
		ba.add_child(mi)
		mi.position = Vector3(c[0], c[1], c[2]) - o
		out.append(mi)
	await get_tree().process_frame
	for mi in out:
		mi.look_at(look_at_pos, Vector3.UP, true)
	return out

func card_mesh(dir: String, bone := 2) -> ArrayMesh:
	var d := DATA + "/" + dir
	var pos := v3(f32(d + "/pos.f32"))
	var n := pos.size()
	var arrays := []
	arrays.resize(Mesh.ARRAY_MAX)
	arrays[Mesh.ARRAY_VERTEX] = pos
	arrays[Mesh.ARRAY_NORMAL] = v3(f32(d + "/nrm.f32"))
	arrays[Mesh.ARRAY_TEX_UV] = v2(f32(d + "/uv.f32"))
	arrays[Mesh.ARRAY_TANGENT] = f32(d + "/tan.f32")
	var b := PackedInt32Array()
	var w := PackedFloat32Array()
	b.resize(n * 4)
	w.resize(n * 4)
	for k in n:
		b[4 * k] = bone
		w[4 * k] = 1.0
	arrays[Mesh.ARRAY_BONES] = b
	arrays[Mesh.ARRAY_WEIGHTS] = w
	arrays[Mesh.ARRAY_INDEX] = i32(d + "/idx.i32")
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	return mesh

func hair_material(mode := 1, kk := 1, texname := "ponytail01_diffuse.png") -> ShaderMaterial:
	var m := ShaderMaterial.new()
	m.shader = shader("hair.gdshader", "#define HAIR_MODE %d\n#define KK %d" % [mode, kk])
	m.set_shader_parameter("hair_tex", tex(texname))
	return m

func add_cards(sk: Skeleton3D, dir: String, mat: Material) -> MeshInstance3D:
	var mi := MeshInstance3D.new()
	mi.mesh = card_mesh(dir)
	mi.material_override = mat
	sk.add_child(mi)
	mi.skin = (sk.get_child(0) as MeshInstance3D).skin
	mi.skeleton = NodePath("..")
	return mi

func wall(z := -0.35, size := Vector2(3, 3)) -> MeshInstance3D:
	var img := Image.create(256, 256, false, Image.FORMAT_RGB8)
	for y in 256:
		for x in 256:
			var v := 0.16 if ((x / 16 + y / 16) % 2 == 0) else 0.1
			img.set_pixel(x, y, Color(v * 1.1, v, v * 0.9))
	var m := StandardMaterial3D.new()
	m.albedo_texture = ImageTexture.create_from_image(img)
	m.roughness = 0.85
	var q := MeshInstance3D.new()
	var pm := QuadMesh.new()
	pm.size = size
	q.mesh = pm
	q.material_override = m
	add_child(q)
	q.position = Vector3(0, 0.6, z)
	return q

# ------------------------------------------------------------------ probes
func p_eyes() -> void:
	var cam_at := Vector3(0.05, 0.668, 0.40)
	var cases := [["painted", 0.0, false, false], ["refracted", 1.0, false, false], ["refracted_glint", 1.0, true, false], ["changed_glint", 1.0, true, true]]
	for c in cases:
		await clear_scene()
		environment()
		locker_lights(0.35)
		var r := rig(self, head_mesh("lod0"), skin_material(2))
		var sk: Skeleton3D = r["skeleton"]
		var ml := eye_material()
		var mr := eye_material()
		for m in [ml, mr]:
			m.set_shader_parameter("refraction", c[1])
		if c[3]:
			ml.set_shader_parameter("milky", 0.75)
			ml.set_shader_parameter("pupil", 0.55)
			mr.set_shader_parameter("pupil", 0.16)
			for m in [ml, mr]:
				m.set_shader_parameter("bloodshot", 0.9)
		var eyes := await add_eyes(sk, cam_at + Vector3(0.0, 0.0, 0.0), ml, mr)
		if c[2]:
			var glint := OmniLight3D.new()
			glint.light_cull_mask = EYE_LAYER
			glint.light_color = Color("e8f0ff")
			glint.light_energy = 0.08
			glint.light_specular = 6.0
			glint.omni_range = 2.0
			glint.position = cam_at + Vector3(0.12, 0.15, 0.05)
			add_child(glint)
		camera(cam_at, Vector3(0.0, 0.665, 0.14), 20.0)
		await shot("eyes_" + c[0])
	# side view: refraction keeps the iris under the cornea
	for refr in [0.0, 1.0]:
		await clear_scene()
		environment()
		locker_lights(0.35)
		var r2 := rig(self, head_mesh("lod0"), skin_material(2))
		var m2 := eye_material()
		m2.set_shader_parameter("refraction", refr)
		var at := Vector3(0.2, 0.672, 0.27)
		await add_eyes(r2["skeleton"], Vector3(0.0, 0.666, 1.0), m2, m2)
		camera(at, Vector3(0.0288, 0.666, 0.146), 16.0)
		await shot("eyes_side_refr%d" % int(refr))
	await timing("eyes_closeup", 30)

func p_hair() -> void:
	var cases := [["scissor", 0, 1, false], ["hash", 1, 1, false], ["a2c_msaa4", 2, 1, true], ["blend_prepass", 3, 1, false], ["hash_noKK", 1, 0, false], ["hash_thinned", 1, 1, false]]
	for c in cases:
		await clear_scene()
		get_viewport().msaa_3d = Viewport.MSAA_4X if c[3] else Viewport.MSAA_DISABLED
		environment()
		locker_lights(0.3)
		var key := OmniLight3D.new()
		key.light_color = Color("ffe2c0")
		key.light_energy = 0.7
		key.omni_range = 4.0
		key.position = Vector3(-0.6, 1.1, -0.5)
		add_child(key)
		var r := rig(self, head_mesh("lod0"), skin_material(2))
		var hm := hair_material(c[1], c[2])
		if c[0] == "hash_thinned":
			hm.set_shader_parameter("thin", 0.45)
		add_cards(r["skeleton"], "hair", hm)
		add_cards(r["skeleton"], "brows", hair_material(c[1], c[2], "eyebrow010.png"))
		camera(Vector3(-0.42, 0.78, -0.38), Vector3(0.0, 0.66, 0.02), 34.0)
		await shot("hair_" + c[0])
		await timing("hair_" + c[0], 30)
	get_viewport().msaa_3d = Viewport.MSAA_DISABLED

func cloth_mesh() -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var nu := 96
	var nv := 40
	for j in nv:
		for i in nu:
			for q in [[0, 0], [1, 0], [1, 1], [0, 0], [1, 1], [0, 1]]:
				var u := float(i + q[0]) / nu
				var v := float(j + q[1]) / nv
				var th := u * PI * 1.2 - PI * 0.6
				var rad := 0.18 + 0.012 * sin(th * 9.0 + v * 4.0) * v + 0.02 * v
				var y := -v * 0.4
				st.set_uv(Vector2(u, v))
				st.add_vertex(Vector3(sin(th) * rad, y, cos(th) * rad))
	st.generate_normals()
	return st.commit()

func p_cloth() -> void:
	environment()
	var key := OmniLight3D.new()
	key.light_color = Color("ffe2c0")
	key.light_energy = 1.0
	key.omni_range = 4.0
	key.position = Vector3(-0.8, 0.5, -0.2)
	add_child(key)
	var fill := OmniLight3D.new()
	fill.light_energy = 0.25
	fill.position = Vector3(0.5, 0.3, 1.0)
	add_child(fill)
	var mesh := cloth_mesh()
	for k in 2:
		var m := ShaderMaterial.new()
		m.shader = shader("cloth.gdshader")
		m.set_shader_parameter("sheen_amt", float(k))
		var mi := MeshInstance3D.new()
		mi.mesh = mesh
		mi.material_override = m
		add_child(mi)
		mi.position = Vector3(-0.22 + 0.44 * k, 0.2, 0)
	camera(Vector3(0.0, 0.25, 0.9), Vector3(0.0, 0.0, 0.0), 40.0)
	await shot("cloth_lambert_vs_sheen")
	await timing("cloth", 30)

func p_screen() -> void:
	environment()
	locker_lights(0.35)
	wall(-0.25)
	var r := rig(self, head_mesh("lod1"), skin_material(1))
	camera(Vector3(0.0, 0.66, 0.85), Vector3(0.0, 0.64, 0.08), 32.0)
	await shot("screen_off")
	await timing("screen_off", 30)
	var q := MeshInstance3D.new()
	var qm := QuadMesh.new()
	qm.size = Vector2(0.55, 0.55)
	q.mesh = qm
	var m := ShaderMaterial.new()
	m.shader = shader("haze.gdshader")
	m.set_shader_parameter("strength", 0.02)
	m.set_shader_parameter("t", 1.3)
	q.material_override = m
	add_child(q)
	q.position = Vector3(0.0, 0.66, 0.05)
	await shot("screen_on")
	await timing("screen_on", 30)

func p_micro() -> void:
	environment()
	locker_lights(0.4)
	var mat := skin_material(2)
	mat.set_shader_parameter("pulse_amp", 0.0018)
	mat.set_shader_parameter("tremor_amp", 0.0009)
	var r := rig(self, head_mesh("lod0"), mat)
	var mi: MeshInstance3D = r["mesh"]
	var sk: Skeleton3D = r["skeleton"]
	var ml := eye_material()
	await add_eyes(sk, Vector3(0.3, 0.6, 1.0), ml, ml)
	camera(Vector3(0.30, 0.60, 0.50), Vector3(0.0, 0.58, 0.07), 30.0)
	# 24 frames at 12 fps: the pulse, the tremor, a breath, and a wrong blink
	for f in 24:
		var t := f / 12.0
		mat.set_shader_parameter("sim_time", t)
		mat.set_shader_parameter("breath", 0.5 - 0.5 * cos(TAU * t / 2.0))
		var bl := clampf(1.0 - absf(t - 1.0) / 0.12, 0.0, 1.0)
		var br := clampf(1.0 - absf(t - 1.17) / 0.12, 0.0, 1.0) * 0.85
		shape(mi, "blink_l", br)
		shape(mi, "blink_r", bl)
		sk.set_bone_pose_rotation(2, Quaternion(Vector3.FORWARD, 0.006 * sin(t * 9.1 * TAU) + 0.004 * sin(t * 13.3 * TAU)))
		await shot("micro_%02d" % f, 1)
	await timing("micro_pulse_tremor_breath", 30)

func capsule_node(a: Vector3, b: Vector3, r: float, mat: Material) -> void:
	var mi := MeshInstance3D.new()
	var cm := CapsuleMesh.new()
	cm.radius = r
	cm.height = (b - a).length() + 2.0 * r
	mi.mesh = cm
	mi.material_override = mat
	add_child(mi)
	var y := (b - a).normalized()
	var x := y.cross(Vector3.FORWARD if absf(y.z) < 0.9 else Vector3.RIGHT).normalized()
	var z := x.cross(y)
	mi.transform = Transform3D(Basis(x, y, z), (a + b) * 0.5)

func p_capsule() -> void:
	var caps := [
		[Vector3(-0.09, 0.06, 0.0), Vector3(-0.1, 0.5, 0.02), 0.035],
		[Vector3(0.09, 0.06, 0.05), Vector3(0.1, 0.5, 0.0), 0.035],
		[Vector3(-0.09, 0.03, -0.03), Vector3(-0.1, 0.03, 0.17), 0.03],
		[Vector3(0.09, 0.03, 0.02), Vector3(0.11, 0.03, 0.22), 0.03],
		[Vector3(0.32, 0.018, 0.1), Vector3(0.36, 0.018, 0.26), 0.02],
		[Vector3(0.28, 0.06, 0.02), Vector3(0.32, 0.02, 0.1), 0.024],
	]
	for strength in [0.0, 1.0]:
		await clear_scene()
		environment(Color(0.12, 0.12, 0.13))
		var key := DirectionalLight3D.new()
		key.light_energy = 0.6
		key.rotation = Vector3(-1.0, 0.5, 0.0)
		add_child(key)
		var limb := StandardMaterial3D.new()
		limb.albedo_color = Color("6e6a64")
		for c in caps:
			capsule_node(c[0], c[1], c[2], limb)
		var fl := MeshInstance3D.new()
		var pm := PlaneMesh.new()
		pm.size = Vector2(2.0, 2.0)
		fl.mesh = pm
		var m := ShaderMaterial.new()
		m.shader = shader("floor_ao.gdshader")
		var pa := PackedVector4Array()
		var pb := PackedVector3Array()
		for c in caps:
			pa.append(Vector4(c[0].x, c[0].y, c[0].z, c[2]))
			pb.append(c[1])
		for k in 12 - caps.size():
			pa.append(Vector4(0, -10, 0, 0.0))
			pb.append(Vector3(0, -10, 0))
		m.set_shader_parameter("cap_a", pa)
		m.set_shader_parameter("cap_b", pb)
		m.set_shader_parameter("count", caps.size())
		m.set_shader_parameter("key_dir", -key.global_transform.basis.z)
		m.set_shader_parameter("strength", strength)
		fl.material_override = m
		add_child(fl)
		camera(Vector3(0.35, 0.75, 1.0), Vector3(0.08, 0.1, 0.05), 40.0)
		await shot("capsule_ao_%d" % int(strength))
		await timing("capsule_ao_%d" % int(strength), 30)
	# reference: the same scene with a real shadow map instead
	await clear_scene()
	environment(Color(0.12, 0.12, 0.13))
	var key2 := DirectionalLight3D.new()
	key2.light_energy = 0.6
	key2.rotation = Vector3(-1.0, 0.5, 0.0)
	key2.shadow_enabled = true
	add_child(key2)
	var limb2 := StandardMaterial3D.new()
	limb2.albedo_color = Color("6e6a64")
	for c in caps:
		capsule_node(c[0], c[1], c[2], limb2)
	var fl2 := MeshInstance3D.new()
	var pm2 := PlaneMesh.new()
	pm2.size = Vector2(2.0, 2.0)
	fl2.mesh = pm2
	var sm := StandardMaterial3D.new()
	sm.albedo_color = Color(0.32, 0.31, 0.30)
	sm.metallic = 0.3
	fl2.material_override = sm
	add_child(fl2)
	camera(Vector3(0.35, 0.75, 1.0), Vector3(0.08, 0.1, 0.05), 40.0)
	await shot("capsule_ref_shadowmap")
	await timing("capsule_ref_shadowmap", 30)

func full_head(parent: Node3D, look: Vector3, quality := 2, hair := true) -> Dictionary:
	var r := rig(parent, head_mesh("lod0" if quality >= 1 else "lod1"), skin_material(quality))
	var sk: Skeleton3D = r["skeleton"]
	r["eyes"] = await add_eyes(sk, look)
	if hair:
		r["hair"] = add_cards(sk, "hair", hair_material(1, 1))
		r["brows"] = add_cards(sk, "brows", hair_material(1, 1, "eyebrow010.png"))
	return r

func set_layers(n: Node, layers: int) -> void:
	if n is VisualInstance3D and (n as VisualInstance3D).layers != EYE_LAYER:
		(n as VisualInstance3D).layers = layers
	for c in n.get_children():
		set_layers(c, layers)

func p_rig() -> void:
	# light count sweep on the full head: does each light cost a pass here?
	environment()
	wall(-0.3)
	var her := Node3D.new()
	add_child(her)
	var cam_at := Vector3(0.25, 0.68, 0.75)
	var r := await full_head(her, cam_at)
	set_layers(her, HER_LAYER | 1)
	camera(cam_at, Vector3(0.0, 0.64, 0.08), 32.0)
	var ls := locker_lights(0.4)
	await shot("rig_0_base")
	await timing("rig_base_2lights", 30)
	var rim := SpotLight3D.new()
	rim.light_color = Color("cfdcff")
	rim.light_energy = 3.0
	rim.spot_range = 3.0
	rim.spot_angle = 25.0
	rim.light_cull_mask = HER_LAYER
	her.add_child(rim)
	rim.look_at_from_position(Vector3(0.35, 0.95, -0.55), Vector3(0.0, 0.65, 0.05))
	await shot("rig_1_rim_her_only")
	await timing("rig_plus_rim", 30)
	var glint := OmniLight3D.new()
	glint.light_cull_mask = EYE_LAYER
	glint.light_energy = 0.08
	glint.light_specular = 6.0
	glint.omni_range = 2.0
	glint.position = cam_at + Vector3(0.1, 0.12, 0.0)
	add_child(glint)
	await shot("rig_2_rim_glint")
	await timing("rig_plus_glint", 30)
	(ls[0] as OmniLight3D).shadow_enabled = true
	await shot("rig_3_bulb_shadowed")
	await timing("rig_bulb_shadowed", 30)
	for k in 4:
		var extra := OmniLight3D.new()
		extra.light_energy = 0.05
		extra.position = Vector3(-1 + k * 0.6, 1.2, 1.0)
		add_child(extra)
	await timing("rig_plus4_more_unshadowed", 30)

func p_lod() -> void:
	environment()
	locker_lights(0.6)
	var her := Node3D.new()
	add_child(her)
	var cam := camera(Vector3(0, 0.66, 0.8), Vector3(0, 0.64, 0.08), 30.0)
	# LOD0: subdivided head, HIGH skin, refracted eyes, hair cards (0-4 m)
	var r0 := await full_head(her, Vector3(0, 0.66, 5.0), 2, true)
	# LOD1: base head, MED skin, same eyes, hair (4-14 m)
	var r1 := rig(her, head_mesh("lod1"), skin_material(1))
	r1["eyes"] = await add_eyes(r1["skeleton"], Vector3(0, 0.66, 5.0))
	r1["hair"] = add_cards(r1["skeleton"], "hair", hair_material(0, 0))
	# LOD2: clustered head, LOW skin, no eyes, no hair (14 m+)
	var r2 := rig(her, head_mesh("lod2", false), skin_material(0))
	var bands := [[0.0, 4.0], [4.0, 14.0], [14.0, 0.0]]
	var sets := [r0, r1, r2]
	for k in 3:
		var nodes: Array = [sets[k]["mesh"]]
		for key in ["eyes", "hair", "brows"]:
			if sets[k].has(key):
				var v = sets[k][key]
				if v is Array:
					nodes.append_array(v)
				else:
					nodes.append(v)
		for nd in nodes:
			var gi := nd as GeometryInstance3D
			gi.visibility_range_begin = bands[k][0]
			gi.visibility_range_end = bands[k][1]
			gi.visibility_range_begin_margin = 0.5 if k > 0 else 0.0
			gi.visibility_range_end_margin = 0.5 if k < 2 else 0.0
			gi.visibility_range_fade_mode = GeometryInstance3D.VISIBILITY_RANGE_FADE_SELF
	for dist in [0.8, 4.0, 9.0, 20.0]:
		cam.look_at_from_position(Vector3(0.0, 0.66, 0.08 + dist), Vector3(0.0, 0.64, 0.08))
		cam.fov = 30.0 * 0.8 / dist if dist > 1.0 else 30.0
		await shot("lod_d%04.1f" % dist)
		await timing("lod_%.1fm" % dist, 20)

func p_quality() -> void:
	var cases := ["standard", "q0", "q1", "q2"]
	for v in cases:
		await clear_scene()
		environment()
		locker_lights(0.4)
		var mat: Material
		if v == "standard":
			var sm := StandardMaterial3D.new()
			sm.albedo_texture = tex("head_albedo.png")
			mat = sm
		else:
			mat = skin_material(int(v.substr(1)))
		rig(self, head_mesh("lod0"), mat)
		camera(Vector3(0.0, 0.655, 0.36), Vector3(0.0, 0.645, 0.1), 40.0)
		await shot("quality_" + v, 2)
		await timing("fullscreen_" + v, 60)

func p_hero() -> void:
	var looks := ["pest", "changed", "dark"]
	for look in looks:
		await clear_scene()
		var cam_at := Vector3(0.2, 0.69, 0.78)
		if look == "dark":
			environment(Color(0.01, 0.012, 0.016))
		else:
			environment(Color(0.035, 0.038, 0.045))
		var her := Node3D.new()
		add_child(her)
		var r := await full_head(her, cam_at)
		set_layers(her, HER_LAYER | 1)
		var mi: MeshInstance3D = r["mesh"]
		var sk: Skeleton3D = r["skeleton"]
		var mat: ShaderMaterial = mi.material_override
		var hm: ShaderMaterial = (r["hair"] as MeshInstance3D).material_override
		var eyes: Array = r["eyes"]
		if look == "pest":
			mat.set_shader_parameter("exposure", 0.15)
			shape(mi, "smile", 0.45)
			sk.set_bone_pose_rotation(2, Quaternion(Vector3.FORWARD, -0.22))
		else:
			mat.set_shader_parameter("exposure", 0.9)
			mat.set_shader_parameter("tint", Color(0.93, 0.92, 0.9))
			hm.set_shader_parameter("thin", 0.6)
			shape(mi, "wide_l", 0.9)
			shape(mi, "wide_r", 1.0)
			shape(mi, "jaw_open", 0.45)
			shape(mi, "lips_back", 0.6)
			sk.set_bone_pose_rotation(1, Quaternion(Vector3.FORWARD, 0.25))
			sk.set_bone_pose_rotation(2, Quaternion(Vector3.FORWARD, 0.4) * Quaternion(Vector3.RIGHT, 0.12))
			var ml: ShaderMaterial = (eyes[0] as MeshInstance3D).material_override
			var mr: ShaderMaterial = (eyes[1] as MeshInstance3D).material_override
			ml.set_shader_parameter("milky", 0.7)
			ml.set_shader_parameter("pupil", 0.5)
			mr.set_shader_parameter("pupil", 0.14)
			for m in [ml, mr]:
				m.set_shader_parameter("bloodshot", 0.85)
		await get_tree().process_frame
		for e in eyes:
			(e as Node3D).look_at(cam_at, Vector3.UP, true)
		if look == "dark":
			var under := OmniLight3D.new()
			under.light_color = Color("dfe8ff")
			under.light_energy = 0.35
			under.omni_range = 1.2
			under.position = Vector3(0.07, 0.36, 0.26)
			add_child(under)
		else:
			locker_lights(0.42)
		var rim := SpotLight3D.new()
		rim.light_color = Color("cfdcff")
		rim.light_energy = 0.35 if look != "dark" else 0.3
		rim.light_specular = 1.5
		rim.spot_range = 3.0
		rim.spot_angle = 18.0
		rim.light_cull_mask = HER_LAYER
		her.add_child(rim)
		rim.look_at_from_position(Vector3(0.3, 0.9, -0.9), Vector3(0.0, 0.66, 0.0))
		var glint := OmniLight3D.new()
		glint.light_cull_mask = EYE_LAYER
		glint.light_energy = 0.08
		glint.light_specular = 6.0
		glint.omni_range = 2.0
		glint.position = cam_at + Vector3(0.1, 0.12, 0.0)
		add_child(glint)
		camera(cam_at, Vector3(0.0, 0.63, 0.08), 30.0)
		await shot("hero_" + look)

func p_dbg() -> void:
	for v in ["bulb_only", "fill_only", "bulb_r1"]:
		await clear_scene()
		environment(Color(0, 0, 0))
		var ls := locker_lights(0.3)
		if v == "fill_only":
			ls[0].visible = false
		else:
			ls[1].visible = false
		if v == "bulb_r1":
			ls[0].omni_range = 1.0
		var sm := StandardMaterial3D.new()
		sm.albedo_color = Color(0.8, 0.8, 0.8)
		sm.roughness = 0.9
		rig(self, head_mesh("lod0"), sm)
		var sp := MeshInstance3D.new()
		sp.mesh = SphereMesh.new()
		sp.mesh.radius = 0.09
		sp.mesh.height = 0.18
		sp.material_override = sm
		sp.position = Vector3(0.25, 0.65, 0.05)
		add_child(sp)
		camera(Vector3(0.22, 0.67, 0.9), Vector3(0.1, 0.645, 0.1), 32.0)
		await shot("dbg_" + v)

func p_alpha() -> void:
	environment(Color(0.3, 0.3, 0.3))
	wall(-0.3)
	var codes := [
		"shader_type spatial; render_mode cull_disabled, unshaded; void fragment(){ ALBEDO = vec3(1.0,0.2,0.1); ALPHA = 0.3; ALPHA_SCISSOR_THRESHOLD = 0.5; }",
		"shader_type spatial; render_mode cull_disabled, unshaded; void fragment(){ ALBEDO = vec3(1.0,0.2,0.1); ALPHA = 0.3; ALPHA_HASH_SCALE = 1.0; }",
		"shader_type spatial; render_mode cull_disabled, unshaded; void fragment(){ ALBEDO = vec3(1.0,0.2,0.1); ALPHA = 0.3; }",
		"",
		"shader_type spatial; render_mode cull_disabled, unshaded; void fragment(){ ALBEDO = vec3(1.0,0.2,0.1); ALPHA = 0.3 + 0.7 * step(0.5, fract(UV.x * 6.0)); ALPHA_SCISSOR_THRESHOLD = 0.5; ALPHA_ANTIALIASING_EDGE = 0.3; ALPHA_TEXTURE_COORDINATE = UV * 64.0; }",
		"shader_type spatial; render_mode cull_disabled, unshaded; void fragment(){ ALBEDO = vec3(1.0,0.2,0.1); float h = fract(sin(dot(floor(FRAGCOORD.xy), vec2(12.9898, 78.233))) * 43758.5453); if (0.3 < h) { discard; } }",
	]
	get_viewport().msaa_3d = Viewport.MSAA_4X
	for k in 6:
		var q := MeshInstance3D.new()
		var qm := QuadMesh.new()
		qm.size = Vector2(0.4, 0.4)
		q.mesh = qm
		if codes[k] != "":
			var sh := Shader.new()
			sh.code = codes[k]
			var m := ShaderMaterial.new()
			m.shader = sh
			q.material_override = m
		else:
			var sm := StandardMaterial3D.new()
			sm.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
			sm.albedo_color = Color(1.0, 0.2, 0.1, 0.3)
			sm.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_HASH
			q.material_override = sm
		add_child(q)
		q.position = Vector3(-0.55 + 0.44 * (k % 3), 0.85 - 0.5 * (k / 3), 0.0)
	camera(Vector3(0, 0.6, 1.8), Vector3(0, 0.6, 0), 40.0)
	await shot("alpha_modes")

func p_ssao() -> void:
	for on in [false, true]:
		await clear_scene()
		var env := environment(Color(0.12, 0.12, 0.13))
		env.environment.ssao_enabled = on
		env.environment.ssao_radius = 0.15
		env.environment.ssao_intensity = 3.0
		locker_lights(0.25)
		rig(self, head_mesh("lod0"), skin_material(2))
		camera(Vector3(0.22, 0.67, 0.62), Vector3(0.0, 0.645, 0.1), 28.0)
		await shot("ssao_%d" % int(on))
		await timing("ssao_%d" % int(on), 30)

# Cost of the transform-feedback pass that does skinning and blend shapes in
# gl_compatibility: weights and bones change every frame, as in play.
var _anim_mi: MeshInstance3D
var _anim_sk: Skeleton3D
var _anim_n := 0
var _anim_t := 0.0
func _process(delta: float) -> void:
	if _anim_mi == null:
		return
	_anim_t += 0.011
	for k in _anim_n:
		_anim_mi.set_blend_shape_value(k, 0.5 + 0.5 * sin(_anim_t * 7.0 + k))
	if _anim_sk != null:
		_anim_sk.set_bone_pose_rotation(2, Quaternion(Vector3.UP, 0.3 * sin(_anim_t * 3.0)))

func p_tf() -> void:
	environment()
	locker_lights(0.4)
	for lod in ["lod0", "lod1"]:
		for cfg in [["static", 0, false], ["bones", 0, true], ["bones+3shapes", 3, true], ["bones+9shapes", 9, true]]:
			await clear_scene()
			environment()
			locker_lights(0.4)
			var sm := StandardMaterial3D.new()
			var r := rig(self, head_mesh(lod), sm)
			_anim_mi = r["mesh"]
			_anim_sk = r["skeleton"] if cfg[2] else null
			_anim_n = cfg[1]
			camera(Vector3(0.0, 0.66, 3.0), Vector3(0.0, 0.64, 0.08), 10.0)
			await timing("tf_%s_%s" % [lod, cfg[0]], 60)
	_anim_mi = null
