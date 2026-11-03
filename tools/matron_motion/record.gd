extends SceneTree
## Her motion as she is drawn, every frame of a touch scenario: the recorder
## for tools/matron_motion/score.py (README.md there). Rule 37: her movement is
## continuous, joints at most 20 rad/s, hands reaching each hold.
##
##   tools/godot/godot --headless --fixed-fps 60 --path godot \
##       -s "$PWD/tools/matron_motion/record.gd" -- --uitest=touch_matron_act --motion-out=<dir>
##
## It plays the real game scene with the scenario's own touches, as
## tools/matron_probe/probe.gd does, and adds one node, run after every other
## node each frame, that reads her skeleton once every view pass is done (the
## pose the renderer skins), her hands as the rig places them, and what the
## simulation says each hand holds. It changes nothing in the game.
##
## Options (after --): --motion-out=<dir> (required), --motion-seconds=<s>
## (stop after this much game time; default: the scenario's own end).
##
## Writes into <dir>: meta.json (bones, parents, hand contact points, fps),
## frames.csv (one row a frame) and rotations.bin (for each frame she is drawn:
## her skeleton's world rotation, then every bone's rotation in its parent's
## frame, as quaternions x y z w, float32).

const FORMAT := 1

var _recorder: Node


func _initialize() -> void:
	var out := ""
	var seconds := 0.0
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--motion-out="):
			out = arg.trim_prefix("--motion-out=")
		elif arg.begins_with("--motion-seconds="):
			seconds = float(arg.trim_prefix("--motion-seconds="))
	if out.is_empty():
		push_error("matron motion: --motion-out=<dir> is required")
		quit(2)
		return
	DirAccess.make_dir_recursive_absolute(out)
	_recorder = Recorder.new()
	_recorder.out_dir = out
	_recorder.stop_after_s = seconds
	_recorder.name = "MatronMotion"
	root.add_child.call_deferred(_recorder)
	change_scene_to_file.call_deferred("res://main.tscn")


func _finalize() -> void:
	if _recorder != null:
		_recorder.close()


class Recorder extends Node:
	const SIDES := ["l", "r"]
	var out_dir := ""
	var stop_after_s := 0.0
	var _csv: FileAccess
	var _bin: FileAccess
	var _frame := 0
	var _records := 0
	var _t_game := 0.0
	var _meta := {}
	var _skel: Skeleton3D
	var _rig: Object
	var _parents := PackedInt32Array()
	# Per side: the wrist's contact points, by name, in the order written.
	var _hand_keys := {}
	var _closed := false

	func _ready() -> void:
		# After every node of the game, and the sample deferred to the frame's
		# last flush (as probe.gd).
		process_priority = 1 << 30
		process_mode = Node.PROCESS_MODE_ALWAYS

	func _process(delta: float) -> void:
		_frame += 1
		_t_game += delta
		_sample.call_deferred(delta)
		if stop_after_s > 0.0 and _t_game >= stop_after_s:
			close()
			print("MATRON_MOTION stop t=%.2f frames=%d records=%d" % [_t_game, _frame, _records])
			get_tree().quit(0)

	func _sample(delta: float) -> void:
		if _closed:
			return
		var main := get_tree().current_scene
		if main == null:
			return
		var view: Object = main.get("_matron_view")
		var native: Object = main.get("_native")
		if view == null or native == null:
			return
		var rig: Object = view.get("_rig")
		var figure: Node3D = view.get("_figure")
		if rig == null or figure == null:
			return
		if _csv == null:
			_open(rig)
		var vis := figure.is_visible_in_tree()
		var rec := -1
		if vis:
			rec = _records
			_write_record()
			_records += 1
		_write_row(main, view, native, delta, vis, rec)
		if _frame % 300 == 0:
			_csv.flush()
			_bin.flush()

	func _open(rig: Object) -> void:
		_rig = rig
		_skel = rig.get("skel")
		var bones := []
		for i in _skel.get_bone_count():
			_parents.append(_skel.get_bone_parent(i))
			bones.append({"name": _skel.get_bone_name(i), "parent": _skel.get_bone_parent(i)})
		var contacts: Dictionary = rig.get("contacts")
		var columns := ["f", "rec", "t", "t_sim", "dt", "frozen", "paused", "vis", "state", "climbing",
			"climb_move_active", "climb_move", "burst", "rx", "ry", "rz",
			"hand_held", "hand_x", "hand_y", "hand_z", "hand2_held", "hand2_x", "hand2_y", "hand2_z",
			"ch_known", "chx", "chy", "chz", "ch2_known", "ch2x", "ch2y", "ch2z", "climb_grips",
			"hold_state", "held", "ankle_x", "ankle_y", "ankle_z"]
		for s in SIDES:
			var keys: Array = (contacts.get("wr_" + s, {}) as Dictionary).keys()
			keys.sort()
			_hand_keys[s] = keys
			columns.append_array(["wr_%s_x" % s, "wr_%s_y" % s, "wr_%s_z" % s])
			for key in keys:
				columns.append_array(["%s_%s_x" % [key, s], "%s_%s_y" % [key, s], "%s_%s_z" % [key, s]])
		_meta = {
			"format": FORMAT,
			"engine": Engine.get_version_info()["string"],
			"args": OS.get_cmdline_user_args(),
			"fixed_fps": _fixed_fps(),
			"bones": bones,
			"hand_keys": _hand_keys,
			"record_floats": 4 + 4 * bones.size(),
			"columns": columns,
		}
		_save_meta()
		_bin = FileAccess.open(out_dir.path_join("rotations.bin"), FileAccess.WRITE)
		_csv = FileAccess.open(out_dir.path_join("frames.csv"), FileAccess.WRITE)
		_csv.store_line(",".join(columns))

	static func _fixed_fps() -> int:
		var args := OS.get_cmdline_args()
		for i in args.size():
			if args[i] == "--fixed-fps" and i + 1 < args.size():
				return int(args[i + 1])
		return 0

	func _save_meta() -> void:
		var f := FileAccess.open(out_dir.path_join("meta.json"), FileAccess.WRITE)
		f.store_string(JSON.stringify(_meta))
		f.close()

	# Her skeleton's world rotation, then each bone's rotation in its parent's
	# frame (the root's in the skeleton's), from the poses the renderer skins.
	func _write_record() -> void:
		var count := _skel.get_bone_count()
		var rec := PackedFloat32Array()
		rec.resize(4 + 4 * count)
		_put_q(rec, 0, _skel.global_transform.basis.get_rotation_quaternion())
		var globals: Array[Basis] = []
		globals.resize(count)
		for i in count:
			globals[i] = _skel.get_bone_global_pose(i).basis.orthonormalized()
		for i in count:
			var local := globals[i] if _parents[i] < 0 else globals[_parents[i]].inverse() * globals[i]
			_put_q(rec, 4 + 4 * i, local.get_rotation_quaternion())
		_bin.store_buffer(rec.to_byte_array())

	static func _put_q(a: PackedFloat32Array, k: int, q: Quaternion) -> void:
		a[k] = q.x
		a[k + 1] = q.y
		a[k + 2] = q.z
		a[k + 3] = q.w

	func _write_row(main: Node, view: Object, native: Object, delta: float, vis: bool, rec: int) -> void:
		var data: Dictionary = native.get_matron_state()
		var hold: Dictionary = native.get_matron_hold_state()
		var rpos: Vector3 = data.get("render_position", data.get("position", Vector3.ZERO))
		var hand: Vector3 = data.get("contact_hand", Vector3.ZERO)
		var hand2: Vector3 = data.get("contact_hand2", Vector3.ZERO)
		var ch: Vector3 = data.get("climb_hand", Vector3.ZERO)
		var ch2: Vector3 = data.get("climb_hand2", ch)
		var ankle: Vector3 = hold.get("hand", Vector3.ZERO)
		var burst: Variant = view.get("burst_active")
		var row := [_frame, rec, "%.5f" % _t_game, "%.5f" % float(native.get_simulation_time_seconds()),
			"%.6f" % delta, int(bool(main.get("freeze_sim"))), int(get_tree().paused), int(vis),
			int(data.get("state", 0)), int(bool(data.get("climbing", false))),
			int(bool(data.get("climb_move_active", false))), str(data.get("climb_move", "")).replace(",", ";"),
			"" if burst == null else str(int(bool(burst))),
			"%.5f" % rpos.x, "%.5f" % rpos.y, "%.5f" % rpos.z,
			int(bool(data.get("contact_hand_held", false))), "%.5f" % hand.x, "%.5f" % hand.y, "%.5f" % hand.z,
			int(bool(data.get("contact_hand2_held", false))), "%.5f" % hand2.x, "%.5f" % hand2.y, "%.5f" % hand2.z,
			int(bool(data.get("climbing", false)) and data.has("climb_hand")), "%.5f" % ch.x, "%.5f" % ch.y, "%.5f" % ch.z,
			int(bool(data.get("climb_hand2_known", false))), "%.5f" % ch2.x, "%.5f" % ch2.y, "%.5f" % ch2.z,
			int(data.get("climb_grips", 0)),
			int(hold.get("state", 0)), int(bool(hold.get("held", false))), "%.5f" % ankle.x, "%.5f" % ankle.y,
			"%.5f" % ankle.z]
		for s in SIDES:
			if vis:
				var wrist: Vector3 = _rig.bone_world("wr_" + s).origin
				row.append_array(["%.5f" % wrist.x, "%.5f" % wrist.y, "%.5f" % wrist.z])
				for key in _hand_keys[s]:
					var p: Vector3 = _rig.point("wr_" + s, key)
					row.append_array(["%.5f" % p.x, "%.5f" % p.y, "%.5f" % p.z])
			else:
				for _i in 3 * (1 + (_hand_keys[s] as Array).size()):
					row.append("")
		var out := PackedStringArray()
		for v in row:
			out.append(str(v))
		_csv.store_line(",".join(out))

	func close() -> void:
		if _closed:
			return
		_closed = true
		if _csv != null:
			_csv.flush()
			_csv.close()
		if _bin != null:
			_bin.flush()
			_bin.close()
		if not _meta.is_empty():
			_meta["frames"] = _frame
			_meta["records"] = _records
			_meta["t_game"] = _t_game
			_save_meta()

	func _exit_tree() -> void:
		close()
