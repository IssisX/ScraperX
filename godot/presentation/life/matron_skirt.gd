extends RefCounted
## Her skirt as cloth (J-011, #118; the ruling at #87): a coarse proxy of
## NC columns by NR rings of verlet points, its top ring pinned at her waist,
## held by distance limits along its columns, round its rings and across one
## diagonal of each cell, pushed out of her thighs and shins (tapered
## capsules), and pulled softly toward where her old linear skin put it, so
## it keeps its cut while it swings and hangs. Each point is a bone added to
## her skeleton (skirt_cCC_kK, each column a chain from her pelvis), and the
## drawn skirt is skinned to those bones, bilinearly inside each cell, never
## to her thigh bones: linear skinning on the thighs tore it into strips
## (8,183 of its 16,812 edges past 1.10x their rest length).
##
## Every skirt bone turns with her pelvis and SWING_SHARE of its column's own
## swing. Linear skinning places a vertex between bones that turn differently
## off by its offset times their difference, which is what stretches a drawn
## edge; at SWING_SHARE 0 the drawn skirt is exactly the proxy interpolated
## plus its rigid fold offset, so no drawn edge stretches more than its cell.
## A point inside a leg leaves it on the side it was on the frame before,
## against the leg where it was then (a swept contact), so a leg swung
## forward carries the front panel ahead of it instead of splitting it.
##
## Presentation only: nothing here reaches the native, her physics or her
## force budget. Cheap: fixed sizes and iteration counts, every array
## allocated once in setup().

const NC := 16                  # columns round her
const NR := 10                  # rings, waist (pinned) to hem
const LOFT_RINGS := 40          # the skirt mesh's loft (tools/body/build_matron_body.py)
const LOFT_COLS := 129          # 128 round her and the seam's copy of the first
const THREAD_VERTS := 10        # each face of a thread hanging from the hem
const ITERATIONS := 3           # each frame
const RESET_ITERATIONS := 16    # the frame she is first drawn, from her old skin's stretched shape
const DAMP := 0.9               # verlet velocity kept per 1/60 s
const GRAVITY := Vector3(0.0, -9.81, 0.0)
# Pull per 1/60 s toward her old skin's place, by ring: firm on her hips,
# loose at the hem, so it hangs and swings below and keeps its line above.
const PULL := [1.0, 0.5, 0.3, 0.2, 0.14, 0.1, 0.075, 0.06, 0.05, 0.045]
const RING_MAX := 1.0           # a ring or diagonal edge never longer than its rest
const RING_MIN := 0.35          # nor shorter than this share of it (folds)
const COL_MIN := 0.6            # a column edge shortens to this share at most
const CLEAR_M := 0.018          # cloth kept this far off her legs
const JUMP_M := 1.0             # her waist moved this far in a frame: a respawn, restart
# Her thighs and shins as tapered capsules (radius at the upper and lower
# joint), measured on her drawn stockings in the bind pose: thigh 0.124 m
# at her hip to 0.083..0.096 by the knee, shin 0.071..0.078 to 0.046..0.053.
const THIGH_R := Vector2(0.126, 0.092)
const SHIN_R := Vector2(0.08, 0.055)
const SWING_SHARE := 0.0
const OLD_SKIN_SHARE := 0.0

var ok := false
var skel: Skeleton3D
var _n := NC * NR
var _bone := PackedInt32Array()         # particle -> skeleton bone
var _parent := PackedInt32Array()       # particle -> skeleton bone of its parent
var _pos := PackedVector3Array()        # world
var _prev := PackedVector3Array()
var _target := PackedVector3Array()
var _bind := PackedVector3Array()       # skeleton space, her bind pose
var _pin_local := PackedVector3Array()  # ring 0 in her pelvis bone's frame
var _old_bind := PackedInt32Array()     # 4 old skin binds per particle
var _old_w := PackedFloat32Array()
var _old_poses: Array[Transform3D] = []
var _old_bones := PackedInt32Array()    # old skin bind -> skeleton bone
var _old_m: Array[Transform3D] = []
var _rest_ring := PackedFloat32Array()  # (c, k) -> (c + 1, k)
var _rest_col := PackedFloat32Array()   # (c, k - 1) -> (c, k)
var _rest_diag := PackedFloat32Array()  # (c + 1, k - 1) -> (c, k)
var _bind_dir := PackedVector3Array()   # each point's column direction, bind pose
var _body_bind_inv := Basis.IDENTITY
var _g: Array[Transform3D] = []         # this frame's frames, skeleton space
var _body := -1
var _legs := PackedInt32Array()         # hp_l, kn_l, an_l, hp_r, kn_r, an_r
var _fresh := true
var _leg_at := PackedVector3Array()
var _leg_was := PackedVector3Array()   # last frame's
var _local := PackedVector3Array()     # this frame's points, skeleton space
# step()'s cost: the last COST_KEEP frames, microseconds (a ring)
const COST_KEEP := 4096
var cost_us := PackedInt32Array()
var cost_n := 0


static func idx(c: int, k: int) -> int:
	return ((c + NC) % NC) * NR + k


static func loft_ring(k: int) -> int:
	return int(round(float(k) * float(LOFT_RINGS - 1) / float(NR - 1)))


## Adds the cloth's bones to `skeleton` and reskins `mi` (her skirt_cloth) to
## them. False, and the skirt left as it was, if the mesh is not the loft
## build_matron_body.py makes.
func setup(skeleton: Skeleton3D, mi: MeshInstance3D) -> bool:
	skel = skeleton
	if mi == null or mi.mesh == null or mi.skin == null or mi.mesh.get_surface_count() < 1:
		push_error("matron skirt: no skirt mesh")
		return false
	var arrays: Array = mi.mesh.surface_get_arrays(0)
	var verts: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
	var old_b: PackedInt32Array = arrays[Mesh.ARRAY_BONES]
	var old_w: PackedFloat32Array = arrays[Mesh.ARRAY_WEIGHTS]
	var nv := verts.size()
	var loft := LOFT_RINGS * LOFT_COLS
	if nv < loft + 2 * LOFT_COLS or (nv - loft - 2 * LOFT_COLS) % THREAD_VERTS != 0 or verts[0].distance_to(verts[LOFT_COLS - 1]) > 1.0e-5 or old_b.size() < nv:
		push_error("matron skirt: the skirt mesh is not the loft (%d verts)" % nv)
		return false
	var bpv := old_b.size() / nv
	var skin := mi.skin
	_body = skel.find_bone("body")
	for nm in ["hp_l", "kn_l", "an_l", "hp_r", "kn_r", "an_r"]:
		_legs.append(skel.find_bone(nm))
	if _body < 0 or _legs.has(-1):
		push_error("matron skirt: her skeleton has no pelvis or legs")
		return false
	# the old skin's binds, to pull toward
	for b in skin.get_bind_count():
		var bone := skin.get_bind_bone(b)
		if bone < 0:
			bone = skel.find_bone(skin.get_bind_name(b))
		_old_bones.append(bone)
		_old_poses.append(skin.get_bind_pose(b))
		_old_m.append(Transform3D.IDENTITY)
	var body_bind := Transform3D.IDENTITY
	for b in skin.get_bind_count():
		if _old_bones[b] == _body:
			body_bind = skin.get_bind_pose(b).affine_inverse()
	_bind.resize(_n)
	_pin_local.resize(_n)
	_old_bind.resize(_n * 4)
	_old_w.resize(_n * 4)
	for c in NC:
		for k in NR:
			var i := idx(c, k)
			var v := loft_ring(k) * LOFT_COLS + c * (LOFT_COLS - 1) / NC
			_bind[i] = verts[v]
			_pin_local[i] = body_bind.affine_inverse() * verts[v]
			for j in 4:
				_old_bind[i * 4 + j] = old_b[v * bpv + j] if j < bpv else 0
				_old_w[i * 4 + j] = old_w[v * bpv + j] if j < bpv else 0.0
	_rest_ring.resize(_n)
	_rest_col.resize(_n)
	_rest_diag.resize(_n)
	for c in NC:
		for k in NR:
			var i := idx(c, k)
			_rest_ring[i] = _bind[i].distance_to(_bind[idx(c + 1, k)])
			if k > 0:
				_rest_col[i] = _bind[i].distance_to(_bind[idx(c, k - 1)])
				_rest_diag[i] = _bind[i].distance_to(_bind[idx(c + 1, k - 1)])
	# the bones: each column a chain from her pelvis, at rest unturned
	_body_bind_inv = body_bind.basis.orthonormalized().inverse()
	_g.resize(_n)
	_bone.resize(_n)
	_parent.resize(_n)
	_bind_dir.resize(_n)
	var bind_g: Array[Transform3D] = []
	bind_g.resize(_n)
	for c in NC:
		for k in NR:
			var i := idx(c, k)
			bind_g[i] = Transform3D(Basis.IDENTITY, _bind[i])
			_bind_dir[i] = (_bind[idx(c, mini(k + 1, NR - 1))] - _bind[idx(c, maxi(k - 1, 0))]).normalized()
	var skin_out := Skin.new()
	for c in NC:
		for k in NR:
			var i := idx(c, k)
			var nm := "skirt_c%02d_k%d" % [c, k]
			var bone := skel.find_bone(nm)
			if bone < 0:
				bone = skel.get_bone_count()
				skel.add_bone(nm)
			var parent := _body if k == 0 else _bone[idx(c, k - 1)]
			skel.set_bone_parent(bone, parent)
			var rest := body_bind.affine_inverse() * bind_g[i] if k == 0 else bind_g[idx(c, k - 1)].affine_inverse() * bind_g[i]
			skel.set_bone_rest(bone, rest)
			skel.reset_bone_pose(bone)
			_bone[i] = bone
			_parent[i] = parent
			skin_out.add_named_bind(nm, bind_g[i].affine_inverse())
			skin_out.set_bind_bone(skin_out.get_bind_count() - 1, bone)
	# the drawn skirt: each loft vertex bilinear in its cell; the hem band and
	# the threads as the nearest hem vertex
	var new_b := PackedInt32Array()
	var new_w := PackedFloat32Array()
	new_b.resize(nv * 4)
	new_w.resize(nv * 4)
	var per := (LOFT_COLS - 1) / NC
	for v in loft:
		var ring := v / LOFT_COLS
		var col := v % LOFT_COLS
		var k := 0
		while k < NR - 2 and loft_ring(k + 1) <= ring:
			k += 1
		var s := float(ring - loft_ring(k)) / float(loft_ring(k + 1) - loft_ring(k))
		var c := col / per
		var u := float(col % per) / float(per)
		var bw := [[idx(c, k), (1.0 - u) * (1.0 - s)], [idx(c + 1, k), u * (1.0 - s)],
			[idx(c, k + 1), (1.0 - u) * s], [idx(c + 1, k + 1), u * s]]
		for j in 4:
			new_b[v * 4 + j] = int(bw[j][0])
			new_w[v * 4 + j] = float(bw[j][1])
	# The hem's inner band (two rings of the loft's columns) each as its hem
	# vertex; each thread face (THREAD_VERTS in a row) all as the hem vertex
	# nearest its root, so a thread moves as one piece and never tears.
	var hem0 := (LOFT_RINGS - 1) * LOFT_COLS
	var band := loft + 2 * LOFT_COLS
	for v in range(loft, nv):
		var best := hem0 + (v - loft) % LOFT_COLS
		if v >= band:
			var root := band + (v - band) / THREAD_VERTS * THREAD_VERTS
			var best_d := INF
			for h in LOFT_COLS - 1:
				var d := verts[root].distance_squared_to(verts[hem0 + h])
				if d < best_d:
					best_d = d
					best = hem0 + h
		for j in 4:
			new_b[v * 4 + j] = new_b[best * 4 + j]
			new_w[v * 4 + j] = new_w[best * 4 + j]
	arrays[Mesh.ARRAY_BONES] = new_b
	arrays[Mesh.ARRAY_WEIGHTS] = new_w
	var mesh := ArrayMesh.new()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
	var mat := mi.mesh.surface_get_material(0)
	if mat != null:
		mesh.surface_set_material(0, mat)
	mi.mesh = mesh
	mi.skin = skin_out
	_pos.resize(_n)
	_prev.resize(_n)
	_target.resize(_n)
	_leg_at.resize(6)
	_leg_was.resize(6)
	_local.resize(_n)
	cost_us.resize(COST_KEEP)
	ok = true
	return true


func _old_targets(skel_xf: Transform3D) -> void:
	for b in _old_bones.size():
		var bone := _old_bones[b]
		_old_m[b] = skel_xf * skel.get_bone_global_pose(bone) * _old_poses[b] if bone >= 0 else skel_xf
	for i in _n:
		var o := Vector3.ZERO
		var p := _bind[i]
		for j in 4:
			var w := _old_w[i * 4 + j]
			if w > 0.0:
				o += (_old_m[_old_bind[i * 4 + j]] * p) * w
		_target[i] = o


static func _limit(pos: PackedVector3Array, a: int, b: int, lo: float, hi: float, wa: float) -> void:
	var d: Vector3 = pos[b] - pos[a]
	var l := d.length()
	if l < 1.0e-9:
		return
	var want := clampf(l, lo, hi)
	if want == l:
		return
	var corr := d * ((l - want) / l)
	pos[a] += corr * wa
	pos[b] -= corr * (1.0 - wa)


## Push q out of the tapered capsule a..b (radius ra at a, rb at b). The
## skirt is one tube round both legs, so along the leg q leaves outward from
## the tube: along `out` (level, from her pelvis's axis through q) to where
## that ray leaves the capsule. Where `out` runs along the leg, toward the
## side `was` lay on against the same capsule last frame (a0..b0). Past the
## capsule's ends, straight out of the end's sphere.
static func _capsule_out(q: Vector3, out: Vector3, was: Vector3, a: Vector3, b: Vector3, a0: Vector3,
		b0: Vector3, ra: float, rb: float) -> Vector3:
	var ab := b - a
	var len2 := ab.length_squared()
	if len2 < 1.0e-9:
		return q
	var u := (q - a).dot(ab) / len2
	var t := clampf(u, 0.0, 1.0)
	var on := a + ab * t
	var r := lerpf(ra, rb, t) + CLEAR_M
	var d := q - on
	var l2 := d.length_squared()
	if l2 >= r * r:
		return q
	if u != t:
		return on + d * (r / sqrt(l2)) if l2 > 1.0e-12 else q
	var e := ab / sqrt(len2)
	var m := out - e * out.dot(e)
	var mm := m.length_squared()
	if mm > 0.25:
		# |d + s m| = r for s >= 0 (d is already across the axis)
		var dm := d.dot(m)
		var s := (-dm + sqrt(maxf(dm * dm - mm * (l2 - r * r), 0.0))) / mm
		return q + out * s
	var side := was - (a0 + (b0 - a0) * t)
	side -= e * side.dot(e)
	if side.length_squared() > 1.0e-10:
		return on + side.normalized() * r
	return on + d * (r / sqrt(l2)) if l2 > 1.0e-12 else q


func step(delta: float, fresh: bool) -> void:
	if not ok:
		return
	var t0 := Time.get_ticks_usec()
	var dt := clampf(delta, 1.0 / 240.0, 1.0 / 30.0)
	var f := dt * 60.0
	var damp := pow(DAMP, f)
	var skel_xf := skel.global_transform
	var body := skel_xf * skel.get_bone_global_pose(_body)
	if OLD_SKIN_SHARE > 0.0:
		_old_targets(skel_xf)
		for i in _n:
			_target[i] = (body * _pin_local[i]).lerp(_target[i], OLD_SKIN_SHARE)
	else:
		for i in _n:
			_target[i] = body * _pin_local[i]
	var leg_at := _leg_at
	var leg_was := _leg_was
	for j in 6:
		leg_at[j] = (skel_xf * skel.get_bone_global_pose(_legs[j])).origin
	var restart := fresh or _fresh or _pos[0].distance_to(body * _pin_local[0]) > JUMP_M
	if restart:
		for j in 6:
			leg_was[j] = leg_at[j]
	_fresh = false
	var g := GRAVITY * dt * dt
	for c in NC:
		for k in NR:
			var i := idx(c, k)
			if k == 0:
				var pin := body * _pin_local[i]
				_pos[i] = pin
				_prev[i] = pin
				continue
			if restart:
				_pos[i] = _target[i]
				_prev[i] = _target[i]
				continue
			var cur := _pos[i]
			var nxt := cur + (cur - _prev[i]) * damp + g
			_prev[i] = cur
			var pull := 1.0 - pow(1.0 - float(PULL[k]), f)
			_pos[i] = nxt.lerp(_target[i], pull)
	var iters := RESET_ITERATIONS if restart else ITERATIONS
	var axis := body.basis.y.normalized()
	# Each iteration: out of her legs, then the cloth's lengths, so the
	# cloth's lengths have the last word (a leg pressed deeper than the skirt
	# can wrap leaves it a little in, never torn).
	for _it in iters:
		for i in _n:
			if i % NR == 0:
				continue
			var q := _pos[i]
			var was := _prev[i]
			var out := q - body.origin
			out = (out - axis * out.dot(axis)).normalized()
			q = _capsule_out(q, out, was, leg_at[0], leg_at[1], leg_was[0], leg_was[1], THIGH_R.x, THIGH_R.y)
			q = _capsule_out(q, out, was, leg_at[3], leg_at[4], leg_was[3], leg_was[4], THIGH_R.x, THIGH_R.y)
			q = _capsule_out(q, out, was, leg_at[1], leg_at[2], leg_was[1], leg_was[2], SHIN_R.x, SHIN_R.y)
			q = _capsule_out(q, out, was, leg_at[4], leg_at[5], leg_was[4], leg_was[5], SHIN_R.x, SHIN_R.y)
			_pos[i] = q
		for c in NC:
			for k in range(1, NR):
				var i := idx(c, k)
				var up := idx(c, k - 1)
				var right := idx(c + 1, k)
				var rc := _rest_col[i]
				_limit(_pos, up, i, rc * COL_MIN, rc, 0.0 if k == 1 else 0.5)
				var rr := _rest_ring[i]
				_limit(_pos, i, right, rr * RING_MIN, rr * RING_MAX, 0.5)
				var rd := _rest_diag[i]
				_limit(_pos, idx(c + 1, k - 1), i, rd * RING_MIN, rd * RING_MAX, 0.0 if k == 1 else 0.5)
	# Last, each column down from her waist: never longer than its rest, so
	# no drawn column edge and no skirt bone ever stretches.
	for c in NC:
		for k in range(1, NR):
			var i := idx(c, k)
			var above := _pos[i - 1]
			var d := _pos[i] - above
			var l := d.length()
			if l > _rest_col[i]:
				_pos[i] = above + d * (_rest_col[i] / l)
	# the bones
	var inv := skel_xf.affine_inverse()
	var local_pts := _local
	for i in _n:
		local_pts[i] = inv * _pos[i]
	for j in 6:
		leg_was[j] = leg_at[j]
	var body_g := skel.get_bone_global_pose(_body)
	var turn := (body_g.basis.orthonormalized() * _body_bind_inv).get_rotation_quaternion()
	for c in NC:
		for k in NR:
			var i := idx(c, k)
			var rot := turn
			if SWING_SHARE > 0.0:
				var now := local_pts[idx(c, mini(k + 1, NR - 1))] - local_pts[idx(c, maxi(k - 1, 0))]
				var was_dir := turn * _bind_dir[i]
				if now.length_squared() > 1.0e-10 and was_dir.dot(now.normalized()) > -0.99:
					rot = Quaternion.IDENTITY.slerp(Quaternion(was_dir, now.normalized()), SWING_SHARE) * turn
			_g[i] = Transform3D(Basis(rot), local_pts[i])
			var parent_g := body_g if k == 0 else _g[i - 1]
			var local := parent_g.affine_inverse() * _g[i]
			skel.set_bone_pose_position(_bone[i], local.origin)
			skel.set_bone_pose_rotation(_bone[i], local.basis.get_rotation_quaternion())
	cost_us[cost_n % COST_KEEP] = int(Time.get_ticks_usec() - t0)
	cost_n += 1


## step()'s cost over the frames kept: [frames, mean, p95, p99, max] in microseconds.
func cost_stats() -> Array:
	var n := mini(cost_n, COST_KEEP)
	if n == 0:
		return [0, 0, 0, 0, 0]
	var v := cost_us.slice(0, n)
	v.sort()
	var total := 0
	for x in v:
		total += x
	return [cost_n, total / n, v[mini(n - 1, int(n * 0.95))], v[mini(n - 1, int(n * 0.99))], v[n - 1]]

