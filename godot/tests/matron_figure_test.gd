extends SceneTree
## Her figure (life/matron/, matron_rig.gd, matron_look.gd) posed in a studio,
## outside the game: the body loads and every part is dressed; her eyes land
## within EYE_TOLERANCE_DEG of a viewer anywhere her sockets can turn to; her
## feet stand on the floor; her hair hangs from her head and swings when she
## moves. With --out=<dir> (and a display), each pose is also saved as a PNG.
##
##   tools/godot/godot --headless --path godot --script res://tests/matron_figure_test.gd
##   DISPLAY=:0 tools/godot/godot --path godot --resolution 1024x1024 \
##       --script res://tests/matron_figure_test.gd -- --out=<dir>

const MatronView := preload("res://presentation/life/matron_view.gd")
const MatronLook := preload("res://presentation/life/matron_look.gd")
const EYE_TOLERANCE_DEG := 5.0
const FLOOR_TOLERANCE_M := 0.03
const DT := 1.0 / 30.0
const POSES := [
	["guard", {"state": 7, "speed": 0.0, "stride": 0.0}],
	["shuffle", {"state": 5, "speed": 1.0, "stride": 0.6}],
	["search", {"state": 4, "speed": 0.0, "stride": 0.0}],
	["peer", {"state": 8, "speed": 0.0, "stride": 0.0}],
	["pest_hobble", {"state": 3, "speed": 5.0, "stride": 0.5, "stage": 0, "sees_player": true}],
	["hunt_a", {"state": 3, "speed": 6.8, "stride": 0.5, "stage": 1, "sees_player": true}],
	["hunt_b", {"state": 3, "speed": 6.8, "stride": 1.2, "stage": 1}],
	["hunt_still", {"state": 3, "speed": 0.0, "stride": 0.0, "stage": 1}],
	["turn_windup", {"state": 3, "speed": 0.0, "stride": 0.0, "turn": 1, "stage": 2}],
	["turn_lunge", {"state": 3, "speed": 9.0, "stride": 0.3, "turn": 2, "stage": 2}],
	["emerge", {"state": 2, "speed": 0.0, "stride": 0.0}],
	["cling", {"state": 3, "speed": 0.0, "stride": 0.0, "clinging": true}],
	["unrig", {"state": 9, "speed": 0.0, "stride": 0.0}],
	["climb", {"state": 3, "speed": 0.0, "stride": 0.45, "climbing": true}],
	["bar_traverse", {"state": 3, "speed": 0.0, "stride": 0.45, "climbing": true, "climb_move_active": true, "climb_move": "rung_up", "climb_bar_traverse": true}],
	["pole_up", {"state": 3, "speed": 0.0, "stride": 0.45, "climbing": true, "climb_move_active": true, "climb_move": "pole_up", "climb_phase": 2}],
	["pole_slip", {"state": 3, "speed": 0.0, "stride": 0.45, "climbing": true, "climb_move_active": true, "climb_move": "pole_up", "climb_phase": 2, "climb_sliding": true}],
	["hang", {"state": 3, "speed": 0.0, "stride": 0.45, "climbing": true, "climb_move_active": true, "climb_move": "hang_drop", "climb_phase": 1}],
	["hang_drop", {"state": 3, "speed": 0.0, "stride": 0.45, "climb_move_active": true, "climb_move": "hang_drop", "climb_phase": 3, "climb_airborne": true}],
	["gap_runup", {"state": 3, "speed": 0.0, "stride": 0.45, "climb_move_active": true, "climb_move": "gap_jump", "climb_phase": 1}],
	["gap_launch", {"state": 3, "speed": 0.0, "stride": 0.45, "climb_move_active": true, "climb_move": "gap_jump", "climb_phase": 2, "climb_leg_driving": true}],
	["gap_airborne", {"state": 3, "speed": 0.0, "stride": 0.45, "climb_move_active": true, "climb_move": "gap_jump", "climb_phase": 3, "climb_airborne": true}],
	["jump_catch", {"state": 3, "speed": 0.0, "stride": 0.45, "climb_move_active": true, "climb_move": "jump_catch", "climb_phase": 3, "climb_airborne": true}],
	["mantle", {"state": 3, "speed": 0.0, "stride": 0.45, "climbing": true, "climb_move_active": true, "climb_move": "rung_up", "climb_phase": 3}],
	["rung_down", {"state": 3, "speed": 0.0, "stride": 0.45, "climbing": true, "climb_move_active": true, "climb_move": "rung_down", "climb_phase": 2}],
	["pipe_listen", {"state": 3, "speed": 0.0, "stride": 0.0, "pipe_listening": true}],
	["stagger", {"state": 10, "speed": 0.0, "stride": 0.0, "fall": Vector2(0.0, -1.0)}],
	["carry", {"state": 5, "speed": 1.0, "stride": 0.3, "carrying": 1}],
	["grey", {"state": 3, "speed": 0.0, "stride": 0.0, "stage": 2, "sees_player": true, "exposure": 1.0}],
]
const STANDING := ["guard", "shuffle", "search", "peer", "carry"]

var _out := ""
var _cam: Camera3D
var _view: Node3D
var _failures := 0


func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--out="):
			_out = arg.substr(6)
	_run.call_deferred()


func _check(ok: bool, what: String, detail: String) -> void:
	print("MATRON_FIGURE %s %s %s" % ["PASS" if ok else "FAIL", what, detail])
	if not ok:
		_failures += 1


func _studio() -> void:
	var env := WorldEnvironment.new()
	env.environment = Environment.new()
	env.environment.background_mode = Environment.BG_COLOR
	env.environment.background_color = Color(0.09, 0.095, 0.1)
	env.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.environment.ambient_light_color = Color(0.62, 0.64, 0.68)
	env.environment.ambient_light_energy = 0.18
	env.environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
	root.add_child(env)
	var key := DirectionalLight3D.new()
	key.light_energy = 0.9
	key.light_color = Color(1.0, 0.95, 0.88)
	key.shadow_enabled = true
	root.add_child(key)
	key.look_at_from_position(Vector3(-2.5, 3.6, 4.0), Vector3(0.0, 1.0, 0.0))
	var rim := DirectionalLight3D.new()
	rim.light_energy = 0.55
	rim.light_color = Color(0.75, 0.82, 1.0)
	root.add_child(rim)
	rim.look_at_from_position(Vector3(1.5, 3.0, -4.0), Vector3(0.0, 1.2, 0.0))
	var floor_mesh := PlaneMesh.new()
	floor_mesh.size = Vector2(30.0, 30.0)
	var fl := MeshInstance3D.new()
	fl.mesh = floor_mesh
	var fm := StandardMaterial3D.new()
	fm.albedo_color = Color(0.22, 0.21, 0.2)
	fm.roughness = 0.9
	fl.material_override = fm
	root.add_child(fl)
	_cam = Camera3D.new()
	root.add_child(_cam)
	_cam.current = true


func _data(pose: Dictionary, yaw: float) -> Dictionary:
	var d := {"state": 7, "speed": 0.0, "stride": 0.0, "turn": 0, "stage": 1, "sees_player": false,
		"alive": true, "released": true, "position": Vector3(0.0, MatronView.STAND_CENTRE, 0.0), "yaw": yaw,
		"exposure": 0.0, "floor_y": 0.0}
	for k in pose:
		d[k] = pose[k]
	return d


# A few frames of the pose, the state's own clock running and her hair settling.
func _settle(data: Dictionary, frames: int) -> void:
	for i in frames:
		_view._clock += DT
		_view._state_age += DT
		_view._update_figure(data, DT, _cam.global_position, _cam.global_position)
		await process_frame


func _shot(name: String, at: Vector3, look: Vector3, fov: float) -> void:
	_cam.fov = fov
	_cam.look_at_from_position(at, look)
	if _out == "":
		return
	await process_frame
	await RenderingServer.frame_post_draw
	root.get_texture().get_image().save_png(_out.path_join(name + ".png"))


# The authored hair, not the sim: the ribbons are thin strands, not flat slabs.
# The texture carries real strands (several alpha bands across the ribbon, tips
# that fade instead of a blunt cut); the ribbons are narrow where they sit on
# her head. The crown is NOT split: the head bone still owns about a third of
# the mesh (J-067 ruling 2026-11-08 — the split drove strands into her head),
# so the head share is reported, not bounded. What this check asserts is the
# authored shape: strands, taper, narrow crown rows. Hair through her body is
# asserted separately by the scorer (tools/matron_probe/score.py) over the 3
# probe poses (guard, 110 degree twist, head down): at most 1 of 3 frames with
# hair inside her, worst 0.040 m, no head hit — the v5 baseline. Reads the
# seated mesh the figure test already built, so it costs one pass over the hair
# arrays and no extra poses.
func _hair_authored(rig: RefCounted) -> void:
	var bands := 0
	var tapered := false
	# Same read as the look itself (matron_look.gd _image): bytes + decode from
	# buffer, so no "loaded as image file" warning on the res:// path.
	var img := Image.new()
	var _bytes := FileAccess.get_file_as_bytes("res://presentation/life/matron/hair_a.png")
	if _bytes.is_empty() or img.load_png_from_buffer(_bytes) != OK:
		img = null
	if img != null and img.get_width() > 0:
		var w := img.get_width()
		var h := img.get_height()
		var mid := h / 2
		var above := false
		for x in w:
			var a := img.get_pixel(x, mid).a
			if a > 0.45 and not above:
				bands += 1
				above = true
			elif a <= 0.45:
				above = false
		var mid_max := 0.0
		var tip_max := 0.0
		for x in w:
			mid_max = maxf(mid_max, img.get_pixel(x, mid).a)
			tip_max = maxf(tip_max, img.get_pixel(x, h - 1).a)
		tapered = tip_max < mid_max - 0.05
	var head_share := 1.0
	var crown_med := 1.0
	var crown_p90 := 1.0
	var skel: Skeleton3D = rig.skel
	var head_idx := skel.find_bone("head")
	for m in (_view._figure as Node3D).find_children("hair", "MeshInstance3D", true, false):
		var mi := m as MeshInstance3D
		if mi.mesh == null or mi.mesh.get_surface_count() < 1:
			continue
		var arr: Array = mi.mesh.surface_get_arrays(0)
		var verts: PackedVector3Array = arr[Mesh.ARRAY_VERTEX]
		var uvs: PackedVector2Array = arr[Mesh.ARRAY_TEX_UV]
		var bones: PackedInt32Array = arr[Mesh.ARRAY_BONES]
		var tris: PackedInt32Array = arr[Mesh.ARRAY_INDEX]
		var n := verts.size()
		var bpv := int(bones.size() / maxi(n, 1))
		var on_head := 0
		for i in n:
			if int(bones[i * bpv]) == head_idx:
				on_head += 1
		head_share = float(on_head) / float(maxi(n, 1))
		# One row of a ribbon is the two verts a triangle holds at the same v;
		# each row is found exactly once, in the triangle that carries its edge.
		# Crown rows are the ones whose middle sits in the crown band (y > 1.64,
		# the lowest vertex the old seat put on the head bone): that is where
		# the ribbons sat wide and dense enough to read as a slab.
		var seen := {}
		var cw: Array = []
		for t in range(0, tris.size() - 2, 3):
			var a := int(tris[t])
			var b := int(tris[t + 1])
			var c := int(tris[t + 2])
			var va := int(roundf(uvs[a].y * 1000.0))
			var vb := int(roundf(uvs[b].y * 1000.0))
			var vc := int(roundf(uvs[c].y * 1000.0))
			var p := -1
			var q := -1
			if va == vb:
				p = a
				q = b
			elif vb == vc:
				p = b
				q = c
			elif va == vc:
				p = a
				q = c
			if p < 0:
				continue
			var key := "%d_%d" % [mini(p, q), maxi(p, q)]
			if seen.has(key):
				continue
			seen[key] = true
			var mid := ((verts[p] as Vector3) + (verts[q] as Vector3)) * 0.5
			if mid.y > 1.64:
				cw.append((verts[p] as Vector3).distance_to(verts[q]))
		cw.sort()
		if not cw.is_empty():
			crown_med = float(cw[cw.size() / 2])
			crown_p90 = float(cw[int(float(cw.size()) * 0.9)])
		break
	_check(bands >= 3 and tapered and crown_med <= 0.0025 and crown_p90 <= 0.005,
		"hair_authored",
		"strands %d bands, taper %s, head owns %.1f%% (reported, not bounded), crown-row width median %.4f m p90 %.4f m" % [bands, str(tapered), head_share * 100.0, crown_med, crown_p90])


func _run() -> void:
	_studio()
	_view = MatronView.new()
	root.add_child(_view)
	var t0 := Time.get_ticks_msec()
	_view._build_figure()
	_view._build_sparks()
	var rig: RefCounted = _view._rig
	_check(rig != null, "loads", "the body, its rig and its look in %d ms" % (Time.get_ticks_msec() - t0))
	if rig == null:
		quit(1)
		return
	var meshes: Array = (_view._figure as Node3D).find_children("*", "MeshInstance3D", true, false)
	var undressed := []
	for m in meshes:
		if (m as MeshInstance3D).material_override == null:
			undressed.append(String(m.name))
	_check(meshes.size() >= 18 and undressed.is_empty(), "dressed", "%d parts, undressed %s" % [meshes.size(), str(undressed)])
	# Exposure contrast: skin stays light (waxy) while the uniform darkens, and
	# their luminance separates more as exposure rises.
	var gaps := []
	for e in [0.0, 0.5, 1.0]:
		var t: Dictionary = MatronLook.exposure_tints(e)
		gaps.append((t["skin"] as Color).get_luminance() - (t["cloth"] as Color).get_luminance())
	var grey_skin: float = (MatronLook.exposure_tints(1.0)["skin"] as Color).get_luminance()
	_check(grey_skin > 0.85 and gaps[2] > 0.5 and gaps[1] > gaps[0] - 0.001 and gaps[2] > gaps[1],
		"contrast", "skin %.2f, skin-cloth gap %s" % [grey_skin, str(gaps)])
	if _out != "":
		DirAccess.make_dir_recursive_absolute(_out)
	# Every pose: the body follows the pivots; standing, her soles on the floor.
	_cam.global_position = Vector3(0.0, 1.5, 3.2)
	var pose_fingerprints := {}
	for entry in POSES:
		var data := _data(entry[1], 0.0)
		await _settle(data, 24)
		pose_fingerprints[String(entry[0])] = Vector3(
			(_view._p["torso"] as Node3D).rotation.x,
			(_view._p["sh_l"] as Node3D).rotation.x,
			(_view._p["hp_l"] as Node3D).rotation.x)
		if entry[0] in STANDING:
			var foot := INF
			for s in ["l", "r"]:
				for key in ["heel", "toe"]:
					foot = minf(foot, rig.point("an_" + s, key).y)
			_check(absf(foot) < FLOOR_TOLERANCE_M, "feet_" + String(entry[0]), "lowest sole %.3f m" % foot)
		_view._figure.rotation.y = -0.45
		await _settle(data, 2)
		await _shot("pose_" + String(entry[0]), Vector3(0.0, 1.1, 4.4), Vector3(0.0, 0.9, 0.0), 32.0)
	var ladder_pose: Vector3 = pose_fingerprints["climb"]
	for name in ["bar_traverse", "pole_up", "hang_drop", "gap_launch", "gap_airborne", "jump_catch", "mantle", "rung_down"]:
		var pose: Vector3 = pose_fingerprints[name]
		var delta_angle := pose.distance_to(ladder_pose)
		_check(delta_angle > 0.2, "pose_distinct_" + name, "joint-angle delta %.2f rad" % delta_angle)
	var pole_up_pose: Vector3 = pose_fingerprints["pole_up"]
	var pole_slip_pose: Vector3 = pose_fingerprints["pole_slip"]
	_check(pole_up_pose.distance_to(pole_slip_pose) > 0.2, "pose_distinct_pole_slip", "sliding grip differs from a secure pole grip")
	var hang_pose: Vector3 = pose_fingerprints["hang"]
	var hang_drop_pose: Vector3 = pose_fingerprints["hang_drop"]
	_check(hang_pose.distance_to(hang_drop_pose) > 0.2, "pose_distinct_hang_drop", "release differs from hanging")
	var gap_launch_pose: Vector3 = pose_fingerprints["gap_launch"]
	var gap_airborne_pose: Vector3 = pose_fingerprints["gap_airborne"]
	_check(gap_launch_pose.distance_to(gap_airborne_pose) > 0.2, "pose_distinct_gap_flight", "leg drive differs from ballistic flight")
	# Her eyes: stopped on the hunt, her head half turned to whoever she has
	# found; a viewer anywhere her sockets can turn to is met by both eyes.
	var data := _data({"state": 3, "stage": 1, "sees_player": true}, 0.0)
	await _settle(data, 10)
	var head: Vector3 = _view.head_world_position()
	var worst := 0.0
	var met := 0
	for yaw_deg in [-40.0, -20.0, 0.0, 20.0, 40.0]:
		for pitch_deg in [-15.0, 0.0, 15.0]:
			var dir := Basis.from_euler(Vector3(deg_to_rad(-pitch_deg), deg_to_rad(yaw_deg), 0.0)) * Vector3(0.0, 0.0, 1.0)
			var viewer := head + dir * 1.6
			for k in 3:
				_view._clock += DT
				_view._update_figure(data, DT, viewer, viewer)
			var head_basis: Basis = rig.bone_world("head").basis
			for s in ["l", "r"]:
				var to: Vector3 = viewer - rig.eye_position(s)
				var d: Vector3 = (head_basis.inverse() * to).normalized()
				var reach_yaw := absf(atan2(d.x, d.z))
				var reach_pitch := absf(asin(clampf(d.y, -1.0, 1.0)))
				if reach_yaw > rig.EYE_YAW_MAX - 0.02 or reach_pitch > rig.EYE_PITCH_MAX - 0.02:
					continue   # past where an eye can turn: the head must turn too
				var off := rad_to_deg((rig.eye_forward(s) as Vector3).angle_to(to))
				worst = maxf(worst, off)
				met += 1
	_check(met >= 12 and worst <= EYE_TOLERANCE_DEG, "eyes_on_you", "worst %.2f deg over %d eye-viewer pairs in reach" % [worst, met])
	# Her hair: hanging from her head at rest, swinging when she turns.
	data = _data({"state": 7}, 0.0)
	await _settle(data, 60)
	var chains: Array = rig._chains
	var hang := 0.0
	for ch in chains:
		hang += float((ch["pts"][0] as Vector3).y - (ch["pts"][ch["pts"].size() - 1] as Vector3).y)
	hang /= maxf(1.0, float(chains.size()))
	var before: Array = []
	for ch in chains:
		before.append(ch["pts"][ch["pts"].size() - 1])
	data["yaw"] = 1.2
	await _settle(data, 6)
	var swing := 0.0
	for c in chains.size():
		swing = maxf(swing, (chains[c]["pts"][chains[c]["pts"].size() - 1] as Vector3).distance_to(before[c]))
	_check(hang > 0.3 and swing > 0.05, "hair", "tips %.2f m under the roots, %.2f m swing on a turn" % [hang, swing])
	_hair_authored(rig)
	# Close shots: face front, three-quarter, her hands.
	data = _data({"state": 7, "sees_player": true}, 0.0)
	await _settle(data, 40)
	head = _view.head_world_position()
	await _shot("face_front", head + Vector3(0.0, 0.0, 0.75), head, 30.0)
	await _settle(data, 2)
	await _shot("face_three_quarter", head + Vector3(0.5, 0.05, 0.6), head, 30.0)
	var hand: Vector3 = rig.bone_world("wr_r").origin
	await _shot("hand", hand + Vector3(0.25, 0.1, 0.4), hand + Vector3(0.0, -0.08, 0.0), 30.0)
	data = _data({"state": 3, "speed": 5.0, "stride": 0.5, "stage": 0, "sees_player": true}, 0.0)
	await _settle(data, 20)
	head = _view.head_world_position()
	var face_dir: Vector3 = rig.bone_world("head").basis.z
	await _shot("mouth_open", head + face_dir * 0.45 + Vector3(0.0, -0.05, 0.0), head + Vector3(0.0, -0.06, 0.0), 30.0)
	data = _data({"state": 3, "speed": 6.8, "stride": 0.5, "stage": 1, "sees_player": true}, 0.0)
	await _settle(data, 30)
	await _shot("crawl_face", head + Vector3(0.0, -0.6, 1.2), _view.head_world_position(), 40.0)
	quit(1 if _failures else 0)
