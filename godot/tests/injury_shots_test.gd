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
	# #309 side fix: the ring fades out at grazing angles (N.V) so no rim
	# stands off the eye from the side. Guard: the ring's material is the
	# fade shader, not a flat StandardMaterial3D.
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
	# Side view for depth: the ring fades out at grazing angles (#309), so
	# no rim stands off the eye; it must not float or sink.
	var eye_side: Vector3 = eye_l_pos + head_bone_close.basis.x * 0.45 + head_bone_close.basis.z * 0.12 + head_bone_close.basis.y * 0.02
	await _shot("injury_black_eye_side", eye_side, eye_l_pos, 35.0)
	# At 150: the lid swells. At 250: the eye is narrowed to a slit.
	_set_injury({"eye_l": {"j_region": 150.0, "j_event": 80.0}})
	await _step(2)
	_check(layer.lid_visible("eye_l") and not layer.eye_slit_visible("eye_l"), "eye_swell", "lid %s slit %s" % [layer.lid_visible("eye_l"), layer.eye_slit_visible("eye_l")])
	_set_injury({"eye_l": {"j_region": 250.0, "j_event": 80.0}})
	await _step(2)
	_check(layer.lid_visible("eye_l") and layer.eye_slit_visible("eye_l"), "eye_slit", "lid %s slit %s droop %.2f" % [layer.lid_visible("eye_l"), layer.eye_slit_visible("eye_l"), layer.lid_droop("eye_l")])
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
