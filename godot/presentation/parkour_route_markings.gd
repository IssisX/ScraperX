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
	_duct_tuck_route(geometry, root, paint)
	_palm_rail_route(geometry, root, paint)
	_workshop_roof_route(geometry, root, paint)
	_upward_wall_step_route(geometry, root, paint)
	_wall_run_route(geometry, root, paint)
	_corner_hang_route(geometry, root, paint)
	_swing_transfer_route(geometry, root, paint)


static func _swing_transfer_route(geometry: WorldGeometry, root: Node3D, paint: Material) -> void:
	var offset := root.position
	_arrow(geometry, root, paint, Vector3(-50.0, 0.0, -54.0) - offset, Vector2(0.0, -1.0))
	_roof_label(root, paint, "SWING TRANSFER\nJUMP TO THE FIRST BAR\nLEAN TOWARD THE NEXT BAR\nACTION: REACH NEXT BAR\nJUMP / DROP: LET GO",
		Vector3(-52.6, 0.009, -60.3) - offset)
	for step in 3:
		_strip(geometry, root, paint,
			Vector3(-52.1 + float(step) * 0.45, 0.15 * float(step + 1), -59.0) - offset,
			Vector2(0.07, 1.2))
	_arrow(geometry, root, paint, Vector3(-50.4, 0.45, -59.0) - offset, Vector2.RIGHT)
	_arrow(geometry, root, paint, Vector3(-46.2, 0.30, -59.0) - offset, Vector2.RIGHT)


static func upper_gallery(geometry: WorldGeometry, root: Node3D, paint: Material,
		perches: Array[Vector3]) -> void:
	_roof_label(root, paint, "HIGH LINE\nSTAGGERED INSPECTION PERCHES\nMISSED JUMP: RETURN BELOW",
		Vector3(-68.6, 0.009, -60.0))
	_arrow(geometry, root, paint, Vector3(-69.2, 0.0, -61.2), Vector2.LEFT)
	for top in perches:
		# Edge paint follows actual deck dimensions and stays inset. It does
		# not add an invisible landing surface across the existing gallery gap.
		for x_sign in [-1.0, 1.0]:
			_strip(geometry, root, paint, top + Vector3(x_sign * 0.59, 0.0, 0.0), Vector2(0.08, 1.5))
		for z_sign in [-1.0, 1.0]:
			_strip(geometry, root, paint, top + Vector3(0.0, 0.0, z_sign * 0.77), Vector2(1.1, 0.08))
		var launch := Vector3(-76.0, 2.4, top.z + 2.9)
		var direction := Vector2(top.x - launch.x, top.z - launch.z).normalized()
		_arrow(geometry, root, paint, launch, direction)
		_arrow(geometry, root, paint, top, Vector2(-direction.x, direction.y))
	_roof_label(root, paint, "HIGH LINE EXIT\nSOUTH STAIRS / RETURN AISLE", Vector3(-76.0, 2.409, -97.0))


static func _corner_hang_route(geometry: WorldGeometry, root: Node3D, paint: Material) -> void:
	# The same actual workshop slab supplies both exposed grip faces. Paint
	# stays inset on its top and supplies no collision or hand support.
	var offset := root.position
	_roof_label(root, paint, "CORNER HANG\nBACK TO EDGE + DROP\nMOVE DIAGONALLY TO TURN\nACTION TO PULL UP",
		Vector3(-41.4, 5.149, -35.9) - offset)
	_strip(geometry, root, paint, Vector3(-43.1, 5.14, -38.32) - offset, Vector2(0.8, 0.08))
	_strip(geometry, root, paint, Vector3(-44.07, 5.14, -37.5) - offset, Vector2(0.08, 0.8))


static func _wall_run_route(geometry: WorldGeometry, root: Node3D, paint: Material) -> void:
	var offset := root.position
	_roof_label(root, paint, "WALL RUN\nSPRINT + JUMP ALONG WALL\nLEAN INTO THE WALL\nJUMP TO KICK AWAY",
		Vector3(-39.0, 0.009, -40.2) - offset)
	_arrow(geometry, root, paint, Vector3(-42.5, 0.0, -39.5) - offset, Vector2.RIGHT)
	_arrow(geometry, root, paint, Vector3(-30.0, 0.0, -39.5) - offset, Vector2.RIGHT)


static func _upward_wall_step_route(geometry: WorldGeometry, root: Node3D, paint: Material) -> void:
	# This grade approach faces the workshop's narrow south-wall lip. It is
	# a step/catch/release opportunity, separate from the roof-access stairs.
	var offset := root.position
	_roof_label(root, paint, "WALL STEP\nSPRINT + JUMP\nCATCH THE LIP\nDROP TO RETRY",
		Vector3(-41.0, 0.009, -45.2) - offset, PI)
	_arrow(geometry, root, paint, Vector3(-41.0, 0.0, -43.2) - offset, Vector2(0.0, 1.0))
	_strip(geometry, root, paint, Vector3(-41.0, 0.0, -42.0) - offset, Vector2(1.2, 0.10))


static func _palm_rail_route(geometry: WorldGeometry, root: Node3D, paint: Material) -> void:
	# Native rail and takeoff coordinates are world-space; the existing yard
	# container supplies only translation. Paint adds no receiving surface.
	var offset := root.position
	_arrow(geometry, root, paint, Vector3(-46.0, 0.0, -65.0) - offset, Vector2.RIGHT)
	_roof_label(root, paint, "RAIL CROSSING\nJUMP, THEN HOLD CROUCH",
		Vector3(-42.0, 0.009, -65.0) - offset)
	_arrow(geometry, root, paint, Vector3(-42.0, 0.0, -66.5) - offset, Vector2(0.0, -1.0))
	_strip(geometry, root, paint, Vector3(-42.0, 0.0, -67.6) - offset, Vector2(1.4, 0.10))
	# This alternative uses actual compact clearance under the same cap.
	_roof_label(root, paint, "OR CROUCH UNDER", Vector3(-42.0, 0.009, -70.2) - offset)
	_strip(geometry, root, paint, Vector3(-42.0, 1.65, -72.0) - offset, Vector2(1.2, 0.07))
	# The west aisle clears both the rail and existing east storage trays.
	_arrow(geometry, root, paint, Vector3(-42.0, 0.0, -75.0) - offset, Vector2.LEFT)
	_arrow(geometry, root, paint, Vector3(-45.0, 0.0, -73.5) - offset, Vector2(0.0, 1.0))


static func _duct_tuck_route(geometry: WorldGeometry, root: Node3D, paint: Material) -> void:
	# The direct jump uses the existing duct's real north face and roof.
	# Ground-level bypasses and the separate crouch-under route remain open.
	var offset := root.position
	_roof_label(root, paint, "DUCT CROSSING\nJUMP, THEN HOLD CROUCH",
		Vector3(-57.0, 0.009, -66.2) - offset)
	_arrow(geometry, root, paint, Vector3(-57.0, 0.0, -68.0) - offset, Vector2(0.0, -1.0))
	_strip(geometry, root, paint, Vector3(-57.0, 0.0, -68.65) - offset, Vector2(1.4, 0.10))
	_strip(geometry, root, paint, Vector3(-57.0, 1.65, -72.65) - offset, Vector2(1.4, 0.10))
	_arrow(geometry, root, paint, Vector3(-57.0, 1.65, -74.0) - offset, Vector2(0.0, -1.0))
	_arrow(geometry, root, paint, Vector3(-57.0, 0.0, -78.0) - offset, Vector2(0.0, -1.0))
	_strip(geometry, root, paint, Vector3(-57.0, 0.0, -81.0) - offset, Vector2(0.12, 1.2))
	_arrow(geometry, root, paint, Vector3(-57.0, 0.0, -84.5) - offset, Vector2.RIGHT)


static func _workshop_roof_route(geometry: WorldGeometry, root: Node3D, paint: Material) -> void:
	# Native service plinth/treads and workshop slab use world coordinates.
	# ParkourYard's container has translation only; remove that translation
	# here so new paint stays on the actual solids without moving old paths.
	var offset := root.position
	_arrow(geometry, root, paint, Vector3(-46.5, 0.0, -43.5) - offset, Vector2.RIGHT)
	_strip(geometry, root, paint, Vector3(-42.5, 0.0, -43.5) - offset, Vector2(1.2, 0.12))
	_arrow(geometry, root, paint, Vector3(-39.2, 0.0, -44.0) - offset, Vector2(0.0, -1.0))
	_strip(geometry, root, paint, Vector3(-39.2, 0.0, -46.5) - offset, Vector2(0.12, 1.2))
	_strip(geometry, root, paint, Vector3(-38.0, 0.0, -48.5) - offset, Vector2(1.2, 0.12))
	_roof_label(root, paint, "ROOF ACCESS", Vector3(-38.7, 0.009, -50.0) - offset)
	_arrow(geometry, root, paint, Vector3(-36.0, 0.0, -48.5) - offset, Vector2(0.0, 1.0))
	# Eight inset nosings expose the real 225 mm rises, clear of tread edges.
	for i in 8:
		var top := 1.8 * float(i + 1) / 8.0
		var near_z := -47.4 + 0.4 * float(i)
		_strip(geometry, root, paint, Vector3(-36.0, top, near_z + 0.08) - offset,
			Vector2(1.2, 0.08))
	# Direction and paired gap-edge bars remain entirely on real concrete.
	_arrow(geometry, root, paint, Vector3(-36.0, 1.8, -42.7) - offset, Vector2(0.0, 1.0))
	_strip(geometry, root, paint, Vector3(-36.0, 1.8, -41.55) - offset, Vector2(2.0, 0.10))
	_strip(geometry, root, paint, Vector3(-36.0, 5.14, -38.35) - offset, Vector2(2.0, 0.10))
	# The roof's west edge faces the open return yard, away from the south
	# wall-step lane. The cue describes the prepared landing at actual grade.
	_roof_label(root, paint, "YARD RETURN\nRUN, CROUCH + DROP", Vector3(-41.8, 5.149, -28.4) - offset)
	_arrow(geometry, root, paint, Vector3(-43.3, 5.14, -30.0) - offset, Vector2.LEFT)
	_strip(geometry, root, paint, Vector3(-44.08, 5.14, -30.0) - offset, Vector2(0.10, 1.6))


static func _roof_label(root: Node3D, paint: Material, text: String, at: Vector3,
		yaw: float = 0.0) -> void:
	var label := Label3D.new()
	label.text = text
	label.font_size = 64
	label.pixel_size = 0.006
	label.modulate = (paint as StandardMaterial3D).albedo_color
	label.outline_size = 0
	label.no_depth_test = false
	label.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	label.position = at
	label.rotation.x = -PI / 2.0
	label.rotation.y = yaw
	label.visibility_range_end = 45.0
	root.add_child(label)


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
