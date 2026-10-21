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
	for entry in POSES:
		var data := _data(entry[1], 0.0)
		await _settle(data, 24)
		if entry[0] in STANDING:
			var foot := INF
			for s in ["l", "r"]:
				for key in ["heel", "toe"]:
					foot = minf(foot, rig.point("an_" + s, key).y)
			_check(absf(foot) < FLOOR_TOLERANCE_M, "feet_" + String(entry[0]), "lowest sole %.3f m" % foot)
		_view._figure.rotation.y = -0.45
		await _settle(data, 2)
		await _shot("pose_" + String(entry[0]), Vector3(0.0, 1.1, 4.4), Vector3(0.0, 0.9, 0.0), 32.0)
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
