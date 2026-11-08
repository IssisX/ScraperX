extends Node3D
## A forklift's moving parts the native does not model as bodies
## (src/sim/forklift.hpp). The truck's frame, counterweight, guard, mast, ram
## barrel and forks are kit bodies, drawn and moved by the kit's own drawing
## (main_machines.gd); its wheels are the native vehicle's, so each is drawn
## here where the native puts it this frame: turned by its steering, rolled
## by its spin, riding its suspension. And its lift as a real mast's is: the
## ram's rod out of its barrel, the crosshead on its top with two sheaves,
## and the two leaf chains over them from the barrel to the carriage, so the
## forks rise twice as far as the rod. Nothing here moves anything.

# Lugs round each tire's face, so a rolling wheel is seen to roll.
const LUGS := 8
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
# Drawn as instances (J-045, #248): one MultiMesh a part for every truck,
# where every tire, hub, lug, rod, sheave and chain run was its own draw (cut
# by sections of the yard, the trucks drew three times as often). A part is
# [MultiMesh, index]; a wheel is its tire, hub and lugs. The trucks' parts are
# gathered as each is set up and drawn once all are (build).
var _wheels: Array = []
var _rod: Array
var _crosshead: Array
var _chains: Array = []   # per side: anchor run, carriage run
static var _parts := {}
static var _meshes := {}


# The forklift `index` of the native's (0 the yard's, then the yard crane's).
func setup(main: Node, native: Object, index: int = 0) -> void:
	name = "ForkliftView%d" % index if index > 0 else "ForkliftView"
	_native = native
	var built: Dictionary = native.get_forklift_state_at(index)
	_chassis_body = int(native.get_kit_body_index(int(built["chassis_entity"])))
	if not _meshes.has("materials"):
		_meshes["materials"] = [main._material(Color("1a1918"), 0.0, 0.92), main._material(Color("b8901f"), 0.25, 0.6),
			main._material(Color("2a2826"), 0.0, 0.85), main._material(Color("d8dcdf"), 0.95, 0.12),
			main._material(Color("2b2a28"), 0.8, 0.45), main._material(Color("3a3936"), 0.7, 0.5)]
	var materials: Array = _meshes["materials"]
	var tyre_material: Material = materials[0]
	var hub_material: Material = materials[1]
	var lug_material: Material = materials[2]
	var rod_material: Material = materials[3]
	var chain_material: Material = materials[4]
	var head_material: Material = materials[5]
	var state: Dictionary = built
	var radii: PackedFloat32Array = state["wheel_radius"]
	var widths: PackedFloat32Array = state["wheel_width"]
	var cell := "all"
	if not _parts.has(cell):
		_parts[cell] = {}
	var parts: Dictionary = _parts[cell]
	# One tire and one hub for every size: the first wheel's, each other's
	# drawn scaled to its own radius and width (a straight cylinder scaled
	# across its axle and along it is that cylinder, its normals unchanged).
	# The lugs, set a fixed 12 mm in from the tread, are made per size.
	if not _meshes.has("tyre"):
		var tyre_mesh := CylinderMesh.new()
		tyre_mesh.top_radius = radii[0]
		tyre_mesh.bottom_radius = radii[0]
		tyre_mesh.height = widths[0]
		tyre_mesh.radial_segments = 24
		tyre_mesh.material = tyre_material
		var hub_mesh := CylinderMesh.new()
		hub_mesh.top_radius = radii[0] * 0.55
		hub_mesh.bottom_radius = radii[0] * 0.55
		hub_mesh.height = widths[0] + 0.02
		hub_mesh.radial_segments = 16
		hub_mesh.material = hub_material
		_meshes["tyre"] = [tyre_mesh, hub_mesh]
	var unit: Array = _meshes["tyre"]
	var unit_radius := float((unit[0] as CylinderMesh).top_radius)
	var unit_width := float((unit[0] as CylinderMesh).height)
	for wheel in radii.size():
		var radius := radii[wheel]
		var width := widths[wheel]
		var size_key := "%f/%f" % [radius, width]
		if not _meshes.has(size_key):
			# A cylinder along its own y: y is the axle.
			var lug_mesh := BoxMesh.new()
			lug_mesh.size = Vector3(0.05, width + 0.01, 0.04)
			var lugs := SurfaceTool.new()
			lugs.begin(Mesh.PRIMITIVE_TRIANGLES)
			for lug in LUGS:
				var angle := TAU * float(lug) / float(LUGS)
				lugs.append_from(lug_mesh, 0, Transform3D(Basis(Vector3.UP, -angle),
					Vector3(cos(angle), 0.0, sin(angle)) * (radius - 0.012)))
			var lug_ring := lugs.commit()
			lug_ring.surface_set_material(0, lug_material)
			_meshes[size_key] = lug_ring
		var across := radius / unit_radius
		var tyre := _part(parts, unit[0])
		tyre.append(Basis.from_scale(Vector3(across, width / unit_width, across)))
		var hub := _part(parts, unit[1])
		hub.append(Basis.from_scale(Vector3(across, (width + 0.02) / (unit_width + 0.02), across)))
		_wheels.append([tyre, hub, _part(parts, _meshes[size_key])])

	if not _meshes.has("mast"):
		_meshes["mast"] = _mast_meshes(rod_material, head_material, chain_material)
	var mast: Array = _meshes["mast"]
	_rod = _part(parts, mast[0])
	_crosshead = _part(parts, mast[1])
	for i in 4:
		_chains.append(_part(parts, mast[2]))


# The mast's moving parts, the same on every truck: the rod, the crosshead,
# a chain run.
static func _mast_meshes(rod_material: Material, head_material: Material, chain_material: Material) -> Array:
	# The rod: a unit-high chromed cylinder, stretched to its stroke.
	var rod_mesh := CylinderMesh.new()
	rod_mesh.top_radius = 0.035
	rod_mesh.bottom_radius = 0.035
	rod_mesh.height = 1.0
	rod_mesh.radial_segments = 12
	rod_mesh.material = rod_material
	# The crosshead: the bar and its two sheaves, one material, one mesh.
	var head := SurfaceTool.new()
	head.begin(Mesh.PRIMITIVE_TRIANGLES)
	var bar_mesh := BoxMesh.new()
	bar_mesh.size = Vector3(0.34, 0.05, 0.09)
	head.append_from(bar_mesh, 0, Transform3D())
	var sheave_mesh := CylinderMesh.new()
	sheave_mesh.top_radius = SHEAVE_RADIUS
	sheave_mesh.bottom_radius = SHEAVE_RADIUS
	sheave_mesh.height = 0.05
	sheave_mesh.radial_segments = 16
	for side in [-1.0, 1.0]:
		# Its axle along x.
		head.append_from(sheave_mesh, 0, Transform3D(Basis(Vector3(0.0, 0.0, 1.0), PI * 0.5),
			Vector3(side * CHAIN_X, 0.0, 0.0)))
	var head_mesh := head.commit()
	head_mesh.surface_set_material(0, head_material)
	# A leaf chain: a unit-high dark strip of plates, stretched to its run.
	var chain_mesh := BoxMesh.new()
	chain_mesh.size = Vector3(0.04, 1.0, 0.014)
	chain_mesh.material = chain_material
	return [rod_mesh, head_mesh, chain_mesh]


# Once every truck is set up: their parts drawn under `parent`, one
# MultiMesh a mesh, each truck's posed by its next update_view.
static func build(parent: Node3D) -> void:
	for cell in _parts:
		var parts: Dictionary = _parts[cell]
		for mesh in parts:
			var multimesh := MultiMesh.new()
			multimesh.transform_format = MultiMesh.TRANSFORM_3D
			multimesh.mesh = mesh
			multimesh.instance_count = (parts[mesh] as Array).size()
			var drawn := MultiMeshInstance3D.new()
			drawn.name = "ForkliftParts"
			drawn.multimesh = multimesh
			parent.add_child(drawn)
			for part in parts[mesh]:
				(part as Array)[0] = multimesh
	_parts = {}
	_meshes = {}


# A part: an instance of its mesh's MultiMesh, made once all are counted.
static func _part(parts: Dictionary, mesh: Mesh) -> Array:
	if not parts.has(mesh):
		parts[mesh] = []
	var part := [null, (parts[mesh] as Array).size()]
	(parts[mesh] as Array).append(part)
	return part


func _place(part: Array, at: Transform3D) -> void:
	if part[0] != null:
		if part.size() > 2:
			at.basis = at.basis * (part[2] as Basis)
		(part[0] as MultiMesh).set_instance_transform(int(part[1]), at)


# The truck's frame in the world this frame.
func body_transform() -> Transform3D:
	return _native.get_kit_body_transform(_chassis_body) if _chassis_body >= 0 else global_transform


# The truck's nose as a view yaw (main.gd's: 0 faces -z, turning left +).
func heading() -> float:
	var nose := body_transform().basis.z
	return atan2(-nose.x, -nose.z)


func update_view(state: Dictionary) -> void:
	var wheels: Array = state["wheels"]
	var to_local := global_transform.affine_inverse()
	for wheel in mini(wheels.size(), _wheels.size()):
		for part in _wheels[wheel]:
			_place(part, to_local * (wheels[wheel] as Transform3D))
	if _chassis_body < 0:
		return
	var body: Transform3D = _native.get_kit_body_transform(_chassis_body)
	var lift := float(state["lift_m"])
	# The crosshead rides up half as far as the forks: each chain is
	# anchored at one end, so its carriage end moves twice the sheave's.
	var head := CROSSHEAD_LOW + 0.5 * lift
	var rod_low := BARREL_TOP - 0.02
	_place(_rod, to_local * body * Transform3D(Basis.from_scale(Vector3(1.0, head - rod_low, 1.0)),
		Vector3(0.0, 0.5 * (rod_low + head), RAM_Z)))
	_place(_crosshead, to_local * body * Transform3D(Basis.IDENTITY, Vector3(0.0, head, RAM_Z)))
	var carriage_end := CARRIAGE_FLOOR + lift + CHAIN_ON_CARRIAGE
	for side in 2:
		var x := (-1.0 if side == 0 else 1.0) * CHAIN_X
		_place_run(_chains[side * 2], to_local * body, x, RAM_Z - SHEAVE_RADIUS, CHAIN_ANCHOR, head)
		_place_run(_chains[side * 2 + 1], to_local * body, x, RAM_Z + SHEAVE_RADIUS, carriage_end, head)


# One run of chain, straight up the truck's frame from `low` to `high`.
func _place_run(run: Array, body: Transform3D, x: float, z: float, low: float, high: float) -> void:
	var length := maxf(high - low, 0.01)
	_place(run, body * Transform3D(Basis.from_scale(Vector3(1.0, length, 1.0)),
		Vector3(x, 0.5 * (low + high), z)))
