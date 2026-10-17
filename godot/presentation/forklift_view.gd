extends Node3D
## The yard's forklift's moving parts the native does not model as bodies
## (src/sim/forklift.hpp). The truck's frame, counterweight, guard, mast, ram
## barrel and forks are kit bodies, drawn and moved by the kit's own drawing
## (main_machines.gd); its wheels are the native vehicle's, so each is drawn
## here where the native puts it this frame: turned by its steering, rolled
## by its spin, riding its suspension. And its lift as a real mast's is: the
## ram's rod out of its barrel, the crosshead on its top with two sheaves,
## and the two leaf chains over them from the barrel to the carriage, so the
## forks rise twice as far as the rod. Nothing here moves anything.

# Lugs round each tyre's face, so a rolling wheel is seen to roll.
const LUGS := 8
const CHASSIS_ENTITY := 2982
# In the truck's frame (forklift.cpp): the ram's barrel stands at z 0.97, its
# top 1.45 m up; the carriage's origin rises from 0.06 m at the forks' lowest.
const RAM_Z := 0.97
const BARREL_TOP := 1.45
const CARRIAGE_FLOOR := 0.06
# The crosshead's height with the forks lowest, and where the chains end: on
# the barrel's mounting (anchor) and on the carriage's middle bar.
const CROSSHEAD_LOW := 1.55
const CHAIN_ANCHOR := 0.56
const CHAIN_ON_CARRIAGE := 0.50
const CHAIN_X := 0.12
const SHEAVE_RADIUS := 0.07

var _native: Object
var _chassis_body := -1
var _wheels: Array[Node3D] = []
var _rod: MeshInstance3D
var _crosshead: Node3D
var _chains: Array[MeshInstance3D] = []   # per side: anchor run, carriage run


func setup(main: Node, native: Object) -> void:
	name = "ForkliftView"
	_native = native
	_chassis_body = int(native.get_kit_body_index(CHASSIS_ENTITY))
	var tyre_material: Material = main._material(Color("1a1918"), 0.0, 0.92)
	var hub_material: Material = main._material(Color("b8901f"), 0.25, 0.6)
	var lug_material: Material = main._material(Color("2a2826"), 0.0, 0.85)
	var rod_material: Material = main._material(Color("d8dcdf"), 0.95, 0.12)
	var chain_material: Material = main._material(Color("2b2a28"), 0.8, 0.45)
	var head_material: Material = main._material(Color("3a3936"), 0.7, 0.5)
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

	# The rod: a unit-high chromed cylinder, stretched to its stroke.
	_rod = MeshInstance3D.new()
	_rod.name = "ForkliftRamRod"
	var rod_mesh := CylinderMesh.new()
	rod_mesh.top_radius = 0.035
	rod_mesh.bottom_radius = 0.035
	rod_mesh.height = 1.0
	rod_mesh.radial_segments = 12
	rod_mesh.material = rod_material
	_rod.mesh = rod_mesh
	add_child(_rod)
	_crosshead = Node3D.new()
	_crosshead.name = "ForkliftCrosshead"
	var bar := MeshInstance3D.new()
	var bar_mesh := BoxMesh.new()
	bar_mesh.size = Vector3(0.34, 0.05, 0.09)
	bar_mesh.material = head_material
	bar.mesh = bar_mesh
	_crosshead.add_child(bar)
	var sheave_mesh := CylinderMesh.new()
	sheave_mesh.top_radius = SHEAVE_RADIUS
	sheave_mesh.bottom_radius = SHEAVE_RADIUS
	sheave_mesh.height = 0.05
	sheave_mesh.radial_segments = 16
	sheave_mesh.material = head_material
	for side in [-1.0, 1.0]:
		var sheave := MeshInstance3D.new()
		sheave.mesh = sheave_mesh
		sheave.rotation.z = PI * 0.5   # axle along x
		sheave.position = Vector3(side * CHAIN_X, 0.0, 0.0)
		_crosshead.add_child(sheave)
	add_child(_crosshead)
	# A leaf chain: a unit-high dark strip of plates, stretched to its run.
	var chain_mesh := BoxMesh.new()
	chain_mesh.size = Vector3(0.04, 1.0, 0.014)
	chain_mesh.material = chain_material
	for i in 4:
		var run := MeshInstance3D.new()
		run.name = "ForkliftChain%d" % i
		run.mesh = chain_mesh
		add_child(run)
		_chains.append(run)
	update_view(state)


# The truck's frame in the world this frame.
func body_transform() -> Transform3D:
	return _native.get_kit_body_transform(_chassis_body) if _chassis_body >= 0 else global_transform


# The truck's nose as a view yaw (main.gd's: 0 faces -z, turning left +).
func heading() -> float:
	var nose := body_transform().basis.z
	return atan2(-nose.x, -nose.z)


func update_view(state: Dictionary) -> void:
	var wheels: Array = state["wheels"]
	for wheel in mini(wheels.size(), _wheels.size()):
		_wheels[wheel].global_transform = wheels[wheel]
	if _chassis_body < 0:
		return
	var body: Transform3D = _native.get_kit_body_transform(_chassis_body)
	var lift := float(state["lift_m"])
	# The crosshead rides up half as far as the forks: each chain is
	# anchored at one end, so its carriage end moves twice the sheave's.
	var head := CROSSHEAD_LOW + 0.5 * lift
	var rod_low := BARREL_TOP - 0.02
	_rod.global_transform = body * Transform3D(Basis.from_scale(Vector3(1.0, head - rod_low, 1.0)),
		Vector3(0.0, 0.5 * (rod_low + head), RAM_Z))
	_crosshead.global_transform = body * Transform3D(Basis.IDENTITY, Vector3(0.0, head, RAM_Z))
	var carriage_end := CARRIAGE_FLOOR + lift + CHAIN_ON_CARRIAGE
	for side in 2:
		var x := (-1.0 if side == 0 else 1.0) * CHAIN_X
		_place_run(_chains[side * 2], body, x, RAM_Z - SHEAVE_RADIUS, CHAIN_ANCHOR, head)
		_place_run(_chains[side * 2 + 1], body, x, RAM_Z + SHEAVE_RADIUS, carriage_end, head)


# One run of chain, straight up the truck's frame from `low` to `high`.
func _place_run(run: MeshInstance3D, body: Transform3D, x: float, z: float, low: float, high: float) -> void:
	var length := maxf(high - low, 0.01)
	run.global_transform = body * Transform3D(Basis.from_scale(Vector3(1.0, length, 1.0)),
		Vector3(x, 0.5 * (low + high), z))
