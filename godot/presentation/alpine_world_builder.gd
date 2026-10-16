extends RefCounted

# Alpine backdrop and vegetation, built into the existing presentation hierarchy.
const WorldGeometry := preload("res://presentation/world_geometry.gd")
const AlpinePeakMesh := preload("res://presentation/alpine_peak_mesh.gd")
var _peak_specs: Array = []
var _native_snow_caps: Array = []
# The yard sits in an alpine basin, not on a slab in a void. The valley floor
# runs out under the rim, 0.3 m below grade (a step the feet walk, down and
# back up). The rim is a closed ring of steep ridges: 44 cones on an 1100 m
# circle, spaced closer than the smallest cone's base radius, so where two
# meet the crease between them never flattens below ~54 degrees until it is
# half their height up -- a wall of rock, not an invisible one, and the
# same solid collision as everything else (solid_export.gd).
const BASIN_CENTER := Vector3(0.0, 0.0, -100.0)
const BASIN_RIM_RADIUS := 1100.0
const BASIN_RIM_RIDGES := 44
const VALLEY_FLOOR_TOP := -0.3

var _geometry: WorldGeometry


func _init(geometry: WorldGeometry) -> void:
	_geometry = geometry


func build_backdrop() -> void:
	# Original cone nodes keep exact collision/export identity. Shared carved
	# MultiMeshes replace only their rendering, including slope-based snow.
	var rock := _geometry.material(Color("2f353d"), 0.04, 0.92)
	var rock_far := _geometry.material(Color("414a55"), 0.02, 0.95)
	var snow := _geometry.material(Color("b9c2cc"), 0.0, 0.78)

	var near_peaks := [
		Vector3(-380.0, 0.0, -560.0), Vector3(-160.0, 0.0, -610.0),
		Vector3(110.0, 0.0, -630.0), Vector3(180.0, 0.0, -540.0),
		Vector3(380.0, 0.0, -580.0), Vector3(580.0, 0.0, -680.0),
	]
	var near_sizes := [520.0, 640.0, 700.0, 600.0, 560.0, 480.0]
	for index in near_peaks.size():
		var base: Vector3 = near_peaks[index]
		var peak_height: float = near_sizes[index]
		var radius := peak_height * 0.62
		_add_peak("MountainPeak", radius, peak_height, base, rock, index, 0.59)
		_export_only_cone("MountainSnowCap", radius * 0.34, peak_height * 0.3,
			base + Vector3(0.0, peak_height * 0.92, 0.0), snow)

	var far_peaks := [
		Vector3(-640.0, 0.0, -880.0), Vector3(-240.0, 0.0, -950.0),
		Vector3(260.0, 0.0, -930.0), Vector3(680.0, 0.0, -900.0),
	]
	for base in far_peaks:
		var peak_height := 900.0
		var radius := peak_height * 0.7
		_add_peak("MountainRidgeFar", radius, peak_height, base, rock_far, _peak_specs.size(), 0.53)
		_export_only_cone("MountainRidgeFarSnow", radius * 0.4, peak_height * 0.32,
			base + Vector3(0.0, peak_height * 0.9, 0.0), snow)

	_build_waterfall(Vector3(200.0, 0.0, -560.0))
	_build_foothill_forest()
	_build_basin()
	AlpinePeakMesh.draw(_geometry.presentation_root, _peak_specs)
	AlpinePeakMesh.draw_near_snow(_geometry.presentation_root, _native_snow_caps)


func _export_only_cone(part: String, radius: float, height: float, at: Vector3,
		finish: Material) -> MeshInstance3D:
	var source := _geometry.add_cone(part, radius, height, at, finish)
	# Same representation policy as StaticWorldBatch. Export ignores layers.
	if part in ["MountainSnowCap", "MountainRidgeFarSnow", "RimRidgeSnow"]:
		_native_snow_caps.append({"radius": radius, "height": height, "centre": at})
	source.layers = 0
	source.set_meta(&"alpine_render_replaced", true)
	return source


func _add_peak(part: String, radius: float, height: float, foot: Vector3,
		finish: StandardMaterial3D, variant: int, snowline: float) -> void:
	_export_only_cone(part, radius, height, foot + Vector3(0.0, height * 0.5, 0.0), finish)
	_peak_specs.append({"radius": radius, "height": height, "foot": foot,
		"rock": finish.albedo_color, "variant": variant, "snowline": snowline})


func _build_basin() -> void:
	var meadow := _geometry.material(Color("3f4634"), 0.0, 0.96, Color.BLACK, 1.0, _geometry.bump_concrete)
	# A frame around the grade (x in [-240, 240], z in [-300, 180]), not a
	# sheet under it: two surfaces 0.3 m apart z-fight from any distance.
	var floor_y := VALLEY_FLOOR_TOP - 0.5
	for piece in [
			[Vector3(5200.0, 1.0, 2400.0), Vector3(0.0, floor_y, -1500.0)],
			[Vector3(5200.0, 1.0, 2320.0), Vector3(0.0, floor_y, 1340.0)],
			[Vector3(2360.0, 1.0, 480.0), Vector3(-1420.0, floor_y, -60.0)],
			[Vector3(2360.0, 1.0, 480.0), Vector3(1420.0, floor_y, -60.0)]]:
		_geometry.add_box("ValleyFloor", piece[0], piece[1], meadow)

	var rock := _geometry.material(Color("353b41"), 0.03, 0.93)
	var rock_warm := _geometry.material(Color("3d3934"), 0.03, 0.93)
	var snow := _geometry.material(Color("b9c2cc"), 0.0, 0.78)
	var rng := RandomNumberGenerator.new()
	rng.seed = 1100
	for index in BASIN_RIM_RIDGES:
		var angle := TAU * float(index) / float(BASIN_RIM_RIDGES) + rng.randf_range(-0.01, 0.01)
		var reach := BASIN_RIM_RADIUS + rng.randf_range(-15.0, 15.0)
		var height := rng.randf_range(280.0, 470.0)
		var radius := height * 0.62
		var base := BASIN_CENTER + Vector3(sin(angle) * reach, 0.0, cos(angle) * reach)
		_add_peak("RimRidge", radius, height, base,
			rock if index % 3 != 0 else rock_warm, index + 10, 0.62 if height > 400.0 else 0.79)
		if height > 400.0:
			_export_only_cone("RimRidgeSnow", radius * 0.3, height * 0.28,
				base + Vector3(0.0, height * 0.93, 0.0), snow)

	# Conifers scattered over the meadow, clear of the yard and the tower.
	var placed := 0
	while placed < 48:
		var angle := rng.randf_range(0.0, TAU)
		var reach := rng.randf_range(330.0, BASIN_RIM_RADIUS - 260.0)
		var at := BASIN_CENTER + Vector3(sin(angle) * reach, VALLEY_FLOOR_TOP, cos(angle) * reach)
		if at.z < -420.0 and absf(at.x) < 700.0:
			continue  # the near peaks' own foothill forest already covers the north
		_add_tree(at, rng.randf_range(8.0, 15.0), rng)
		placed += 1


# A treeline at the base of the near peaks, mixed in with the rock cones
# already there -- the mountains stop being bare geometry and start being a
# real slope something grows on.
func _build_foothill_forest() -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = 4021
	var bands := [
		{"x": -380.0, "z": -520.0, "spread": 140.0},
		{"x": -160.0, "z": -560.0, "spread": 150.0},
		{"x": 140.0, "z": -570.0, "spread": 160.0},
		{"x": 420.0, "z": -540.0, "spread": 130.0},
	]
	for band in bands:
		var bx: float = band["x"]
		var bz: float = band["z"]
		var spread: float = band["spread"]
		for _tree in range(14):
			var tx := bx + rng.randf_range(-spread, spread)
			var tz := bz + rng.randf_range(-spread * 0.5, spread * 0.5)
			_add_tree(Vector3(tx, 0.0, tz), rng.randf_range(7.0, 13.0), rng)


# One stylised conifer: a trunk and three descending, widening canopy tiers.
# Cheap enough to scatter by the dozen, varied enough per-instance (scale,
# yaw, a hue jitter across the greens) that a cluster doesn't read as one
# mesh copy-pasted.
func _add_tree(at: Vector3, height: float, rng: RandomNumberGenerator) -> void:
	var hue_jitter := rng.randf_range(-0.03, 0.03)
	var canopy := _geometry.material(Color(0.16 + hue_jitter, 0.28 + hue_jitter, 0.14, 1.0), 0.0, 0.92)
	var trunk_material := _geometry.material(Color("362316"), 0.0, 0.9)

	var tree := Node3D.new()
	tree.name = "Conifer"
	tree.position = at
	tree.rotation.y = rng.randf_range(0.0, TAU)
	tree.scale = Vector3.ONE * rng.randf_range(0.85, 1.25)
	_geometry.presentation_root.add_child(tree)

	var trunk_height := height * 0.32
	_geometry.add_cylinder("TreeTrunk", height * 0.045, trunk_height,
		Vector3(0.0, trunk_height * 0.5, 0.0), trunk_material, tree)
	for tier in range(3):
		var t := float(tier) / 2.0
		var tier_radius := lerpf(height * 0.34, height * 0.11, t)
		var tier_height := height * 0.4
		var tier_y := trunk_height + t * height * 0.5
		_geometry.add_cone("TreeCanopy", tier_radius, tier_height,
			Vector3(0.0, tier_y + tier_height * 0.5, 0.0), canopy, tree)


# Low scrub and ground bushes: irregular clusters of squashed spheres, no
# trunk, filling the gap between bare grade and full trees.
func _add_bush(at: Vector3, spread: float, rng: RandomNumberGenerator) -> void:
	var hue_jitter := rng.randf_range(-0.04, 0.04)
	var bush_material := _geometry.material(Color(0.2 + hue_jitter, 0.3 + hue_jitter, 0.15, 1.0), 0.0, 0.94)
	var bush := Node3D.new()
	bush.name = "Scrub"
	bush.position = at
	_geometry.presentation_root.add_child(bush)
	for _lobe in range(rng.randi_range(3, 5)):
		var lobe_at := Vector3(rng.randf_range(-spread, spread), rng.randf_range(0.1, spread * 0.5),
			rng.randf_range(-spread, spread))
		var lobe := _geometry.add_sphere("ScrubLobe", rng.randf_range(spread * 0.45, spread * 0.75),
			lobe_at, bush_material, bush)
		lobe.scale.y = 0.72


# Foliage scattered through the yard itself: away from every kernel/stack/
# plant footprint, so it reads as the site being slowly reclaimed rather than
# clipping through a wall. Two open bands exist by construction -- west of
# the tower and plant, and along the north/entrance edge -- and this stays
# inside them.
func build_foliage() -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = 7733

	for _tree in range(22):
		var tx := rng.randf_range(-225.0, -95.0)
		var tz := rng.randf_range(-260.0, 30.0)
		_add_tree(Vector3(tx, 0.0, tz), rng.randf_range(6.0, 11.0), rng)
	for _tree in range(10):
		var tx := rng.randf_range(-190.0, 190.0)
		var tz := rng.randf_range(110.0, 165.0)
		_add_tree(Vector3(tx, 0.0, tz), rng.randf_range(6.0, 10.0), rng)

	for _bush in range(26):
		var bx := rng.randf_range(-225.0, -90.0)
		var bz := rng.randf_range(-260.0, 40.0)
		_add_bush(Vector3(bx, 0.0, bz), rng.randf_range(1.1, 2.4), rng)
	for _bush in range(14):
		var bx := rng.randf_range(-190.0, 190.0)
		var bz := rng.randf_range(100.0, 170.0)
		_add_bush(Vector3(bx, 0.0, bz), rng.randf_range(1.0, 2.0), rng)


func _build_waterfall(at: Vector3) -> void:
	var falls := StandardMaterial3D.new()
	falls.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	falls.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	falls.albedo_color = Color(0.86, 0.9, 0.94, 0.55)
	falls.cull_mode = BaseMaterial3D.CULL_DISABLED
	var quad := QuadMesh.new()
	quad.size = Vector2(26.0, 420.0)
	quad.material = falls
	var instance := MeshInstance3D.new()
	instance.name = "Waterfall"
	instance.set_meta(&"part", "Waterfall")
	instance.mesh = quad
	instance.position = at + Vector3(0.0, 210.0, 0.0)
	_geometry.presentation_root.add_child(instance)

	var mist := CPUParticles3D.new()
	mist.name = "WaterfallMist"
	mist.position = at + Vector3(0.0, 8.0, 8.0)
	mist.amount = 50
	mist.lifetime = 6.0
	mist.direction = Vector3(0.0, 1.0, 0.4)
	mist.spread = 40.0
	mist.gravity = Vector3(0.0, 1.4, 1.6)
	mist.scale_amount_min = 8.0
	mist.scale_amount_max = 20.0
	mist.emitting = true
	var mist_quad := QuadMesh.new()
	mist_quad.size = Vector2(2.0, 2.0)
	var mist_material := StandardMaterial3D.new()
	mist_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	mist_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mist_material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	mist_material.albedo_color = Color(0.88, 0.91, 0.94, 0.16)
	mist_quad.material = mist_material
	mist.mesh = mist_quad
	_geometry.presentation_root.add_child(mist)
