extends Node3D
# The throw's path (the owner, 2026-10-19: "a quick appearing throw-arc helper
# non obstructive overlay"): while the thumb swipes up with a piece of loose
# stock in the hands, faint beads along where the native would send it and a
# ring where it would come down; gone as the thumb lifts. The path is the
# native's (get_throw_prediction), cut at the first thing it would hit.

const BEADS := 40
const BEAD_SPACING_M := 0.22
const APPEAR_RATE := 14.0   # quick in
const FADE_RATE := 7.0      # and out

var _beads: MultiMeshInstance3D
var _ring: MeshInstance3D
var _glow: StandardMaterial3D
var _shown := 0.0


func setup() -> void:
	name = "ThrowArcView"
	_glow = StandardMaterial3D.new()
	_glow.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_glow.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	_glow.albedo_color = Color(1.0, 0.86, 0.55, 0.0)
	_glow.no_depth_test = false
	var bead := SphereMesh.new()
	bead.radius = 0.022
	bead.height = 0.044
	bead.radial_segments = 6
	bead.rings = 3
	var beads := MultiMesh.new()
	beads.transform_format = MultiMesh.TRANSFORM_3D
	beads.mesh = bead
	beads.instance_count = BEADS
	beads.visible_instance_count = 0
	_beads = MultiMeshInstance3D.new()
	_beads.name = "ThrowPath"
	_beads.multimesh = beads
	_beads.material_override = _glow
	add_child(_beads)
	var ring := TorusMesh.new()
	ring.inner_radius = 0.2
	ring.outer_radius = 0.24
	ring.rings = 24
	ring.ring_segments = 4
	_ring = MeshInstance3D.new()
	_ring.name = "ThrowLanding"
	_ring.mesh = ring
	_ring.material_override = _glow
	_ring.visible = false
	add_child(_ring)


# `aiming`: the thumb is mid-swipe with a throwable in the hands; `path`, the
# native's path then (kept as last drawn while it fades).
func update_view(path: PackedVector3Array, aiming: bool, delta: float) -> void:
	if aiming:
		_shown = minf(1.0, _shown + APPEAR_RATE * delta)
		_lay(path)
	else:
		_shown = maxf(0.0, _shown - FADE_RATE * delta)
	_glow.albedo_color.a = 0.55 * _shown
	visible = _shown > 0.001


# Beads evenly along the path's length (after the first, which sits in the
# hands), and the ring flat where it ends.
func _lay(path: PackedVector3Array) -> void:
	var count := 0
	if path.size() >= 2:
		var next_at := BEAD_SPACING_M
		var run := 0.0
		for i in range(1, path.size()):
			var a := path[i - 1]
			var b := path[i]
			var length := a.distance_to(b)
			while run + length >= next_at and count < BEADS:
				var t := (next_at - run) / maxf(length, 1.0e-6)
				_beads.multimesh.set_instance_transform(count, Transform3D(Basis.IDENTITY, a.lerp(b, t)))
				count += 1
				next_at += BEAD_SPACING_M
			run += length
	_beads.multimesh.visible_instance_count = count
	_ring.visible = path.size() >= 2
	if _ring.visible:
		_ring.position = path[path.size() - 1] + Vector3.UP * 0.03
