extends Node3D

# AS-012, the swing, as the player sees it beyond its kit bodies: the parallel
# link that keeps the seat and the ram level on their arms, the rubber buffer
# on the ram's face closing as it strikes, and the trip wire from the seat's
# kick bar up the arm to the ram's hook. Every position is the native's;
# nothing here moves the machine.

const LINK_OFFSET := 0.85
const BUFFER_FREE_LENGTH := 2.0
const BUFFER_DISCS := 7
const RAM_RADIUS := 0.35
const RAM_DRUM_BELOW_PIN := 1.7
const SEAT_PIN_ABOVE_FLOOR := 2.4
const HOOK := Vector3(-15.0, 251.75, -97.05)

var _rust: StandardMaterial3D
var _rubber: StandardMaterial3D
var _steel: StandardMaterial3D
var _wire: StandardMaterial3D
var _links: Array[MeshInstance3D] = []
var _brackets: Array[MeshInstance3D] = []
var _discs: Array[MeshInstance3D] = []
var _plates: Array[MeshInstance3D] = []
var _wire_runs: Array[MeshInstance3D] = []


func setup() -> void:
	name = "SwingView"
	_rust = _material(Color("7a4a2c"), 0.85, 0.35)
	_rubber = _material(Color("161616"), 0.9, 0.0)
	_steel = _material(Color("8a8f94"), 0.45, 0.8)
	_wire = _material(Color("c9c2a8"), 0.5, 0.6)
	var chord := CylinderMesh.new()
	chord.top_radius = 0.05
	chord.bottom_radius = 0.05
	chord.height = 1.0
	chord.radial_segments = 8
	chord.rings = 1
	for index in 4:
		_links.append(_mesh(chord, _rust))
	var bracket := CylinderMesh.new()
	bracket.top_radius = 0.07
	bracket.bottom_radius = 0.07
	bracket.height = 1.0
	bracket.radial_segments = 8
	bracket.rings = 1
	for index in 4:
		_brackets.append(_mesh(bracket, _steel))
	var disc := CylinderMesh.new()
	disc.top_radius = 0.30
	disc.bottom_radius = 0.30
	disc.height = 1.0
	disc.radial_segments = 20
	disc.rings = 1
	var plate := CylinderMesh.new()
	plate.top_radius = 0.33
	plate.bottom_radius = 0.33
	plate.height = 0.04
	plate.radial_segments = 20
	plate.rings = 1
	for index in BUFFER_DISCS:
		_discs.append(_mesh(disc, _rubber))
	for index in BUFFER_DISCS + 1:
		_plates.append(_mesh(plate, _steel))
	var wire := CylinderMesh.new()
	wire.top_radius = 0.02
	wire.bottom_radius = 0.02
	wire.height = 1.0
	wire.radial_segments = 6
	wire.rings = 1
	for index in 3:
		_wire_runs.append(_mesh(wire, _wire))


func update_view(state: Dictionary) -> void:
	var seat_pivot: Vector3 = state["seat_pivot"]
	var ram_pivot: Vector3 = state["ram_pivot"]
	var seat_pin: Vector3 = state["seat_pin"]
	var ram_pin: Vector3 = state["ram_pin"]
	# The parallel links: below and behind each axle, to below and behind
	# each pin, so each hung body stays level as its arm swings.
	var seat_offset := Vector3(0.0, -LINK_OFFSET, LINK_OFFSET)
	var ram_offset := Vector3(0.0, -LINK_OFFSET, -LINK_OFFSET)
	_link_pair(0, seat_pivot + seat_offset, seat_pin + seat_offset)
	_link_pair(2, ram_pivot + ram_offset, ram_pin + ram_offset)
	_segment(_brackets[0], seat_pivot, seat_pivot + seat_offset)
	_segment(_brackets[1], seat_pin, seat_pin + seat_offset)
	_segment(_brackets[2], ram_pivot, ram_pivot + ram_offset)
	_segment(_brackets[3], ram_pin, ram_pin + ram_offset)
	# The buffer: rubber discs between steel plates, from the ram's face to
	# its free end, closing by what the native says it is compressed.
	var face := ram_pin + Vector3(0.0, -RAM_DRUM_BELOW_PIN, -RAM_RADIUS)
	var length := maxf(BUFFER_FREE_LENGTH - float(state["buffer_compression_m"]), 0.2)
	var pitch := length / float(BUFFER_DISCS)
	for index in BUFFER_DISCS + 1:
		_plates[index].transform = Transform3D(Basis(Vector3.RIGHT, PI * 0.5), face + Vector3(0.0, 0.0, -pitch * index))
	for index in BUFFER_DISCS:
		var z := -pitch * (float(index) + 0.5)
		_discs[index].transform = Transform3D(Basis(Vector3.RIGHT, PI * 0.5).scaled(Vector3(1.0, maxf(pitch - 0.05, 0.02), 1.0)),
			face + Vector3(0.0, 0.0, z))
	# The trip wire: kick bar, up the seat's yoke, up the arm, over to the hook.
	var bar := seat_pin + Vector3(0.0, -SEAT_PIN_ABOVE_FLOOR + 0.38, -0.68)
	_segment(_wire_runs[0], bar, seat_pin + Vector3(0.0, 0.0, -0.1))
	_segment(_wire_runs[1], seat_pin + Vector3(0.0, 0.0, -0.1), seat_pivot + Vector3(0.0, 0.2, 0.0))
	_segment(_wire_runs[2], seat_pivot + Vector3(0.0, 0.2, 0.0), HOOK)


func _link_pair(first: int, pivot: Vector3, pin: Vector3) -> void:
	for side in 2:
		var x := -0.25 if side == 0 else 0.25
		_segment(_links[first + side], pivot + Vector3(x, 0.0, 0.0), pin + Vector3(x, 0.0, 0.0))


func _material(color: Color, roughness: float, metal: float) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = roughness
	material.metallic = metal
	return material


func _mesh(geometry: Mesh, material: Material) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.mesh = geometry
	instance.material_override = material
	add_child(instance)
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
