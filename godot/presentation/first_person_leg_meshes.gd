extends RefCounted

# Shared presentation-only meshes. Metres; forward is -Z. Segment Y spans
# [-0.5, 0.5] so the rig alone controls bone length and contact placement.
const SIDES := 16

static func trouser_segment(upper: bool) -> ArrayMesh:
	var surface := _surface()
	var rings: Array[PackedVector3Array] = []
	var shades: Array[float] = []
	var heights := [-0.5, -0.47, -0.40, -0.25, -0.06, 0.14, 0.32, 0.46, 0.5]
	var widths := [0.077, 0.079, 0.082, 0.091, 0.102, 0.110, 0.109, 0.103, 0.101] if upper else [0.047, 0.051, 0.052, 0.059, 0.076, 0.080, 0.073, 0.069, 0.070]
	var depths := [0.074, 0.077, 0.080, 0.086, 0.095, 0.101, 0.103, 0.100, 0.096] if upper else [0.050, 0.054, 0.053, 0.060, 0.078, 0.086, 0.077, 0.066, 0.065]
	for row in heights.size():
		var ring := PackedVector3Array()
		for side in SIDES:
			var angle := TAU * float(side) / SIDES
			# Small irregular cloth folds follow the leg, rather than making
			# every section a perfectly round pipe.
			var fold := sin(angle * 5.0 + row * 0.85) * 0.0015
			var rear := 0.008 if upper else 0.012 * sin(PI * float(row) / 8.0)
			ring.append(Vector3(cos(angle) * (widths[row] + fold), heights[row], sin(angle) * (depths[row] + fold) + rear))
		rings.append(ring)
		shades.append(0.78 if row == 1 else (0.90 if row == 0 or row == 7 else 1.0))
	_loft(surface, rings, shades, true)
	return _finish(surface)

static func knee() -> ArrayMesh:
	var surface := _surface()
	# Reinforced cloth shell: broad flat front, bevels, and softer rear.
	var rings: Array[PackedVector3Array] = []
	for row in 5:
		var y: float = [-0.080, -0.061, 0.0, 0.061, 0.080][row]
		var width: float = [0.058, 0.077, 0.084, 0.077, 0.058][row]
		var depth: float = [0.050, 0.066, 0.073, 0.066, 0.050][row]
		var ring := PackedVector3Array()
		for side in SIDES:
			var angle := TAU * float(side) / SIDES
			var z := sin(angle) * depth
			if z < -0.035:
				z = -depth + (z + depth) * 0.32
			ring.append(Vector3(cos(angle) * width, y, z))
		rings.append(ring)
	_loft(surface, rings, [0.69, 0.80, 0.87, 0.80, 0.69], true)
	return _finish(surface)

static func boot() -> ArrayMesh:
	var surface := _surface()
	var outline := _footprint()
	var rings: Array[PackedVector3Array] = []
	for row in 5:
		var ring := PackedVector3Array()
		for point in outline:
			var taper: float = [0.95, 1.0, 0.98, 0.90, 0.64][row]
			var toe := clampf((-point.y - 0.015) / 0.16, 0.0, 1.0)
			var height: float = [-0.070, -0.056, -0.030, 0.025, 0.048][row]
			if row >= 3:
				height -= toe * (0.032 if row == 3 else 0.044)
			ring.append(Vector3(point.x * taper, height, point.y * taper))
		rings.append(ring)
	_loft(surface, rings, [0.65, 0.78, 0.92, 1.0, 0.95], true)
	# Raised heel counter and ankle shaft, with a rolled dark collar.
	rings.clear()
	for row in 6:
		var ring := PackedVector3Array()
		var y: float = [-0.006, 0.025, 0.080, 0.142, 0.158, 0.166][row]
		var width: float = [0.065, 0.064, 0.058, 0.059, 0.064, 0.059][row]
		var depth: float = [0.069, 0.067, 0.057, 0.055, 0.059, 0.054][row]
		for side in SIDES:
			var angle := TAU * float(side) / SIDES
			ring.append(Vector3(cos(angle) * width, y, 0.058 + sin(angle) * depth))
		rings.append(ring)
	_loft(surface, rings, [0.81, 0.94, 0.93, 0.83, 0.55, 0.65], true)
	# Toe-cap seam is a narrow raised leather strip, not a second material.
	var cap := PackedVector3Array([
		Vector3(-0.066, -0.004, -0.085), Vector3(-0.047, 0.003, -0.095),
		Vector3(0.0, 0.007, -0.100), Vector3(0.047, 0.003, -0.095),
		Vector3(0.066, -0.004, -0.085)])
	for i in cap.size() - 1:
		_strip(surface, cap[i], cap[i + 1], 0.002, 0.72)
	# A tongue and restrained cross-laces sit on the sloping instep.
	for row in 5:
		var y := 0.030 + float(row) * 0.021
		var z := -0.038 + float(row) * 0.012
		_strip(surface, Vector3(-0.025, y, z), Vector3(0.025, y + 0.008, z - 0.005), 0.0022, 0.42)
		_strip(surface, Vector3(0.025, y, z), Vector3(-0.025, y + 0.008, z - 0.005), 0.0022, 0.42)
	return _finish(surface)

static func sole() -> ArrayMesh:
	var surface := _surface()
	var rings: Array[PackedVector3Array] = []
	var outline := _footprint()
	for row in 5:
		var ring := PackedVector3Array()
		var scale: float = [0.93, 1.02, 1.04, 1.01, 0.96][row]
		var y: float = [-0.0125, -0.007, 0.005, 0.014, 0.018][row]
		for point in outline:
			ring.append(Vector3(point.x * scale, y, point.y * scale))
		rings.append(ring)
	# All tread relief stays on the sidewall, above the true contact plane.
	_loft(surface, rings, [0.62, 0.77, 0.88, 0.66, 0.97], true)
	return _finish(surface)

static func pelvis() -> ArrayMesh:
	var surface := _surface()
	var rings: Array[PackedVector3Array] = []
	for row in 5:
		var ring := PackedVector3Array()
		var width: float = [0.195, 0.200, 0.188, 0.170, 0.170][row]
		var depth: float = [0.112, 0.116, 0.104, 0.090, 0.090][row]
		var y: float = [0.0, 0.065, 0.145, 0.193, 0.210][row]
		# The eyes are forward of the belt. Keep the upper waist behind
		# that viewing line while the lower ring still joins the leg roots.
		var rear: float = [0.006, 0.045, 0.105, 0.155, 0.160][row]
		for side in SIDES:
			var angle := TAU * float(side) / SIDES
			# Lift the crotch between the two leg roots; keep outer hips low.
			var rise := 0.045 * absf(sin(angle)) if row == 0 else 0.0
			ring.append(Vector3(cos(angle) * width, y + rise, sin(angle) * depth + rear))
		rings.append(ring)
	_loft(surface, rings, [0.86, 1.0, 1.0, 0.68, 0.78], true)
	return _finish(surface)

static func _footprint() -> PackedVector2Array:
	# Clockwise viewed from above: rounded broad toe, narrow heel.
	return PackedVector2Array([
		Vector2(0.079, -0.065), Vector2(0.072, -0.124),
		Vector2(0.053, -0.158), Vector2(0.027, -0.172),
		Vector2(-0.027, -0.172), Vector2(-0.053, -0.158),
		Vector2(-0.072, -0.124), Vector2(-0.079, -0.065),
		Vector2(-0.071, 0.005), Vector2(-0.067, 0.088),
		Vector2(-0.049, 0.126), Vector2(0.0, 0.132),
		Vector2(0.049, 0.126), Vector2(0.067, 0.088),
		Vector2(0.071, 0.005), Vector2(0.077, -0.030)])

static func _surface() -> SurfaceTool:
	var surface := SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	return surface

static func _finish(surface: SurfaceTool) -> ArrayMesh:
	surface.index()
	return surface.commit()

static func _loft(surface: SurfaceTool, rings: Array[PackedVector3Array], shades: Array, caps: bool) -> void:
	var count := rings[0].size()
	# Foot outline runs opposite to the circular cloth sections.
	var clockwise := rings[0][0].cross(rings[0][1]).y > 0.0
	for row in rings.size() - 1:
		for side in count:
			var next := (side + 1) % count
			var a := _ring_vertex(rings, shades, row, side)
			var b := _ring_vertex(rings, shades, row, next)
			var c := _ring_vertex(rings, shades, row + 1, next)
			var d := _ring_vertex(rings, shades, row + 1, side)
			_quad(surface, a, b, c, d, clockwise)
	if caps:
		for row in [0, rings.size() - 1]:
			var center := Vector3.ZERO
			for point in rings[row]:
				center += point / count
			var normal := Vector3.DOWN if row == 0 else Vector3.UP
			for side in count:
				var next := (side + 1) % count
				var a := [center, normal, Vector2(0.5, 0.5), shades[row] * 0.86]
				var b := [rings[row][side], normal, Vector2(float(side) / count, 0.0), shades[row]]
				var c := [rings[row][next], normal, Vector2(float(next) / count, 0.0), shades[row]]
				_triangle(surface, a, b, c, clockwise != (row == 0))

static func _ring_vertex(rings: Array[PackedVector3Array], shades: Array, row: int, side: int) -> Array:
	var count := rings[0].size()
	var around := rings[row][(side + 1) % count] - rings[row][(side + count - 1) % count]
	var along := rings[mini(row + 1, rings.size() - 1)][side] - rings[maxi(row - 1, 0)][side]
	var normal := along.cross(around).normalized()
	var center := Vector3.ZERO
	for point in rings[row]:
		center += point / count
	var radial := rings[row][side] - center
	radial.y = 0.0
	if normal.dot(radial) < 0.0:
		normal = -normal
	var seam := 0.87 if side == 0 or side == count / 2 else 1.0
	return [rings[row][side], normal, Vector2(float(side) / count, float(row) / (rings.size() - 1)), shades[row] * seam]

static func _quad(surface: SurfaceTool, a: Array, b: Array, c: Array, d: Array, reverse: bool) -> void:
	_triangle(surface, a, b, c, reverse)
	_triangle(surface, a, c, d, reverse)

static func _triangle(surface: SurfaceTool, a: Array, b: Array, c: Array, reverse: bool) -> void:
	var vertices := [a, c, b] if reverse else [a, b, c]
	for vertex in vertices:
		surface.set_normal(vertex[1])
		surface.set_uv(vertex[2])
		var shade: float = vertex[3]
		surface.set_color(Color(shade, shade, shade, 1.0))
		surface.add_vertex(vertex[0])

static func _strip(surface: SurfaceTool, start: Vector3, end: Vector3, radius: float, shade: float) -> void:
	var direction := (end - start).normalized()
	var across := direction.cross(Vector3.UP).normalized() * radius
	var up := Vector3.UP * radius
	var points := [start - across - up, end - across - up, end + across - up, start + across - up,
		start - across + up, end - across + up, end + across + up, start + across + up]
	for face in [[0, 3, 2, 1], [4, 5, 6, 7], [0, 1, 5, 4], [3, 7, 6, 2], [0, 4, 7, 3], [1, 2, 6, 5]]:
		var normal: Vector3 = (points[face[2]] - points[face[0]]).cross(points[face[1]] - points[face[0]]).normalized()
		var vertices: Array = []
		for corner in 4:
			vertices.append([points[face[corner]], normal, Vector2(float(corner % 2), float(corner / 2)), shade])
		_quad(surface, vertices[0], vertices[1], vertices[2], vertices[3], false)
