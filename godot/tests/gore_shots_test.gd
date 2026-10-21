extends SceneTree
## The gore in a studio deck, driven by hold-state dictionaries shaped exactly
## like the native's get_matron_hold_state() at each moment of the tears and
## the fatality. Checks the gore answered each edge; with --out=<dir> and a
## display saves each moment as a PNG.
##   xvfb-run -a godot --path godot --fixed-fps 30 --resolution 960x720 \
##       --script res://tests/gore_shots_test.gd -- --out=/workspace/gore_shots

const GoreView := preload("res://presentation/life/gore/gore_view.gd")
const MatronView := preload("res://presentation/life/matron_view.gd")
const DT := 1.0 / 30.0

var _out := ""
var _cam: Camera3D
var _gore: Node3D
var _her: Node3D
var _hold := {}
var _fails := 0
var _her_pose := {"state": 7, "speed": 0.0, "stride": 0.0, "turn": 0, "stage": 1, "sees_player": true,
	"alive": true, "released": true, "position": Vector3(1.5, MatronView.STAND_CENTRE, -1.0), "yaw": -2.4,
	"exposure": 0.6, "floor_y": 0.0}


func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--out="):
			_out = arg.substr(6)
	_run.call_deferred()


func _check(ok: bool, what: String, detail: String) -> void:
	print("GORE %s %s %s" % ["PASS" if ok else "FAIL", what, detail])
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
	# Deck plates and a wall, steel.
	var steel := StandardMaterial3D.new()
	steel.albedo_color = Color(0.32, 0.31, 0.3)
	steel.metallic = 0.55
	steel.roughness = 0.5
	var deck := MeshInstance3D.new()
	var pm := PlaneMesh.new()
	pm.size = Vector2(16, 16)
	deck.mesh = pm
	deck.material_override = steel
	root.add_child(deck)
	var wall := MeshInstance3D.new()
	var bm := BoxMesh.new()
	bm.size = Vector3(16, 6, 0.3)
	wall.mesh = bm
	wall.material_override = steel
	wall.position = Vector3(0, 3, -2.2)
	root.add_child(wall)
	_cam = Camera3D.new()
	root.add_child(_cam)
	_cam.current = true


func _base_hold() -> Dictionary:
	var edges := PackedInt64Array()
	edges.resize(10)
	return {"held": true, "free_rotation": true, "claimed": false, "fatality_beat": -1,
		"fatality_beat_edges": edges, "player_position": Vector3(0, 0.18, 0),
		"body_rotation": Quaternion(Vector3.RIGHT, PI * 0.5), "left_arm_lost": false, "right_arm_lost": false,
		"left_leg_lost": false, "right_leg_lost": false, "wrist_side": 1, "wrist_strain_ns": 0.0,
		"wrist_tear_ns": 600.0, "leg_strain_ns": Vector2.ZERO, "leg_tear_ns": 1200.0, "leg_load_n": 0.0,
		"arm_tear_count": 0, "leg_tear_count": 0, "last_severed_side": -1, "severed_arm_present": false,
		"severed_legs": [{"present": false}, {"present": false}], "knee_pin_count": 0, "knee_down": false,
		"fall_landing_count": 0, "fall_landing_speed_mps": 24.0, "matron_hand": Vector3(0.6, 1.0, 0.3),
		"matron_hand2": Vector3(0.9, 1.0, 0.1)}


func _step(frames: int) -> void:
	for i in frames:
		_gore.update_view(_hold, {}, DT, _cam, 0.0)
		if _her != null:
			_her._clock += DT
			_her._state_age += DT
			_her._update_figure(_her_pose, DT, _cam.global_position, _cam.global_position)
		await process_frame


func _shot(name: String, at: Vector3, look: Vector3, fov: float = 50.0) -> void:
	_cam.fov = fov
	_cam.look_at_from_position(at, look)
	_gore.update_view(_hold, {}, 0.0, _cam, 0.0)
	if _out == "":
		await process_frame
		return
	await process_frame
	await RenderingServer.frame_post_draw
	root.get_texture().get_image().save_png(_out.path_join(name + ".png"))
	print("GORE SHOT ", _out.path_join(name + ".png"))


func _edge(beat: int) -> void:
	var e: PackedInt64Array = (_hold["fatality_beat_edges"] as PackedInt64Array).duplicate()
	e[beat] += 1
	_hold["fatality_beat_edges"] = e
	_hold["fatality_beat"] = beat


func _run() -> void:
	_studio()
	_gore = GoreView.new()
	root.add_child(_gore)
	_her = MatronView.new()
	root.add_child(_her)
	_her._build_figure()
	_her._build_sparks()
	_gore.matron_view = _her
	_gore.lens_on = false
	_hold = _base_hold()
	await _step(2)
	# The victim is drawn only while claimed; for the arm tear shots show it
	# (in game the arm tear is first person: the FP stump; here from outside).
	_hold["claimed"] = true
	_hold["fatality_beat"] = 0
	await _step(1)
	var shoulder: Vector3 = _gore.victim.stumps[1].global_position
	# --- the arm: strain climbing, the split, the tear ---
	for s in [0.35, 0.7, 0.95]:
		_hold["wrist_strain_ns"] = 600.0 * s
		await _step(3)
		var out: Vector3 = (shoulder - _gore.victim.global_position)
		out.y = 0.0
		out = out.normalized()
		await _shot("arm_1_strain_%02d" % int(s * 100), shoulder + out * 0.25 + Vector3(0, 0.35, 0), shoulder, 38.0)
	_hold["wrist_strain_ns"] = 0.0
	_hold["right_arm_lost"] = true
	_hold["arm_tear_count"] = 1
	_hold["last_severed_side"] = 1
	_hold["severed_arm_present"] = true
	_hold["severed_arm_position"] = Vector3(0.9, 0.06, 0.5)
	_hold["severed_arm_rotation"] = Quaternion(Vector3.FORWARD, PI * 0.5) * Quaternion(Vector3.UP, 0.4)
	await _step(4)
	await _shot("arm_2_tear_moment", Vector3(1.4, 1.0, 1.5), Vector3(0.3, 0.2, 0.0), 55.0)
	await _step(25)
	await _shot("arm_3_tear_1s", Vector3(1.4, 1.0, 1.5), Vector3(0.4, 0.15, 0.1), 55.0)
	shoulder = _gore.victim.stumps[1].global_position
	var out2: Vector3 = (shoulder - _gore.victim.global_position)
	out2.y = 0.0
	out2 = out2.normalized()
	await _shot("arm_4_stump_close", shoulder + out2 * 0.32 + Vector3(0, 0.25, 0), shoulder, 38.0)
	var arm_root: Vector3 = _gore.arm_piece.root_transform().origin
	await _shot("arm_5_severed_close", arm_root + Vector3(0.3, 0.3, 0.25), arm_root + Vector3(-0.1, -0.05, 0.0), 40.0)
	_check(_gore.tears_shown == 1 and _gore.fx.pulses > 0 and _gore.fx.splats_laid > 0, "arm_tear",
		"tears %d pulses %d splats %d" % [_gore.tears_shown, _gore.fx.pulses, _gore.fx.splats_laid])
	# --- the fatality ---
	_edge(1)
	_hold["knee_pin_count"] = 1
	_hold["knee_down"] = true
	await _step(14)
	await _shot("fatal_1_pin_crush", _gore.victim.head_position() + Vector3(0.5, 0.55, -0.35), _gore.victim.head_position() + Vector3(0, -0.05, 0.3), 45.0)
	_check(_gore.crunches == 1 and _gore.victim.ribs[0].visible, "pin", "crunches %d" % _gore.crunches)
	_edge(2)
	var hip: Vector3 = _gore.victim.stumps[2].global_position
	for s in [0.5, 0.9]:
		_hold["leg_strain_ns"] = Vector2(1200.0 * s, 0.0)
		await _step(3)
		await _shot("fatal_2_leg0_strain_%02d" % int(s * 100), hip + Vector3(-0.5, 0.45, 0.5), hip, 40.0)
	_hold["leg_strain_ns"] = Vector2.ZERO
	_hold["left_leg_lost"] = true
	_hold["leg_tear_count"] = 1
	_hold["severed_legs"] = [{"present": true, "position": Vector3(-0.9, 0.08, 1.1),
		"rotation": Quaternion(Vector3.FORWARD, PI * 0.5) * Quaternion(Vector3.UP, -0.3)}, {"present": false}]
	await _step(5)
	await _shot("fatal_2_leg0_tear", Vector3(-1.2, 1.1, 2.0), Vector3(-0.3, 0.15, 0.5), 55.0)
	_edge(3)
	_hold["right_leg_lost"] = true
	_hold["leg_tear_count"] = 2
	var legs: Array = _hold["severed_legs"]
	legs[1] = {"present": true, "position": Vector3(1.1, 0.08, 1.3),
		"rotation": Quaternion(Vector3.FORWARD, -PI * 0.5) * Quaternion(Vector3.UP, 0.5)}
	await _step(20)
	await _shot("fatal_3_leg1_tear", Vector3(0.2, 1.4, 2.2), Vector3(0.0, 0.1, 0.4), 55.0)
	var hips: Vector3 = (_gore.victim.stumps[2].global_position + _gore.victim.stumps[3].global_position) * 0.5
	var away: Vector3 = (hips - _gore.victim.head_position()).normalized()
	await _shot("fatal_3_hip_stumps_close", hips + away * 0.6 + Vector3(0, 0.45, 0), hips, 42.0)
	_check(_gore.tears_shown == 3, "legs", "tears %d" % _gore.tears_shown)
	# Drag: pulled across the plates by the collar, a smear behind.
	_edge(4)
	_hold["knee_down"] = false
	for i in 40:
		_hold["player_position"] = Vector3(-0.03 * i, 0.18, -0.02 * i)
		_her_pose["position"] = Vector3(-0.03 * i - 0.8, MatronView.STAND_CENTRE, -0.02 * i - 0.6)
		await _step(1)
	await _shot("fatal_4_drag_smear", Vector3(0.4, 2.0, 1.8), Vector3(-0.6, 0.0, -0.4), 55.0)
	_check(_gore.smears > 5, "drag", "smears %d" % _gore.smears)
	# Haul: up on the rope, bleeding off the stumps.
	_edge(5)
	_edge(6)
	_hold["body_rotation"] = Quaternion.IDENTITY
	_hold["player_position"] = Vector3(-1.2, 2.2, -0.8)
	var rope := MeshInstance3D.new()
	var cm := CylinderMesh.new()
	cm.top_radius = 0.015
	cm.bottom_radius = 0.015
	cm.height = 4.0
	rope.mesh = cm
	rope.position = Vector3(-1.2, 4.6, -0.8)
	root.add_child(rope)
	for i in 30:
		_hold["player_position"] = Vector3(-1.2, 2.2 + 0.03 * i, -0.8)
		await _step(1)
	await _shot("fatal_6_haul_bleeding", Vector3(0.6, 1.3, 1.6), Vector3(-1.2, 2.4, -0.8), 55.0)
	# Fall: down onto the plates, the ruin.
	_edge(7)
	rope.queue_free()
	_hold["player_position"] = Vector3(-1.0, 0.16, 0.2)
	_hold["body_rotation"] = Quaternion(Vector3.RIGHT, PI * 0.5) * Quaternion(Vector3.FORWARD, 0.7)
	_hold["fall_landing_count"] = 1
	await _step(4)
	await _shot("fatal_7_fall_impact", Vector3(0.4, 1.8, 2.2), Vector3(-1.0, 0.1, 0.2), 55.0)
	await _step(30)
	await _shot("fatal_7_fall_ruin", Vector3(-0.2, 1.4, 1.5), Vector3(-1.0, 0.1, 0.2), 50.0)
	_check(_gore.ruins == 1, "fall", "ruins %d" % _gore.ruins)
	# Wipe: her hands down her apron.
	_edge(8)
	_her_pose["position"] = Vector3(0.6, MatronView.STAND_CENTRE, -0.6)
	_her_pose["yaw"] = 0.4
	_hold["matron_hand"] = Vector3(0.45, 0.95, -0.4)
	_hold["matron_hand2"] = Vector3(0.75, 0.95, -0.4)
	await _step(60)
	# Her hands as her own skeleton has them (in game: the sim's hand points).
	for sk in _her.find_children("*", "Skeleton3D", true, false):
		var skl := sk as Skeleton3D
		for pair in [["wr_l", "matron_hand"], ["wr_r", "matron_hand2"]]:
			var bi := skl.find_bone(pair[0])
			if bi >= 0:
				_hold[pair[1]] = skl.global_transform * skl.get_bone_global_pose(bi).origin + Vector3(0, -0.07, 0)
	await _step(2)
	await _shot("fatal_8_wipe_apron", Vector3(1.3, 1.3, 1.7), Vector3(0.6, 1.0, -0.6), 45.0)
	# The lens: first person on a tear, and the red-out.
	_gore.lens_on = true
	_hold["arm_tear_count"] = 1
	_gore._lens_blood = 0.9
	_gore._lens_run = 0.4
	_gore._vignette = 0.9
	await _shot("lens_1_tear_blood", Vector3(0.1, 0.5, 0.6), Vector3(0.6, 0.6, -0.6), 70.0)
	_gore._lens_blood = 0.3
	_gore._redout = 0.62
	await _shot("lens_2_claimed_redout", Vector3(0.1, 0.5, 0.6), Vector3(0.6, 0.9, -0.6), 70.0)
	print("GORE %s (%d failed)" % ["PASS" if _fails == 0 else "FAIL", _fails])
	quit(1 if _fails > 0 else 0)
