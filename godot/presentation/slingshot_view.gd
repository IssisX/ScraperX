extends Node3D

# The ground slingshot as the player sees it (after the ChatGPT branch's
# slingshot_view): two thick rubber bands from the fork's tips to the pouch,
# thinning as they stretch and sagging when slack; the leather pouch drawn
# from the same surface the native pouch is built on; and, while a rider is
# seated, the predicted shot as a chain of beads ending in a ring where it
# lands. Every position is the native's; nothing here moves the machine.

const POUCH_ENTITY := 2900
const BAND_SEGMENTS := 8
const BAND_RADIUS := 0.26
const PREDICTION_BEADS := 96
# The leather sheet, sampled 19 x 9 as the native's leather_surface().
const LEATHER_COLUMNS := 18
const LEATHER_ROWS := 8

var _main: Node3D
var _rubber: StandardMaterial3D
var _brass: StandardMaterial3D
var _leather: StandardMaterial3D
var _bands: Array = []
var _anchor_details: Array[Node3D] = []
var _pouch: Node3D
var _leather_mesh: MeshInstance3D
var _leather_fold := INF
var _beads: MultiMeshInstance3D
var _target: MeshInstance3D


func setup(main: Node3D) -> void:
	_main = main
	name = "SlingshotView"
	_rubber = _material(Color("141613"), 0.72, 0.0)
	_brass = _material(Color("b69048"), 0.42, 0.65)
	_leather = _material(Color("6b4426"), 0.86, 0.0)
	_build_bands()
	_build_pouch()
	_build_prediction()


func update_view(state: Dictionary, prediction: PackedVector3Array) -> void:
	_update_bands(state)
	_update_pouch(state)
	var aiming := bool(state.get("seated", false)) and not bool(state.get("released", false))
	_update_prediction(prediction if aiming else PackedVector3Array())


# The native pouch's surface, in its own frame (Slingshot::leather_surface):
# a cupped sheet 1.68 m across and 1.30 m deep, its back rolled up and its
# sides lifted; `fold` sags its middle under the rider.
static func leather_surface(u: float, v: float, fold: float = 0.0) -> Vector3:
	var x := (u - 0.5) * 1.68
	var z := (v - 0.5) * 1.30
	var back := clampf((z - 0.42) / 0.23, 0.0, 1.0)
	var nx := clampf(x / 0.84, -1.0, 1.0)
	var nz := clampf(z / 0.65, -1.0, 1.0)
	var sag := fold * (1.0 - nx * nx) * (1.0 - nz * nz)
	return Vector3(x, -0.17 - 0.12 * pow(maxf(0.0, -z) / 0.65, 2.0) +
		0.44 * pow(absf(x) / 0.84, 3.0) + 0.60 * back * back * (3.0 - 2.0 * back) + sag, z)


func _material(color: Color, roughness: float, metal: float) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = roughness
	material.metallic = metal
	return material


func _mesh(parent: Node3D, geometry: Mesh, material: Material, at: Vector3 = Vector3.ZERO) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.mesh = geometry
	instance.material_override = material
	instance.position = at
	parent.add_child(instance)
	return instance


func _box(size: Vector3) -> BoxMesh:
	var mesh := BoxMesh.new()
	mesh.size = size
	return mesh


func _build_bands() -> void:
	var band := CylinderMesh.new()
	band.top_radius = BAND_RADIUS
	band.bottom_radius = BAND_RADIUS
	band.height = 1.0
	band.radial_segments = 20
	band.rings = 1
	for side in 2:
		var pieces: Array[MeshInstance3D] = []
		for segment in BAND_SEGMENTS:
			var piece := _mesh(self, band, _rubber)
			piece.name = "RubberBand%d_%d" % [side, segment]
			pieces.append(piece)
		_bands.append(pieces)
		# Brass plates and coach bolts where each band is lashed to its tip.
		var detail := Node3D.new()
		detail.name = "BandAnchor%d" % side
		add_child(detail)
		_mesh(detail, _box(Vector3(0.96, 0.36, 0.52)), _brass)
		for bolt_x in [-0.33, 0.0, 0.33]:
			var bolt := SphereMesh.new()
			bolt.radius = 0.055
			bolt.height = 0.11
			_mesh(detail, bolt, _brass, Vector3(bolt_x, 0.195, 0.0)).scale.y = 0.35
		_mesh(detail, _box(Vector3(0.62, 0.11, 0.80)), _rubber, Vector3(0.0, -0.09, 0.25))
		_anchor_details.append(detail)


# A unit cylinder along y stretched from a to b, thinned across by `width`.
func _segment(instance: MeshInstance3D, a: Vector3, b: Vector3, width: float) -> void:
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
	instance.transform = Transform3D(Basis(x * width, y * span.length(), z * width), (a + b) * 0.5)


func _update_bands(state: Dictionary) -> void:
	var pouch: Vector3 = state.get("pouch_position", Vector3.ZERO)
	var anchors := [state.get("anchor_left", pouch), state.get("anchor_right", pouch)]
	var rest := maxf(float(state.get("band_rest_m", 10.44)), 0.001)
	for side in 2:
		var anchor: Vector3 = anchors[side]
		_anchor_details[side].position = anchor
		var length := (pouch - anchor).length()
		# Slack, the band hangs; stretched, it thins as rubber does, keeping
		# its volume.
		var sag := minf(maxf(rest - length, 0.0) * 0.10, 0.70)
		var thickness := sqrt(minf(rest / maxf(length, 0.1), 1.0))
		var attachment := pouch + Vector3(-0.84 if side == 0 else 0.84, 0.28, 0.0)
		for segment in BAND_SEGMENTS:
			var a := float(segment) / float(BAND_SEGMENTS)
			var b := float(segment + 1) / float(BAND_SEGMENTS)
			var start := anchor.lerp(attachment, a) - Vector3.UP * (sin(a * PI) * sag)
			var end := anchor.lerp(attachment, b) - Vector3.UP * (sin(b * PI) * sag)
			_segment(_bands[side][segment], start, end, thickness)


func _build_pouch() -> void:
	_pouch = Node3D.new()
	_pouch.name = "LeatherPouch"
	add_child(_pouch)
	_leather_mesh = MeshInstance3D.new()
	_leather_mesh.name = "Leather"
	_leather_mesh.material_override = _leather
	_pouch.add_child(_leather_mesh)
	# The band loops at the rim and their brass keepers.
	for side in [-1.0, 1.0]:
		_mesh(_pouch, _box(Vector3(0.055, 0.14, 0.50)), _leather, Vector3(side * 0.84, 0.45, 0.0))
		_mesh(_pouch, _box(Vector3(0.075, 0.10, 0.20)), _brass, Vector3(side * 0.84, 0.48, 0.0))


func _rebuild_leather(fold: float) -> void:
	var surface := SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	for row in LEATHER_ROWS:
		for column in LEATHER_COLUMNS:
			for corner in [Vector2i(0, 0), Vector2i(0, 1), Vector2i(1, 1),
					Vector2i(0, 0), Vector2i(1, 1), Vector2i(1, 0)]:
				var u := float(column + corner.x) / float(LEATHER_COLUMNS)
				var v := float(row + corner.y) / float(LEATHER_ROWS)
				surface.set_uv(Vector2(u, v))
				surface.add_vertex(leather_surface(u, v, fold))
	surface.generate_normals()
	_leather_mesh.mesh = surface.commit()
	_leather_fold = fold


func _update_pouch(state: Dictionary) -> void:
	_pouch.visible = state.has("pouch_position")
	_pouch.position = state.get("pouch_position", Vector3.ZERO)
	var fold := float(state.get("leather_deflection_m", 0.0))
	if absf(fold - _leather_fold) > 0.002:
		_rebuild_leather(fold)


func _build_prediction() -> void:
	var bead := SphereMesh.new()
	bead.radius = 0.16
	bead.height = 0.32
	bead.radial_segments = 8
	bead.rings = 4
	var glow := _material(Color("f2b84b"), 1.0, 0.0)
	glow.emission_enabled = true
	glow.emission = Color("f2b84b")
	glow.emission_energy_multiplier = 1.6
	glow.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	var beads := MultiMesh.new()
	beads.transform_format = MultiMesh.TRANSFORM_3D
	beads.mesh = bead
	beads.instance_count = PREDICTION_BEADS
	beads.visible_instance_count = 0
	_beads = MultiMeshInstance3D.new()
	_beads.name = "ShotPrediction"
	_beads.multimesh = beads
	_beads.material_override = glow
	add_child(_beads)
	var ring := TorusMesh.new()
	ring.inner_radius = 0.9
	ring.outer_radius = 1.2
	_target = _mesh(self, ring, glow)
	_target.name = "ShotLanding"
	_target.visible = false


func _update_prediction(points: PackedVector3Array) -> void:
	var count := mini(points.size(), PREDICTION_BEADS)
	_beads.multimesh.visible_instance_count = count
	for index in count:
		# Spread the beads over the whole arc, however long it is.
		var at := points[int(float(index) * float(points.size() - 1) / maxf(float(count - 1), 1.0))]
		_beads.multimesh.set_instance_transform(index, Transform3D(Basis.IDENTITY, at))
	_target.visible = count > 1
	if _target.visible:
		_target.position = points[points.size() - 1] + Vector3.UP * 0.05
