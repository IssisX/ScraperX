extends Node3D
## The yard crane's parts the native does not model as bodies
## (src/sim/yard_crane.hpp). Its mast, slewing jib, trolley, hook block,
## sling, cabinet, remote and docks are kit bodies, drawn by the kit's own
## drawing, the sling as a kit rope. The native hangs the hook block on one
## rope from the trolley's sheaves: drawn here as the reeving's four falls
## between the trolley's two sheaves and the block's. And the red obstruction
## lamps a tower crane carries on its tower head and at its jib's tip, turning
## with the jib. Nothing here moves anything.

const FALLS := 4
# The falls either side of the rope's line, along the jib: the trolley's
# sheaves 0.36 m apart, the block's a little closer.
const FALL_SPREAD_TOP := 0.18
const FALL_SPREAD_BOTTOM := 0.12
# The slewing assembly (YardCrane::kJibEntity), and on it, in its frame, the
# tower head's apex and the jib's tip.
const JIB_ENTITY := 2984
const LAMP_APEX := Vector3(0.0, 13.4, 0.0)
const LAMP_TIP := Vector3(38.45, 3.85, 0.0)

var _native: Object
var _jib_body := -1
var _falls: Array[MeshInstance3D] = []
var _lamps: Array[MeshInstance3D] = []


func setup(main: Node, native: Object) -> void:
	name = "YardCraneView"
	_native = native
	_jib_body = int(native.get_kit_body_index(JIB_ENTITY))
	var rope_material: Material = main._material(Color("2a2927"), 0.85, 0.42)
	# A unit-high wire rope, stretched to each fall's run.
	var rope_mesh := CylinderMesh.new()
	rope_mesh.top_radius = 0.016
	rope_mesh.bottom_radius = 0.016
	rope_mesh.height = 1.0
	rope_mesh.radial_segments = 6
	rope_mesh.rings = 1
	rope_mesh.material = rope_material
	for i in FALLS:
		var fall := MeshInstance3D.new()
		fall.name = "CraneFall%d" % i
		fall.mesh = rope_mesh
		add_child(fall)
		_falls.append(fall)
	var lamp_material := StandardMaterial3D.new()
	lamp_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	lamp_material.albedo_color = Color(1.0, 0.12, 0.08)
	lamp_material.emission_enabled = true
	lamp_material.emission = Color(1.0, 0.1, 0.05)
	lamp_material.emission_energy_multiplier = 3.0
	var lamp_mesh := SphereMesh.new()
	lamp_mesh.radius = 0.16
	lamp_mesh.height = 0.32
	lamp_mesh.radial_segments = 10
	lamp_mesh.rings = 5
	lamp_mesh.material = lamp_material
	for i in 2:
		var lamp := MeshInstance3D.new()
		lamp.name = "CraneLamp%d" % i
		lamp.mesh = lamp_mesh
		lamp.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		# Its red glow on the jib's steel round it, as a warning lamp's at
		# night: a lamp gives light of its own (floating_audit.gd, dark lamps).
		var glow := OmniLight3D.new()
		glow.name = "CraneLampGlow"
		glow.light_color = Color(1.0, 0.12, 0.08)
		glow.light_energy = 1.5
		glow.omni_range = 5.0
		glow.omni_attenuation = 1.4
		glow.distance_fade_enabled = true
		glow.distance_fade_begin = 100.0
		glow.distance_fade_length = 20.0
		lamp.add_child(glow)
		add_child(lamp)
		_lamps.append(lamp)
	update_view(native.get_yard_crane_state())


# The slewing assembly's frame in the world this frame.
func jib_transform() -> Transform3D:
	return _native.get_kit_body_transform(_jib_body) if _jib_body >= 0 else global_transform


func update_view(state: Dictionary) -> void:
	var top: Vector3 = state["rope_top"]
	var bottom: Vector3 = state["rope_bottom"]
	var jib := jib_transform()
	var along := jib.basis.x.normalized()
	for i in FALLS:
		var share := (float(i) - 1.5) / 1.5
		_place_fall(_falls[i], top + along * (FALL_SPREAD_TOP * share), bottom + along * (FALL_SPREAD_BOTTOM * share))
	_lamps[0].global_position = jib * LAMP_APEX
	_lamps[1].global_position = jib * LAMP_TIP


func _place_fall(fall: MeshInstance3D, from: Vector3, to: Vector3) -> void:
	var run := to - from
	var length := run.length()
	if length < 0.01:
		fall.visible = false
		return
	fall.visible = true
	var y := run / length
	var x := y.cross(Vector3.FORWARD if absf(y.dot(Vector3.FORWARD)) < 0.9 else Vector3.RIGHT).normalized()
	var z := x.cross(y)
	fall.global_transform = Transform3D(Basis(x, y * length, z), from + 0.5 * run)
