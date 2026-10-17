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
# Each line's trolley parts and its block, and the places last drawn.
var _trolleys: Array[Node3D] = []
var _hangers: Array[MeshInstance3D] = []
var _bars: Array[Node3D] = []
var _blocks: Array[Node3D] = []
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
	for entry in lines:
		var top: Vector3 = entry["top"]
		var bottom: Vector3 = entry["bottom"]
		_tops.push_back(top)
		_directions.push_back((bottom - top).normalized())
		# The trolley: its frame sits on the cable, z along the line.
		var trolley := Node3D.new()
		add_child(trolley)
		for side in [-1.0, 1.0]:
			var side_plate := _mesh(plate, _steel, trolley)
			side_plate.position = Vector3(side * 0.055, SHEAVE_RADIUS - 0.03, 0.0)
		for along in [-0.5, 0.5]:
			var wheel := _mesh(sheave, _steel, trolley)
			wheel.rotation = Vector3(0.0, 0.0, PI * 0.5)
			wheel.position = Vector3(0.0, SHEAVE_RADIUS, along * SHEAVE_GAP)
			var pin := _mesh(axle, _rust, trolley)
			pin.rotation = Vector3(0.0, 0.0, PI * 0.5)
			pin.position = wheel.position
		_trolleys.append(trolley)
		_hangers.append(_mesh(hanger, _steel, self))
		# The bar, with its rubber grips, hung under the hanger.
		var bar_node := Node3D.new()
		add_child(bar_node)
		var rod := _mesh(bar, _steel, bar_node)
		rod.rotation = Vector3(0.0, 0.0, PI * 0.5)
		for side in [-1.0, 1.0]:
			var hold := _mesh(grip, _rubber, bar_node)
			hold.rotation = Vector3(0.0, 0.0, PI * 0.5)
			hold.position = Vector3(side * (BAR_HALF - 0.09), 0.0, 0.0)
		_bars.append(bar_node)
		var clamp_node := Node3D.new()
		add_child(clamp_node)
		var body := _mesh(block, _hazard, clamp_node)
		body.position = Vector3.ZERO
		var stripe := _mesh(plate, _rubber, clamp_node)
		stripe.scale = Vector3(1.0, 0.5, 0.45)
		stripe.position = Vector3(BLOCK_HALF.x + 0.003, 0.0, 0.0)
		_blocks.append(clamp_node)
		_drawn.push_back(-1.0)
		_drawn.push_back(-1.0)


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
		_trolleys[i].transform = Transform3D(frame, on_cable)
		var bar_at: Vector3 = state["bar"] if on_ride else on_cable - Vector3(0.0, BAR_DROP, 0.0)
		_bars[i].transform = Transform3D(frame, bar_at)
		_segment(_hangers[i], on_cable - Vector3(0.0, 0.04, 0.0), bar_at + Vector3(0.0, 0.02, 0.0))
		_blocks[i].transform = Transform3D(frame, top + direction * block_m)
	_ridden = ridden


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


func _mesh(geometry: Mesh, material: Material, parent: Node) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.mesh = geometry
	instance.material_override = material
	parent.add_child(instance)
	return instance


# A unit cylinder along y stretched from a to b.
func _segment(instance: MeshInstance3D, a: Vector3, b: Vector3) -> void:
	var span := b - a
	if span.length_squared() < 0.000001:
		instance.visible = false
		return
	instance.visible = true
	var y := span.normalized()
	var x := y.cross(Vector3.FORWARD)
	if x.length_squared() < 0.001:
		x = y.cross(Vector3.RIGHT)
	x = x.normalized()
	var z := x.cross(y).normalized()
	instance.transform = Transform3D(Basis(x, y * span.length(), z), (a + b) * 0.5)
