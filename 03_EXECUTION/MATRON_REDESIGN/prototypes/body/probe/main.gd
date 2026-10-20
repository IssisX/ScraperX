extends Node3D
## Body probe: loads the built bodies (glb), drives them with the real
## matron_view.gd _pose() through rig_adapter.gd, and saves studio renders.
## MODE=views|lineup|compare|poses|detail  OUT_DIR=<dir>

const MV := preload("res://presentation/life/matron_view.gd")
const Adapter := preload("res://rig_adapter.gd")

var out_dir := ""
var mats := {}
var figures: Array = []   # {mv, adapter (or null), name}
var cam: Camera3D
var log_lines: PackedStringArray = []

const POSES := [
	["guard", {"state": 7, "speed": 0.0, "stride": 0.0, "turn": 0}],
	["shuffle", {"state": 5, "speed": 1.0, "stride": 0.6, "turn": 0}],
	["search", {"state": 4, "speed": 0.0, "stride": 0.0, "turn": 0}],
	["peer", {"state": 8, "speed": 0.0, "stride": 0.0, "turn": 0}],
	["hunt_a", {"state": 3, "speed": 6.8, "stride": 0.5, "turn": 0, "sees_player": true}],
	["hunt_b", {"state": 3, "speed": 6.8, "stride": 1.2, "turn": 0}],
	["hunt_still", {"state": 3, "speed": 0.0, "stride": 0.0, "turn": 0}],
	["turn_windup", {"state": 3, "speed": 0.0, "stride": 0.0, "turn": 1}],
	["turn_lunge_a", {"state": 3, "speed": 9.0, "stride": 0.3, "turn": 2}],
	["turn_lunge_b", {"state": 3, "speed": 9.0, "stride": 0.9, "turn": 2}],
	["turn_vault", {"state": 3, "speed": 4.0, "stride": 0.3, "turn": 2, "_fall": 2.0}],
	["emerge", {"state": 2, "speed": 0.0, "stride": 0.0, "turn": 0, "_age": 0.9}],
	["cling", {"state": 3, "speed": 0.0, "stride": 0.0, "turn": 0, "clinging": true}],
	["unrig", {"state": 9, "speed": 0.0, "stride": 0.0, "turn": 0}],
	["on_line", {"state": 3, "speed": 4.0, "stride": 0.0, "turn": 0, "clinging": true, "on_line": true}],
	["climb", {"state": 3, "speed": 0.0, "stride": 0.45, "turn": 0, "climbing": true}],
	["carry", {"state": 5, "speed": 1.0, "stride": 0.3, "turn": 0, "carrying": 1}],
	["fall", {"state": 3, "speed": 0.0, "stride": 0.0, "turn": 0, "_falling": true}],
]


func _ready() -> void:
	out_dir = OS.get_environment("OUT_DIR")
	var mode := OS.get_environment("MODE")
	if mode == "mood":
		_dark()
	else:
		_studio()
	_make_materials()
	match mode:
		"views":
			await _views()
		"lineup":
			await _lineup()
		"compare":
			await _compare()
		"poses":
			await _poses()
		"detail":
			await _detail()
		"calib":
			await _calib()
		"neck":
			await _neck()
		"lod":
			await _lod()
		"mood":
			await _mood()
	var f := FileAccess.open(out_dir + "/" + mode + ".log", FileAccess.WRITE)
	f.store_string("\n".join(log_lines))
	f.close()
	get_tree().quit()


func _studio() -> void:
	var env := WorldEnvironment.new()
	env.environment = Environment.new()
	env.environment.background_mode = Environment.BG_COLOR
	env.environment.background_color = Color(0.16, 0.165, 0.17)
	env.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.environment.ambient_light_color = Color(0.62, 0.64, 0.68)
	env.environment.ambient_light_energy = 0.12
	env.environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	add_child(env)
	var key := DirectionalLight3D.new()
	key.light_energy = 0.4
	key.light_color = Color(1.0, 0.97, 0.93)
	key.shadow_enabled = true
	key.directional_shadow_max_distance = 14.0
	add_child(key)
	key.look_at_from_position(Vector3(-3.0, 4.2, 5.0), Vector3(0.0, 0.8, 0.0))
	var fill := DirectionalLight3D.new()
	fill.light_energy = 0.1
	fill.light_color = Color(0.9, 0.94, 1.0)
	add_child(fill)
	fill.look_at_from_position(Vector3(5.0, 1.5, 3.0), Vector3(0.0, 1.0, 0.0))
	var rim := DirectionalLight3D.new()
	rim.light_energy = 0.3
	add_child(rim)
	rim.look_at_from_position(Vector3(1.0, 3.5, -5.0), Vector3(0.0, 1.0, 0.0))
	var floor_mesh := PlaneMesh.new()
	floor_mesh.size = Vector2(30.0, 30.0)
	var fl := MeshInstance3D.new()
	fl.mesh = floor_mesh
	var fm := StandardMaterial3D.new()
	fm.albedo_color = Color(0.2, 0.2, 0.21)
	fm.roughness = 0.95
	fl.material_override = fm
	add_child(fl)
	cam = Camera3D.new()
	add_child(cam)
	cam.current = true


func _tex(file: String) -> ImageTexture:
	var img := Image.load_from_file(ProjectSettings.globalize_path("res://assets/" + file))
	img.generate_mipmaps()
	return ImageTexture.create_from_image(img)


func _make_materials() -> void:
	var cloth_n := _tex("cloth_n.png")
	var knit_n := _tex("knit_n.png")
	var skin := StandardMaterial3D.new()
	skin.albedo_texture = _tex("skin_a.png")
	skin.roughness = 0.58
	skin.rim_enabled = true
	skin.rim = 0.25
	skin.rim_tint = 0.6
	mats["skin"] = skin
	var tunic := StandardMaterial3D.new()
	tunic.albedo_color = Color(0.86, 0.845, 0.80)
	tunic.roughness = 0.93
	tunic.normal_enabled = true
	tunic.normal_texture = cloth_n
	tunic.normal_scale = 0.45
	tunic.uv1_scale = Vector3(1.4, 1.4, 1.0)
	tunic.cull_mode = BaseMaterial3D.CULL_DISABLED
	mats["tunic"] = tunic
	var collar := tunic.duplicate() as StandardMaterial3D
	collar.albedo_color = Color(0.93, 0.925, 0.9)
	collar.roughness = 0.7
	mats["collar"] = collar
	var stock := StandardMaterial3D.new()
	stock.albedo_color = Color(0.13, 0.115, 0.105)
	stock.roughness = 0.5
	stock.normal_enabled = true
	stock.normal_texture = knit_n
	stock.normal_scale = 0.35
	stock.uv1_scale = Vector3(60.0, 60.0, 1.0)
	stock.rim_enabled = true
	stock.rim = 0.5
	stock.rim_tint = 0.3
	mats["stocking"] = stock
	var shoe := StandardMaterial3D.new()
	shoe.albedo_color = Color(0.06, 0.055, 0.05)
	shoe.roughness = 0.38
	shoe.cull_mode = BaseMaterial3D.CULL_DISABLED
	mats["shoe"] = shoe
	var cap := StandardMaterial3D.new()
	cap.albedo_color = Color(0.95, 0.945, 0.92)
	cap.roughness = 0.62
	cap.normal_enabled = true
	cap.normal_texture = cloth_n
	cap.normal_scale = 0.3
	cap.cull_mode = BaseMaterial3D.CULL_DISABLED
	mats["cap"] = cap
	var hair := StandardMaterial3D.new()
	hair.albedo_color = Color(0.86, 0.85, 0.82)
	hair.albedo_texture = _tex("hair_a.png")
	hair.vertex_color_use_as_albedo = true
	hair.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR
	hair.alpha_scissor_threshold = 0.45
	hair.roughness = 0.55
	hair.cull_mode = BaseMaterial3D.CULL_DISABLED
	mats["hair"] = hair
	var bun := StandardMaterial3D.new()
	bun.albedo_color = Color(0.66, 0.65, 0.62)
	bun.roughness = 0.65
	mats["bun"] = bun
	var eye := StandardMaterial3D.new()
	eye.vertex_color_use_as_albedo = true
	eye.roughness = 0.12
	mats["eye"] = eye
	var steel := StandardMaterial3D.new()
	steel.albedo_color = Color(0.62, 0.62, 0.64)
	steel.metallic = 0.9
	steel.roughness = 0.3
	mats["steel"] = steel
	var button := StandardMaterial3D.new()
	button.albedo_color = Color(0.9, 0.88, 0.82)
	button.roughness = 0.25
	mats["button"] = button
	var wood := StandardMaterial3D.new()
	wood.albedo_color = Color(0.3, 0.19, 0.1)
	wood.roughness = 0.5
	mats["wood"] = wood
	var grey := StandardMaterial3D.new()
	grey.albedo_color = Color(0.5, 0.52, 0.55)
	grey.roughness = 0.75
	mats["reference"] = grey


const PART_MAT := {"skin": "skin", "stockings": "stocking", "tunic": "tunic", "collar": "collar", "pocket": "tunic",
	"placket": "tunic", "skirt": "tunic", "belt": "tunic", "buckle": "steel", "penlight": "steel", "buttons": "button",
	"shoes": "shoe", "hair": "hair", "bun": "bun", "cap": "cap", "eyes": "eye", "music_box": "wood"}


func _add_figure(preset: String, at: Vector3, old := false, bare := false) -> Dictionary:
	var mv: Node3D = MV.new()
	add_child(mv)
	mv.position = at
	mv._build_figure()
	var fig := {"mv": mv, "adapter": null, "name": preset, "root": null}
	if old:
		mv._figure.visible = true
		figures.append(fig)
		return fig
	mv._figure.visible = true
	# hide today's primitives: only the pivots are kept
	for c in mv._figure.find_children("*", "MeshInstance3D", true, false):
		(c as MeshInstance3D).visible = false
	var doc := GLTFDocument.new()
	var st := GLTFState.new()
	var err := doc.append_from_file(ProjectSettings.globalize_path("res://assets/%s.glb" % preset), st)
	if err != OK:
		push_error("glb load failed " + preset)
	var root: Node3D = doc.generate_scene(st)
	mv._figure.add_child(root)
	fig["root"] = root
	var skel: Skeleton3D = root.find_children("*", "Skeleton3D", true, false)[0]
	for mi in root.find_children("*", "MeshInstance3D", true, false):
		var m := mi as MeshInstance3D
		var key := String(m.name)
		if preset == "reference" or preset.ends_with("_bare") or (bare and key in ["skin", "stockings"]):
			m.material_override = mats["reference"]
		elif bare:
			m.visible = key == "eyes"
		elif PART_MAT.has(key):
			m.material_override = mats[PART_MAT[key]]
		m.extra_cull_margin = 2.0
	var rig: Dictionary = JSON.parse_string(FileAccess.get_file_as_string("res://assets/%s_rig.json" % preset))
	var ad = Adapter.new()
	ad.setup(skel, rig, mv)
	fig["adapter"] = ad
	figures.append(fig)
	return fig


func _pose_fig(fig: Dictionary, data: Dictionary, clock: float) -> void:
	var mv: Node3D = fig["mv"]
	var d: Dictionary = {"state": 7, "speed": 0.0, "stride": 0.0, "turn": 0, "sees_player": false, "climbing": false,
		"clinging": false, "on_line": false, "wary": false, "carrying": 0}
	for k in data:
		d[k] = data[k]
	mv._state_age = float(d.get("_age", 3.0))
	mv._fall_speed = float(d.get("_fall", 0.0))
	var falling := bool(d.get("_falling", false))
	var player := mv.global_position + Vector3(0.0, 1.5, 3.0)
	for k in 8:
		mv._clock = clock + 0.05 * float(k)
		mv._pose(d, 0.05, player, falling)
	if fig.get("contact", false):
		_crawl_retune(mv, d)
	if fig["adapter"] != null:
		fig["adapter"].apply()
		if fig.get("contact", false):
			fig["adapter"].contact(d, falling)


# The proposed new numbers for the all-fours crawl with real arm lengths:
# chest 0.42 rad lower (the shoulders come down to arm's length off the deck),
# hips lower, the arms and head keeping their angle in the world.
func _crawl_retune(mv: Node3D, d: Dictionary) -> void:
	var crawl := int(d["state"]) == 3 and not bool(d["clinging"]) and not bool(d["climbing"]) \
		and (float(d["speed"]) > 1.0 or int(d["turn"]) == 2) and float(d.get("_fall", 0.0)) <= 1.0
	if not crawl:
		return
	var p: Dictionary = mv._p
	# a head-down bear crawl: hips high on bent legs, chest down at arm's length
	(p["torso"] as Node3D).rotation.x += 0.75
	for s in ["l", "r"]:
		(p["sh_" + s] as Node3D).rotation.x -= 0.75
	(p["head"] as Node3D).rotation.x -= 0.55
	(p["neck"] as Node3D).rotation.x -= 0.2


func _shot(name: String, cam_pos: Vector3, look: Vector3, fov := 18.0, ortho := 0.0) -> void:
	if ortho > 0.0:
		cam.projection = Camera3D.PROJECTION_ORTHOGONAL
		cam.size = ortho
	else:
		cam.projection = Camera3D.PROJECTION_PERSPECTIVE
		cam.fov = fov
	cam.look_at_from_position(cam_pos, look)
	for i in 3:
		await get_tree().process_frame
	await RenderingServer.frame_post_draw
	get_viewport().get_texture().get_image().save_png(out_dir + "/" + name + ".png")


func _measure(fig: Dictionary, pose: String) -> void:
	var ad = fig["adapter"]
	var mv: Node3D = fig["mv"]
	var y0 := mv.global_position.y
	if ad == null:
		var p: Dictionary = mv._p
		var hand := INF
		for s in ["l", "r"]:
			for chain in p["fingers_" + s]:
				var last: Node3D = chain[chain.size() - 1]
				hand = minf(hand, (last.global_transform * Vector3(0.0, -0.035, 0.0)).y)
		var foot := INF
		for s in ["l", "r"]:
			foot = minf(foot, (p["an_" + s] as Node3D).global_position.y - 0.06)
		var top := ((p["head"] as Node3D).global_transform * Vector3(0.0, 0.208, 0.0)).y
		log_lines.append("%s old  hands %.2f  feet %.2f  crown %.2f" % [pose, hand - y0, foot - y0, top - y0])
		return
	var hand2 := INF
	var foot2 := INF
	for s in ["l", "r"]:
		hand2 = minf(hand2, ad.point("f12_" + s, "tip").y)
		hand2 = minf(hand2, ad.point("wr_" + s, "knuckles").y)
		foot2 = minf(foot2, ad.point("an_" + s, "heel").y)
		foot2 = minf(foot2, ad.point("an_" + s, "toe").y)
	var crown: float = ad.point("head", "top").y
	log_lines.append("%s new  hands %.2f  feet %.2f  crown %.2f" % [pose, hand2 - y0, foot2 - y0, crown - y0])


func _views() -> void:
	var refm := _add_figure("reference", Vector3(-0.55, 0.0, 0.0))
	var her := _add_figure("matron", Vector3(0.45, 0.0, 0.0))
	for fig in figures:
		_pose_fig(fig, POSES[0][1], 2.0)
	_measure(her, "guard")
	log_lines.append("stand_body_y %.3f k_height %.3f" % [her["adapter"].stand_body_y, her["adapter"].k_height])
	var views := [["front", 0.0], ["side", -PI * 0.5], ["three_quarter", -PI * 0.25]]
	for v in views:
		for fig in figures:
			(fig["mv"] as Node3D).rotation.y = float(v[1])
		await _shot("view_" + v[0], Vector3(0.0, 1.55, 8.0), Vector3(0.0, 0.98, 0.0), 17.0)


func _lineup() -> void:
	var names := ["reference", "stick_bare", "thin_bare", "matron_bare", "average_bare"]
	var x := -1.7
	for n in names:
		_add_figure(n, Vector3(x, 0.0, 0.0), false, false)
		x += 0.85
	for fig in figures:
		_pose_fig(fig, POSES[0][1], 2.0)
	await _shot("lineup_front", Vector3(0.0, 0.98, 11.0), Vector3(0.0, 0.98, 0.0), 0.0, 2.3)
	for fig in figures:
		(fig["mv"] as Node3D).rotation.y = -PI * 0.5
	await _shot("lineup_side", Vector3(0.0, 0.98, 11.0), Vector3(0.0, 0.98, 0.0), 0.0, 2.3)


func _compare() -> void:
	var refm := _add_figure("reference", Vector3(-0.9, 0.0, 0.0))
	var old := _add_figure("old", Vector3(0.0, 0.0, 0.0), true)
	var her := _add_figure("matron", Vector3(0.9, 0.0, 0.0))
	for pose in [POSES[0], POSES[1]]:
		for fig in figures:
			_pose_fig(fig, pose[1], 2.0)
		for i in 40:   # the old figure's noise textures build on a thread
			await get_tree().process_frame
		_measure(old, pose[0])
		_measure(her, pose[0])
		for fig in figures:
			(fig["mv"] as Node3D).rotation.y = 0.0
		await _shot("compare_%s_front" % pose[0], Vector3(0.0, 1.55, 9.0), Vector3(0.0, 1.0, 0.0), 0.0, 2.4)
		for fig in figures:
			(fig["mv"] as Node3D).rotation.y = -PI * 0.3
		await _shot("compare_%s_34" % pose[0], Vector3(0.0, 1.55, 9.0), Vector3(0.0, 1.0, 0.0), 0.0, 2.4)


func _poses() -> void:
	var old := _add_figure("old", Vector3(-1.2, 0.0, 0.0), true)
	var her := _add_figure("matron", Vector3(0.0, 0.0, 0.0))
	var her2 := _add_figure("matron", Vector3(1.2, 0.0, 0.0))
	her2["contact"] = true
	for i in 40:
		await get_tree().process_frame
	var t := 2.0
	for pose in POSES:
		for fig in figures:
			(fig["mv"] as Node3D).rotation.y = -PI * 0.3
			_pose_fig(fig, pose[1], t)
		_measure(old, pose[0])
		_measure(her, pose[0])
		_measure(her2, pose[0] + "+contact")
		log_lines.append_array(her2["adapter"].debug)
		her2["adapter"].debug.clear()
		await _shot("pose_" + pose[0], Vector3(0.0, 1.3, 9.0), Vector3(0.0, 1.0, 0.0), 0.0, 3.0)
		t += 0.37


func _detail() -> void:
	var her := _add_figure("matron", Vector3(0.0, 0.0, 0.0))
	_pose_fig(her, POSES[0][1], 2.0)
	(her["mv"] as Node3D).rotation.y = -PI * 0.22
	await _shot("detail_upper_34", Vector3(0.0, 1.55, 2.6), Vector3(0.0, 1.35, 0.0), 22.0)
	(her["mv"] as Node3D).rotation.y = PI
	await _shot("detail_back", Vector3(0.0, 1.5, 4.2), Vector3(0.0, 1.15, 0.0), 26.0)
	(her["mv"] as Node3D).rotation.y = 0.0
	await _shot("detail_face", Vector3(0.0, 1.66, 1.1), Vector3(0.0, 1.62, 0.0), 22.0)
	(her["mv"] as Node3D).rotation.y = -PI * 0.5
	await _shot("detail_feet", Vector3(0.0, 0.35, 2.2), Vector3(0.0, 0.25, 0.0), 26.0)


func _calib() -> void:
	var cols := [0.18, 0.5, 0.86]
	var x := -0.8
	for c in cols:
		var b := BoxMesh.new()
		b.size = Vector3(0.5, 0.5, 0.05)
		var mi := MeshInstance3D.new()
		mi.mesh = b
		var m := StandardMaterial3D.new()
		m.albedo_color = Color(c, c, c)
		m.roughness = 0.9
		mi.material_override = m
		mi.position = Vector3(x, 1.0, 0.0)
		add_child(mi)
		x += 0.8
	await _shot("calib", Vector3(0.0, 1.0, 5.0), Vector3(0.0, 1.0, 0.0), 30.0)
	var img := Image.load_from_file(out_dir + "/calib.png")
	for i in 3:
		var px := img.get_pixel(int(512 + (-0.8 + 0.8 * i) / 2.68 * 1024 * 0.5 * 1.0), 512)
		log_lines.append("card %d %s" % [i, str(px)])
	log_lines.append("floor " + str(img.get_pixel(512, 1000)))


# The Turn's look back: the neck pivot at 2.6 rad, shared by two neck bones.
func _neck() -> void:
	var her := _add_figure("matron", Vector3(0.0, 0.0, 0.0))
	her["contact"] = true
	var d := {"state": 7, "speed": 0.0, "stride": 0.0, "turn": 0}
	_pose_fig(her, d, 2.0)
	var p: Dictionary = (her["mv"] as Node3D)._p
	for yaw in [0.0, 1.3, 2.6]:
		(p["neck"] as Node3D).rotation = Vector3(0.0, yaw, 0.0)
		(p["head"] as Node3D).rotation = Vector3(0.0, 0.0, 0.0)
		her["adapter"].apply()
		(her["mv"] as Node3D).rotation.y = -0.5
		await _shot("neck_%d" % int(yaw * 10), Vector3(0.0, 1.62, 1.6), Vector3(0.0, 1.5, 0.0), 30.0)


# Triangle counts: today's primitives vs the new body, and Godot's own
# automatic LODs (meshoptimizer, via ImporterMesh.generate_lods) for distance.
func _lod() -> void:
	var old := _add_figure("old", Vector3(-1.0, 0.0, 0.0), true)
	var tri_old := 0
	var calls_old := 0
	for c in (old["mv"] as Node3D)._figure.find_children("*", "MeshInstance3D", true, false):
		var m := (c as MeshInstance3D).mesh
		if m == null:
			continue
		calls_old += m.get_surface_count()
		for si in m.get_surface_count():
			var arr := m.surface_get_arrays(si)
			var idx = arr[Mesh.ARRAY_INDEX]
			tri_old += (idx.size() / 3) if idx != null else (arr[Mesh.ARRAY_VERTEX].size() / 3)
	log_lines.append("old figure: %d triangles in %d mesh draws" % [tri_old, calls_old])
	var doc := GLTFDocument.new()
	var st := GLTFState.new()
	doc.append_from_file(ProjectSettings.globalize_path("res://assets/matron.glb"), st)
	var total := 0
	var t0 := Time.get_ticks_msec()
	for gm in st.get_meshes():
		var im: ImporterMesh = gm.mesh
		var tris := 0
		for si in im.get_surface_count():
			tris += im.get_surface_arrays(si)[Mesh.ARRAY_INDEX].size() / 3
		im.generate_lods(25.0, 60.0, [])
		var lods := []
		for si in im.get_surface_count():
			for li in im.get_surface_lod_count(si):
				lods.append([snappedf(im.get_surface_lod_size(si, li), 0.001), im.get_surface_lod_indices(si, li).size() / 3])
		total += tris
		log_lines.append("%s: %d tris, lods %s" % [gm.resource_name if gm.resource_name != "" else str(gm), tris, str(lods)])
	log_lines.append("new total %d tris; lod generation %d ms" % [total, Time.get_ticks_msec() - t0])


# Not the look-dev of the skin or the light (other tracks): just this body in
# something like her deck at night -- one caged bulb, the sky's cold spill.
func _dark() -> void:
	var env := WorldEnvironment.new()
	env.environment = Environment.new()
	env.environment.background_mode = Environment.BG_COLOR
	env.environment.background_color = Color(0.015, 0.018, 0.025)
	env.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.environment.ambient_light_color = Color(0.3, 0.36, 0.5)
	env.environment.ambient_light_energy = 0.06
	env.environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	env.environment.fog_enabled = true
	env.environment.fog_light_color = Color(0.03, 0.035, 0.05)
	env.environment.fog_density = 0.05
	add_child(env)
	var bulb := OmniLight3D.new()
	bulb.light_color = Color("ffd49a")
	bulb.light_energy = 1.3
	bulb.omni_range = 6.0
	bulb.shadow_enabled = true
	bulb.position = Vector3(-0.9, 2.3, 0.5)
	add_child(bulb)
	var sky := DirectionalLight3D.new()
	sky.light_color = Color(0.55, 0.65, 0.85)
	sky.light_energy = 0.08
	add_child(sky)
	sky.look_at_from_position(Vector3(2.0, 3.0, -3.0), Vector3.ZERO)
	var floor_mesh := PlaneMesh.new()
	floor_mesh.size = Vector2(30.0, 30.0)
	var fl := MeshInstance3D.new()
	fl.mesh = floor_mesh
	var fm := StandardMaterial3D.new()
	fm.albedo_color = Color(0.16, 0.15, 0.14)
	fm.roughness = 0.8
	fm.metallic = 0.3
	fl.material_override = fm
	add_child(fl)
	cam = Camera3D.new()
	add_child(cam)
	cam.current = true


func _mood() -> void:
	var her := _add_figure("matron", Vector3(0.0, 0.0, 0.0))
	her["contact"] = true
	_pose_fig(her, POSES[1][1], 3.1)
	(her["mv"] as Node3D).rotation.y = 0.25
	await _shot("mood_shuffle", Vector3(0.6, 1.52, 3.4), Vector3(0.0, 1.25, 0.0), 40.0)
	_pose_fig(her, POSES[4][1], 2.2)
	(her["mv"] as Node3D).rotation.y = -0.2
	await _shot("mood_crawl", Vector3(0.4, 1.52, 3.0), Vector3(0.0, 0.5, 0.0), 45.0)
	var d := {"state": 7, "speed": 0.0, "stride": 0.0, "turn": 0}
	_pose_fig(her, d, 2.0)
	var p: Dictionary = (her["mv"] as Node3D)._p
	(p["neck"] as Node3D).rotation = Vector3(0.1, 2.6, 0.1)
	(p["head"] as Node3D).rotation = Vector3(-0.15, 0.0, 0.2)
	her["adapter"].apply()
	(her["mv"] as Node3D).rotation.y = PI
	await _shot("mood_lookback", Vector3(0.3, 1.55, 1.7), Vector3(0.0, 1.45, 0.0), 40.0)
