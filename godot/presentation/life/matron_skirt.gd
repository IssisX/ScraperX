extends RefCounted
## Her skirt as cloth (J-011, #118; the ruling at #87): a coarse proxy of
## NC columns by NR rings of verlet points, its top ring pinned at her waist,
## held by distance limits along its columns and round its rings and by a
## shear limit in each cell, pushed out of her thighs and shins (tapered
## capsules), and pulled softly toward its cut carried by her pelvis (below
## her hips turned partly toward the world's down while she is on her feet),
## so it keeps its line while it swings and hangs. Each point is a bone added to her
## skeleton (skirt_cCC_kK, each column a chain from her pelvis), and the drawn
## skirt is skinned to those bones, bilinearly inside each cell, never to her
## thigh bones: linear skinning on the thighs tore it into strips (8,183 of
## its 16,812 edges past 1.10x their rest length).
##
## The skirt is cut full (tools/body/build_matron_body.py, Cory 11-10): in a
## lunge or the locker crouch her legs span more than a skirt cut to her hips
## has, so it flares from the waist and its pleats lie on the cloth's columns.
##
## Every skirt bone turns with her pelvis only, and every drawn vertex of the
## loft is put on its cell's bilinear surface when she is set up (the panels
## between the cloth's columns are flat, its pleats and torn hem kept), so the
## drawn skirt is exactly the proxy interpolated: no drawn edge stretches more
## than its cell does. A cell's edges are held under their rest, and its
## corners never open or close more than SHEAR_DEG past their cut, which is
## what keeps the drawn diagonals in it. Measured with the probe
## (tools/matron_probe/edge_stretch.py): no drawn skirt edge past 1.10x its
## rest on touch_matron_heave, touch_matron_act and the look sets.
##
## Presentation only: nothing here reaches the native, her physics or her
## force budget. Cheap: fixed sizes and sweep counts, every array allocated
## once in setup(), the legs tested only where a point is near one.

const NC := 16                  # columns round her (the builder's SKIRT_PANELS)
const NR := 8                   # rings, waist (pinned) to hem
const LOFT_RINGS := 40          # the skirt mesh's loft (tools/body/build_matron_body.py)
const LOFT_COLS := 129          # 128 round her and the seam's copy of the first
const THREAD_VERTS := 10        # each face of a thread hanging from the hem
const SWEEPS := 3               # passes over the cloth's limits each frame
const RESET_SWEEPS := 24        # the frame it starts again from its cut
const DAMP := 0.9               # verlet velocity kept per 1/60 s
const GRAVITY := Vector3(0.0, -9.81, 0.0)
# Pull per 1/60 s toward its cut on her pelvis, by ring: firm on her hips,
# loose at the hem, so it hangs and swings below and keeps its line above.
const PULL := [1.0, 0.44, 0.24, 0.15, 0.096, 0.069, 0.053, 0.045]
# A ring edge or cell diagonal is held to this share of its rest, so what
# SWEEPS passes leave of a hard frame still stays under its rest.
const SPAN_MAX := 0.9
const RING_MIN := 0.35          # a ring edge shortens to this share at most (folds)
const COL_MIN := 0.6            # a column edge shortens to this share at most
const SHEAR_DEG := 10.0         # a cell's corner opens or closes this far past its cut at most
const COLLIDE_FROM := 3         # rings above this ride her hips, out of her legs' reach
const PUSH_MAX_M := 0.03        # her legs move a point this far a frame at most
const CLEAR_M := 0.018          # cloth kept this far off her legs
const JUMP_M := 0.25            # her waist moved this far in a frame: a snap, start again
const HANG := 0.6               # share of the turn to the world's down at the hem
const HANG_FROM := 2            # the ring the hanging cut turns about (her hips)
const HANG_STAND_M := 0.2       # her hips this far over her lowest knee or ankle: no hang yet,
const HANG_RAMP_M := 0.3        # and the full share this much higher
const CARRY := 0.7              # share of her pelvis's move each frame the cloth is carried by
# Her thighs and shins as tapered capsules (radius at the upper and lower
# joint), measured on her drawn stockings in the bind pose: thigh 0.124 m
# at her hip to 0.083..0.096 by the knee, shin 0.071..0.078 to 0.046..0.053.
const THIGH_R := Vector2(0.126, 0.092)
const SHIN_R := Vector2(0.08, 0.055)

var ok := false
var skel: Skeleton3D
var _n := NC * NR
var _bone := PackedInt32Array()         # particle -> skeleton bone
var _pos := PackedVector3Array()        # world
var _prev := PackedVector3Array()
var _pin_local := PackedVector3Array()  # each point's cut in her pelvis bone's frame
var _rest_ring := PackedFloat32Array()  # (c, k) -> (c + 1, k)
var _rest_col := PackedFloat32Array()   # (c, k - 1) -> (c, k)
var _rest_d1 := PackedFloat32Array()    # (c + 1, k - 1) -> (c, k)
var _rest_d2 := PackedFloat32Array()    # (c, k - 1) -> (c + 1, k)
# each cell's corner angles as cut, past which it may open (at its corners
# (c, k - 1) and (c + 1, k)) or close (at (c + 1, k - 1) and (c, k)), as cosines
var _open_a := PackedFloat32Array()
var _open_d := PackedFloat32Array()
var _close_b := PackedFloat32Array()
var _close_c := PackedFloat32Array()
var _pull_k := PackedFloat32Array()
var _hang_k: Array[Basis] = []
var _body_bind_inv := Basis.IDENTITY
var _body := -1
var _legs := PackedInt32Array()         # hp_l, kn_l, an_l, hp_r, kn_r, an_r
var _fresh := true
var _body_was := Transform3D.IDENTITY
var _leg_at := PackedVector3Array()
var _leg_was := PackedVector3Array()    # last frame's
# step()'s cost: the last COST_KEEP frames, microseconds (a ring)
const COST_KEEP := 4096
var cost_us := PackedInt32Array()
var cost_n := 0


static func idx(c: int, k: int) -> int:
	return ((c + NC) % NC) * NR + k


static func loft_ring(k: int) -> int:
	return int(round(float(k) * float(LOFT_RINGS - 1) / float(NR - 1)))


## The point on cell row k's column line at u (0..1 across columns c, c + 1)
## nearest p: [row, s] with s in 0..1, moved up or down a row while p lies past
## the row's ends (a column torn shorter puts its rings higher).
static func _on_column(b: PackedVector3Array, c: int, u: float, k0: int, p: Vector3) -> Array:
	var k := k0
	var s := 0.0
	for _try in NR:
		var top := b[idx(c, k)].lerp(b[idx(c + 1, k)], u)
		var bot := b[idx(c, k + 1)].lerp(b[idx(c + 1, k + 1)], u)
		var d := bot - top
		s = (p - top).dot(d) / maxf(d.length_squared(), 1.0e-12)
		if s < 0.0 and k > 0:
			k -= 1
		elif s > 1.0 and k < NR - 2:
			k += 1
		else:
			break
	return [k, clampf(s, 0.0, 1.0)]


## Four bilinear weights quantized as the mesh stores them (16 bits, rounded
## down), summing exactly to one: the renderer and the probe skin far from the
## world origin, where weights short of one by 2e-5 move a vertex millimetres.
static func _weights(w: Array) -> PackedFloat32Array:
	var q := PackedInt32Array([0, 0, 0, 0])
	var total := 0
	var big := 0
	for j in 4:
		q[j] = int(floor(float(w[j]) * 65535.0 + 0.5))
		total += q[j]
		if q[j] > q[big]:
			big = j
	q[big] += 65535 - total
	var out := PackedFloat32Array([0.0, 0.0, 0.0, 0.0])
	for j in 4:
		out[j] = (float(q[j]) + 0.5) / 65535.0 if q[j] > 0 else 0.0
	return out


static func _cos_at(o: Vector3, p: Vector3, q: Vector3, turn: float) -> float:
	var a := clampf((p - o).normalized().dot((q - o).normalized()), -1.0, 1.0)
	return cos(clampf(acos(a) + turn, 0.0, PI))


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
	var nv := verts.size()
	var loft := LOFT_RINGS * LOFT_COLS
	var per := (LOFT_COLS - 1) / NC
	if nv < loft + 2 * LOFT_COLS or (nv - loft - 2 * LOFT_COLS) % THREAD_VERTS != 0 or verts[0].distance_to(verts[LOFT_COLS - 1]) > 1.0e-5 or old_b.size() < nv or per * NC != LOFT_COLS - 1:
		push_error("matron skirt: the skirt mesh is not the loft (%d verts)" % nv)
		return false
	var skin := mi.skin
	_body = skel.find_bone("body")
	for nm in ["hp_l", "kn_l", "an_l", "hp_r", "kn_r", "an_r"]:
		_legs.append(skel.find_bone(nm))
	if _body < 0 or _legs.has(-1):
		push_error("matron skirt: her skeleton has no pelvis or legs")
		return false
	var body_bind := Transform3D.IDENTITY
	for b in skin.get_bind_count():
		var bone := skin.get_bind_bone(b)
		if bone < 0:
			bone = skel.find_bone(skin.get_bind_name(b))
		if bone == _body:
			body_bind = skin.get_bind_pose(b).affine_inverse()
	# the cloth's points: the loft's vertices at its rings and columns
	var bind := PackedVector3Array()
	bind.resize(_n)
	_pin_local.resize(_n)
	for c in NC:
		for k in NR:
			var i := idx(c, k)
			bind[i] = verts[loft_ring(k) * LOFT_COLS + c * per]
			_pin_local[i] = body_bind.affine_inverse() * bind[i]
	_rest_ring.resize(_n)
	_rest_col.resize(_n)
	_rest_d1.resize(_n)
	_rest_d2.resize(_n)
	_open_a.resize(_n)
	_open_d.resize(_n)
	_close_b.resize(_n)
	_close_c.resize(_n)
	var shear := deg_to_rad(SHEAR_DEG)
	for c in NC:
		for k in NR:
			var i := idx(c, k)
			_rest_ring[i] = bind[i].distance_to(bind[idx(c + 1, k)])
			if k == 0:
				continue
			var a := bind[idx(c, k - 1)]
			var b := bind[idx(c + 1, k - 1)]
			var d := bind[idx(c + 1, k)]
			_rest_col[i] = bind[i].distance_to(a)
			_rest_d1[i] = b.distance_to(bind[i])
			_rest_d2[i] = a.distance_to(d)
			_open_a[i] = _cos_at(a, b, bind[i], shear)
			_open_d[i] = _cos_at(d, bind[i], b, shear)
			_close_b[i] = _cos_at(b, a, d, -shear)
			_close_c[i] = _cos_at(bind[i], a, d, -shear)
	# the bones: each column a chain from her pelvis, at rest unturned
	_body_bind_inv = body_bind.basis.orthonormalized().inverse()
	_bone.resize(_n)
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
			var at := Transform3D(Basis.IDENTITY, bind[i])
			var rest := body_bind.affine_inverse() * at if k == 0 else Transform3D(Basis.IDENTITY, bind[i] - bind[i - 1])
			skel.set_bone_rest(bone, rest)
			skel.reset_bone_pose(bone)
			_bone[i] = bone
			skin_out.add_named_bind(nm, at.affine_inverse())
			skin_out.set_bind_bone(skin_out.get_bind_count() - 1, bone)
	# The drawn loft: each vertex on its cell's bilinear surface (its column
	# fraction across the cell, its own height down it), so it is exactly the
	# cloth interpolated. Each thread face (THREAD_VERTS in a row) moves as the
	# hem vertex nearest its root, as one piece, so it never tears.
	var new_b := PackedInt32Array()
	var new_w := PackedFloat32Array()
	new_b.resize(nv * 4)
	new_w.resize(nv * 4)
	var moved := verts.duplicate()
	for v in loft + LOFT_COLS:
		var ring := v / LOFT_COLS if v < loft else LOFT_RINGS - 1
		var col := v % LOFT_COLS
		var c := mini(col / per, NC - 1)
		var u := float(col - c * per) / float(per)
		var k := 0
		while k < NR - 2 and loft_ring(k + 1) <= ring:
			k += 1
		var on := _on_column(bind, c, u, k, verts[v])
		k = on[0]
		var s: float = on[1]
		var top := bind[idx(c, k)].lerp(bind[idx(c + 1, k)], u)
		var bot := bind[idx(c, k + 1)].lerp(bind[idx(c + 1, k + 1)], u)
		moved[v] = top.lerp(bot, s)
		var w := _weights([(1.0 - u) * (1.0 - s), u * (1.0 - s), (1.0 - u) * s, u * s])
		var bones := [idx(c, k), idx(c + 1, k), idx(c, k + 1), idx(c + 1, k + 1)]
		for j in 4:
			new_b[v * 4 + j] = int(bones[j])
			new_w[v * 4 + j] = w[j]
	# The hem's inner band (a strip 2 cm up inside the hem) folded flat onto
	# the hem: any step off the cloth is held rigid and stretches its short
	# edges as the cloth turns, and the cloth is drawn double-sided anyway.
	for v in range(loft + LOFT_COLS, loft + 2 * LOFT_COLS):
		moved[v] = moved[v - LOFT_COLS]
		for j in 4:
			new_b[v * 4 + j] = new_b[(v - LOFT_COLS) * 4 + j]
			new_w[v * 4 + j] = new_w[(v - LOFT_COLS) * 4 + j]
	var hem0 := (LOFT_RINGS - 1) * LOFT_COLS
	var band := loft + 2 * LOFT_COLS
	for root in range(band, nv, THREAD_VERTS):
		var best := hem0
		var best_d := INF
		for h in LOFT_COLS - 1:
			var d := verts[root].distance_squared_to(verts[hem0 + h])
			if d < best_d:
				best_d = d
				best = hem0 + h
		var shift := moved[best] - verts[best]
		for v in range(root, root + THREAD_VERTS):
			moved[v] = verts[v] + shift
			for j in 4:
				new_b[v * 4 + j] = new_b[best * 4 + j]
				new_w[v * 4 + j] = new_w[best * 4 + j]
	arrays[Mesh.ARRAY_VERTEX] = moved
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
	_leg_at.resize(6)
	_leg_was.resize(6)
	_pull_k.resize(NR)
	_hang_k.resize(NR)
	cost_us.resize(COST_KEEP)
	ok = true
	return true


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
	var body_g := skel.get_bone_global_pose(_body)
	var body := skel_xf * body_g
	var legs := _leg_at
	var legs_was := _leg_was
	for j in 6:
		legs[j] = (skel_xf * skel.get_bone_global_pose(_legs[j])).origin
	var pos := _pos
	var prev := _prev
	_pos = PackedVector3Array()
	_prev = PackedVector3Array()
	var restart := fresh or _fresh or pos[0].distance_to(body * _pin_local[0]) > JUMP_M
	if restart:
		for j in 6:
			legs_was[j] = legs[j]
	elif CARRY > 0.0:
		# a share of her pelvis's own move this frame carried to the cloth, so
		# a sprint or a fast turn swings it without flinging it
		var turn_q := Quaternion.IDENTITY.slerp((body.basis.orthonormalized() * _body_was.basis.orthonormalized().inverse()).get_rotation_quaternion(), CARRY)
		var carry := Basis(turn_q)
		var o0 := _body_was.origin
		var pivot := o0.lerp(body.origin, CARRY)
		for i in _n:
			pos[i] = carry * (pos[i] - o0) + pivot
			prev[i] = carry * (prev[i] - o0) + pivot
	_fresh = false
	_body_was = body
	for k in NR:
		_pull_k[k] = 1.0 - pow(1.0 - float(PULL[k]), f)
	var g := GRAVITY * dt * dt
	# Its cut hangs: below her hips (ring HANG_FROM) each ring's cut is turned
	# from her pelvis's down toward the world's, a growing share to the hem,
	# so bent over or on all fours it falls from her hips instead of standing
	# out behind her. Only on her feet (her knees and ankles all well under
	# her hips): lying down or legs up, it keeps its cut, off the deck.
	var low := minf(minf(legs[1].y, legs[2].y), minf(legs[4].y, legs[5].y))
	var standing := clampf((body.origin.y - low - HANG_STAND_M) / HANG_RAMP_M, 0.0, 1.0)
	var hang := Quaternion(-body.basis.y.normalized(), Vector3.DOWN)
	for k in NR:
		_hang_k[k] = Basis(Quaternion.IDENTITY.slerp(hang, standing * HANG * clampf(float(k - HANG_FROM) / float(NR - 1 - HANG_FROM), 0.0, 1.0)))
	var hip := Vector3.ZERO
	for i in _n:
		var pin := body * _pin_local[i]
		var k := i % NR
		if k == HANG_FROM:
			hip = pin
		elif k > HANG_FROM:
			pin = hip + _hang_k[k] * (pin - hip)
		if k == 0 or restart:
			pos[i] = pin
			prev[i] = pin
			continue
		var cur := pos[i]
		var nxt := cur + (cur - prev[i]) * damp + g
		prev[i] = cur
		pos[i] = nxt.lerp(pin, _pull_k[k])
	# Out of her legs once, then the cloth's limits, so the cloth has the last
	# word: a leg pressed deeper than the skirt can wrap stays a little in it
	# for a frame, never tearing it.
	var axis := body.basis.y.normalized()
	var box := AABB(legs[0], Vector3.ZERO)
	for j in range(1, 6):
		box = box.expand(legs[j])
	box = box.grow(THIGH_R.x + CLEAR_M)
	for i in _n:
		if i % NR < COLLIDE_FROM:
			continue
		var q := pos[i]
		if not box.has_point(q):
			continue
		var was := prev[i]
		var out := q - body.origin
		out = (out - axis * out.dot(axis)).normalized()
		q = _capsule_out(q, out, was, legs[0], legs[1], legs_was[0], legs_was[1], THIGH_R.x, THIGH_R.y)
		q = _capsule_out(q, out, was, legs[3], legs[4], legs_was[3], legs_was[4], THIGH_R.x, THIGH_R.y)
		q = _capsule_out(q, out, was, legs[1], legs[2], legs_was[1], legs_was[2], SHIN_R.x, SHIN_R.y)
		q = _capsule_out(q, out, was, legs[4], legs[5], legs_was[4], legs_was[5], SHIN_R.x, SHIN_R.y)
		var dq := q - pos[i]
		var lq := dq.length()
		pos[i] = pos[i] + dq * (PUSH_MAX_M / lq) if lq > PUSH_MAX_M else q
	# The limits, written out (a call each costs more than its sums here):
	# per cell, its column edge a..i, its ring edge i..d, then its corners,
	# opening at a and d (the diagonal b..i growing) and closing at b and i
	# (the diagonal a..d shrinking). The top ring is pinned (wa 0 on it).
	for _sw in (RESET_SWEEPS if restart else SWEEPS):
		for c in NC:
			var right := ((c + 1) % NC) * NR
			for k in range(1, NR):
				var i := c * NR + k
				var a := i - 1
				var b := right + k - 1
				var d := right + k
				var wa := 0.0 if k == 1 else 0.5
				var pa := pos[a]
				var pi := pos[i]
				var e := pi - pa
				var l := e.length()
				var rc := _rest_col[i]
				if l > rc or (l < rc * COL_MIN and l > 1.0e-9):
					e *= (l - clampf(l, rc * COL_MIN, rc)) / l
					pa += e * wa
					pi -= e * (1.0 - wa)
				var pd := pos[d]
				e = pd - pi
				l = e.length()
				var rr := _rest_ring[i]
				if l > rr * SPAN_MAX or (l < rr * RING_MIN and l > 1.0e-9):
					e *= (l - clampf(l, rr * RING_MIN, rr * SPAN_MAX)) / l
					pi += e * 0.5
					pd -= e * 0.5
				var pb := pos[b]
				var lab := pa.distance_to(pb)
				var lai := pa.distance_to(pi)
				var ldi := pd.distance_to(pi)
				var ldb := pd.distance_to(pb)
				var hi := minf(minf(sqrt(maxf(lab * lab + lai * lai - 2.0 * lab * lai * _open_a[i], 0.0)),
					sqrt(maxf(ldi * ldi + ldb * ldb - 2.0 * ldi * ldb * _open_d[i], 0.0))), _rest_d1[i] * SPAN_MAX)
				e = pi - pb
				l = e.length()
				if l > hi:
					e *= (l - hi) / l
					pb += e * wa
					pi -= e * (1.0 - wa)
				lab = pb.distance_to(pa)
				var lbd := pb.distance_to(pd)
				lai = pi.distance_to(pa)
				ldi = pi.distance_to(pd)
				var lo := maxf(sqrt(maxf(lab * lab + lbd * lbd - 2.0 * lab * lbd * _close_b[i], 0.0)),
					sqrt(maxf(lai * lai + ldi * ldi - 2.0 * lai * ldi * _close_c[i], 0.0)))
				var hi2 := _rest_d2[i] * SPAN_MAX
				e = pd - pa
				l = e.length()
				if l > hi2 or (l < lo and l > 1.0e-9):
					e *= (l - clampf(l, lo, hi2)) / l
					pa += e * wa
					pd -= e * (1.0 - wa)
				pos[a] = pa
				pos[b] = pb
				pos[i] = pi
				pos[d] = pd
	# Last, each column down from her waist: never longer than its rest, so
	# no drawn column edge and no skirt bone ever stretches.
	for i in _n:
		if i % NR == 0:
			continue
		var above := pos[i - 1]
		var dv := pos[i] - above
		var l := dv.length()
		if l > _rest_col[i]:
			pos[i] = above + dv * (_rest_col[i] / l)
	for j in 6:
		legs_was[j] = legs[j]
	# the bones: each turned as her pelvis, so each one's place is its step
	# down the column in its parent's frame (the top ring is her pelvis's own)
	var turn := (body_g.basis.orthonormalized() * _body_bind_inv).orthonormalized()
	var to_local := (skel_xf.basis * turn).inverse()
	for i in _n:
		if i % NR != 0:
			skel.set_bone_pose_position(_bone[i], to_local * (pos[i] - pos[i - 1]))
	_pos = pos
	_prev = prev
	_leg_at = legs
	_leg_was = legs_was
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
