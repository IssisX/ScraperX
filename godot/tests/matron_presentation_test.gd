extends SceneTree
## Her climbing moves, seen: one frame per move.
## Modeled on matron_figure_test.gd, but focused on the climb poses.
## Each move is posed through MatronView._pose with the exact keys the
## bridge sends (climb_move, climb_phase, climb_airborne, climb_hand,
## floor_y, support_n), plus the climb_move_* aliases. A frame record is
## written for every move: with --out=<dir> it lands in that dir as
## matron_presentation_frames.json; without --out it lands in
## artifacts/matron_presentation_frames.json under the godot dir.
## PNG shots need a display (see matron_figure_test.gd); headless runs
## still prove the poses differ and the ladder pose is untouched.
##
##   tools/godot/godot --headless --path godot \
##       --script res://tests/matron_presentation_test.gd
##   tools/godot/godot --headless --path godot \
##       --script res://tests/matron_presentation_test.gd -- --out=<dir>

const MatronView := preload("res://presentation/life/matron_view.gd")
const DT := 1.0 / 30.0
const MOVES := [
	["ladder", {"climbing": true}],
	["rung_up", {"climbing": true, "climb_move_active": true, "climb_move": "rung_up", "climb_phase": 1}],
	["rung_down_phase0", {"climbing": true, "climb_move_active": true, "climb_move": "rung_down", "climb_phase": 0}],
	["rung_down_phase2", {"climbing": true, "climb_move_active": true, "climb_move": "rung_down", "climb_phase": 2}],
	["mantle", {"climbing": true, "climb_move_active": true, "climb_move": "mantle", "climb_phase": 2}],
	["step_down_grounded", {"climbing": true, "climb_move_active": true, "climb_move": "step_down", "climb_phase": 1}],
	["step_down_airborne", {"climbing": true, "climb_move_active": true, "climb_move": "step_down", "climb_phase": 2, "climb_airborne": true}],
	["pole_up", {"climbing": true, "climb_move_active": true, "climb_move": "pole_up", "climb_phase": 2}],
	["pole_down", {"climbing": true, "climb_move_active": true, "climb_move": "pole_down", "climb_phase": 2}],
	["jump_catch_windup", {"climbing": true, "climb_move_active": true, "climb_move": "jump_catch", "climb_phase": 1}],
	["jump_catch_airborne", {"climbing": true, "climb_move_active": true, "climb_move": "jump_catch", "climb_phase": 3, "climb_airborne": true}],
	["hang_drop_phase0", {"climbing": true, "climb_move_active": true, "climb_move": "hang_drop", "climb_phase": 0}],
	["hang_drop_airborne", {"climbing": true, "climb_move_active": true, "climb_move": "hang_drop", "climb_phase": 3, "climb_airborne": true}],
	["gap_jump_drive", {"climbing": true, "climb_move_active": true, "climb_move": "gap_jump", "climb_phase": 1, "climb_leg_driving": true}],
	["gap_jump_airborne", {"climbing": true, "climb_move_active": true, "climb_move": "gap_jump", "climb_phase": 3, "climb_airborne": true}],
]
# Moves whose fingerprint must differ from the ladder baseline by > 0.2 rad.
const DISTINCT := ["rung_down_phase0", "rung_down_phase2", "mantle", "step_down_grounded", "step_down_airborne", "pole_up", "pole_down", "jump_catch_windup", "jump_catch_airborne", "hang_drop_phase0", "hang_drop_airborne", "gap_jump_drive", "gap_jump_airborne"]

var _out := ""
var _view: Node3D
var _cam: Camera3D
var _failures := 0


func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--out="):
			_out = arg.substr(6)
	_run.call_deferred()


func _check(ok: bool, what: String, detail: String) -> void:
	print("MATRON_PRESENTATION %s %s %s" % ["PASS" if ok else "FAIL", what, detail])
	if not ok:
		_failures += 1


func _data(pose: Dictionary) -> Dictionary:
	var d := {"state": 3, "speed": 0.0, "stride": 0.45, "turn": 0, "stage": 1, "sees_player": false,
		"alive": true, "released": true, "position": Vector3(0.0, MatronView.STAND_CENTRE, 0.0), "yaw": 0.0,
		"exposure": 0.0, "floor_y": 0.0, "support_n": 0.0, "climb_hand": Vector3(0.3, 2.0, 0.0)}
	for k in pose:
		d[k] = pose[k]
	return d


func _settle(data: Dictionary, frames: int) -> void:
	for i in frames:
		_view._clock += DT
		_view._state_age += DT
		_view._update_figure(data, DT, Vector3(0.0, 1.5, 3.2), Vector3(0.0, 1.5, 3.2))
		await process_frame


func _studio() -> void:
	# Only used when a display is present: headless runs skip PNGs.
	var env := WorldEnvironment.new()
	env.environment = Environment.new()
	env.environment.background_mode = Environment.BG_COLOR
	env.environment.background_color = Color(0.09, 0.095, 0.1)
	env.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
	env.environment.ambient_light_color = Color(0.62, 0.64, 0.68)
	env.environment.ambient_light_energy = 0.18
	root.add_child(env)
	var key := DirectionalLight3D.new()
	key.light_energy = 0.9
	key.light_color = Color(1.0, 0.95, 0.88)
	root.add_child(key)
	key.look_at_from_position(Vector3(-2.5, 3.6, 4.0), Vector3(0.0, 1.0, 0.0))
	var fl := MeshInstance3D.new()
	fl.mesh = PlaneMesh.new()
	(fl.mesh as PlaneMesh).size = Vector2(30.0, 30.0)
	var fm := StandardMaterial3D.new()
	fm.albedo_color = Color(0.22, 0.21, 0.2)
	fl.material_override = fm
	root.add_child(fl)
	_cam = Camera3D.new()
	root.add_child(_cam)
	_cam.current = true
	_cam.global_position = Vector3(0.0, 1.5, 3.2)


func _shot(name: String) -> bool:
	if _cam == null:
		return false
	_cam.fov = 32.0
	_cam.look_at_from_position(Vector3(0.0, 1.1, 4.4), Vector3(0.0, 0.9, 0.0))
	await process_frame
	await RenderingServer.frame_post_draw
	var err := root.get_texture().get_image().save_png(_out.path_join(name + ".png"))
	return err == OK and FileAccess.file_exists(_out.path_join(name + ".png"))


func _fingerprint() -> Vector3:
	return Vector3(
		(_view._p["torso"] as Node3D).rotation.x,
		(_view._p["sh_l"] as Node3D).rotation.x,
		(_view._p["hp_l"] as Node3D).rotation.x)


func _run() -> void:
	# Bridge keys: only when the native class is present. Headless runs
	# without a built .so still prove the poses below; the bridge part is
	# then NOT_RUN, not a pass claimed without a binary.
	var native = ClassDB.instantiate("ScraperXSimulation")
	if native != null:
		native.advance_frame(1.0 / 90.0)
		var state: Dictionary = native.get_matron_state()
		for key in ["climb_move", "climb_phase", "climb_airborne", "climb_move_phase", "climb_move_airborne", "climb_hand", "climb_hand2", "climb_hand2_known", "climb_feet_support", "climb_feet_support_known", "climb_feet_floor_y", "climb_support_n", "support_n", "floor_y"]:
			_check(state.has(key), "bridge_" + key, "present" if state.has(key) else "missing")
		_check(state.get("climb_move_phase") == state.get("climb_phase") and state.get("climb_move_airborne") == state.get("climb_airborne"), "bridge_alias", "climb_move_* aliases match canonical keys")
	else:
		print("MATRON_PRESENTATION NOT_RUN bridge_keys (no native class in this run)")
	_view = MatronView.new()
	root.add_child(_view)
	if _out != "" and OS.get_environment("DISPLAY") != "":
		_studio()
	_view._build_figure()
	_view._build_sparks()
	var rig: RefCounted = _view._rig
	_check(rig != null, "loads", "the body and its rig load")
	if rig == null:
		quit(1)
		return
	var frames := {}
	var fingerprints := {}
	for entry in MOVES:
		var data := _data(entry[1])
		await _settle(data, 24)
		var fp := _fingerprint()
		fingerprints[String(entry[0])] = fp
		frames[String(entry[0])] = {"torso_pitch": fp.x, "shoulder_l": fp.y, "hip_l": fp.z,
			"climb_move": String(data.get("climb_move", "")), "climb_phase": int(data.get("climb_phase", 0)),
			"climb_airborne": bool(data.get("climb_airborne", false)), "floor_y": float(data.get("floor_y", 0.0)),
			"support_n": float(data.get("support_n", 0.0))}
		_check(true, "frame_" + String(entry[0]), "torso %.2f shoulder %.2f hip %.2f" % [fp.x, fp.y, fp.z])
		if _cam != null:
			_check(await _shot("pose_" + String(entry[0])), "png_" + String(entry[0]), _out.path_join("pose_" + String(entry[0]) + ".png"))
			# A distinct-hold frame for every two-handed move: the same
			# synthetic second hold the mantle assertions measure, so
			# the frame shows what the rig does when the bridge reports
			# a validated second hold (not what native play shows for
			# moves whose second hold stays unvalidated).
			if String(data.get("climb_move", "")) in ["mantle", "jump_catch", "pole_up", "pole_down", "hang_drop"]:
				var base: Vector3 = (rig._gpos("sh_l") + rig._gpos("sh_r")) * 0.5 + Vector3(0.0, 0.1, 0.35)
				var wbase: Vector3 = rig.skel.global_transform * base
				var wleft: Vector3 = rig.skel.global_transform * (base + Vector3(-0.45, 0.0, 0.0))
				rig.contact({"state": 3, "climbing": true, "climb_hand": wbase, "climb_hand2": wleft,
					"climb_hand2_known": true, "climb_move_active": true,
					"climb_move": String(data.get("climb_move", ""))}, false)
				_check(await _shot("distinct_" + String(entry[0])), "png_distinct_" + String(entry[0]), _out.path_join("distinct_" + String(entry[0]) + ".png"))
		else:
			print("MATRON_PRESENTATION NOT_RUN png_" + String(entry[0]) + " (no display)")
			if String(data.get("climb_move", "")) in ["mantle", "jump_catch", "pole_up", "pole_down", "hang_drop"]:
				print("MATRON_PRESENTATION NOT_RUN png_distinct_" + String(entry[0]) + " (no display)")
	var ladder: Vector3 = fingerprints["ladder"]
	for name in DISTINCT:
		var delta_angle := (fingerprints[name] as Vector3).distance_to(ladder)
		_check(delta_angle > 0.2, "distinct_" + name, "joint-angle delta %.2f rad from ladder" % delta_angle)
	# rung_up is the ladder move: it must NOT change the ladder pose.
	var rung_up_delta := (fingerprints["rung_up"] as Vector3).distance_to(ladder)
	_check(rung_up_delta < 0.2, "ladder_unchanged_rung_up", "rung_up delta %.2f rad" % rung_up_delta)
	# No climb_move key at all: ladder pose untouched.
	var no_key := _data({"climbing": true})
	no_key.erase("climb_move")
	await _settle(no_key, 24)
	var no_key_delta := _fingerprint().distance_to(ladder)
	_check(no_key_delta < 0.2, "ladder_unchanged_no_key", "no-key delta %.2f rad" % no_key_delta)
	# Both hands to the one climb hold on a two-handed move (mantle).
	# The hold must be reachable by BOTH shoulders: a point in front of
	# the chest, not next to one wrist (the far shoulder is ~0.75 m from
	# a point beside the near wrist, past its ~0.51 m arm, so that target
	# is physically impossible for the far hand and is not a rig failure).
	var data := _data({"climbing": true, "climb_move_active": true, "climb_move": "mantle", "climb_phase": 2})
	await _settle(data, 24)
	var target: Vector3 = (rig._gpos("sh_l") + rig._gpos("sh_r")) * 0.5 + Vector3(0.0, 0.1, 0.35)
	var world_target: Vector3 = rig.skel.global_transform * target
	var contact_data := {"state": 3, "climbing": true, "climb_hand": world_target, "climb_move_active": true, "climb_move": "mantle"}
	rig.contact(contact_data, false)
	var dr: float = rig._gpos("wr_r").distance_to(target)
	var dl: float = rig._gpos("wr_l").distance_to(target)
	_check(dr < 0.05, "hand_right_mantle", "right wrist %.3f m from hold" % dr)
	_check(dl < 0.05, "hand_left_mantle", "left wrist %.3f m from same hold" % dl)
	# Distinct second hold + real feet support on a two-handed mantle.
	# The left hold is one shoulder-width (0.45 m) to her left of the
	# right hold, in skeleton space (her left is local -x, the side
	# sh_l sits on); the feet support is 0.2 m above the lowest foot,
	# inside the rig's lift clamp, so the lift-to-support mechanism is
	# what is measured, not a support already at the feet.
	var left_target: Vector3 = target + Vector3(-0.45, 0.0, 0.0)
	var world_left: Vector3 = rig.skel.global_transform * left_target
	var foot_before: float = rig._lowest_foot()
	var support_skel := Vector3(0.0, foot_before + 0.2, 0.0)
	var world_support: Vector3 = rig.skel.global_transform * support_skel
	var distinct_data := {"state": 3, "climbing": true, "climb_hand": world_target, "climb_hand2": world_left,
		"climb_hand2_known": true, "climb_feet_support": world_support, "climb_feet_support_known": true,
		"climb_move_active": true, "climb_move": "mantle"}
	rig.contact(distinct_data, false)
	var dr2: float = rig._gpos("wr_r").distance_to(target)
	var dl2: float = rig._gpos("wr_l").distance_to(left_target)
	var foot_after: float = rig._lowest_foot()
	_check(left_target.distance_to(target) > 0.3, "holds_distinct_mantle", "holds %.2f m apart" % left_target.distance_to(target))
	_check(dr2 < 0.05, "hand_right_own_hold", "right wrist %.3f m from its own hold" % dr2)
	_check(dl2 < 0.05, "hand_left_own_hold", "left wrist %.3f m from its own hold" % dl2)
	_check(absf(foot_after - (foot_before + 0.2)) < 0.05, "feet_on_support_mantle", "lowest foot %.3f m, support %.3f m (skeleton space)" % [foot_after, foot_before + 0.2])
	var sr: float = rig._gpos("sh_r").distance_to(target)
	var sl: float = rig._gpos("sh_l").distance_to(left_target)
	print("MATRON_PRESENTATION INFO shoulder_to_hold right %.3f m left %.3f m (arm max ~0.51 m)" % [sr, sl])
	# An unvalidated second hold must NOT move the left wrist: with
	# climb_hand2_known false the left wrist stays on the one hold.
	rig.contact(contact_data, false)
	var dl_fallback: float = rig._gpos("wr_l").distance_to(target)
	_check(dl_fallback < 0.05, "hand_left_fallback_single_hold", "left wrist %.3f m from the one hold when second hold unvalidated" % dl_fallback)
	var out_dir := _out if _out != "" else "artifacts"
	DirAccess.make_dir_recursive_absolute(out_dir)
	var f := FileAccess.open(out_dir.path_join("matron_presentation_frames.json"), FileAccess.WRITE)
	if f != null:
		f.store_string(JSON.stringify(frames, "\t"))
		f.close()
		print("MATRON_PRESENTATION frames_written " + out_dir.path_join("matron_presentation_frames.json"))
	else:
		_check(false, "frames_written", "could not open " + out_dir)
	print("MATRON_PRESENTATION %s" % ("PASS" if _failures == 0 else "FAIL"))
	quit(0 if _failures == 0 else 1)
