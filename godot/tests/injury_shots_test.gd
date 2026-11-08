extends SceneTree
## J-044i first slice: the injury render on her body, driven by the per-region
## injury dictionary J-044g will fill (J-044a §D/E: region -> {j_region, j_event}).
## Bruise, swelling and red mark only; cuts and blood need J-044d's surface
## feature and come in the next slice. Thresholds are the J-044a calibration:
## red mark J_event >= 25 N·s, bruise J_region >= 100, swelling J_region >= 200.
## Injuries never heal within a run (Cory's no-heal ruling): a smaller later
## value never lowers a region. With --out=<dir> saves each moment as a PNG.
##   xvfb-run -a godot --path godot --fixed-fps 30 --resolution 960x720 \
##       --script res://tests/injury_shots_test.gd -- --out=/workspace/injury_shots

const MatronView := preload("res://presentation/life/matron_view.gd")
const DT := 1.0 / 30.0

var _out := ""
var _cam: Camera3D
var _her: Node3D
var _fails := 0
var _pose := {"state": 7, "speed": 0.0, "stride": 0.0, "turn": 0, "stage": 1, "sees_player": true,
	"alive": true, "released": true, "position": Vector3(0, MatronView.STAND_CENTRE, 0), "yaw": 0.0,
	"render_yaw": 0.0, "exposure": 0.6, "floor_y": 0.0, "injury": {}}


func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--out="):
			_out = arg.substr(6)
	_run.call_deferred()


func _check(ok: bool, what: String, detail: String) -> void:
	print("INJURY %s %s %s" % ["PASS" if ok else "FAIL", what, detail])
	if not ok:
		_fails += 1


func _studio() -> void:
	var env := WorldEnvironment.new()
	env.environment = Environment.new()
	env.environment.background_mode = Environment.BG_COLOR
	env.environment.background_color = Color(0.05, 0.055, 0.06)
	env.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.environment.ambient_light_color = Color(0.6, 0.62, 0.66)
	env.environment.ambient_light_energy = 0.25
	env.environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	root.add_child(env)
	var key := DirectionalLight3D.new()
	key.light_energy = 1.1
	key.light_color = Color(1.0, 0.93, 0.85)
	key.shadow_enabled = true
	root.add_child(key)
	key.look_at_from_position(Vector3(-2.0, 4.0, 3.0), Vector3(0, 0.3, 0))
	var fill := OmniLight3D.new()
	fill.light_energy = 1.4
	fill.omni_range = 6.0
	fill.position = Vector3(1.5, 1.6, 1.8)
	root.add_child(fill)
	_cam = Camera3D.new()
	root.add_child(_cam)
	_cam.current = true


func _step(frames: int) -> void:
	for i in frames:
		if _her != null:
			_her._clock += DT
			_her._state_age += DT
			_her._update_figure(_pose, DT, _cam.global_position, _cam.global_position)
		await process_frame


func _shot(name: String, at: Vector3, look: Vector3, fov: float = 50.0) -> void:
	_cam.fov = fov
	_cam.look_at_from_position(at, look)
	if _out == "":
		await process_frame
		return
	await process_frame
	await RenderingServer.frame_post_draw
	root.get_texture().get_image().save_png(_out.path_join(name + ".png"))
	print("INJURY SHOT ", _out.path_join(name + ".png"))


func _layer() -> Variant:
	return _her.get("_injury") if _her != null else null


func _ring_band(img: Image, vertical: bool, first: bool) -> float:
	# Mean (R+G+B)/3 of the ring texture's annulus (r 0.55..0.90) on one
	# side: vertical+first = bottom (v > 0.5), vertical+!first = top
	# (v < -0.5), !vertical+first = inner (u < -0.5), !vertical+!first =
	# outer (u > 0.5). eye_l's texture (mirror=false); eye_r mirrors u.
	# The baked colour is RING_BASE.lerp(RING_DARK, w), so a side whose
	# weight is high reads darker.
	var size := img.get_width()
	var sum := 0.0
	var n := 0
	for y in size:
		for x in size:
			var u := (float(x) / float(size - 1)) * 2.0 - 1.0
			var v := (float(y) / float(size - 1)) * 2.0 - 1.0
			var r := Vector2(u, v).length()
			if r < 0.55 or r > 0.90:
				continue
			if vertical:
				if first and v < 0.5:
					continue
				if not first and v > -0.5:
					continue
			else:
				if first and u > -0.5:
					continue
				if not first and u < 0.5:
					continue
			var c: Color = img.get_pixel(x, y)
			sum += (c.r + c.g + c.b) / 3.0
			n += 1
	return sum / float(maxi(n, 1))


func _grab(at: Vector3, look: Vector3, fov: float) -> Image:
	# One rendered frame from this camera, for a pixel measurement.
	_cam.fov = fov
	_cam.look_at_from_position(at, look)
	await process_frame
	await RenderingServer.frame_post_draw
	return root.get_texture().get_image()


func _darkened(lit: Image, bare: Image, centre: Vector2, half: int, step_lum: float) -> Vector2:
	# In a (2 half)² box round centre: how many pixels are darker in lit
	# than in bare by more than step_lum (luminance 0..1), and the box's
	# mean darkening (bare - lit). Same pose and camera, only the ring
	# differs, so the difference is the bruise.
	var n := 0
	var sum := 0.0
	var px := 0
	for y in range(int(centre.y) - half, int(centre.y) + half):
		for x in range(int(centre.x) - half, int(centre.x) + half):
			if x < 0 or y < 0 or x >= lit.get_width() or y >= lit.get_height():
				continue
			var d := bare.get_pixel(x, y).get_luminance() - lit.get_pixel(x, y).get_luminance()
			sum += d
			px += 1
			if d > step_lum:
				n += 1
	return Vector2(float(n), sum / float(maxi(px, 1)))


func _skinned_skin_near(centre: Vector3, radius: float) -> PackedVector3Array:
	# Her skin mesh's vertices within radius of centre, skinned on the CPU
	# at the current pose: world = skeleton * bone pose * bind * vertex,
	# weighted. What the renderer draws as her skin there.
	var out := PackedVector3Array()
	var skel: Skeleton3D = _her._rig.skel
	var found := skel.find_children("skin", "MeshInstance3D", false, false)
	if found.is_empty():
		return out
	var mi := found[0] as MeshInstance3D
	var sk: Skin = mi.skin
	var mats: Array[Transform3D] = []
	for k in sk.get_bind_count():
		var bi := sk.get_bind_bone(k)
		if bi < 0:
			bi = skel.find_bone(sk.get_bind_name(k))
		mats.append(skel.global_transform * skel.get_bone_global_pose(bi) * sk.get_bind_pose(k))
	var arr: Array = mi.mesh.surface_get_arrays(0)
	var v: PackedVector3Array = arr[Mesh.ARRAY_VERTEX]
	var b: PackedInt32Array = arr[Mesh.ARRAY_BONES]
	var w: PackedFloat32Array = arr[Mesh.ARRAY_WEIGHTS]
	var bpv := int(b.size() / v.size())
	for i in v.size():
		var p := Vector3.ZERO
		for j in bpv:
			var wt := w[i * bpv + j]
			if wt > 0.0:
				p += (mats[b[i * bpv + j]] * v[i]) * wt
		if p.distance_to(centre) < radius:
			out.append(p)
	return out


func _max_off_skin(mi: MeshInstance3D, centre: Vector3, radius: float) -> float:
	# The farthest any vertex of mi's mesh (as placed) lies from her skin's
	# nearest vertex. -1 when there is nothing to measure.
	if mi == null or mi.mesh == null or mi.mesh.get_surface_count() < 1:
		return -1.0
	var skin := _skinned_skin_near(centre, radius)
	if skin.is_empty():
		return -1.0
	var verts: PackedVector3Array = mi.mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX]
	var worst := 0.0
	for lv in verts:
		var p := mi.global_transform * lv
		var best := INF
		for s in skin:
			best = minf(best, p.distance_squared_to(s))
		worst = maxf(worst, sqrt(best))
	return worst


func _bruise_area_facing(mi: MeshInstance3D, img: Image, cam: Vector3, min_alpha: float, min_facing: float) -> float:
	# World area (m²) of mi's triangles whose bruise alpha (the ring texture
	# as matron_injury_ring.gdshader projects it, at the triangle centre)
	# is at least min_alpha and whose surface faces cam by more than
	# min_facing (N.V).
	if mi == null or mi.mesh == null or mi.mesh.get_surface_count() < 1:
		return 0.0
	var mat := mi.material_override as ShaderMaterial
	if mat == null or mat.get_shader_parameter("uv_half") == null:
		return 0.0
	var c: Vector2 = mat.get_shader_parameter("uv_center")
	var h: Vector2 = mat.get_shader_parameter("uv_half")
	var col: Color = mat.get_shader_parameter("albedo_color")
	var arr: Array = mi.mesh.surface_get_arrays(0)
	var v: PackedVector3Array = arr[Mesh.ARRAY_VERTEX]
	var nrm: PackedVector3Array = arr[Mesh.ARRAY_NORMAL]
	var idx: PackedInt32Array = arr[Mesh.ARRAY_INDEX]
	var xf := mi.global_transform
	var size := img.get_width()
	var area := 0.0
	for t in range(0, idx.size(), 3):
		var a := idx[t]
		var bb := idx[t + 1]
		var cc := idx[t + 2]
		var p := (v[a] + v[bb] + v[cc]) / 3.0
		var uv := Vector2((p.x - c.x) / h.x, (c.y - p.y) / h.y) * 0.5 + Vector2(0.5, 0.5)
		if uv.x < 0.0 or uv.x > 1.0 or uv.y < 0.0 or uv.y > 1.0:
			continue
		var ln := (nrm[a] + nrm[bb] + nrm[cc]).normalized()
		var alpha := img.get_pixel(int(uv.x * (size - 1)), int(uv.y * (size - 1))).a * col.a * smoothstep(-0.35, 0.05, ln.z)
		if alpha < min_alpha:
			continue
		var wp := xf * p
		var wn := (xf.basis * ln).normalized()
		if wn.dot((cam - wp).normalized()) <= min_facing:
			continue
		area += 0.5 * ((xf * v[bb]) - (xf * v[a])).cross((xf * v[cc]) - (xf * v[a])).length()
	return area


func _posed_copy(mi: MeshInstance3D) -> MeshInstance3D:
	# The cheek bruise's skin copy follows the jaw and neck as her skin does
	# (matron_injury_ring.gdshader `blend`: per-vertex CUSTOM0 weights on
	# bone_a..bone_d, the rest on the head). This bakes that blend on the
	# CPU into a hidden twin at the same place with the same material, so
	# _max_off_skin and _bruise_area_facing measure what is drawn. Returns
	# mi itself when it does not blend; free the twin after use.
	if mi == null or mi.mesh == null or mi.mesh.get_surface_count() < 1:
		return mi
	var mat := mi.material_override as ShaderMaterial
	if mat == null or not bool(mat.get_shader_parameter("blend")):
		return mi
	var arr: Array = mi.mesh.surface_get_arrays(0)
	var v: PackedVector3Array = arr[Mesh.ARRAY_VERTEX]
	var nrm: PackedVector3Array = arr[Mesh.ARRAY_NORMAL]
	var cw: PackedFloat32Array = arr[Mesh.ARRAY_CUSTOM0]
	var bones: Array[Transform3D] = []
	for k in ["bone_a", "bone_b", "bone_c", "bone_d"]:
		bones.append(mat.get_shader_parameter(k) as Transform3D)
	var out_v := PackedVector3Array()
	var out_n := PackedVector3Array()
	for i in v.size():
		var wh := 1.0
		var p := Vector3.ZERO
		var n := Vector3.ZERO
		for k in 4:
			var w := cw[i * 4 + k]
			wh -= w
			p += (bones[k] * v[i]) * w
			n += (bones[k].basis * nrm[i]) * w
		out_v.append(p + v[i] * wh)
		out_n.append((n + nrm[i] * wh).normalized())
	var surf := []
	surf.resize(Mesh.ARRAY_MAX)
	surf[Mesh.ARRAY_VERTEX] = out_v
	surf[Mesh.ARRAY_NORMAL] = out_n
	surf[Mesh.ARRAY_INDEX] = arr[Mesh.ARRAY_INDEX]
	var am := ArrayMesh.new()
	am.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, surf)
	var twin := MeshInstance3D.new()
	twin.mesh = am
	twin.material_override = mat
	twin.visible = false
	root.add_child(twin)
	twin.global_transform = mi.global_transform
	return twin


func _set_injury(injury: Dictionary) -> void:
	_pose["injury"] = injury


func _run() -> void:
	_studio()
	_her = MatronView.new()
	root.add_child(_her)
	_her._build_figure()
	_her._build_sparks()
	await _step(2)
	# Miss: nothing recorded, nothing shows.
	_set_injury({})
	await _step(2)
	var layer: Variant = _layer()
	_check(layer != null, "layer", "injury layer exists on the view")
	if layer == null:
		print("INJURY FAIL (1 failed)")
		quit(1)
		return
	_check(layer.visible_regions().is_empty(), "miss", "regions %s" % [layer.visible_regions()])
	# Glance: 20 N·s event, below the 25 N·s red-mark line: nothing.
	_set_injury({"face_l": {"j_region": 30.0, "j_event": 20.0}})
	await _step(2)
	_check(layer.visible_regions().is_empty(), "glance", "regions %s" % [layer.visible_regions()])
	# Light tap: 30 N·s event marks, 30 N·s in the region bruises nothing.
	_set_injury({"face_l": {"j_region": 30.0, "j_event": 30.0}})
	await _step(2)
	_check(layer.mark_visible("face_l") and not layer.bruise_visible("face_l") and not layer.swelling_visible("face_l"),
		"tap", "mark %s bruise %s swell %s" % [layer.mark_visible("face_l"), layer.bruise_visible("face_l"), layer.swelling_visible("face_l")])
	# Solid right punch on the left cheek: 150 N·s in the region bruises it, and only it.
	_set_injury({"face_l": {"j_region": 150.0, "j_event": 60.0}})
	await _step(2)
	_check(layer.bruise_visible("face_l") and not layer.bruise_visible("face_r") and layer.patch_visible("face_l"), "left_cheek",
		"l %s r %s patch %s" % [layer.bruise_visible("face_l"), layer.bruise_visible("face_r"), layer.patch_visible("face_l")])
	# Heavy: 250 N·s swells the same cheek.
	_set_injury({"face_l": {"j_region": 250.0, "j_event": 60.0}})
	await _step(2)
	_check(layer.bruise_visible("face_l") and layer.swelling_visible("face_l"), "swell",
		"bruise %s swell %s" % [layer.bruise_visible("face_l"), layer.swelling_visible("face_l")])
	# Right cheek, same dose: bruises on the right, the left keeps its swelling.
	_set_injury({"face_l": {"j_region": 250.0, "j_event": 60.0}, "face_r": {"j_region": 150.0, "j_event": 60.0}})
	await _step(2)
	_check(layer.bruise_visible("face_r") and layer.swelling_visible("face_l"), "right_cheek",
		"r %s l_swell %s" % [layer.bruise_visible("face_r"), layer.swelling_visible("face_l")])
	# Cheeks must land on the correct side: face_l on her left, face_r on
	# her right. The head bone's local +X is her left (right-handed basis,
	# local +Z out of the face), so the lateral dot is +0.055 for face_l
	# and -0.055 for face_r; a sign flip here means the bruise shows on
	# the wrong cheek, which the visibility flags above cannot catch.
	var head_bone_side: Transform3D = _her._rig.bone_world("head")
	var left_dir: Vector3 = head_bone_side.basis.x
	var fl_lat: float = ((layer._patches["face_l"] as MeshInstance3D).global_position - head_bone_side.origin).dot(left_dir)
	var fr_lat: float = ((layer._patches["face_r"] as MeshInstance3D).global_position - head_bone_side.origin).dot(left_dir)
	_check(fl_lat > 0.02 and fr_lat < -0.02, "cheek_sides",
		"face_l lateral %.3f face_r lateral %.3f" % [fl_lat, fr_lat])
	# Cheek quads must wrap the cheek, not float past the face edge: the
	# quad's outward normal tilts laterally off the face normal, toward
	# her left for face_l and her right for face_r, by 20-40 degrees
	# (the cheek surface normal at the anchor is ~33 degrees off +Z).
	# A flat +Z normal reads 0.000 here and overhangs the face edge
	# straight on (Claude #306 eye-check follow-up).
	var fl_n: Vector3 = (layer._patches["face_l"] as MeshInstance3D).global_transform.basis.z.normalized()
	var fr_n: Vector3 = (layer._patches["face_r"] as MeshInstance3D).global_transform.basis.z.normalized()
	var fl_tilt: float = fl_n.dot(left_dir)
	var fr_tilt: float = fr_n.dot(left_dir)
	_check(fl_tilt > 0.34 and fl_tilt < 0.64 and fr_tilt < -0.34 and fr_tilt > -0.64, "cheek_angle",
		"face_l tilt %.3f face_r tilt %.3f" % [fl_tilt, fr_tilt])
	# J-059 cheeks on her skin, the eye's three guards for each cheek
	# (face_l swollen at 250, face_r bruised at 150 here). From the side
	# a flat cheek quad stands off the face like the old eye "monocle";
	# the bruise must lie on her skin and curve round the cheek with it.
	# The side camera stands on the bruised cheek's side (head +X for
	# face_l, -X for face_r), placed as the eye's side camera is.
	# 1. cheek_on_skin: every vertex the bruise is drawn on lies within
	#    1.5 mm of a CPU-skinned skin vertex as posed now (the flat quads
	#    measured 43.6 mm / 28.7 mm off on 2026-11-08, overhanging the jaw
	#    line; the cheek skin also follows the jaw, so this is measured on
	#    the copy as the shader blends it, _posed_copy).
	# 2. cheek_side_facing: bruise surface with alpha >= 0.3 that faces
	#    the side camera (N.V > 0.25) covers at least 6 cm² (measured
	#    28.4 / 10.6 cm² on 2026-11-08).
	# 3. cheek_side_pixels (rendered runs only): pixels round the cheek in
	#    the side view are darker with the bruise than with it hidden,
	#    same pose: at least 1500 px darker by > 0.04 in a 140 px box and
	#    a mean darkening >= 0.012 (measured 12397 px, 0.065 / 5792 px,
	#    0.051 at 960x720; the flat quads 4080 / 3730 px).
	var cheek_tex: Image = (layer._texture as Texture2D).get_image()
	var anchors: Dictionary = (layer.get_script() as Script).get_script_constant_map()["ANCHORS"]
	for cheek: String in ["face_l", "face_r"]:
		var cheek_mi := layer._patches[cheek] as MeshInstance3D
		var head_cheek: Transform3D = _her._rig.bone_world("head")
		var cheek_off: Vector3 = anchors[cheek][1]
		var cheek_pos: Vector3 = head_cheek.origin + head_cheek.basis * cheek_off
		var cheek_dir := 1.0 if cheek_off.x > 0.0 else -1.0
		var cheek_side: Vector3 = cheek_pos + head_cheek.basis.x * (0.45 * cheek_dir) + head_cheek.basis.z * 0.12 + head_cheek.basis.y * 0.02
		var cheek_drawn := _posed_copy(cheek_mi)
		var cheek_off_skin := _max_off_skin(cheek_drawn, cheek_pos, 0.10)
		_check(cheek_off_skin >= 0.0 and cheek_off_skin < 0.0015, "cheek_on_skin_" + cheek.right(1),
			"%s vertex farthest off her skin %.4f m" % [cheek, cheek_off_skin])
		var cheek_area := _bruise_area_facing(cheek_drawn, cheek_tex, cheek_side, 0.3, 0.25)
		if cheek_drawn != cheek_mi:
			cheek_drawn.free()
		_check(cheek_area >= 6.0e-4, "cheek_side_facing_" + cheek.right(1),
			"%s bruise area facing the side camera %.2f cm²" % [cheek, cheek_area * 1e4])
		if DisplayServer.get_name() == "headless":
			print("INJURY SKIP cheek_side_pixels_%s headless: no render to measure" % cheek.right(1))
		else:
			await _shot("injury_cheek_side_" + cheek.right(1), cheek_side, cheek_pos, 35.0)
			var with_bruise := await _grab(cheek_side, cheek_pos, 35.0)
			cheek_mi.visible = false
			var cheek_bare := await _grab(cheek_side, cheek_pos, 35.0)
			cheek_mi.visible = true
			var cheek_px: Vector2 = _cam.unproject_position(cheek_pos) * Vector2(with_bruise.get_size()) / root.get_visible_rect().size
			var cheek_dark := _darkened(with_bruise, cheek_bare, cheek_px, 70, 0.04)
			_check(cheek_dark.x >= 1500.0 and cheek_dark.y >= 0.012, "cheek_side_pixels_" + cheek.right(1),
				"%s side view: %d px darker by >0.04 round the cheek, mean darkening %.3f" % [cheek, int(cheek_dark.x), cheek_dark.y])
	# Stills of both bruised cheeks: straight on, and close on the left
	# cheek square to her face (head frame).
	var head_cheeks: Transform3D = _her._rig.bone_world("head")
	var cheek_l_pos: Vector3 = head_cheeks.origin + head_cheeks.basis * (anchors["face_l"][1] as Vector3)
	await _shot("injury_cheek_front", head_cheeks.origin + head_cheeks.basis.z * 0.7 + head_cheeks.basis.y * 0.05, head_cheeks.origin, 45.0)
	await _shot("injury_cheek_close", cheek_l_pos + head_cheeks.basis.z * 0.28 + head_cheeks.basis.x * 0.06 + head_cheeks.basis.y * 0.02, cheek_l_pos, 30.0)
	# No healing: a smaller later value never lowers a region.
	_set_injury({"face_l": {"j_region": 30.0, "j_event": 30.0}, "face_r": {"j_region": 150.0, "j_event": 60.0}})
	await _step(2)
	_check(layer.bruise_visible("face_l") and layer.swelling_visible("face_l"), "no_heal",
		"sev %.0f" % layer.severity("face_l"))
	# Skull: 150 N·s bruises the back of the head, and the patch must face
	# OUT of the skull (the old quad faced into it and culling hid it).
	_set_injury({"skull": {"j_region": 150.0, "j_event": 60.0}})
	await _step(2)
	var head_bone: Transform3D = _her._rig.bone_world("head")
	var skull_out: float = layer.patch_outward("skull", head_bone.origin)
	var face_out: float = layer.patch_outward("face_l", head_bone.origin)
	_check(layer.bruise_visible("skull") and layer.patch_visible("skull") and skull_out > 0.0 and face_out > 0.0,
		"skull", "bruise %s patch %s outward %.3f face %.3f" % [layer.bruise_visible("skull"), layer.patch_visible("skull"), skull_out, face_out])
	# J-059 black eye: a 75+ N·s punch to her left eye bruises it dark and
	# swells the lid. eye_l/eye_r (J-044a §D, 14 regions): a fist contact
	# on the front of the head within 0.035 m of a socket centre goes to
	# that eye instead of face_l/face_r, split by the head-local sagittal
	# plane, never world X. The socket centres are the eye bones' rest
	# offsets — but the rig's eye bone NAMES sit on the opposite sides
	# (the bone called eye_l is at head-local -X), so the anchors follow
	# anatomy (+X is her left), not the bone names. The classification is
	# a pure function of a head-local point; the live feed is J-044g's.
	var socket_l := Vector3(0.0315, 0.0505, 0.0847)
	var socket_r := Vector3(-0.0315, 0.0505, 0.0847)
	_check(layer.classify_head_hit(socket_l) == "eye_l", "eye_class_l", "at left socket -> %s" % layer.classify_head_hit(socket_l))
	_check(layer.classify_head_hit(socket_r) == "eye_r", "eye_class_r", "at right socket -> %s" % layer.classify_head_hit(socket_r))
	_check(layer.classify_head_hit(Vector3(0.02, 0.05, 0.085)) == "eye_l", "eye_class_near_l", "0.012 from left socket")
	_check(layer.classify_head_hit(Vector3(-0.02, 0.05, 0.085)) == "eye_r", "eye_class_near_r", "0.012 from right socket")
	_check(layer.classify_head_hit(Vector3(0.055, -0.015, 0.085)) == "face_l", "eye_class_cheek", "cheek point stays face_l")
	_check(layer.classify_head_hit(Vector3(0.08, 0.05, 0.085)) == "face_l", "eye_class_far", "0.05 from socket stays face_l")
	# Below 75: nothing on the eye — no red mark, no ring, no lid.
	_set_injury({"eye_l": {"j_region": 30.0, "j_event": 30.0}})
	await _step(2)
	_check(not layer.black_eye_visible("eye_l") and not layer.patch_visible("eye_l") and not layer.lid_visible("eye_l") and "eye_l" not in layer.visible_regions(), "eye_below", "regions %s" % [layer.visible_regions()])
	_set_injury({"eye_l": {"j_region": 74.0, "j_event": 80.0}})
	await _step(2)
	_check(not layer.black_eye_visible("eye_l") and not layer.patch_visible("eye_l"),
		"eye_below74", "74 N·s still nothing")
	# At 75: the ring shows, faint at first (blood pools over ~15 s).
	_set_injury({"eye_l": {"j_region": 75.0, "j_event": 80.0}})
	await _step(2)
	_check(layer.black_eye_visible("eye_l") and layer.patch_visible("eye_l") and layer.black_eye_darkness("eye_l") < 0.5,
		"eye_ring", "ring %s dark %.2f" % [layer.patch_visible("eye_l"), layer.black_eye_darkness("eye_l")])
	# 15 s later the ring is at full darkness and holds.
	layer._clock += 15.0
	await _step(2)
	_check(layer.black_eye_darkness("eye_l") >= 0.99, "eye_dark", "dark %.2f" % layer.black_eye_darkness("eye_l"))
	# #309 side fix, kept: the ring's material is a shader (since 11-08 the
	# one that projects the ring onto her skin, matron_injury_ring.gdshader),
	# not a flat StandardMaterial3D.
	var ring_mat: Variant = (layer._patches["eye_l"] as MeshInstance3D).material_override
	_check(ring_mat is ShaderMaterial, "eye_fade",
		"ring material is %s" % (ring_mat.get_class() if ring_mat != null else "null"))
	# #309 rotation: the ring's dark weight must sit on the lower lid and
	# the inner corner, not the upper lid / outer corner (a 180° flip).
	# A rendered-pixel check was measured and does not fit: the close
	# still's below/above luminance gap flips sign with band placement
	# (eyeball and lower-lid brightness swamp the arc), so it would be a
	# flaky CI check. This reads the baked texture instead: the bottom
	# annulus must be darker than the top, and the inner darker than the
	# outer, for eye_l. A sign flip in _ring_texture reverses both and
	# fails here instead of passing. The mapping (image-bottom = lower
	# lid, image-left = inner corner for eye_l) is verified by the marker
	# stills in the #309 eye check, not by this check alone.
	var ring_img: Image = (layer._ring_textures[false] as Texture2D).get_image()
	var ring_bot := _ring_band(ring_img, true, true)
	var ring_top := _ring_band(ring_img, true, false)
	var ring_in := _ring_band(ring_img, false, true)
	var ring_out := _ring_band(ring_img, false, false)
	_check(ring_bot + 0.02 < ring_top and ring_in + 0.02 < ring_out, "eye_ring_sides",
		"bottom %.3f top %.3f inner %.3f outer %.3f" % [ring_bot, ring_top, ring_in, ring_out])
	# Stills: the black left eye at full darkness, front and three-quarters
	# from her left. No swell or slit yet: this is the 75-dose black eye.
	var head_eye: Vector3 = _her.head_world_position()
	await _shot("injury_black_eye_front", head_eye + Vector3(0.0, 0.05, 0.7), head_eye, 45.0)
	await _shot("injury_black_eye_three_quarter", head_eye + Vector3(0.45, 0.08, 0.55), head_eye, 40.0)
	# Close-up on the left eye alone (head frame, so the camera is square to
	# her face despite the pose's head turn): the ring's darkest arc must
	# read on the lower lid and the inner corner.
	var eye_l_pos: Vector3 = (layer._patches["eye_l"] as MeshInstance3D).global_position
	var head_bone_close: Transform3D = _her._rig.bone_world("head")
	var eye_cam: Vector3 = eye_l_pos + head_bone_close.basis.z * 0.28 + head_bone_close.basis.x * 0.06 + head_bone_close.basis.y * 0.02
	await _shot("injury_black_eye_close", eye_cam, eye_l_pos, 30.0)
	# Side view for depth: the ring lies on her skin and curves round the
	# face with it; it must not float or sink.
	var eye_side: Vector3 = eye_l_pos + head_bone_close.basis.x * 0.45 + head_bone_close.basis.z * 0.12 + head_bone_close.basis.y * 0.02
	await _shot("injury_black_eye_side", eye_side, eye_l_pos, 35.0)
	# Cory 11-08: from the side the bruise must show, lying on her skin and
	# curving round the face (the flat ring stood off the eye like a
	# monocle; the #309 fade then hid it, leaving a white eyeball). The
	# camera is on her left (head +X), the side of the bruised eye_l.
	# Three guards, the first two need no display (CI runs headless):
	# 1. eye_on_skin: every vertex the ring is drawn on lies on her skin as
	#    posed now (within 1.5 mm of a CPU-skinned skin vertex), so no card
	#    stands off the face.
	# 2. eye_side_facing: ring surface with bruise alpha >= 0.3 that faces
	#    the side camera (N.V > 0.25) covers at least 6 cm² (measured
	#    23.7 cm² on 2026-11-08).
	# 3. eye_side_pixels (rendered runs only): pixels round the eye in the
	#    side view are darker with the ring than with it hidden, same pose:
	#    at least 1500 px darker by > 0.04 in a 140 px box and a mean
	#    darkening >= 0.012 (measured 5678 px, 0.038 at 960x720; the #309
	#    faded quad measures 0 px).
	var ring_mi := layer._patches["eye_l"] as MeshInstance3D
	_check(head_bone_close.basis.x.dot(eye_l_pos - head_bone_close.origin) > 0.02 and head_bone_close.basis.x.dot(eye_side - eye_l_pos) > 0.3,
		"eye_side_cam", "side camera on the bruised (left) eye")
	var off_skin := _max_off_skin(ring_mi, eye_l_pos, 0.09)
	_check(off_skin >= 0.0 and off_skin < 0.0015, "eye_on_skin", "ring vertex farthest off her skin %.4f m" % off_skin)
	var side_area := _bruise_area_facing(ring_mi, ring_img, eye_side, 0.3, 0.25)
	_check(side_area >= 6.0e-4, "eye_side_facing", "bruise area facing the side camera %.2f cm²" % (side_area * 1e4))
	if DisplayServer.get_name() == "headless":
		print("INJURY SKIP eye_side_pixels headless: no render to measure")
	else:
		var with_ring := await _grab(eye_side, eye_l_pos, 35.0)
		ring_mi.visible = false
		if layer._lids.has("eye_l"):
			(layer._lids["eye_l"] as MeshInstance3D).visible = false
		var bare := await _grab(eye_side, eye_l_pos, 35.0)
		ring_mi.visible = true
		# unproject_position is in the stretched canvas's units, not the
		# captured image's pixels: scale across.
		var centre_px: Vector2 = _cam.unproject_position(eye_l_pos) * Vector2(with_ring.get_size()) / root.get_visible_rect().size
		var dark := _darkened(with_ring, bare, centre_px, 70, 0.04)
		_check(dark.x >= 1500.0 and dark.y >= 0.012, "eye_side_pixels",
			"side view: %d px darker by >0.04 round the eye, mean darkening %.3f" % [int(dark.x), dark.y])
	# At 150: the lid swells. At 250: the eye is narrowed to a slit.
	_set_injury({"eye_l": {"j_region": 150.0, "j_event": 80.0}})
	await _step(2)
	_check(layer.lid_visible("eye_l") and not layer.eye_slit_visible("eye_l"), "eye_swell", "lid %s slit %s" % [layer.lid_visible("eye_l"), layer.eye_slit_visible("eye_l")])
	_set_injury({"eye_l": {"j_region": 250.0, "j_event": 80.0}})
	await _step(2)
	_check(layer.lid_visible("eye_l") and layer.eye_slit_visible("eye_l"), "eye_slit", "lid %s slit %s droop %.2f" % [layer.lid_visible("eye_l"), layer.eye_slit_visible("eye_l"), layer.lid_droop("eye_l")])
	# Stills of the swollen lid on her skin, close and from the side (the
	# lid band lies on the same skin copy as the ring).
	var head_bone_lid: Transform3D = _her._rig.bone_world("head")
	var eye_l_lid: Vector3 = (layer._patches["eye_l"] as MeshInstance3D).global_position
	await _shot("injury_black_eye_swollen_close", eye_l_lid + head_bone_lid.basis.z * 0.28 + head_bone_lid.basis.x * 0.06 + head_bone_lid.basis.y * 0.02, eye_l_lid, 30.0)
	await _shot("injury_black_eye_swollen_side", eye_l_lid + head_bone_lid.basis.x * 0.45 + head_bone_lid.basis.z * 0.12 + head_bone_lid.basis.y * 0.02, eye_l_lid, 35.0)
	# Side check: eye_l on her left, eye_r on her right — same guard as
	# cheek_sides, against the same sign-flip bug (#306).
	_set_injury({"eye_l": {"j_region": 150.0, "j_event": 80.0}, "eye_r": {"j_region": 150.0, "j_event": 80.0}})
	await _step(2)
	var head_bone_eye: Transform3D = _her._rig.bone_world("head")
	var el_lat: float = ((layer._patches["eye_l"] as MeshInstance3D).global_position - head_bone_eye.origin).dot(head_bone_eye.basis.x)
	var er_lat: float = ((layer._patches["eye_r"] as MeshInstance3D).global_position - head_bone_eye.origin).dot(head_bone_eye.basis.x)
	_check(el_lat > 0.02 and er_lat < -0.02, "eye_sides",
		"eye_l lateral %.3f eye_r lateral %.3f" % [el_lat, er_lat])
	# No healing: a smaller later value never lowers the eye.
	_set_injury({"eye_l": {"j_region": 30.0, "j_event": 30.0}, "eye_r": {"j_region": 150.0, "j_event": 80.0}})
	await _step(2)
	_check(layer.black_eye_visible("eye_l") and layer.lid_visible("eye_l"), "eye_no_heal",
		"sev %.0f" % layer.severity("eye_l"))
	# Stills: the bruised left cheek, then the face straight on. The left
	# cheek camera stands on her left (+X): -X would photograph her right
	# cheek under the same name.
	var head: Vector3 = _her.head_world_position()
	await _shot("injury_left_cheek", head + Vector3(0.45, 0.08, 0.55), head, 40.0)
	await _shot("injury_face_front", head + Vector3(0.0, 0.05, 0.7), head, 45.0)
	await _shot("injury_skull_back", head + Vector3(0.0, 0.15, -0.7), head, 45.0)
	print("INJURY %s (%d failed)" % ["PASS" if _fails == 0 else "FAIL", _fails])
	quit(1 if _fails > 0 else 0)
