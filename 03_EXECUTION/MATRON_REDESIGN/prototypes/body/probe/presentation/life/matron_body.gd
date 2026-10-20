extends RefCounted
## Smooth, hand-sculpted forms for the Matron, built from code: lofted tubes
## for limbs, torso and dress (a ring of an ellipse per step along a spine,
## every ring its own size and offset), a head sculpted by pushing a sphere in
## and out with soft blobs (brow, hollow eyes, cheek hollows, a hooked nose, a
## pointed chin), and tapering ribbons for hair and rags. No files: the same
## numbers give the same shape on every run.

const SEGMENTS := 14


# A tube along +y. `rings` is an array of [y, rx, rz, ox, oz, fold]: the
# ellipse's half-widths along x and z, its centre offset, and how much the
# ring's radius ripples round it (cloth). Both ends should taper to nearly
# nothing so the tube is closed and rounded.
static func tube(rings: Array, segments := SEGMENTS, fold_freq := 7.0) -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var count := rings.size()
	for i in count:
		var ring: Array = rings[i]
		var fold := float(ring[5]) if ring.size() > 5 else 0.0
		for j in segments:
			var a := TAU * float(j) / float(segments)
			var ripple := 1.0 + fold * sin(a * fold_freq + float(i) * 1.7)
			st.set_smooth_group(0)
			st.add_vertex(Vector3(float(ring[3]) + float(ring[1]) * cos(a) * ripple, float(ring[0]),
				float(ring[4]) + float(ring[2]) * sin(a) * ripple))
	# Front faces wind clockwise seen from outside; which way that is depends on
	# whether the rings climb or descend.
	var descending := float((rings[count - 1] as Array)[0]) < float((rings[0] as Array)[0])
	for i in count - 1:
		for j in segments:
			var j2 := (j + 1) % segments
			var a0 := i * segments + j
			var a1 := i * segments + j2
			var b0 := (i + 1) * segments + j
			var b1 := (i + 1) * segments + j2
			var order := [a0, a1, b0, a1, b1, b0]
			if descending:
				order = [a0, b0, a1, a1, b0, b1]
			for idx in order:
				st.add_index(idx)
	st.generate_normals()
	return st.commit()


# A limb: a smooth taper from `r0` at y=0 down to `r1` at y=-length, with an
# optional swell (a calf, a muscle) at `swell_at` (0..1) of `swell` extra radius.
static func limb(length: float, r0: float, r1: float, swell := 0.0, swell_at := 0.3, flat := 1.0) -> ArrayMesh:
	var rings := []
	var steps := 9
	rings.append([0.012, r0 * 0.25, r0 * 0.25 * flat, 0.0, 0.0, 0.0])
	for k in steps + 1:
		var u := float(k) / float(steps)
		var r := lerpf(r0, r1, smoothstep(0.0, 1.0, u))
		r += swell * exp(-pow((u - swell_at) / 0.22, 2.0))
		rings.append([-u * length, r, r * flat, 0.0, 0.0, 0.0])
	rings.append([-length - 0.012, r1 * 0.3, r1 * 0.3 * flat, 0.0, 0.0, 0.0])
	return tube(rings, 10)


# A finger segment: a thin taper downward with a rounded tip.
static func finger(length: float, r0: float, r1: float) -> ArrayMesh:
	var rings := [[0.004, r0 * 0.5, r0 * 0.5, 0.0, 0.0, 0.0], [0.0, r0, r0 * 0.85, 0.0, 0.0, 0.0],
		[-length * 0.5, (r0 + r1) * 0.5, (r0 + r1) * 0.45, 0.0, 0.0, 0.0],
		[-length, r1, r1 * 0.85, 0.0, 0.0, 0.0], [-length - r1 * 0.9, r1 * 0.3, r1 * 0.25, 0.0, 0.0, 0.0]]
	return tube(rings, 7)


# An ellipsoid (a ball, a lump).
static func blob(rx: float, ry: float, rz: float, segments := 12) -> ArrayMesh:
	var rings := []
	var steps := 8
	for k in steps + 1:
		var u := float(k) / float(steps)
		var phi := PI * u
		rings.append([ry * cos(phi), maxf(rx * sin(phi), 0.0005), maxf(rz * sin(phi), 0.0005), 0.0, 0.0, 0.0])
	return tube(rings, segments)


# The torso, waist (y=0) to the base of the neck: a narrow ribcage, a hunched
# back that falls away behind (oz < 0) and rounded shoulders.
static func torso() -> ArrayMesh:
	var rings := [
		[-0.02, 0.05, 0.04, 0.0, 0.0, 0.0],
		[0.0, 0.12, 0.085, 0.0, 0.0, 0.0],
		[0.1, 0.108, 0.078, 0.0, -0.004, 0.0],
		[0.22, 0.125, 0.085, 0.0, -0.014, 0.0],
		[0.36, 0.152, 0.095, 0.0, -0.034, 0.0],
		[0.48, 0.17, 0.097, 0.0, -0.06, 0.0],
		[0.57, 0.178, 0.09, 0.0, -0.075, 0.0],
		[0.63, 0.11, 0.07, 0.0, -0.082, 0.0],
		[0.66, 0.045, 0.045, 0.0, -0.07, 0.0],
	]
	return tube(rings, 16)


# The dress above the waist: the same form a little larger, sleeveless, with
# the ripple of old cloth.
static func bodice() -> ArrayMesh:
	var rings := [
		[-0.03, 0.14, 0.1, 0.0, 0.0, 0.02],
		[0.1, 0.12, 0.09, 0.0, -0.004, 0.025],
		[0.22, 0.138, 0.098, 0.0, -0.014, 0.03],
		[0.36, 0.165, 0.108, 0.0, -0.034, 0.03],
		[0.48, 0.183, 0.11, 0.0, -0.06, 0.025],
		[0.55, 0.186, 0.104, 0.0, -0.072, 0.02],
		[0.575, 0.15, 0.09, 0.0, -0.076, 0.0],
	]
	return tube(rings, 18, 5.0)


# The skirt, waist to a ragged hem well below the knee.
static func skirt() -> ArrayMesh:
	var rings := [
		[0.04, 0.13, 0.1, 0.0, 0.0, 0.0],
		[0.0, 0.145, 0.115, 0.0, 0.0, 0.03],
		[-0.15, 0.185, 0.15, 0.0, 0.0, 0.07],
		[-0.32, 0.225, 0.19, 0.0, 0.0, 0.09],
		[-0.48, 0.255, 0.215, 0.0, 0.0, 0.1],
		[-0.6, 0.27, 0.23, 0.0, 0.0, 0.11],
	]
	return tube(rings, 22, 8.0)


# The shawl over the shoulders: a wide, drooping cape of heavy wool, open at the
# neck, uneven at the hem.
static func shawl() -> ArrayMesh:
	var rings := [
		[0.0, 0.09, 0.07, 0.0, 0.0, 0.0],
		[-0.02, 0.2, 0.125, 0.0, -0.01, 0.02],
		[-0.08, 0.26, 0.15, 0.0, -0.02, 0.04],
		[-0.22, 0.3, 0.17, 0.0, -0.04, 0.07],
		[-0.38, 0.3, 0.175, 0.0, -0.06, 0.1],
	]
	return tube(rings, 20, 6.0)


# A tapering ribbon of cloth or hair from y=0 downward: `length` long, `w0` wide
# at the top and `w1` at the tip, curving back by `curl`. Double-sided material.
static func ribbon(length: float, w0: float, w1: float, curl := 0.0, steps := 5) -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	for k in steps + 1:
		var u := float(k) / float(steps)
		var w := lerpf(w0, w1, u) * 0.5
		var y := -u * length
		var z := curl * u * u * length
		st.set_smooth_group(0)
		st.add_vertex(Vector3(-w, y, z))
		st.set_smooth_group(0)
		st.add_vertex(Vector3(w, y, z))
	for k in steps:
		var a := k * 2
		st.add_index(a)
		st.add_index(a + 2)
		st.add_index(a + 1)
		st.add_index(a + 1)
		st.add_index(a + 2)
		st.add_index(a + 3)
	st.generate_normals()
	return st.commit()


# --- the head ---------------------------------------------------------------

# Soft blobs: [centre, sigma (per axis), amplitude]: positive pushes the skin out.
const FEATURES := [
	[Vector3(0.0, 0.03, 0.078), Vector3(0.055, 0.012, 0.03), 0.013],       # brow ridge
	[Vector3(-0.032, 0.022, 0.08), Vector3(0.016, 0.014, 0.02), -0.019],   # eye sockets
	[Vector3(0.032, 0.022, 0.08), Vector3(0.016, 0.014, 0.02), -0.019],
	[Vector3(-0.047, -0.028, 0.066), Vector3(0.022, 0.026, 0.03), -0.015], # hollow cheeks
	[Vector3(0.047, -0.028, 0.066), Vector3(0.022, 0.026, 0.03), -0.015],
	[Vector3(-0.052, -0.002, 0.058), Vector3(0.014, 0.012, 0.02), 0.008],  # cheekbones
	[Vector3(0.052, -0.002, 0.058), Vector3(0.014, 0.012, 0.02), 0.008],
	[Vector3(0.0, 0.008, 0.092), Vector3(0.007, 0.022, 0.016), 0.012],     # nose bridge
	[Vector3(0.0, -0.02, 0.1), Vector3(0.012, 0.011, 0.012), 0.018],       # hooked tip
	[Vector3(0.0, -0.049, 0.075), Vector3(0.032, 0.004, 0.02), -0.009],    # mouth groove
	[Vector3(0.0, -0.041, 0.078), Vector3(0.022, 0.006, 0.016), 0.004],    # upper lip
	[Vector3(0.0, -0.088, 0.058), Vector3(0.017, 0.016, 0.02), 0.013],     # pointed chin
	[Vector3(-0.07, 0.03, 0.03), Vector3(0.02, 0.025, 0.04), -0.009],      # temples
	[Vector3(0.07, 0.03, 0.03), Vector3(0.02, 0.025, 0.04), -0.009],
	[Vector3(0.0, 0.0, -0.085), Vector3(0.04, 0.05, 0.02), 0.006],         # back of the skull
]
const HEAD_RADII := Vector3(0.071, 0.108, 0.088)
const JAW_SPLIT_Y := -0.045


static func _head_point(dir: Vector3) -> Vector3:
	var base := Vector3(dir.x * HEAD_RADII.x, dir.y * HEAD_RADII.y, dir.z * HEAD_RADII.z)
	var push := 0.0
	for f in FEATURES:
		var d: Vector3 = base - (f[0] as Vector3)
		var s: Vector3 = f[1]
		push += float(f[2]) * exp(-0.5 * (d.x * d.x / (s.x * s.x) + d.y * d.y / (s.y * s.y) + d.z * d.z / (s.z * s.z)))
	return base + dir * push


# The skull and the lower jaw, as two meshes from one sculpted sphere: the jaw
# is the lower front, so opening it shows the dark inside.
static func head() -> Dictionary:
	var lat := 26
	var lon := 32
	var points := []
	for i in lat + 1:
		var phi := PI * float(i) / float(lat)
		for j in lon:
			var theta := TAU * float(j) / float(lon)
			var dir := Vector3(sin(phi) * sin(theta), cos(phi), sin(phi) * cos(theta))
			points.append(_head_point(dir))
	var skull := SurfaceTool.new()
	var jaw := SurfaceTool.new()
	skull.begin(Mesh.PRIMITIVE_TRIANGLES)
	jaw.begin(Mesh.PRIMITIVE_TRIANGLES)
	for p in points:
		skull.set_smooth_group(0)
		skull.add_vertex(p)
		jaw.set_smooth_group(0)
		jaw.add_vertex(p)
	for i in lat:
		for j in lon:
			var j2 := (j + 1) % lon
			var a: int = i * lon + j
			var b: int = i * lon + j2
			var c: int = (i + 1) * lon + j
			var d: int = (i + 1) * lon + j2
			var tris := [[a, b, c], [b, d, c]]
			for t in tris:
				var in_jaw := true
				for idx in t:
					var p: Vector3 = points[idx]
					in_jaw = in_jaw and p.y < JAW_SPLIT_Y and p.z > -0.01
				var target := jaw if in_jaw else skull
				for idx in t:
					target.add_index(idx)
	skull.generate_normals()
	jaw.generate_normals()
	return {"skull": skull.commit(), "jaw": jaw.commit()}
