extends Node3D

# J-096 (#414): the bending planks (src/sim/flex_plank.hpp) as the player
# sees them: one smooth board bent through its segments as the native lays
# them, never the segments as boxes. The board's centre line runs through
# its hinges, curved between them (a cubic Hermite through each joint with
# the two segments' mean direction as its tangent), and its section is swept
# along it. A snapped board draws what still holds as one bent piece and
# each loose segment as the straight piece of timber it is. Every place is
# the native's; nothing here moves a plank.

const SAMPLES_PER_SEGMENT := 6

var _native: Object
var _wood: StandardMaterial3D
var _meshes: Array[MeshInstance3D] = []


func setup(native: Object) -> void:
	name = "FlexPlankView"
	_native = native
	# A weathered softwood scaffold board, planed and grey-brown.
	_wood = StandardMaterial3D.new()
	_wood.albedo_color = Color("8a7458")
	_wood.roughness = 0.88
	_wood.cull_mode = BaseMaterial3D.CULL_DISABLED


func _process(_delta: float) -> void:
	if _native == null:
		return
	var planks: Array = _native.get_flex_planks()
	var used := 0
	for plank in planks:
		var segments: Array = plank["segments"]
		var length := float(plank["length"])
		var half_w := 0.5 * float(plank["width"])
		var half_h := 0.5 * float(plank["thickness"])
		var s := length / float(segments.size())
		var snapped := int(plank["snapped_joint"])
		# The pieces: [first segment, last segment] runs drawn as one board.
		var runs: Array = []
		if snapped < 0:
			runs.append([0, segments.size() - 1])
		else:
			if snapped > 0:
				runs.append([0, snapped - 1])
			for i in range(snapped, segments.size()):
				runs.append([i, i])
		for run in runs:
			var mesh := _board(segments, int(run[0]), int(run[1]), s, half_w, half_h)
			if used >= _meshes.size():
				var instance := MeshInstance3D.new()
				instance.material_override = _wood
				add_child(instance)
				_meshes.append(instance)
			_meshes[used].mesh = mesh
			_meshes[used].visible = true
			used += 1
	for i in range(used, _meshes.size()):
		_meshes[i].visible = false


# Segments first..last as one board: the centre line through their joints,
# each segment's own frame (x along it, y up through its face, z across).
func _board(segments: Array, first: int, last: int, s: float, half_w: float, half_h: float) -> ArrayMesh:
	var points: Array[Vector3] = []
	var tangents: Array[Vector3] = []
	var sides: Array[Vector3] = []
	for i in range(first, last + 1):
		var t: Transform3D = segments[i]
		points.append(t.origin - t.basis.x * (0.5 * s))
	var end: Transform3D = segments[last]
	points.append(end.origin + end.basis.x * (0.5 * s))
	for j in points.size():
		var before: Transform3D = segments[clampi(first + j - 1, first, last)]
		var after: Transform3D = segments[clampi(first + j, first, last)]
		tangents.append((before.basis.x + after.basis.x).normalized())
		sides.append((before.basis.z + after.basis.z).normalized())
	var tool := SurfaceTool.new()
	tool.begin(Mesh.PRIMITIVE_TRIANGLES)
	var rings: Array = []
	for j in points.size() - 1:
		var p0 := points[j]
		var p1 := points[j + 1]
		var m0 := tangents[j] * s
		var m1 := tangents[j + 1] * s
		var steps := SAMPLES_PER_SEGMENT if j < points.size() - 2 else SAMPLES_PER_SEGMENT + 1
		for k in steps:
			var u := float(k) / float(SAMPLES_PER_SEGMENT)
			var u2 := u * u
			var u3 := u2 * u
			var at := (2.0 * u3 - 3.0 * u2 + 1.0) * p0 + (u3 - 2.0 * u2 + u) * m0 \
				+ (-2.0 * u3 + 3.0 * u2) * p1 + (u3 - u2) * m1
			var along := ((6.0 * u2 - 6.0 * u) * p0 + (3.0 * u2 - 4.0 * u + 1.0) * m0 \
				+ (-6.0 * u2 + 6.0 * u) * p1 + (3.0 * u2 - 2.0 * u) * m1).normalized()
			var side := sides[j].lerp(sides[j + 1], u).normalized()
			var up := side.cross(along).normalized()
			side = along.cross(up).normalized()
			rings.append([at, along, up, side])
	# Four faces swept along the rings, and the two end grains.
	var corners := [Vector2(1, 1), Vector2(-1, 1), Vector2(-1, -1), Vector2(1, -1)]
	for r in rings.size() - 1:
		var a: Array = rings[r]
		var b: Array = rings[r + 1]
		for c in 4:
			var c0: Vector2 = corners[c]
			var c1: Vector2 = corners[(c + 1) % 4]
			var a0: Vector3 = a[0] + a[3] * (c0.x * half_w) + a[2] * (c0.y * half_h)
			var a1: Vector3 = a[0] + a[3] * (c1.x * half_w) + a[2] * (c1.y * half_h)
			var b0: Vector3 = b[0] + b[3] * (c0.x * half_w) + b[2] * (c0.y * half_h)
			var b1: Vector3 = b[0] + b[3] * (c1.x * half_w) + b[2] * (c1.y * half_h)
			var mid := c0 + c1
			var normal: Vector3 = (a[3] * mid.x + a[2] * mid.y).normalized()
			tool.set_normal(normal)
			tool.add_vertex(a0)
			tool.add_vertex(b0)
			tool.add_vertex(b1)
			tool.add_vertex(a0)
			tool.add_vertex(b1)
			tool.add_vertex(a1)
	for end_ring in [[rings[0], -1.0], [rings[rings.size() - 1], 1.0]]:
		var e: Array = end_ring[0]
		var sign: float = end_ring[1]
		var q: Array[Vector3] = []
		for c in corners:
			q.append(e[0] + e[3] * (c.x * half_w) + e[2] * (c.y * half_h))
		tool.set_normal(e[1] * sign)
		if sign > 0.0:
			for v in [q[0], q[1], q[2], q[0], q[2], q[3]]:
				tool.add_vertex(v)
		else:
			for v in [q[0], q[2], q[1], q[0], q[3], q[2]]:
				tool.add_vertex(v)
	return tool.commit()
