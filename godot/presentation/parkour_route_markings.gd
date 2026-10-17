extends RefCounted
## Surface paint for the existing west-apron yard; never a support surface.
const WorldGeometry := preload("res://presentation/world_geometry.gd")
const PAINT_HEIGHT := 0.003
const SURFACE_OFFSET := 0.004

static func build(geometry: WorldGeometry, root: Node3D, paint: Material) -> void:
	# These local points put the branch cue directly ahead of freight spawn
	# after the yard's 4 m translation; it reads before the distant obstacles.
	_arrow(geometry, root, paint, Vector3(-97.0, 0.0, -46.0), Vector2.RIGHT)
	var entrance := Label3D.new()
	entrance.text = "WEST APRON\nFOOT ACCESS"
	entrance.font_size = 64
	entrance.pixel_size = 0.006
	entrance.modulate = (paint as StandardMaterial3D).albedo_color
	entrance.outline_size = 0
	entrance.no_depth_test = false
	entrance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	entrance.position = Vector3(-98.0, 0.009, -48.0)
	entrance.rotation.x = -PI / 2.0
	entrance.visibility_range_end = 45.0
	root.add_child(entrance)
	# The north approach passes above the stock at x=-82,z=-48, then turns
	# toward the utility bay. Short breaks leave the ground visible.
	for x in [-88.0, -80.0, -72.0, -64.0, -56.0]:
		_strip(geometry, root, paint, Vector3(x, 0.0, -43.5), Vector2(2.0, 0.12))
	_arrow(geometry, root, paint, Vector3(-84.0, 0.0, -43.5), Vector2.RIGHT)
	for z in [-46.0, -48.6]:
		_strip(geometry, root, paint, Vector3(-56.0, 0.0, z), Vector2(0.12, 1.4))
	_arrow(geometry, root, paint, Vector3(-56.0, 0.0, -51.2), Vector2(0.0, -1.0))
	_arrow(geometry, root, paint, Vector3(-50.0, 0.0, -51.2), Vector2(0.0, -1.0))

	# Paired inset edge bars expose both sides of each real gap without
	# coating the landing areas. Heights match the existing concrete tops.
	for i in 5:
		var x := -56.0 - float(i) * 4.0
		var height := 0.45 + float(i) * 0.18
		for dx in [-1.04, 1.04]:
			_strip(geometry, root, paint, Vector3(x + dx, height, -55.0), Vector2(0.12, 1.6))
	for i in 3:
		var x := -70.0 + float(i) * 7.0
		var height := 1.5 if i == 1 else 1.2
		for dx in [-2.32, 2.32]:
			_strip(geometry, root, paint, Vector3(x + dx, height, -90.0), Vector2(0.12, 2.4))

	# Gallery gap bars sit on the steel decks, clear of their outside edges.
	for z in [-62.0, -73.0, -84.0, -95.0]:
		for dz in [-4.32, 4.32]:
			_strip(geometry, root, paint, Vector3(-76.0, 2.4, z + dz), Vector2(2.2, 0.12))
	# The same edge treatment identifies the two ground-access stair runs.
	for z in [-62.0, -98.0]:
		for i in 3:
			var height := 0.6 + float(i) * 0.6
			_strip(geometry, root, paint,
				Vector3(-69.18 - float(i) * 2.0, height, z), Vector2(0.12, 1.6))

	# Service duct and dock entries retain their full usable top surfaces.
	for x in [-64.82, -53.18]:
		_strip(geometry, root, paint, Vector3(x, 1.65, -74.0), Vector2(0.12, 2.2))
	_strip(geometry, root, paint, Vector3(-50.68, 0.8, -74.0), Vector2(0.12, 2.2))
	_strip(geometry, root, paint, Vector3(-51.68, 0.6, -90.0), Vector2(0.12, 2.2))

static func _strip(geometry: WorldGeometry, root: Node3D, paint: Material,
		at: Vector3, size: Vector2, yaw: float = 0.0) -> void:
	var instance := geometry.add_box_to("ParkourPaint",
		Vector3(size.x, PAINT_HEIGHT, size.y), at + Vector3.UP * SURFACE_OFFSET, paint, root)
	instance.rotation.y = yaw
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	instance.visibility_range_end = 100.0
	instance.visibility_range_end_margin = 8.0

static func _arrow(geometry: WorldGeometry, root: Node3D, paint: Material,
		at: Vector3, direction: Vector2) -> void:
	var forward := Vector3(direction.x, 0.0, direction.y)
	var side := Vector3(-direction.y, 0.0, direction.x)
	var tip := at + forward * 0.75
	_stroke(geometry, root, paint, at - forward * 0.75, tip)
	for sign_value in [-1.0, 1.0]:
		_stroke(geometry, root, paint, tip, tip - forward * 0.6 + side * sign_value * 0.4)

static func _stroke(geometry: WorldGeometry, root: Node3D, paint: Material,
		from: Vector3, to: Vector3) -> void:
	var delta := to - from
	_strip(geometry, root, paint, (from + to) * 0.5,
		Vector2(0.12, delta.length()), atan2(delta.x, delta.z))
