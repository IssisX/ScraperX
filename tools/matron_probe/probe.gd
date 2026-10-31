extends SceneTree
## The Matron as she is drawn, every frame of a touch scenario: the recorder for
## tools/matron_probe/score.py (README.md there). It plays the real game scene
## with the scenario's own touches and adds one node, run after every other
## node each frame, that reads her body once every view pass is done (her pose,
## the IK and contact pass, her eyes and her hair), from the skeleton the
## renderer skins: each bone's pose as posed, her hair solver's own points, her
## state and holds. It changes nothing in the game. The surface under her hands
## and feet is read with the simulation's own read-only ray (get_surface_below).
##
##   tools/godot/godot --headless --fixed-fps 60 --path godot \
##       -s "$PWD/tools/matron_probe/probe.gd" -- --uitest=touch_matron_act --probe-out=<dir>
##
## Options (after --): --probe-out=<dir> (required), --probe-seconds=<s> (stop
## after this much game time; default: the scenario's own end).
##
## Writes into <dir>: meta.json (her bones, parts, skins, hair rig, contacts),
## mesh.bin (each part's vertices, normals, UVs, bone indices, weights and
## triangles, as the game loaded them), frames.csv (one row a frame) and
## bones.bin (for each frame she is drawn: the skeleton's world transform, every
## bone's posed transform, the hair solver's points before and after).

const FORMAT := 1
const STAND_CENTRE := 1.05        # her capsule's centre over her soles (matron.hpp kStandCentre)
const CAPSULE_RADIUS := 0.30      # matron.hpp kRadius
const SURFACE_REACH := 4.0
const SURFACE_LIFT := 0.15        # a limb sunk into the steel up to this deep still finds its top

var _recorder: Node


func _initialize() -> void:
	var out := ""
	var seconds := 0.0
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--probe-out="):
			out = arg.trim_prefix("--probe-out=")
		elif arg.begins_with("--probe-seconds="):
			seconds = float(arg.trim_prefix("--probe-seconds="))
	if out.is_empty():
		push_error("matron probe: --probe-out=<dir> is required")
		quit(2)
		return
	DirAccess.make_dir_recursive_absolute(out)
	_recorder = Recorder.new()
	_recorder.out_dir = out
	_recorder.stop_after_s = seconds
	_recorder.name = "MatronProbe"
	root.add_child.call_deferred(_recorder)
	change_scene_to_file.call_deferred("res://main.tscn")


func _finalize() -> void:
	if _recorder != null:
		_recorder.close()


class Recorder extends Node:
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
	var _chain_count := 0
	var _chain_points := 0
	var _closed := false

	func _ready() -> void:
		# After every node of the game (all at priority 0), and the sample itself
		# deferred to the frame's last flush, after anything queued for later.
		process_priority = 1 << 30
		process_mode = Node.PROCESS_MODE_ALWAYS

	func _process(delta: float) -> void:
		_frame += 1
		_t_game += delta
		_sample.call_deferred(delta)
		if stop_after_s > 0.0 and _t_game >= stop_after_s:
			close()
			print("MATRON_PROBE stop t=%.2f frames=%d records=%d" % [_t_game, _frame, _records])
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
		var data: Dictionary = native.get_matron_state()
		var hold: Dictionary = native.get_matron_hold_state()
		var vis := figure.is_visible_in_tree()
		var rec := -1
		if vis:
			rec = _records
			_write_record()
			_records += 1
		_write_row(main, view, native, data, hold, delta, vis, rec)
		if _frame % 300 == 0:
			_csv.flush()
			_bin.flush()

	# --- once: what she is made of, as the game loaded it ---------------------

	func _open(rig: Object) -> void:
		_rig = rig
		_skel = rig.get("skel")
		var chains: Array = rig.get("_chains")
		_chain_count = chains.size()
		_chain_points = 0 if chains.is_empty() else (chains[0]["idx"] as Array).size() + 1
		var bones := []
		for i in _skel.get_bone_count():
			bones.append({"name": _skel.get_bone_name(i), "parent": _skel.get_bone_parent(i),
				"pose_position": _v3(_skel.get_bone_pose_position(i)), "rest": _xf(_skel.get_bone_rest(i))})
		var mesh_file := FileAccess.open(out_dir.path_join("mesh.bin"), FileAccess.WRITE)
		var parts := []
		for node in _skel.get_children():
			var mi := node as MeshInstance3D
			if mi == null or mi.mesh == null:
				continue
			parts.append(_write_part(mi, mesh_file))
		mesh_file.close()
		var consts: Dictionary = (rig.get_script() as Script).get_script_constant_map()
		var hair_consts := {}
		for key in ["HAIR_DAMP", "HAIR_STIFFNESS", "HAIR_ITERATIONS", "HAIR_JUMP_M", "BACK_RADIUS", "LOCK_VARY",
				"WIND_MS2", "WIND_RATE"]:
			if consts.has(key):
				hair_consts[key] = consts[key]
		for key in ["HAIR_GRAVITY", "WIND_DIR"]:
			if consts.has(key):
				hair_consts[key] = _v3(consts[key])
		var chain_out := []
		for c in chains:
			chain_out.append({"idx": c["idx"], "root_local": _v3(c["root_local"]), "seg": c["seg"],
				"damp": c["damp"], "give": c["give"], "phase": c["phase"]})
		_meta = {
			"format": FORMAT,
			"engine": Engine.get_version_info()["string"],
			"args": OS.get_cmdline_user_args(),
			"fixed_fps": _fixed_fps(),
			"bones": bones,
			"parts": parts,
			"record_floats": 12 + 12 * bones.size() + 2 * 3 * _chain_count * _chain_points + 1,
			"hair": {"chains": chain_out, "chain_points": _chain_points, "skull_local": _v3(rig.get("_skull_local")),
				"skull_radius": rig.get("_skull_radius"), "back_local": [0.0, 0.05, -0.05], "consts": hair_consts},
			"contacts": rig.get("contacts"),
			"stand_centre": STAND_CENTRE,
			"capsule_radius": CAPSULE_RADIUS,
		}
		_save_meta()
		_bin = FileAccess.open(out_dir.path_join("bones.bin"), FileAccess.WRITE)
		_csv = FileAccess.open(out_dir.path_join("frames.csv"), FileAccess.WRITE)
		_csv.store_line(",".join(COLUMNS))

	static func _fixed_fps() -> int:
		var args := OS.get_cmdline_args()
		for i in args.size():
			if args[i] == "--fixed-fps" and i + 1 < args.size():
				return int(args[i + 1])
		return 0

	func _write_part(mi: MeshInstance3D, file: FileAccess) -> Dictionary:
		var arrays := mi.mesh.surface_get_arrays(0)
		var verts: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
		var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL] if arrays[Mesh.ARRAY_NORMAL] != null else PackedVector3Array()
		var uvs: PackedVector2Array = arrays[Mesh.ARRAY_TEX_UV] if arrays[Mesh.ARRAY_TEX_UV] != null else PackedVector2Array()
		var bones: PackedInt32Array = arrays[Mesh.ARRAY_BONES]
		var weights: PackedFloat32Array = arrays[Mesh.ARRAY_WEIGHTS]
		var index: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
		var n := verts.size()
		if normals.size() != n:
			normals.resize(n)
		if uvs.size() != n:
			uvs.resize(n)
		var at := file.get_position()
		# float32 throughout (a single-precision engine's vectors are already so).
		var vf := PackedFloat32Array()
		vf.resize(n * 3)
		var nf := PackedFloat32Array()
		nf.resize(n * 3)
		var uf := PackedFloat32Array()
		uf.resize(n * 2)
		for i in n:
			vf[i * 3] = verts[i].x
			vf[i * 3 + 1] = verts[i].y
			vf[i * 3 + 2] = verts[i].z
			nf[i * 3] = normals[i].x
			nf[i * 3 + 1] = normals[i].y
			nf[i * 3 + 2] = normals[i].z
			uf[i * 2] = uvs[i].x
			uf[i * 2 + 1] = uvs[i].y
		file.store_buffer(vf.to_byte_array())
		file.store_buffer(nf.to_byte_array())
		file.store_buffer(uf.to_byte_array())
		file.store_buffer(bones.to_byte_array())
		file.store_buffer(weights.to_byte_array())
		file.store_buffer(index.to_byte_array())
		var binds := []
		var bind_poses := []
		var skin := mi.skin
		for b in skin.get_bind_count():
			var bone := skin.get_bind_bone(b)
			if bone < 0:
				bone = _skel.find_bone(skin.get_bind_name(b))
			binds.append(bone)
			bind_poses.append(_xf(skin.get_bind_pose(b)))
		var rel := _skel.global_transform.affine_inverse() * mi.global_transform
		return {"name": String(mi.name), "offset": at, "vertices": n, "triangles": index.size() / 3,
			"bones_per_vertex": bones.size() / maxi(n, 1), "binds": binds, "bind_poses": bind_poses,
			"to_skeleton": _xf(rel), "visible": mi.visible}

	func _save_meta() -> void:
		var f := FileAccess.open(out_dir.path_join("meta.json"), FileAccess.WRITE)
		f.store_string(JSON.stringify(_meta))
		f.close()

	# --- every frame -------------------------------------------------------------

	func _write_record() -> void:
		var rec := PackedFloat32Array()
		rec.resize(int(_meta["record_floats"]))
		var k := _put_xf(rec, 0, _skel.global_transform)
		for i in _skel.get_bone_count():
			k = _put_xf(rec, k, _skel.get_bone_global_pose(i))
		var chains: Array = _rig.get("_chains")
		for key in ["pts", "prev"]:
			for c in _chain_count:
				var pts: Array = chains[c][key]
				for j in _chain_points:
					var p: Vector3 = pts[j] if j < pts.size() else Vector3(NAN, NAN, NAN)
					rec[k] = p.x
					rec[k + 1] = p.y
					rec[k + 2] = p.z
					k += 3
		rec[k] = float(_rig.get("_wind_clock"))
		_bin.store_buffer(rec.to_byte_array())

	func _write_row(main: Node, view: Object, native: Object, data: Dictionary, hold: Dictionary, delta: float,
			vis: bool, rec: int) -> void:
		var pos: Vector3 = data.get("position", Vector3.ZERO)
		var rpos: Vector3 = data.get("render_position", pos)
		var hand: Vector3 = hold.get("hand", Vector3.ZERO)
		var hand2: Vector3 = hold.get("hand2", Vector3.ZERO)
		var ch: Vector3 = data.get("climb_hand", Vector3.ZERO)
		var ch2: Vector3 = data.get("climb_hand2", ch)
		var player: Vector3 = native.get_player_render_position()
		var surf := []
		for limb in [["an_l", "heel", "toe"], ["an_r", "heel", "toe"], ["wr_l", "knuckles", ""], ["wr_r", "knuckles", ""]]:
			surf.append(_surface(native, pos, limb, vis))
		var burst: Variant = view.get("burst_active")
		var contact_hand2: Vector3 = data.get("contact_hand2", Vector3.ZERO)
		var row := [_frame, rec, "%.5f" % _t_game, "%.5f" % float(native.get_simulation_time_seconds()), "%.6f" % delta,
			int(bool(main.get("freeze_sim"))), int(get_tree().paused), int(vis),
			int(data.get("state", 0)), int(data.get("stage", 0)), int(data.get("turn", 0)),
			"%.4f" % float(data.get("speed", 0.0)), "%.4f" % float(data.get("stride", 0.0)),
			int(bool(data.get("sees_player", false))), int(bool(data.get("alive", false))), int(bool(data.get("released", false))),
			int(bool(data.get("climbing", false))), int(bool(data.get("on_pipe", false))), int(bool(data.get("on_line", false))),
			int(bool(data.get("clinging", false))), int(bool(data.get("wary", false))), int(bool(data.get("pipe_head_down", false))),
			int(bool(data.get("kneeling", false))), int(bool(data.get("climb_airborne", false))),
			int(bool(data.get("climb_move_active", false))), str(data.get("climb_move", "")).replace(",", ";"),
			int(data.get("carrying", 0)), int(bool(data.get("bracing", false))), int(bool(data.get("mantling", false))),
			"%.4f" % float(view.get("_fall_speed")),
			"%.5f" % pos.x, "%.5f" % pos.y, "%.5f" % pos.z, "%.5f" % rpos.x, "%.5f" % rpos.y, "%.5f" % rpos.z,
			"%.5f" % float(data.get("yaw", 0.0)), "%.5f" % float(data.get("render_yaw", data.get("yaw", 0.0))),
			"%.4f" % float(data.get("floor_y", 0.0)),
			int(hold.get("state", 0)), int(bool(hold.get("held", false))), int(bool(hold.get("grip_on", false))),
			"%.5f" % hand.x, "%.5f" % hand.y, "%.5f" % hand.z, "%.5f" % hand2.x, "%.5f" % hand2.y, "%.5f" % hand2.z,
			int(bool(data.get("climbing", false)) and data.has("climb_hand")), "%.5f" % ch.x, "%.5f" % ch.y, "%.5f" % ch.z,
			int(bool(data.get("climb_hand2_known", false))), "%.5f" % ch2.x, "%.5f" % ch2.y, "%.5f" % ch2.z,
			"%.5f" % player.x, "%.5f" % player.y, "%.5f" % player.z,
			surf[0], surf[1], surf[2], surf[3], "" if burst == null else str(int(bool(burst))),
			int(bool(data.get("contact_hand2_held", false))),
			"%.5f" % contact_hand2.x, "%.5f" % contact_hand2.y, "%.5f" % contact_hand2.z]
		# Her collision capsule as the native exports it (matron_shape.cpp), blank before it did.
		if data.has("shape_r"):
			var sa: Vector3 = data["shape_a"]
			var sb: Vector3 = data["shape_b"]
			row.append_array([int(data.get("stance", 0)), "%.5f" % sa.x, "%.5f" % sa.y, "%.5f" % sa.z,
				"%.5f" % sb.x, "%.5f" % sb.y, "%.5f" % sb.z, "%.4f" % float(data["shape_r"])])
		else:
			row.append_array(["", "", "", "", "", "", "", ""])
		var out := PackedStringArray()
		for v in row:
			out.append(str(v))
		_csv.store_line(",".join(out))

	# The first surface under a hand or foot, by the simulation's own ray. Her
	# own capsule stands in that ray under her feet, so a limb inside its round
	# is read from just under the capsule.
	func _surface(native: Object, centre: Vector3, limb: Array, vis: bool) -> String:
		if not vis:
			return ""
		var bone: String = limb[0]
		var p: Vector3 = _rig.point(bone, limb[1])
		if limb[2] != "":
			p = (p + (_rig.point(bone, limb[2]) as Vector3)) * 0.5
		var from := p + Vector3(0.0, SURFACE_LIFT, 0.0)
		if Vector2(p.x - centre.x, p.z - centre.z).length() < CAPSULE_RADIUS + 0.06:
			from.y = minf(from.y, centre.y - STAND_CENTRE - 0.01)
		var y: float = native.get_surface_below(from, SURFACE_REACH)
		return "" if is_nan(y) else "%.4f" % y

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

	static func _v3(v: Vector3) -> Array:
		return [v.x, v.y, v.z]

	# Basis columns (x, y, z) then the origin.
	static func _xf(t: Transform3D) -> Array:
		return [t.basis.x.x, t.basis.x.y, t.basis.x.z, t.basis.y.x, t.basis.y.y, t.basis.y.z,
			t.basis.z.x, t.basis.z.y, t.basis.z.z, t.origin.x, t.origin.y, t.origin.z]

	static func _put_xf(a: PackedFloat32Array, k: int, t: Transform3D) -> int:
		a[k] = t.basis.x.x
		a[k + 1] = t.basis.x.y
		a[k + 2] = t.basis.x.z
		a[k + 3] = t.basis.y.x
		a[k + 4] = t.basis.y.y
		a[k + 5] = t.basis.y.z
		a[k + 6] = t.basis.z.x
		a[k + 7] = t.basis.z.y
		a[k + 8] = t.basis.z.z
		a[k + 9] = t.origin.x
		a[k + 10] = t.origin.y
		a[k + 11] = t.origin.z
		return k + 12

	const COLUMNS := ["f", "rec", "t", "t_sim", "dt", "frozen", "paused", "vis", "state", "stage", "turn", "speed", "stride",
		"sees", "alive", "released", "climbing", "on_pipe", "on_line", "clinging", "wary", "head_down", "kneeling",
		"climb_airborne", "climb_move_active", "climb_move", "carrying", "bracing", "mantling", "fall_v",
		"px", "py", "pz", "rx", "ry", "rz", "yaw", "ryaw", "floor_y", "hold_state", "held", "grip_on",
		"hx", "hy", "hz", "h2x", "h2y", "h2z", "ch_known", "chx", "chy", "chz", "ch2_known", "ch2x", "ch2y", "ch2z",
		"plx", "ply", "plz", "surf_foot_l", "surf_foot_r", "surf_palm_l", "surf_palm_r", "burst",
		"contact_hand2_held", "contact_hand2x", "contact_hand2y", "contact_hand2z",
		"stance", "shape_ax", "shape_ay", "shape_az", "shape_bx", "shape_by", "shape_bz", "shape_r"]
