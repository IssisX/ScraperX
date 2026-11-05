extends Node3D

# The ziplines' trolleys and brake blocks (sim/zipline.hpp), as the player
# sees them beyond each line's kit body (its posts, stand, ramp and cable):
# on every line a trolley -- two sheaves in a bracket over the cable, a
# hanger, and the bar the hands hold -- where the native puts it (parked
# at the top until ridden, then wherever the ride left it), and the brake
# block, a clamp on the cable at its rest until the trolley runs into it
# and pushes it to the stop. Ridden, the bar is drawn at the native's bar
# (the hands) and the hanger swings from the sheaves to it. Every position
# is the native's; nothing here moves anything.

const BAR_DROP := 0.28          # Zipline::kBarDropM
const SHEAVE_RADIUS := 0.065
const SHEAVE_GAP := 0.2         # the two sheaves' spacing along the cable
const BAR_HALF := 0.27
const BLOCK_HALF := Vector3(0.07, 0.07, 0.12)

var _steel: StandardMaterial3D
var _rubber: StandardMaterial3D
var _hazard: StandardMaterial3D
var _rust: StandardMaterial3D
# Each line's anchors (Godot space) and its unit direction, top to bottom.
var _tops := PackedVector3Array()
var _directions := PackedVector3Array()
# Drawn as instances (J-045, #248): one MultiMesh per part for every line,
# where every piece of every line was its own draw (cut by sections, the lines
# spread up the tower drew several times as often). Each part is
# [MultiMesh, index, its place in its assembly]; per line, its trolley's,
# bar's and block's parts, and its hanger.
var _trolleys: Array = []
var _hangers: Array = []
var _bars: Array = []
var _blocks: Array = []
var _drawn := PackedFloat32Array()   # [trolley_m, block_m] a line
var _ridden := -1


func setup(native: Object) -> void:
	name = "ZiplineView"
	_steel = _material(Color("8a8f94"), 0.45, 0.8)
	_rubber = _material(Color("1a1a1a"), 0.9, 0.0)
	_hazard = _material(Color("c9741c"), 0.7, 0.2)
	_rust = _material(Color("6b4a30"), 0.85, 0.3)
	var sheave := CylinderMesh.new()
	sheave.top_radius = SHEAVE_RADIUS
	sheave.bottom_radius = SHEAVE_RADIUS
	sheave.height = 0.035
	sheave.radial_segments = 14
	sheave.rings = 1
	var plate := BoxMesh.new()
	plate.size = Vector3(0.012, 0.19, SHEAVE_GAP + 2.0 * SHEAVE_RADIUS + 0.04)
	var axle := CylinderMesh.new()
	axle.top_radius = 0.012
	axle.bottom_radius = 0.012
	axle.height = 0.11
	axle.radial_segments = 6
	axle.rings = 1
	var hanger := CylinderMesh.new()
	hanger.top_radius = 0.016
	hanger.bottom_radius = 0.016
	hanger.height = 1.0
	hanger.radial_segments = 6
	hanger.rings = 1
	var bar := CylinderMesh.new()
	bar.top_radius = 0.018
	bar.bottom_radius = 0.018
	bar.height = 2.0 * BAR_HALF
	bar.radial_segments = 8
	bar.rings = 1
	var grip := CylinderMesh.new()
	grip.top_radius = 0.026
	grip.bottom_radius = 0.026
	grip.height = 0.13
	grip.radial_segments = 8
	grip.rings = 1
	var block := BoxMesh.new()
	block.size = BLOCK_HALF * 2.0
	var lines: Array = native.get_zipline_lines()
	var parts := {}
	var on_axle := Basis(Vector3(0.0, 0.0, 1.0), PI * 0.5)
	# Each assembly's parts of one material as one mesh: the trolley's plates
	# and sheaves, its pins; the bar, its grips; the block, its stripe.
	var trolley_steel := SurfaceTool.new()
	trolley_steel.begin(Mesh.PRIMITIVE_TRIANGLES)
	var trolley_rust := SurfaceTool.new()
	trolley_rust.begin(Mesh.PRIMITIVE_TRIANGLES)
	for side in [-1.0, 1.0]:
		trolley_steel.append_from(plate, 0, Transform3D(Basis(), Vector3(side * 0.055, SHEAVE_RADIUS - 0.03, 0.0)))
	for along in [-0.5, 0.5]:
		var at := Vector3(0.0, SHEAVE_RADIUS, along * SHEAVE_GAP)
		trolley_steel.append_from(sheave, 0, Transform3D(on_axle, at))
		trolley_rust.append_from(axle, 0, Transform3D(on_axle, at))
	var bar_steel := SurfaceTool.new()
	bar_steel.begin(Mesh.PRIMITIVE_TRIANGLES)
	bar_steel.append_from(bar, 0, Transform3D(on_axle, Vector3.ZERO))
	var bar_rubber := SurfaceTool.new()
	bar_rubber.begin(Mesh.PRIMITIVE_TRIANGLES)
	for side in [-1.0, 1.0]:
		bar_rubber.append_from(grip, 0, Transform3D(on_axle, Vector3(side * (BAR_HALF - 0.09), 0.0, 0.0)))
	var stripe := SurfaceTool.new()
	stripe.begin(Mesh.PRIMITIVE_TRIANGLES)
	stripe.append_from(plate, 0, Transform3D(Basis.from_scale(Vector3(1.0, 0.5, 0.45)),
		Vector3(BLOCK_HALF.x + 0.003, 0.0, 0.0)))
	var trolley_meshes := [[trolley_steel.commit(), _steel], [trolley_rust.commit(), _rust]]
	var bar_meshes := [[bar_steel.commit(), _steel], [bar_rubber.commit(), _rubber]]
	var block_meshes := [[block, _hazard], [stripe.commit(), _rubber]]
	for entry in lines:
		var top: Vector3 = entry["top"]
		var bottom: Vector3 = entry["bottom"]
		_tops.push_back(top)
		_directions.push_back((bottom - top).normalized())
		# The trolley: its frame sits on the cable, z along the line.
		_trolleys.append(trolley_meshes.map(func(m: Array) -> Array: return _part(parts, m[0], m[1])))
		_hangers.append(_part(parts, hanger, _steel))
		# The bar, with its rubber grips, hung under the hanger.
		_bars.append(bar_meshes.map(func(m: Array) -> Array: return _part(parts, m[0], m[1])))
		_blocks.append(block_meshes.map(func(m: Array) -> Array: return _part(parts, m[0], m[1])))
		_drawn.push_back(-1.0)
		_drawn.push_back(-1.0)
	for key in parts:
		var list: Array = parts[key]
		var multimesh := MultiMesh.new()
		multimesh.transform_format = MultiMesh.TRANSFORM_3D
		multimesh.mesh = list[0][0]
		multimesh.instance_count = list.size()
		var drawn := MultiMeshInstance3D.new()
		drawn.multimesh = multimesh
		drawn.material_override = list[0][1]
		add_child(drawn)
		for entry in list:
			(entry[2] as Array)[0] = multimesh
			multimesh.set_instance_transform(int((entry[2] as Array)[1]), Transform3D(Basis().scaled(Vector3.ZERO),
				Vector3.ZERO))


# One part of a line's assembly, an instance in the set of this mesh and
# material; its MultiMesh is filled in once all are counted.
func _part(parts: Dictionary, mesh: Mesh, material: Material) -> Array:
	var key := "%d/%d" % [mesh.get_instance_id(), material.get_instance_id()]
	if not parts.has(key):
		parts[key] = []
	var part := [null, (parts[key] as Array).size(), Transform3D()]
	(parts[key] as Array).append([mesh, material, part])
	return part


func line_count() -> int:
	return _tops.size()


# The native's places for every trolley and block this frame, and the bar
# in the rider's hands when a line is ridden.
func update_view(trolleys: PackedFloat32Array, state: Dictionary) -> void:
	var riding := bool(state["riding"])
	var ridden := int(state["line"]) if riding else -1
	for i in _tops.size():
		var trolley_m := trolleys[2 * i]
		var block_m := trolleys[2 * i + 1]
		var on_ride := i == ridden
		if not on_ride and i != _ridden and trolley_m == _drawn[2 * i] and block_m == _drawn[2 * i + 1]:
			continue
		_drawn[2 * i] = trolley_m
		_drawn[2 * i + 1] = block_m
		var top := _tops[i]
		var direction := _directions[i]
		var on_cable := top + direction * trolley_m
		var frame := _along(direction)
		_pose(_trolleys[i], Transform3D(frame, on_cable))
		var bar_at: Vector3 = state["bar"] if on_ride else on_cable - Vector3(0.0, BAR_DROP, 0.0)
		_pose(_bars[i], Transform3D(frame, bar_at))
		_segment(_hangers[i], on_cable - Vector3(0.0, 0.04, 0.0), bar_at + Vector3(0.0, 0.02, 0.0))
		_pose(_blocks[i], Transform3D(frame, top + direction * block_m))
	_ridden = ridden


# An assembly's parts where the assembly stands (its frame in the world).
func _pose(assembly: Array, at: Transform3D) -> void:
	for part in assembly:
		(part[0] as MultiMesh).set_instance_transform(int(part[1]), at * (part[2] as Transform3D))


# A basis with z down the line and y up (the line never stands vertical).
func _along(direction: Vector3) -> Basis:
	var z := direction
	var x := Vector3.UP.cross(z).normalized()
	var y := z.cross(x).normalized()
	return Basis(x, y, z)


func _material(color: Color, roughness: float, metal: float) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = roughness
	material.metallic = metal
	return material


# A unit cylinder along y stretched from a to b (none when they meet).
func _segment(part: Array, a: Vector3, b: Vector3) -> void:
	var span := b - a
	var at := Transform3D(Basis().scaled(Vector3.ZERO), a)
	if span.length_squared() >= 0.000001:
		var y := span.normalized()
		var x := y.cross(Vector3.FORWARD)
		if x.length_squared() < 0.001:
			x = y.cross(Vector3.RIGHT)
		x = x.normalized()
		var z := x.cross(y).normalized()
		at = Transform3D(Basis(x, y * span.length(), z), (a + b) * 0.5)
	(part[0] as MultiMesh).set_instance_transform(int(part[1]), at)
