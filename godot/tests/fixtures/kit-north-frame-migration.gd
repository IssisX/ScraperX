extends RefCounted

# Independent authored geometry of src/sim/north_service_frame.cpp at
# 86fa732e1344f9ea66197c81cd02aaead1c04859 and its parent. No live parts,
# KitView builder or scene capture supplies this appearance expectation.
# Godot 4.7 BoxMesh + SurfaceTool preserve the pinned primitive/channel API.
# The caller must validate the complete BEFORE channels against the frozen
# oracle before applying AFTER channels. Materials and signs stay frozen.

static func _f(value: float) -> float:
	return PackedFloat32Array([value])[0]


static func _box(parts: Array, low: Vector3, high: Vector3, material: int) -> void:
	parts.append([(high - low) * 0.5, (high + low) * 0.5, material])


# now: left/right, bottom/top, first/count, rung Z/fascia Z, two mounts.
# old: rail bottom, rail Z bounds, last rung, Z-switch height, two rung Zs.
static func _ladder(parts: Array, after: bool, now: Array, old: Array) -> void:
	var left := _f(now[0])
	var right := _f(now[1])
	var bottom := _f(now[2] if after else old[0])
	var top := _f(now[3])
	var z := _f(now[6])
	var fascia := _f(now[7])
	for x in [left, right]:
		_box(parts, Vector3(_f(x - _f(0.05)), bottom, _f(z - _f(0.05)) if after else old[1]),
			Vector3(_f(x + _f(0.05)), top, _f(z + _f(0.05)) if after else old[2]), 7)
		if after:
			_box(parts, Vector3(_f(x - _f(0.09)), _f(bottom - _f(0.10)), _f(z - _f(0.22))),
				Vector3(_f(x + _f(0.09)), _f(bottom + _f(0.10)), _f(z + _f(0.22))), 0)
			for mount in [now[8], now[9]]:
				var y := _f(mount)
				_box(parts, Vector3(_f(x - _f(0.09)), _f(y - _f(0.10)), minf(_f(z - _f(0.05)), fascia)),
					Vector3(_f(x + _f(0.09)), _f(y + _f(0.10)), maxf(_f(z + _f(0.05)), fascia)), 0)
	var y := _f(now[4])
	var rung := 0
	# BEFORE reproduces the source's float32 repeated addition and endpoint
	# comparison; AFTER uses its declared integer count and indexed multiply.
	while (rung < int(now[5])) if after else (y <= _f(old[3])):
		if after:
			y = _f(_f(now[4]) + _f(_f(0.30) * float(rung)))
		else:
			z = _f(old[6] if y > _f(old[4]) else old[5])
		_box(parts, Vector3(left, _f(y - _f(0.04)), _f(z - _f(0.04))),
			Vector3(right, _f(y + _f(0.04)), _f(z + _f(0.04))), 0)
		rung += 1
		y = _f(y + _f(0.30))


static func parts(after: bool) -> Array:
	var parts: Array = []
	_box(parts, Vector3(14.0, 87.72, -180.0), Vector3(18.0, 88.0, -175.75), 5)
	for x in [14.2, 17.8]:
		_box(parts, Vector3(_f(_f(x) - _f(0.175)), 77.0, -179.15), Vector3(_f(_f(x) + _f(0.175)), 87.72, -178.8), 0)
	_box(parts, Vector3(14.0, 76.25, -179.15), Vector3(18.0, 76.72, -175.8), 0)
	_box(parts, Vector3(14.0, 86.8, -179.15), Vector3(18.0, 87.4, -178.8), 1)
	_ladder(parts, after, [15.45, 16.55, 88.0, 93.80, 89.25, 16, -179.97, -180.14, 92.8, 93.6], [88.55, -180.3, -180.2, 93.75, 93.6, -180.25, -179.96])
	_box(parts, Vector3(14.3, 93.72, -183.0), Vector3(17.7, 94.0, -180.0), 5)
	_box(parts, Vector3(15.3, 92.6, -180.30), Vector3(16.7, 94.0, -179.99), 1)
	for x in [14.5, 17.5]:
		_box(parts, Vector3(_f(_f(x) - _f(0.175)), 77.0, -181.8), Vector3(_f(_f(x) + _f(0.175)), 93.72, -181.45), 0)
	_box(parts, Vector3(14.15, 93.25, -181.75), Vector3(17.85, 93.72, -181.35), 1)
	_box(parts, Vector3(10.0, 93.72, -181.9), Vector3(14.6, 94.0, -180.5), 5)
	_box(parts, Vector3(8.5, 93.72, -183.0), Vector3(11.5, 94.0, -179.25), 5)
	_box(parts, Vector3(8.5, 76.25, -182.0), Vector3(11.5, 76.72, -175.8), 0)
	for x in [8.8, 11.2]:
		_box(parts, Vector3(_f(_f(x) - _f(0.175)), 76.72, -181.8), Vector3(_f(_f(x) + _f(0.175)), 93.72, -181.45), 0)
	_ladder(parts, after, [9.45, 10.55, 94.0, 97.30, 95.25, 7, -179.13, -178.95, 96.3, 97.1], [94.55, -178.9, -178.8, 97.05, 96.9, -178.85, -179.14])
	_box(parts, Vector3(8.5, 97.22, -179.1), Vector3(11.5, 97.5, -176.8), 5)
	_box(parts, Vector3(9.3, 96.1, -179.11), Vector3(10.7, 97.5, -178.80), 1)
	_box(parts, Vector3(8.8, 98.72, -176.8), Vector3(11.2, 99.0, -176.05), 5)
	_box(parts, Vector3(9.3, 97.4, -176.85), Vector3(10.7, 99.0, -176.55), 1)
	_box(parts, Vector3(14.0, 98.72, -180.0), Vector3(18.0, 99.0, -175.75), 5)
	for x in [14.2, 17.8]:
		_box(parts, Vector3(_f(_f(x) - _f(0.175)), 88.0, -179.15), Vector3(_f(_f(x) + _f(0.175)), 98.72, -178.8), 0)
	_ladder(parts, after, [15.45, 16.55, 99.0, 103.30, 100.25, 11, -179.97, -180.14, 102.3, 103.1], [99.55, -180.3, -180.2, 103.25, 103.1, -180.25, -179.96])
	_box(parts, Vector3(14.3, 103.22, -183.0), Vector3(17.7, 103.5, -180.0), 5)
	_box(parts, Vector3(15.3, 102.1, -180.30), Vector3(16.7, 103.5, -179.99), 1)
	for x in [14.5, 17.5]:
		_box(parts, Vector3(_f(_f(x) - _f(0.175)), 94.0, -181.8), Vector3(_f(_f(x) + _f(0.175)), 103.22, -181.45), 0)
	_box(parts, Vector3(12.2, 105.72, -183.0), Vector3(14.0, 106.0, -180.0), 5)
	_box(parts, Vector3(13.50 if after else 13.94, 104.6, -182.95), Vector3(14.03, 106.0, -180.05), 1)
	_box(parts, Vector3(12.2, 107.2, -181.25), Vector3(14.2, 107.6, -180.0), 0)
	_box(parts, Vector3(12.15, 106.0, -180.22), Vector3(12.4, 107.2, -179.98), 0)
	_box(parts, Vector3(12.3, 94.0, -181.8), Vector3(12.65, 105.72, -181.45), 0)
	_box(parts, Vector3(12.2, 102.8, -181.8), Vector3(14.6, 103.22, -181.45), 1)
	_ladder(parts, after, [12.45, 13.55, 106.0, 108.30, 107.25, 4, -183.07, -183.24, 107.3, 108.1], [106.55, -183.45, -183.35, 108.15, 108.0, -183.4, -183.12])
	_box(parts, Vector3(11.4, 108.22, -185.0), Vector3(14.6, 108.5, -183.1), 5)
	_box(parts, Vector3(12.3, 107.1, -183.40), Vector3(13.7, 108.5, -183.09), 1)
	_box(parts, Vector3(12.1, 106.0, -183.25), Vector3(12.35, 108.22, -182.9), 0)
	_box(parts, Vector3(13.0, 108.22, -184.4), Vector3(16.2, 108.5, -183.2), 5)
	_box(parts, Vector3(15.0, 108.22, -184.4), Vector3(16.2, 108.5, -176.8), 5)
	_box(parts, Vector3(15.45, 103.5, -184.0), Vector3(15.80, 108.22, -183.65), 0)
	_box(parts, Vector3(15.45, 102.95, -184.0), Vector3(15.80, 103.22, -182.8), 1)
	_box(parts, Vector3(14.3, 109.72, -176.8), Vector3(16.7, 110.0, -176.05), 5)
	_box(parts, Vector3(14.8, 108.4, -176.85), Vector3(16.2, 110.0, -176.55), 1)
	return parts


static func channels(after: bool) -> Array:
	var batches: Dictionary = {}
	for part in parts(after):
		var material: int = part[2]
		if not batches.has(material):
			var surface := SurfaceTool.new()
			surface.begin(Mesh.PRIMITIVE_TRIANGLES)
			batches[material] = surface
		var box := BoxMesh.new()
		box.size = part[0] * 2.0
		(batches[material] as SurfaceTool).append_from(box, 0, Transform3D(Basis.IDENTITY, part[1]))
	var result: Array = []
	for material in batches:
		var mesh := (batches[material] as SurfaceTool).commit()
		result.append(mesh.surface_get_arrays(0))
	return result
