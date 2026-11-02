extends SceneTree
## Studio poses for the hair scorer. Not a game recording. Writes one probe
## directory the existing score.py can read: guard, a 110 degree neck twist,
## and the head hung off the neck.

const MatronView := preload("res://presentation/life/matron_view.gd")
const DT := 1.0 / 30.0

var _out := "/tmp/hair-pose-probe"


func _initialize() -> void:
	for arg in OS.get_cmdline_user_args():
		if arg.begins_with("--probe-out="):
			_out = arg.trim_prefix("--probe-out=")
	_run.call_deferred()


func _run() -> void:
	DirAccess.make_dir_recursive_absolute(_out)
	var view := MatronView.new()
	root.add_child(view)
	view._build_figure()
	view._build_sparks()
	var rig: Object = view._rig
	if rig == null:
		push_error("no rig")
		quit(1)
		return
	var writer := _Writer.new()
	writer.open(rig, _out)
	await _pose(view, _base(7), 60, 0.0)
	writer.record(rig, 7, 0)
	view._twist_shown = 1.92
	await _pose(view, _base(7), 30, 0.0)
	writer.record(rig, 7, 1)
	view._twist_shown = 0.0
	var down := _base(3)
	down["pipe_stunned"] = true
	await _pose(view, down, 40, 0.0)
	writer.record(rig, 3, 2)
	writer.close()
	print("HAIR_POSE_PROBE records=3 out=%s" % _out)
	quit(0)


func _base(state: int) -> Dictionary:
	return {"state": state, "speed": 0.0, "stride": 0.0, "turn": 0, "stage": 1, "sees_player": false,
		"alive": true, "released": true, "position": Vector3(0.0, MatronView.STAND_CENTRE, 0.0), "yaw": 0.0,
		"exposure": 0.0, "floor_y": 0.0}


func _pose(view: Node, data: Dictionary, frames: int, _yaw: float) -> void:
	var cam := Vector3(0.0, 1.5, 3.2)
	for _i in frames:
		view._clock += DT
		view._state_age += DT
		view._update_figure(data, DT, cam, cam)
		await process_frame


class _Writer:
	var meta := {}
	var bin: FileAccess
	var csv: FileAccess
	var skel: Skeleton3D
	var rig: Object
	var chain_count := 0
	var chain_points := 0
	var records := 0
	var out_dir := ""
	var header := "f,rec,t,t_sim,dt,frozen,paused,vis,state,stage,turn,speed,stride,sees,alive,released,climbing,on_pipe,on_line,clinging,wary,head_down,kneeling,climb_airborne,climb_move_active,climb_move,carrying,bracing,mantling,fall_v,px,py,pz,rx,ry,rz,yaw,ryaw,floor_y,hold_state,held,grip_on,hx,hy,hz,h2x,h2y,h2z,ch_known,chx,chy,chz,ch2_known,ch2x,ch2y,ch2z,plx,ply,plz,surf_foot_l,surf_foot_r,surf_palm_l,surf_palm_r,burst,contact_hand2_held,contact_hand2x,contact_hand2y,contact_hand2z"

	func open(the_rig: Object, dir: String) -> void:
		out_dir = dir
		rig = the_rig
		skel = rig.skel
		var chains: Array = rig._chains
		chain_count = chains.size()
		chain_points = (chains[0]["idx"] as Array).size() + 1
		var bones := []
		for i in skel.get_bone_count():
			bones.append({"name": skel.get_bone_name(i), "parent": skel.get_bone_parent(i),
				"pose_position": _v3(skel.get_bone_pose_position(i)), "rest": _xf(skel.get_bone_rest(i))})
		var mesh_file := FileAccess.open(out_dir.path_join("mesh.bin"), FileAccess.WRITE)
		var parts := []
		for node in skel.get_children():
			var mi := node as MeshInstance3D
			if mi == null or mi.mesh == null:
				continue
			parts.append(_part(mi, mesh_file))
		mesh_file.close()
		var consts: Dictionary = (rig.get_script() as Script).get_script_constant_map()
		var hair_consts := {}
		for key in ["HAIR_DAMP", "HAIR_STIFFNESS", "HAIR_ITERATIONS", "HAIR_JUMP_M", "BACK_RADIUS", "LOCK_VARY", "WIND_MS2", "WIND_RATE"]:
			if consts.has(key):
				hair_consts[key] = consts[key]
		for key in ["HAIR_GRAVITY", "WIND_DIR"]:
			if consts.has(key):
				hair_consts[key] = _v3(consts[key])
		var chain_out := []
		for c in chains:
			chain_out.append({"idx": c["idx"], "root_local": _v3(c["root_local"]), "seg": c["seg"],
				"damp": c["damp"], "give": c["give"], "phase": c["phase"]})
		meta = {
			"format": 1, "engine": Engine.get_version_info()["string"], "args": ["--uitest=studio-hair"],
			"fixed_fps": 30, "bones": bones, "parts": parts,
			"record_floats": 12 + 12 * bones.size() + 2 * 3 * chain_count * chain_points + 1,
			"hair": {"chains": chain_out, "chain_points": chain_points, "skull_local": _v3(rig._skull_local),
				"skull_radius": rig._skull_radius, "back_local": [0.0, 0.05, -0.05], "consts": hair_consts},
			"contacts": rig.contacts, "stand_centre": 1.05, "capsule_radius": 0.30,
		}
		bin = FileAccess.open(out_dir.path_join("bones.bin"), FileAccess.WRITE)
		csv = FileAccess.open(out_dir.path_join("frames.csv"), FileAccess.WRITE)
		csv.store_line(header)

	func record(the_rig: Object, state: int, tag: int) -> void:
		var rec := PackedFloat32Array()
		rec.resize(int(meta["record_floats"]))
		var k := _put(rec, 0, skel.global_transform)
		for i in skel.get_bone_count():
			k = _put(rec, k, skel.get_bone_global_pose(i))
		var chains: Array = the_rig._chains
		for key in ["pts", "prev"]:
			for c in chain_count:
				var pts: Array = chains[c][key]
				for j in chain_points:
					var p: Vector3 = pts[j]
					rec[k] = p.x
					rec[k + 1] = p.y
					rec[k + 2] = p.z
					k += 3
		rec[k] = float(the_rig._wind_clock)
		bin.store_buffer(rec.to_byte_array())
		var hang := 0.0
		for c in chains:
			var pts: Array = c["pts"]
			hang += (pts[0] as Vector3).y - (pts[pts.size() - 1] as Vector3).y
		hang /= float(chains.size())
		print("HAIR_POSE tag=%d state=%d hang=%.3f" % [tag, state, hang])
		var cols := PackedStringArray()
		cols.resize(header.split(",").size())
		cols.fill("0")
		cols[0] = str(records)
		cols[1] = str(records)
		cols[2] = "%.5f" % (records * DT)
		cols[3] = "%.5f" % (records * DT)
		cols[4] = "0.033333"
		cols[7] = "1"
		cols[8] = str(state)
		csv.store_line(",".join(cols))
		records += 1

	func close() -> void:
		bin.close()
		csv.close()
		meta["frames"] = records
		meta["records"] = records
		var f := FileAccess.open(out_dir.path_join("meta.json"), FileAccess.WRITE)
		f.store_string(JSON.stringify(meta))
		f.close()

	func _part(mi: MeshInstance3D, file: FileAccess) -> Dictionary:
		var arrays := mi.mesh.surface_get_arrays(0)
		var verts: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
		var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
		var uvs: PackedVector2Array = arrays[Mesh.ARRAY_TEX_UV]
		var bones: PackedInt32Array = arrays[Mesh.ARRAY_BONES]
		var weights: PackedFloat32Array = arrays[Mesh.ARRAY_WEIGHTS]
		var index: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
		var n := verts.size()
		if normals == null or normals.size() != n:
			normals = PackedVector3Array()
			normals.resize(n)
		if uvs == null or uvs.size() != n:
			uvs = PackedVector2Array()
			uvs.resize(n)
		var at := file.get_position()
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
				bone = skel.find_bone(skin.get_bind_name(b))
			binds.append(bone)
			bind_poses.append(_xf(skin.get_bind_pose(b)))
		var rel := skel.global_transform.affine_inverse() * mi.global_transform
		return {"name": String(mi.name), "offset": at, "vertices": n, "triangles": index.size() / 3,
			"bones_per_vertex": bones.size() / maxi(n, 1), "binds": binds, "bind_poses": bind_poses,
			"to_skeleton": _xf(rel), "visible": mi.visible}

	func _v3(v: Vector3) -> Array:
		return [v.x, v.y, v.z]

	func _xf(t: Transform3D) -> Array:
		return [t.basis.x.x, t.basis.x.y, t.basis.x.z, t.basis.y.x, t.basis.y.y, t.basis.y.z,
			t.basis.z.x, t.basis.z.y, t.basis.z.z, t.origin.x, t.origin.y, t.origin.z]

	func _put(a: PackedFloat32Array, k: int, t: Transform3D) -> int:
		var flat: Array = _xf(t)
		for i in 12:
			a[k + i] = flat[i]
		return k + 12
