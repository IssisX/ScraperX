extends Node3D
# Matron look-dev probe (gl_compatibility). Renders a turntable of the aged CC0 head at
# phone pixel density: a 1024x1024 SubViewport at fov 51.3 deg = the middle 1024 px of the
# Galaxy Fold inner screen (1856 px tall at the game's 82 deg vertical fov).
# Env: SHOTS=comma list of rig:dist:yaw, OUTDIR, VARIANT (real|wrong), EXPO (0..1), WRONG (0..1)

const RES := 1024
const FOV := 51.3
const IRIS_ANGLE := 0.50   # 11.8 mm iris on a 12.2 mm-radius eyeball
var vp: SubViewport
var cam: Camera3D
var head_root: Node3D
var eyes: Array = []
var eye_mats: Array = []
var cornea_mats: Array = []
var eye_side: Array = []
var skin_mat: ShaderMaterial
var env: Environment
var rig_nodes: Array = []
var variant := "real"
var expo := 0.0
var wrong := 0.0

func _env_f(n: String, d: float) -> float:
	var s := OS.get_environment(n)
	return float(s) if s != "" else d

func _tex(path: String, srgb_hint := true) -> ImageTexture:
	var img := Image.load_from_file(ProjectSettings.globalize_path(path))
	img.generate_mipmaps()
	return ImageTexture.create_from_image(img)

func _load_bin(path: String, shapes_path := "") -> ArrayMesh:
	var f := FileAccess.open(path, FileAccess.READ)
	var nv := f.get_32(); var ni := f.get_32(); var has_uv := f.get_32(); var has_tan := f.get_32(); var has_col := f.get_32()
	var pos := f.get_buffer(nv * 12).to_float32_array()
	var nrm := f.get_buffer(nv * 12).to_float32_array()
	var arr := []
	arr.resize(Mesh.ARRAY_MAX)
	var P := PackedVector3Array(); var N := PackedVector3Array()
	for i in nv:
		P.append(Vector3(pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2]))
		N.append(Vector3(nrm[i * 3], nrm[i * 3 + 1], nrm[i * 3 + 2]))
	arr[Mesh.ARRAY_VERTEX] = P; arr[Mesh.ARRAY_NORMAL] = N
	if has_uv:
		var uv := f.get_buffer(nv * 8).to_float32_array(); var U := PackedVector2Array()
		for i in nv: U.append(Vector2(uv[i * 2], uv[i * 2 + 1]))
		arr[Mesh.ARRAY_TEX_UV] = U
	if has_tan:
		arr[Mesh.ARRAY_TANGENT] = f.get_buffer(nv * 16).to_float32_array()
	if has_col:
		var c := f.get_buffer(nv * 16).to_float32_array(); var C := PackedColorArray()
		for i in nv: C.append(Color(c[i * 4], c[i * 4 + 1], c[i * 4 + 2], c[i * 4 + 3]))
		arr[Mesh.ARRAY_COLOR] = C
	var idx := f.get_buffer(ni * 4).to_int32_array()
	arr[Mesh.ARRAY_INDEX] = idx
	var m := ArrayMesh.new()
	var bs := []
	if shapes_path != "":
		# RELATIVE blend shapes take position deltas in this Compatibility build
		m.blend_shape_mode = Mesh.BLEND_SHAPE_MODE_RELATIVE
		var g := FileAccess.open(shapes_path, FileAccess.READ)
		var ns := g.get_32(); var nvs := g.get_32()
		for k in ns:
			var nm := g.get_buffer(32).get_string_from_utf8()
			var dd := g.get_buffer(nvs * 12).to_float32_array()
			var D := PackedVector3Array()
			for i in nvs: D.append(Vector3(dd[i * 3], dd[i * 3 + 1], dd[i * 3 + 2]))
			m.add_blend_shape(nm)
			var sa := []; sa.resize(Mesh.ARRAY_MAX)
			sa[Mesh.ARRAY_VERTEX] = D
			var zN := PackedVector3Array(); zN.resize(nvs)
			sa[Mesh.ARRAY_NORMAL] = zN
			if has_tan:
				var zT := PackedFloat32Array(); zT.resize(nvs * 4)
				sa[Mesh.ARRAY_TANGENT] = zT
			bs.append(sa)
	m.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arr, bs)
	return m

func _mi(mesh: Mesh, mat: Material, parent: Node3D, shadow := true) -> MeshInstance3D:
	var mi := MeshInstance3D.new(); mi.mesh = mesh; mi.material_override = mat; parent.add_child(mi)
	if not shadow: mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	return mi

func _shader_mat(path: String) -> ShaderMaterial:
	var m := ShaderMaterial.new(); m.shader = load(path); return m

func _eyeball(radius: float) -> ArrayMesh:
	# UV sphere, +Z forward, with the cornea bulging ~0.6 mm over the iris
	var st := SurfaceTool.new(); st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var rings := 28; var segs := 40
	var verts := []
	for r in rings + 1:
		var th := PI * float(r) / float(rings)   # 0 at the front (+Z)
		for s in segs + 1:
			var ph := TAU * float(s) / float(segs)
			var d := Vector3(sin(th) * cos(ph), sin(th) * sin(ph), cos(th))
			var bulge := 0.0
			var cth := IRIS_ANGLE * 1.15
			if th < cth:
				bulge = 0.0011 * pow(cos(th / cth * PI * 0.5), 2.0)
			verts.append(d * (radius + bulge))
	for r in rings:
		for s in segs:
			var a: int = r * (segs + 1) + s; var b: int = a + segs + 1
			for k in [a, a + 1, b, a + 1, b + 1, b]:
				var v: Vector3 = verts[k]
				st.set_normal(v.normalized()); st.add_vertex(v)
	st.index()
	return st.commit()

func _ready() -> void:
	variant = OS.get_environment("VARIANT") if OS.get_environment("VARIANT") != "" else "real"
	expo = _env_f("EXPO", 0.0)
	wrong = _env_f("WRONG", 0.0)
	vp = SubViewport.new(); vp.size = Vector2i(RES, RES); vp.msaa_3d = Viewport.MSAA_4X
	vp.render_target_update_mode = SubViewport.UPDATE_ALWAYS
	vp.own_world_3d = true
	add_child(vp)
	var world := Node3D.new(); vp.add_child(world)
	var we := WorldEnvironment.new(); env = Environment.new(); we.environment = env; world.add_child(we)
	env.tonemap_mode = Environment.TONE_MAPPER_ACES
	env.tonemap_white = 4.0
	cam = Camera3D.new(); cam.fov = FOV; cam.near = 0.02; world.add_child(cam); cam.current = true
	head_root = Node3D.new(); world.add_child(head_root)
	# ---- head ----
	skin_mat = _shader_mat("res://shaders/skin.gdshader")
	var alb := "res://tex/albedo_%s.png" % variant
	skin_mat.set_shader_parameter("albedo_tex", _tex(alb))
	skin_mat.set_shader_parameter("normal_tex", _tex("res://tex/normal.png"))
	skin_mat.set_shader_parameter("maskA_tex", _tex("res://tex/maskA.png"))
	skin_mat.set_shader_parameter("maskB_tex", _tex("res://tex/maskB.png"))
	skin_mat.set_shader_parameter("skin_lut", _tex("res://tex/skin_lut.png"))
	skin_mat.set_shader_parameter("exposure", expo)
	skin_mat.set_shader_parameter("wrongness", wrong)
	skin_mat.set_shader_parameter("debug_view", int(_env_f("DEBUG", 0)))
	for kv in OS.get_environment("SKINP").split(",", false):
		var pp := kv.split("=")
		skin_mat.set_shader_parameter(pp[0], float(pp[1]))
	var mode := OS.get_environment("MODE")
	if mode == "flat":
		skin_mat.set_shader_parameter("flat_mode", 1.0)
	var head_mi := _mi(_load_bin("res://mesh/head.bin", "res://mesh/head_shapes.bin"), skin_mat, head_root)
	for kv in OS.get_environment("SHAPEW").split(",", false):
		var p := kv.split("=")
		var bi: int = (head_mi.mesh as ArrayMesh).find_blend_shape_by_name(p[0])
		if bi >= 0: head_mi.set_blend_shape_value(bi, float(p[1]))
		else: print("no shape ", p[0])
	# ---- eyes ----
	var ej: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://mesh/eyes.json"))
	var side := 0
	for e in ej["eyes"]:
		var c: Array = e["centre"]
		var pivot := Node3D.new(); pivot.position = Vector3(c[0], c[1], c[2]); head_root.add_child(pivot)
		var em := _shader_mat("res://shaders/eye.gdshader")
		var R: float = float(e["radius"])
		em.set_shader_parameter("eye_radius", R)
		em.set_shader_parameter("eye_debug", int(_env_f("EYEDBG", 0)))
		em.set_shader_parameter("iris_angle", IRIS_ANGLE)
		# her left eye (+x) is the burned side: in the 'wrong' look it clouds and its pupil stays wide
		var is_left: bool = c[0] > 0.0
		em.set_shader_parameter("seed", 1.7 if is_left else 4.3)
		em.set_shader_parameter("cloud", (0.55 * wrong + 0.35 * expo) if is_left else 0.04)
		em.set_shader_parameter("pupil_size", (0.30 + 0.22 * wrong) if is_left else 0.27)
		em.set_shader_parameter("vein_amt", 0.45 + 0.4 * expo + (0.35 * expo if is_left else 0.0))
		em.set_shader_parameter("bleed", (smoothstep(0.55, 0.9, expo)) if is_left else 0.0)
		var eb := _eyeball(R)
		_mi(eb, em, pivot, false)
		var cm := _shader_mat("res://shaders/cornea.gdshader")
		cm.render_priority = 2
		cm.set_shader_parameter("strength", _env_f("CSTR", 2.5))
		_mi(eb, cm, pivot, false)
		cornea_mats.append(cm)
		eyes.append(pivot); eye_mats.append(em); eye_side.append(1.0 if is_left else -1.0)
		side += 1
	var occ := _shader_mat("res://shaders/eye_occlusion.gdshader")
	occ.set_shader_parameter("strength", _env_f("OCC", 0.9))
	occ.render_priority = 1
	_mi(_load_bin("res://mesh/eye_occlusion.bin"), occ, head_root, false)
	var tear := _shader_mat("res://shaders/tear.gdshader")
	tear.set_shader_parameter("wet", 1.0 + 0.8 * wrong)
	_mi(_load_bin("res://mesh/tear.bin"), tear, head_root, false)
	var hair_mat := _shader_mat("res://shaders/hair_card.gdshader")
	hair_mat.set_shader_parameter("tex", _tex("res://tex/eyebrow010.png"))
	hair_mat.set_shader_parameter("tint", Color(0.58, 0.56, 0.53))
	hair_mat.set_shader_parameter("sparse", 0.2)
	hair_mat.set_shader_parameter("thin", 0.8)
	if OS.get_environment("BROWS") != "0":
		_mi(_load_bin("res://mesh/brows.bin"), hair_mat, head_root, false)
	var lash_mat := _shader_mat("res://shaders/hair_card.gdshader")
	lash_mat.set_shader_parameter("tex", _tex("res://tex/eyelashes02.png"))
	lash_mat.set_shader_parameter("tint", Color(0.22, 0.21, 0.2))
	lash_mat.set_shader_parameter("thin", 0.3)
	if OS.get_environment("LASHES") != "0":
		_mi(_load_bin("res://mesh/lashes.bin"), lash_mat, head_root, false)
	var cloth := _shader_mat("res://shaders/cloth.gdshader")
	cloth.set_shader_parameter("exposure", expo)
	var capm := _shader_mat("res://shaders/cloth.gdshader"); capm.set_shader_parameter("cut_mode", 1)
	var bandm := _shader_mat("res://shaders/cloth.gdshader"); bandm.set_shader_parameter("cut_mode", 2)
	if OS.get_environment("CAP") == "1":
		_mi(_load_bin("res://mesh/cap.bin"), capm, head_root)
		_mi(_load_bin("res://mesh/cap_band.bin"), bandm, head_root)
	if OS.get_environment("HAIR") != "0":
		var hj: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://mesh/hair.json"))
		var hm := _shader_mat("res://shaders/hair_shell.gdshader")
		hm.set_shader_parameter("maskB_tex", _tex("res://tex/maskB.png"))
		var bcv: Array = hj["bun_centre"]; var bav: Array = hj["bun_axis"]; var hcv: Array = hj["head_centre"]
		hm.set_shader_parameter("bun_centre", Vector3(bcv[0], bcv[1], bcv[2]))
		hm.set_shader_parameter("bun_axis", Vector3(bav[0], bav[1], bav[2]))
		hm.set_shader_parameter("head_centre", Vector3(hcv[0], hcv[1], hcv[2]))
		hm.set_shader_parameter("density_scale", _env_f("HDENS", 1.0))
		_mi(_load_bin("res://mesh/hair_shells.bin"), hm, head_root)
		_mi(_load_bin("res://mesh/bun.bin"), hm, head_root)
	_mi(_load_bin("res://mesh/tunic.bin"), cloth, head_root)
	if OS.get_environment("STRAYS") != "0":
		_mi(_load_bin("res://mesh/strays.bin"), _shader_mat("res://shaders/hair_strand.gdshader"), head_root, false)
	await _shoot()
	get_tree().quit()

func _clear_rig() -> void:
	for n in rig_nodes: n.queue_free()
	rig_nodes.clear()

func _rig(name: String) -> void:
	_clear_rig()
	var parent := cam.get_parent()
	var tm := OS.get_environment("TM")
	env.tonemap_mode = Environment.TONE_MAPPER_AGX if tm == "" or tm == "agx" else (Environment.TONE_MAPPER_ACES if tm == "aces" else Environment.TONE_MAPPER_FILMIC)
	env.tonemap_white = 6.0
	if tm == "linear": env.tonemap_mode = Environment.TONE_MAPPER_LINEAR
	skin_mat.set_shader_parameter("wrap_all", _env_f("WRAPD", 0.3) if name == "day" else _env_f("WRAPN", 0.04))
	if name == "day":
		# Overcast day on the 154 m deck. The camera white-balances to the cloud (neutral), the light
		# is the sky dome: bright from above, dark from the deck below, no hard sun.
		var sky := Sky.new(); var sm := ProceduralSkyMaterial.new()
		sm.sky_top_color = Color(0.86, 0.87, 0.88); sm.sky_horizon_color = Color(0.70, 0.70, 0.70)
		sm.ground_horizon_color = Color(_env_f("GH", 0.46), _env_f("GH", 0.46), _env_f("GH", 0.46) * 0.98); sm.ground_bottom_color = Color(_env_f("GB", 0.32), _env_f("GB", 0.32), _env_f("GB", 0.32) * 0.98)
		sm.sun_angle_max = 0.0; sm.sky_energy_multiplier = 1.0; sm.sky_curve = 0.25; sm.ground_curve = 0.05
		sky.sky_material = sm
		env.background_mode = Environment.BG_COLOR
		env.background_color = Color(0.40, 0.41, 0.42)
		env.sky = sky
		env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
		env.ambient_light_energy = _env_f("AMB", 1.35)
		env.reflected_light_source = Environment.REFLECTION_SOURCE_DISABLED if OS.get_environment("NOREFL") == "1" else Environment.REFLECTION_SOURCE_SKY
		env.tonemap_exposure = _env_f("EXPD", 1.0)
		# the brightest patch of cloud: high, in front, a little to her right; very soft shadow
		var sun := DirectionalLight3D.new(); sun.light_color = Color(1.0, 0.99, 0.97); sun.light_energy = _env_f("SUN", 0.32)
		sun.rotation_degrees = Vector3(-62, _env_f("SUNYAW", -20), 0); sun.shadow_enabled = true; sun.shadow_blur = 10.0; sun.shadow_opacity = _env_f("SOP", 0.4)
		sun.directional_shadow_max_distance = 4.0; sun.shadow_normal_bias = 1.5
		parent.add_child(sun); rig_nodes.append(sun)
		for cm in cornea_mats:
			cm.set_shader_parameter("env_sky", Vector3(_env_f("SKYE", 6.0), _env_f("SKYE", 6.0) * 1.01, _env_f("SKYE", 6.0) * 1.03)); cm.set_shader_parameter("env_band", Vector3(1.6, 1.6, 1.62)); cm.set_shader_parameter("env_ground", Vector3(0.05, 0.048, 0.045))
		for em in eye_mats:
			em.set_shader_parameter("env_sky", Vector3(_env_f("SKYE", 6.0), _env_f("SKYE", 6.0) * 1.01, _env_f("SKYE", 6.0) * 1.03)); em.set_shader_parameter("env_band", Vector3(1.6, 1.6, 1.62)); em.set_shader_parameter("env_ground", Vector3(0.05, 0.048, 0.045))
	else:
		# Night: the caged work bulb (matron_view.gd ffd49a) about a metre off, up and to her right;
		# a cold glow from the sky behind; the city's sodium haze far below. Exposed like a phone
		# camera at night: the lit side holds, everything else falls into black.
		env.background_mode = Environment.BG_COLOR
		env.background_color = Color(0.004, 0.005, 0.008)
		env.sky = null
		env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
		env.ambient_light_color = Color(0.20, 0.23, 0.30); env.ambient_light_energy = _env_f("NAMB", 0.035)
		env.reflected_light_source = Environment.REFLECTION_SOURCE_DISABLED
		env.tonemap_exposure = _env_f("EXPN", 1.0)
		var bulb := OmniLight3D.new(); bulb.light_color = Color(1.0, 0.86, 0.70) if OS.get_environment("RAWBULB") != "1" else Color("ffd49a"); bulb.light_energy = _env_f("BULB", 1.6)
		bulb.omni_range = 8.0; bulb.omni_attenuation = 2.0
		bulb.position = Vector3(_env_f("BX", -0.75), _env_f("BY", 0.55), _env_f("BZ", 0.55))
		bulb.shadow_enabled = true; bulb.shadow_blur = 1.0; bulb.shadow_normal_bias = _env_f("SNB", 0.3); bulb.shadow_bias = 0.02
		parent.add_child(bulb); rig_nodes.append(bulb)
		var rim := DirectionalLight3D.new(); rim.light_color = Color(0.55, 0.65, 0.85); rim.light_energy = _env_f("RIM", 0.10)
		rim.rotation_degrees = Vector3(-15, 150, 0)
		parent.add_child(rim); rig_nodes.append(rim)
		var haze := DirectionalLight3D.new(); haze.light_color = Color(1.0, 0.62, 0.32); haze.light_energy = _env_f("HAZE", 0.03)
		haze.rotation_degrees = Vector3(35, -40, 0)   # from below: the lit city under the deck
		parent.add_child(haze); rig_nodes.append(haze)
		var fill := OmniLight3D.new(); fill.light_color = Color(0.85, 0.88, 0.95); fill.light_energy = _env_f("FILL", 0.02)
		fill.omni_range = 3.0; fill.position = Vector3(0.06, 0.1, 1.2)
		parent.add_child(fill); rig_nodes.append(fill)
		for cm in cornea_mats:
			cm.set_shader_parameter("env_sky", Vector3(0.012, 0.014, 0.02)); cm.set_shader_parameter("env_band", Vector3(0.004, 0.004, 0.006)); cm.set_shader_parameter("env_ground", Vector3(0.03, 0.016, 0.006))
		for em in eye_mats:
			em.set_shader_parameter("env_sky", Vector3(0.012, 0.014, 0.02)); em.set_shader_parameter("env_band", Vector3(0.002, 0.002, 0.003)); em.set_shader_parameter("env_ground", Vector3(0.03, 0.016, 0.006))

func _aim_eyes() -> void:
	for ei in eyes.size():
		var pv: Node3D = eyes[ei]
		var to := cam.global_position
		pv.look_at(to, Vector3.UP, true)
		# clamp to a human range relative to the head
		var local: Vector3 = (head_root.global_transform.affine_inverse() * to) - pv.position
		var yaw := atan2(local.x, local.z); var pitch := atan2(local.y, Vector2(local.x, local.z).length())
		var lim := _env_f("EYECLAMP", 0.6)
		yaw = clampf(yaw, -lim, lim); pitch = clampf(pitch, -0.45, 0.45)
		# wrong: her burned (left) eye has stopped following; it drifts out and a little up
		if float(eye_side[ei]) > 0.0:
			yaw += 0.13 * wrong; pitch += 0.03 * wrong
		pv.rotation = Vector3(-pitch, yaw, 0.0)

var post_vp: SubViewport
var post_rect: TextureRect
func _post(src: Image, rig: String) -> Image:
	if post_vp == null:
		post_vp = SubViewport.new(); post_vp.size = Vector2i(RES, RES); post_vp.render_target_update_mode = SubViewport.UPDATE_ALWAYS
		add_child(post_vp)
		post_rect = TextureRect.new(); post_rect.size = Vector2(RES, RES)
		var m := ShaderMaterial.new(); m.shader = load("res://shaders/post.gdshader"); post_rect.material = m
		post_vp.add_child(post_rect)
	var m2: ShaderMaterial = post_rect.material
	m2.set_shader_parameter("night", 1.0 if rig == "night" else 0.0)
	m2.set_shader_parameter("seed", float(Time.get_ticks_msec() % 997))
	post_rect.texture = ImageTexture.create_from_image(src)
	for i in 2: await get_tree().process_frame
	await RenderingServer.frame_post_draw
	return post_vp.get_texture().get_image()

func _shoot() -> void:
	var outdir := OS.get_environment("OUTDIR")
	var shots := OS.get_environment("SHOTS").split(",", false)
	var last_rig := ""
	for s in shots:
		var p := s.split(":")
		var rig := p[0]; var dist := float(p[1]); var yaw := float(p[2])
		if rig != last_rig: _rig(rig); last_rig = rig
		head_root.rotation = Vector3(deg_to_rad(_env_f("PITCH", 0.0)), deg_to_rad(yaw), deg_to_rad(_env_f("ROLL", 0.0)))
		var face := Vector3(0.0, -0.012, 0.085)
		cam.position = face + Vector3(0, 0.004, dist)
		cam.look_at(face, Vector3.UP)
		_aim_eyes()
		for i in 3: await get_tree().process_frame
		await RenderingServer.frame_post_draw
		var img := vp.get_texture().get_image()
		if OS.get_environment("POST") == "1":
			img = await _post(img, rig)
		var fn := "%s/%s_%s_d%03d_y%+04d.png" % [outdir, variant, rig, int(dist * 100), int(yaw)]
		img.save_png(fn)
		print("SHOT ", fn)
