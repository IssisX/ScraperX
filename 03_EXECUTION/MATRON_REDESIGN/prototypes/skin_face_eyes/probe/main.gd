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
		em.set_shader_parameter("iris_angle", IRIS_ANGLE)
		# her left eye (+x) is the burned side: in the 'wrong' look it clouds and its pupil stays wide
		var is_left: bool = c[0] > 0.0
		em.set_shader_parameter("seed", 1.7 if is_left else 4.3)
		em.set_shader_parameter("cloud", (0.55 * wrong + 0.35 * expo) if is_left else 0.04)
		em.set_shader_parameter("pupil_size", (0.30 + 0.22 * wrong) if is_left else 0.27)
		em.set_shader_parameter("vein_amt", 0.45 + 0.4 * expo + (0.35 * expo if is_left else 0.0))
		em.set_shader_parameter("bleed", (smoothstep(0.55, 0.9, expo)) if is_left else 0.0)
		_mi(_eyeball(R), em, pivot, false)
		eyes.append(pivot); eye_mats.append(em)
		side += 1
	var occ := _shader_mat("res://shaders/eye_occlusion.gdshader")
	occ.set_shader_parameter("strength", _env_f("OCC", 1.4))
	_mi(_load_bin("res://mesh/eye_occlusion.bin"), occ, head_root, false)
	var tear := _shader_mat("res://shaders/tear.gdshader")
	tear.set_shader_parameter("wet", 1.0 + 0.8 * wrong)
	_mi(_load_bin("res://mesh/tear.bin"), tear, head_root, false)
	var hair_mat := _shader_mat("res://shaders/hair_card.gdshader")
	hair_mat.set_shader_parameter("tex", _tex("res://tex/eyebrow010.png"))
	hair_mat.set_shader_parameter("tint", Color(0.42, 0.39, 0.36))
	hair_mat.set_shader_parameter("thin", 1.0)
	_mi(_load_bin("res://mesh/brows.bin"), hair_mat, head_root, false)
	var lash_mat := _shader_mat("res://shaders/hair_card.gdshader")
	lash_mat.set_shader_parameter("tex", _tex("res://tex/eyelashes02.png"))
	lash_mat.set_shader_parameter("tint", Color(0.22, 0.21, 0.2))
	lash_mat.set_shader_parameter("thin", 0.3)
	_mi(_load_bin("res://mesh/lashes.bin"), lash_mat, head_root, false)
	var cloth := _shader_mat("res://shaders/cloth.gdshader")
	cloth.set_shader_parameter("exposure", expo)
	var capm := _shader_mat("res://shaders/cloth.gdshader"); capm.set_shader_parameter("cut_mode", 1)
	var bandm := _shader_mat("res://shaders/cloth.gdshader"); bandm.set_shader_parameter("cut_mode", 2)
	_mi(_load_bin("res://mesh/cap.bin"), capm, head_root)
	_mi(_load_bin("res://mesh/cap_band.bin"), bandm, head_root)
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
	if name == "day":
		# overcast day on the 154 m deck: a bright, soft sky; the sun a dim smear high and to one side
		var sky := Sky.new(); var sm := ProceduralSkyMaterial.new()
		sm.sky_top_color = Color(0.62, 0.64, 0.67); sm.sky_horizon_color = Color(0.74, 0.75, 0.76)
		sm.ground_horizon_color = Color(0.46, 0.43, 0.40); sm.ground_bottom_color = Color(0.30, 0.27, 0.24)
		sm.sun_angle_max = 0.0; sm.sky_energy_multiplier = 1.0
		sky.sky_material = sm
		env.background_mode = Environment.BG_COLOR
		env.background_color = Color(0.52, 0.55, 0.58)
		env.sky = sky
		env.ambient_light_source = Environment.AMBIENT_SOURCE_SKY
		env.ambient_light_energy = _env_f("AMB", 0.6)
		env.reflected_light_source = Environment.REFLECTION_SOURCE_SKY
		env.tonemap_exposure = _env_f("EXPD", 0.95)
		var sun := DirectionalLight3D.new(); sun.light_color = Color(0.93, 0.95, 1.0); sun.light_energy = _env_f("SUN", 0.55)
		sun.rotation_degrees = Vector3(-55, -25, 0); sun.shadow_enabled = true; sun.shadow_blur = 5.0; sun.shadow_opacity = 0.55
		sun.directional_shadow_max_distance = 4.0; sun.shadow_normal_bias = 1.5
		parent.add_child(sun); rig_nodes.append(sun)
		for em in eye_mats:
			em.set_shader_parameter("env_sky", Color(0.55, 0.57, 0.6)); em.set_shader_parameter("env_ground", Color(0.12, 0.11, 0.1))
	else:
		# night: the caged work bulb by her locker (matron_view.gd: ffd49a), up and to her right, in front
		env.background_mode = Environment.BG_COLOR
		env.background_color = Color(0.006, 0.008, 0.013)
		env.sky = null
		env.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
		env.ambient_light_color = Color(0.16, 0.19, 0.26); env.ambient_light_energy = 0.08
		env.reflected_light_source = Environment.REFLECTION_SOURCE_DISABLED
		env.tonemap_exposure = 2.3
		var bulb := OmniLight3D.new(); bulb.light_color = Color("ffd49a"); bulb.light_energy = 1.2
		bulb.omni_range = 6.0; bulb.omni_attenuation = 1.0
		bulb.position = Vector3(-0.55, 0.42, 0.42)
		parent.add_child(bulb); rig_nodes.append(bulb)
		# cold sky behind her through the steel: a rim, so thin ears show light through them
		var rim := DirectionalLight3D.new(); rim.light_color = Color(0.55, 0.65, 0.85); rim.light_energy = 0.35
		rim.rotation_degrees = Vector3(-15, 150, 0)
		parent.add_child(rim); rig_nodes.append(rim)
		# a faint bounce from the player's side (deck, wet steel): enough for a living catchlight
		var fill := OmniLight3D.new(); fill.light_color = Color(0.85, 0.88, 0.95); fill.light_energy = _env_f("FILL", 0.05)
		fill.omni_range = 3.0; fill.position = Vector3(0.06, 0.1, 1.2)
		parent.add_child(fill); rig_nodes.append(fill)
		for em in eye_mats:
			em.set_shader_parameter("env_sky", Color(0.02, 0.022, 0.03)); em.set_shader_parameter("env_ground", Color(0.01, 0.008, 0.006))

func _aim_eyes() -> void:
	for p in eyes:
		var pv: Node3D = p
		var to := cam.global_position
		pv.look_at(to, Vector3.UP, true)
		# clamp to a human range relative to the head
		var local: Vector3 = (head_root.global_transform.affine_inverse() * to) - pv.position
		var yaw := atan2(local.x, local.z); var pitch := atan2(local.y, Vector2(local.x, local.z).length())
		var lim := _env_f("EYECLAMP", 0.6)
		yaw = clampf(yaw, -lim, lim); pitch = clampf(pitch, -0.45, 0.45)
		pv.rotation = Vector3(-pitch, yaw, 0.0)

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
		var fn := "%s/%s_%s_d%03d_y%+04d.png" % [outdir, variant, rig, int(dist * 100), int(yaw)]
		img.save_png(fn)
		print("SHOT ", fn)
