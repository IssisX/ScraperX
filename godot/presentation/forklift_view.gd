extends Node3D
## The yard's forklift's wheels (src/sim/forklift.hpp). The truck's frame,
## counterweight, guard, mast and forks are kit bodies, drawn and moved by the
## kit's own drawing (main_machines.gd); its wheels are the native vehicle's,
## so each is drawn here where the native puts it this frame: turned by its
## steering, rolled by its spin, riding its suspension. Nothing here moves
## anything.

# Lugs round each tyre's face, so a rolling wheel is seen to roll.
const LUGS := 8

var _native: Object
var _wheels: Array[Node3D] = []


func setup(main: Node, native: Object) -> void:
	name = "ForkliftView"
	_native = native
	var tyre_material: Material = main._material(Color("1a1918"), 0.0, 0.92)
	var hub_material: Material = main._material(Color("b8901f"), 0.25, 0.6)
	var lug_material: Material = main._material(Color("2a2826"), 0.0, 0.85)
	var state: Dictionary = native.get_forklift_state()
	var radii: PackedFloat32Array = state["wheel_radius"]
	var widths: PackedFloat32Array = state["wheel_width"]
	for wheel in radii.size():
		var radius := radii[wheel]
		var width := widths[wheel]
		var node := Node3D.new()
		node.name = "ForkliftWheel%d" % wheel
		# A cylinder along its own y: y is the axle.
		var tyre := MeshInstance3D.new()
		var tyre_mesh := CylinderMesh.new()
		tyre_mesh.top_radius = radius
		tyre_mesh.bottom_radius = radius
		tyre_mesh.height = width
		tyre_mesh.radial_segments = 24
		tyre_mesh.material = tyre_material
		tyre.mesh = tyre_mesh
		node.add_child(tyre)
		var hub := MeshInstance3D.new()
		var hub_mesh := CylinderMesh.new()
		hub_mesh.top_radius = radius * 0.55
		hub_mesh.bottom_radius = radius * 0.55
		hub_mesh.height = width + 0.02
		hub_mesh.radial_segments = 16
		hub_mesh.material = hub_material
		hub.mesh = hub_mesh
		node.add_child(hub)
		var lug_mesh := BoxMesh.new()
		lug_mesh.size = Vector3(0.05, width + 0.01, 0.04)
		lug_mesh.material = lug_material
		for lug in LUGS:
			var angle := TAU * float(lug) / float(LUGS)
			var block := MeshInstance3D.new()
			block.mesh = lug_mesh
			block.position = Vector3(cos(angle), 0.0, sin(angle)) * (radius - 0.012)
			block.rotation.y = -angle
			node.add_child(block)
		add_child(node)
		_wheels.append(node)
	update_view()


func update_view() -> void:
	var wheels: Array = _native.get_forklift_state()["wheels"]
	for wheel in mini(wheels.size(), _wheels.size()):
		_wheels[wheel].global_transform = wheels[wheel]
