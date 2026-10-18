extends RefCounted
## Authored west-apron practice network. Native SolidExport owns all collision.
## Construction alone is not route acceptance; see PARKOUR_YARD_LAYOUT_20261015.
const WorldGeometry := preload("res://presentation/world_geometry.gd")
const RouteMarkings := preload("res://presentation/parkour_route_markings.gd")
const YARD_OFFSET := Vector3(4.0, 0.0, 0.0)
# Local deck-top centres. The alternating maintenance perches branch off the
# existing2.4m gallery; native export supplies their actual collision.
const UPPER_GALLERY_PERCHES: Array[Vector3] = [
	Vector3(-73.8, 3.0, -67.5),
	Vector3(-78.2, 3.2, -78.5),
	Vector3(-73.8, 3.0, -89.5),
]
var _g: WorldGeometry
var _root: Node3D
var _concrete: Material
var _steel: Material
var _paint: Material

func _init(geometry: WorldGeometry) -> void:
	_g = geometry

func build() -> void:
	_root = Node3D.new()
	_root.name = "WestApronParkour"
	# Clear the existing storage cage at x=-76.22 without removing it.
	_root.position = YARD_OFFSET
	_root.set_meta(&"static_render_container", true)
	_g.presentation_root.add_child(_root)
	_concrete = _g.material(Color("858679"), 0.0, 0.94, Color.BLACK, 1.0, _g.bump_concrete)
	_steel = _g.material(Color("42595b"), 0.58, 0.78, Color.BLACK, 1.0, _g.bump_steel)
	# Explicit export metadata; native solved contact chooses the sound. Values
	# match SupportSoundSurface and do not change friction or collision.
	_concrete.set_meta(&"support_sound_surface", 1)
	_steel.set_meta(&"support_sound_surface", 2)
	_paint = _g.material(Color("bcaa75"), 0.0, 0.95)
	_utility_bay()
	_duct_gallery()
	_loading_apron()
	_return_gallery()
	_upper_gallery()
	# The service aisle remains an ordinary walkable recovery route.
	for z in [-54.0, -66.0, -78.0, -90.0, -99.0]:
		_box("ParkourPaint", Vector3(0.12, 0.003, 3.0), Vector3(-50.0, 0.004, z), _paint)
	RouteMarkings.build(_g, _root, _paint)
	RouteMarkings.upper_gallery(_g, _root, _paint, UPPER_GALLERY_PERCHES)

func _utility_bay() -> void:
	# Ground-bearing concrete equipment plinths: 1.6m gaps, 2.4m landings.
	for i in 5:
		var height := 0.45 + float(i) * 0.18
		_box("ParkourStructure", Vector3(2.4, height, 2.4),
			Vector3(-56.0 - float(i) * 4.0, height * 0.5, -55.0), _concrete)
	# Low utility-bay partitions allow a jump/vault line or a walk around.
	for x in [-58.0, -64.0]:
		_box("ParkourStructure", Vector3(3.5, 0.8, 0.45), Vector3(x, 0.4, -62.0), _concrete)

func _duct_gallery() -> void:
	# A real supported service duct: 1.4m underside clearance, top at 1.65m.
	_box("ParkourStructure", Vector3(12.0, 0.25, 3.0), Vector3(-59.0, 1.525, -74.0), _steel)
	for x in [-64.5, -59.0, -53.5]:
		for z in [-75.25, -72.75]:
			_box("ParkourStructure", Vector3(0.25, 1.4, 0.25), Vector3(x, 0.7, z), _steel)
	# End braces remain at the edges, outside the 2.25m crouch aisle.
	for z in [-75.25, -72.75]:
		_strut(Vector3(-64.5, 0.15, z), Vector3(-62.5, 1.4, z))
		_strut(Vector3(-55.5, 1.4, z), Vector3(-53.5, 0.15, z))
	# Two concrete retaining faces form a 3.6m wide, open-ended wall lane.
	# Optional wall plants/catches have an ordinary ground exit at either end.
	for x in [-71.0, -67.0]:
		_box("ParkourStructure", Vector3(0.4, 2.6, 8.0), Vector3(x, 1.3, -78.0), _concrete)
	# A broad end plinth makes the duct top accessible with ordinary jumps.
	_box("ParkourStructure", Vector3(2.0, 0.8, 3.0), Vector3(-51.5, 0.4, -74.0), _concrete)

func _loading_apron() -> void:
	# Solid ground-bearing dock foundations, 2m gaps and 5x6m receiving areas.
	for i in 3:
		var height := 1.5 if i == 1 else 1.2
		_box("ParkourStructure", Vector3(5.0, height, 6.0),
			Vector3(-70.0 + float(i) * 7.0, height * 0.5, -90.0), _concrete)
	# Low service step at the east entrance; dock perimeter stays walkable.
	_box("ParkourStructure", Vector3(2.0, 0.6, 3.0), Vector3(-52.5, 0.3, -90.0), _concrete)

func _return_gallery() -> void:
	# Four braced inspection bays. 2m gaps separate the 3m-wide decks.
	# Every deck bears directly on four posts; braces meet those same joints.
	for z in [-62.0, -73.0, -84.0, -95.0]:
		_box("ParkourStructure", Vector3(3.0, 0.2, 9.0), Vector3(-76.0, 2.3, z), _steel)
		for x in [-77.2, -74.8]:
			for dz in [-4.0, 4.0]:
				_box("ParkourStructure", Vector3(0.22, 2.2, 0.22), Vector3(x, 1.1, z + dz), _steel)
			_strut(Vector3(x, 0.11, z - 4.0), Vector3(x, 2.2, z + 4.0))
	# Broad concrete maintenance stairs at both ends, 0.6m rise per jump.
	# These are jump steps, not an assertion of automatic stair climbing.
	for z in [-62.0, -98.0]:
		for i in 3:
			var height := 0.6 + float(i) * 0.6
			_box("ScrambleRung", Vector3(2.0, height, 2.4),
				Vector3(-70.0 - float(i) * 2.0, height * 0.5, z), _concrete)

func _upper_gallery() -> void:
	# Brackets bear on the existing gallery's paired post heads. New ground
	# posts would block the proven recovery aisle atz=-67.5; every bracket
	# stays above2m. The west middle perch clears both existing storage cages.
	for index in UPPER_GALLERY_PERCHES.size():
		var top := UPPER_GALLERY_PERCHES[index]
		var deck := _box("ParkourStructure", Vector3(1.4, 0.18, 1.8),
			top - Vector3.UP * 0.09, _steel)
		deck.set_meta(&"upper_gallery_perch", index)
		var post_x := -74.8 if top.x > -76.0 else -77.2
		for side in [-1.0, 1.0]:
			_box("ParkourStructure", Vector3(1.4, 0.16, 0.16),
				top + Vector3(0.0, -0.26, side * 0.58), _steel)
			for end_x in [-0.5, 0.5]:
				_strut(Vector3(post_x, 2.14, top.z + side * 1.5),
					top + Vector3(end_x, -0.34, side * 0.58))


func _box(part: String, size: Vector3, at: Vector3, material: Material) -> MeshInstance3D:
	var instance := _g.add_box_to(part, size, at, material, _root)
	instance.visibility_range_end = 180.0
	instance.visibility_range_end_margin = 12.0
	return instance

func _strut(from: Vector3, to: Vector3) -> void:
	var instance := _g.add_strut("ParkourStructure", from, to, 0.14, _steel, _root)
	instance.visibility_range_end = 180.0
	instance.visibility_range_end_margin = 12.0
