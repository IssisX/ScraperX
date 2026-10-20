extends "res://presentation/main_geometry.gd"

# The main scene, layer 2 of 6: the world. The Stack, the tower skin, the
# yard, lighting, the basin's mountains, forest and waterfall, and the
# works' signage. (The stock yard's containers, steel and drums are loose
# stock, kit bodies: stock_view.gd.)

const MountainMesh := preload("res://presentation/mountain_mesh.gd")
const YardGround := preload("res://presentation/yard_ground.gd")

# The stack. Mirrors the kStack* constants in simulation.cpp exactly -- these
# are the native collision sizes, so what is drawn is what you stand on.
const STACK_CENTER := Vector3(0.0, 0.0, -150.0)
const STACK_HALF_EXTENT := 26.0
const STACK_LEVEL_HEIGHT := 11.0
const STACK_LEVEL_COUNT := 14
const STACK_DECK_THICKNESS := 0.5
const STACK_DECK_BAND_DEPTH := 9.0
const STACK_COLUMN_SIZE := 1.6
var _vent_tops := PackedVector3Array()
@onready var _light_rig: Node3D = $LightRig

# --- world ------------------------------------------------------------------
#
# Industrial palette: mill scale, oxidised steel, poured concrete, wet asphalt,
# galvanised mesh, faded warning yellow, chipped hazard orange. Nothing here
# emits light except a sodium fitting, a fire box, or a vent -- colour is a
# consequence of material and weather, not a shader preset.

func _build_world() -> void:
	_build_bump_textures()

	# Palette: oxidised iron and rust carry the structure, weathered timber
	# softens it, mill scale is the dark shadow value, crane yellow and brass
	# lamplight are the warm accents -- and now verdigris copper, painted
	# machinery blue, and lichen staining break the rust/iron monochrome, the
	# way a real decades-old industrial site actually weathers.
	# The yard's asphalt: worn, patched, cracked and stained (yard_ground.gd).
	var asphalt := YardGround.material()
	var concrete := _material(Color("4e4841"), 0.0, 0.94, Color.BLACK, 1.0, _bump_concrete)
	# A real value ladder: near-black iron in shadow, mid rust for the frame,
	# brighter oxide only where light catches an edge. Bump-mapped: these
	# cover most of the structure's surface area, so this is where per-pixel
	# normal detail matters most.
	var mill_scale := _material(Color("1d1a17"), 0.72, 0.6, Color.BLACK, 1.0, _bump_steel)
	var oxidised := _material(Color("6b3520"), 0.3, 0.92, Color.BLACK, 1.0, _bump_steel)
	var rust_deep := _material(Color("3b1f13"), 0.28, 0.95, Color.BLACK, 1.0, _bump_steel)
	var rust_bright := _material(Color("9a5326"), 0.34, 0.82, Color.BLACK, 1.0, _bump_steel)
	var galvanised := _material(Color("5a5d5e"), 0.66, 0.5, Color.BLACK, 1.0, _bump_steel)
	var faded_yellow := _material(Color("b08a22"), 0.16, 0.68)
	var hazard := _material(Color("a04d16"), 0.18, 0.76)
	var tar := _material(Color("0e0f11"), 0.05, 0.62)
	var timber := _material(Color("4a3420"), 0.02, 0.9, Color.BLACK, 1.0, _bump_timber)
	# New accents: living colour against the rust.
	var verdigris := _material(Color("3f6b5c"), 0.42, 0.68, Color.BLACK, 1.0, _bump_steel)
	var lichen := _material(Color("57642e"), 0.0, 0.96)

	# Grade and the tower's upper mass: sizes mirror the native Jolt bodies.
	_add_box("Grade", Vector3(480.0, 1.0, 480.0), Vector3(0.0, -0.5, -60.0), asphalt)
	# The neighbouring shaft stands on the ground: 1.6 km from grade up.
	_add_box("TowerMass", Vector3(92.0, 1600.0, 80.0), Vector3(-30.0, 800.0, -330.0), concrete)

	_build_stack(mill_scale, oxidised, rust_deep, rust_bright, galvanised, faded_yellow, timber)
	_build_stack_accents(verdigris, lichen)
	_build_tower_skin(mill_scale, oxidised, galvanised, faded_yellow, timber)
	_build_yard(concrete, faded_yellow, tar)
	_build_mountains_and_waterfall()
	_build_kellerworks_signage(timber, faded_yellow)
	_build_foliage()
	_build_lighting()

# The stack: the tower's lower section. Deck rings and columns mirror real
# native collision one-for-one; bracing, rails, pipework and lamps are
# dressing hung on that frame. There is no stair: a level is gained by a
# machine or a climb. The player is inside this structure, so it is built to
# be seen from within as well as from the yard.
const MATRON_DECK_LEVEL := 14
const MATRON_GATE_WIDTH := 3.0
# Deck 13's walkway plate to the riser up to the 154 m deck (C3,
# src/sim/band_stack.cpp) meets the deck's north band at the shaft: the rail
# there is open from its post at x -8.5 to the plate's east end.
const DECK13_LEVEL := 13
const DECK13_GATE_WEST := -8.4
const DECK13_GATE_EAST := -5.1

func _build_stack(mill_scale: Material, oxidised: Material, rust_deep: Material,
		rust_bright: Material, galvanised: Material, faded: Material, timber: Material) -> void:
	var band_center := STACK_HALF_EXTENT - STACK_DECK_BAND_DEPTH * 0.5
	var inner_half := STACK_HALF_EXTENT - STACK_DECK_BAND_DEPTH
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z

	for level in range(1, STACK_LEVEL_COUNT + 1):
		var deck_y := float(level) * STACK_LEVEL_HEIGHT
		var slab_y := deck_y - STACK_DECK_THICKNESS * 0.5
		var deck_material: Material = galvanised if level % 2 == 1 else mill_scale

		for sz in [1.0, -1.0]:
			_add_box("StackDeck", Vector3(STACK_HALF_EXTENT * 2.0, STACK_DECK_THICKNESS,
				STACK_DECK_BAND_DEPTH), Vector3(cx, slab_y, cz + sz * band_center), deck_material)
		for sx in [1.0, -1.0]:
			_add_box("StackDeck", Vector3(STACK_DECK_BAND_DEPTH, STACK_DECK_THICKNESS,
				inner_half * 2.0), Vector3(cx + sx * band_center, slab_y, cz), deck_material)

		# Edge beams around the shaft and the outer face: the structure reads
		# as fabricated plate girders, not floating slabs.
		for sz in [1.0, -1.0]:
			_add_box("ShaftEdgeBeam", Vector3(inner_half * 2.0, 0.9, 0.5),
				Vector3(cx, deck_y - 0.55, cz + sz * inner_half), oxidised)
			_add_box("OuterEdgeBeam", Vector3(STACK_HALF_EXTENT * 2.0, 1.1, 0.6),
				Vector3(cx, deck_y - 0.7, cz + sz * STACK_HALF_EXTENT), rust_deep)
		for sx in [1.0, -1.0]:
			_add_box("ShaftEdgeBeam", Vector3(0.5, 0.9, inner_half * 2.0),
				Vector3(cx + sx * inner_half, deck_y - 0.55, cz), oxidised)
			_add_box("OuterEdgeBeam", Vector3(0.6, 1.1, STACK_HALF_EXTENT * 2.0),
				Vector3(cx + sx * STACK_HALF_EXTENT, deck_y - 0.7, cz), rust_deep)

		# Handrails around the open shaft -- the safety line you walk beside.
		for sz in [1.0, -1.0]:
			if level == DECK13_LEVEL and sz > 0.0:
				for run in [[cx - inner_half, DECK13_GATE_WEST], [DECK13_GATE_EAST, cx + inner_half]]:
					_add_box("ShaftRail", Vector3(float(run[1]) - float(run[0]), 0.08, 0.08),
						Vector3((float(run[0]) + float(run[1])) * 0.5, deck_y + 1.05, cz + sz * inner_half), galvanised)
				continue
			_add_box("ShaftRail", Vector3(inner_half * 2.0, 0.08, 0.08),
				Vector3(cx, deck_y + 1.05, cz + sz * inner_half), galvanised)
		for sx in [1.0, -1.0]:
			if level == MATRON_DECK_LEVEL:
				# The 154 m deck's catwalk crosses the shaft here (src/sim/matron.hpp):
				# the rail on each side is open for MATRON_GATE_WIDTH of it.
				var run := inner_half - MATRON_GATE_WIDTH * 0.5
				for end in [1.0, -1.0]:
					_add_box("ShaftRail", Vector3(0.08, 0.08, run),
						Vector3(cx + sx * inner_half, deck_y + 1.05, cz + end * (MATRON_GATE_WIDTH * 0.5 + run * 0.5)),
						galvanised)
				continue
			_add_box("ShaftRail", Vector3(0.08, 0.08, inner_half * 2.0),
				Vector3(cx + sx * inner_half, deck_y + 1.05, cz), galvanised)
		for post_x in [-inner_half, -inner_half * 0.5, 0.0, inner_half * 0.5, inner_half]:
			for sz in [1.0, -1.0]:
				_add_box("ShaftPost", Vector3(0.09, 1.1, 0.09),
					Vector3(cx + post_x, deck_y + 0.55, cz + sz * inner_half), galvanised)

		# Timber decking planks laid over the walking band, warm against iron.
		for plank in range(-2, 3):
			for sz in [1.0, -1.0]:
				var plank_z := band_center + float(plank) * 1.35
				_add_box("DeckPlank", Vector3(STACK_HALF_EXTENT * 2.0 - 2.0, 0.08, 1.1),
					Vector3(cx, deck_y + 0.05, cz + sz * plank_z), timber)

	# Columns, and the diagonal bracing that makes a frame a frame.
	for level in range(0, STACK_LEVEL_COUNT):
		var base_y := float(level) * STACK_LEVEL_HEIGHT
		var mid_y := base_y + STACK_LEVEL_HEIGHT * 0.5

		for sx in [1.0, -1.0]:
			for sz in [1.0, -1.0]:
				_add_box("StackColumn", Vector3(STACK_COLUMN_SIZE, STACK_LEVEL_HEIGHT,
					STACK_COLUMN_SIZE), Vector3(cx + sx * STACK_HALF_EXTENT, mid_y,
					cz + sz * STACK_HALF_EXTENT), rust_deep)
			_add_box("StackColumn", Vector3(STACK_COLUMN_SIZE, STACK_LEVEL_HEIGHT,
				STACK_COLUMN_SIZE), Vector3(cx + sx * STACK_HALF_EXTENT, mid_y, cz), rust_deep)
			_add_box("StackColumn", Vector3(STACK_COLUMN_SIZE, STACK_LEVEL_HEIGHT,
				STACK_COLUMN_SIZE), Vector3(cx, mid_y, cz + sx * STACK_HALF_EXTENT), rust_deep)

	# The outer faces' bracing: a diagrid of two-storey diagonals, 22 m up over
	# 13 m along the face, meeting at the corner and mid-span columns and at
	# the edge beams half way between. At 59 degrees a brace's top is too steep
	# to stand on (the native's support limit is 56), so no brace is a ramp
	# from one deck to the next.
	var tier_height := STACK_LEVEL_HEIGHT * 2.0
	var half_bay := STACK_HALF_EXTENT * 0.5
	for tier in range(0, int(STACK_LEVEL_COUNT / 2.0)):
		var base_y := float(tier) * tier_height
		for side in [1.0, -1.0]:
			var outer := [0.0, side * half_bay, side * STACK_HALF_EXTENT]
			var legs: Array = []
			if tier % 2 == 0:
				legs = [[outer[0], outer[1]], [outer[2], outer[1]]]
			else:
				legs = [[outer[1], outer[0]], [outer[1], outer[2]]]
			for leg in legs:
				var along0: float = leg[0]
				var along1: float = leg[1]
				var length := sqrt(pow(along1 - along0, 2.0) + pow(tier_height, 2.0))
				var pitch := atan2(tier_height, along1 - along0)
				var mid_along := 0.5 * (along0 + along1)
				var mid_y := base_y + tier_height * 0.5
				for face in [1.0, -1.0]:
					var brace := _add_box("StackBrace", Vector3(length, 0.45, 0.45),
						Vector3(cx + mid_along, mid_y, cz + face * STACK_HALF_EXTENT), oxidised)
					brace.rotation = Vector3(0.0, 0.0, pitch)
					var brace_z := _add_box("StackBrace", Vector3(0.45, 0.45, length),
						Vector3(cx + face * STACK_HALF_EXTENT, mid_y, cz + mid_along), oxidised)
					brace_z.rotation = Vector3(-pitch, 0.0, 0.0)

	_build_stack_dressing(mill_scale, oxidised, rust_deep, rust_bright, galvanised, faded)
	_build_stack_megastructure(mill_scale, oxidised, rust_deep, rust_bright, galvanised,
		faded, timber)

# Everything that makes the frame read as one vast building rather than a
# repeated scaffold: splayed footings, clad machine halls with lit windows,
# company signage, and walkways striking out into the air. No machine is
# dressing: a machine here is a native one that works.
func _build_stack_megastructure(mill_scale: Material, oxidised: Material, rust_deep: Material,
		rust_bright: Material, galvanised: Material, faded: Material, timber: Material) -> void:
	var banner_cloth := _material(Color("5e2220"), 0.0, 0.95)
	var sign_plate := _material(Color("46423b"), 0.2, 0.86)
	var window_lit := _material(Color("2a2118"), 0.1, 0.7, Color("ffb45c"), 2.8)

	_build_stack_footings(rust_deep, oxidised, mill_scale)
	_build_stack_halls(timber, mill_scale, rust_deep, window_lit)
	_build_stack_signage(banner_cloth, sign_plate)
	_build_stack_bridges(galvanised, faded, rust_deep, mill_scale)

# Splayed footings. The references all plant their towers on legs that kick
# out well past the shaft, which is what gives them their sense of weight.
func _build_stack_footings(rust_deep: Material, oxidised: Material, mill_scale: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var reach := STACK_HALF_EXTENT + 13.0
	var meet_y := STACK_LEVEL_HEIGHT * 3.0

	for sx in [1.0, -1.0]:
		for sz in [1.0, -1.0]:
			var foot := Vector3(cx + sx * reach, 0.0, cz + sz * reach)
			var head := Vector3(cx + sx * STACK_HALF_EXTENT, meet_y, cz + sz * STACK_HALF_EXTENT)
			_add_strut("Buttress", foot, head, 2.4, rust_deep)
			_add_box("ButtressFoot", Vector3(6.0, 2.6, 6.0),
				foot + Vector3(0.0, 1.3, 0.0), mill_scale)
			# Secondary tie back into the frame, one storey down.
			var tie_foot := foot.lerp(head, 0.42)
			var tie_head := Vector3(cx + sx * STACK_HALF_EXTENT, STACK_LEVEL_HEIGHT,
				cz + sz * STACK_HALF_EXTENT)
			_add_strut("ButtressTie", tie_foot, tie_head, 1.1, oxidised)

# Clad machine halls bolted onto the frame: solid volumes with lit windows,
# so the tower is not uniformly see-through and has interior worth reading.
func _build_stack_halls(timber: Material, mill_scale: Material, rust_deep: Material,
		window_lit: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var halls := [
		{"level": 3, "sx": -1.0, "storeys": 2.0, "depth": 13.0},
		{"level": 7, "sx": 1.0, "storeys": 3.0, "depth": 11.0},
		{"level": 11, "sx": -1.0, "storeys": 2.0, "depth": 12.0},
	]
	for hall in halls:
		var level: float = hall["level"]
		var sx: float = hall["sx"]
		var storeys: float = hall["storeys"]
		var depth: float = hall["depth"]
		var height := storeys * STACK_LEVEL_HEIGHT
		var width := 9.0
		var centre := Vector3(cx + sx * (STACK_HALF_EXTENT + width * 0.5 - 1.0),
			level * STACK_LEVEL_HEIGHT + height * 0.5 - 1.0, cz)

		_add_box("MachineHall", Vector3(width, height, depth), centre, timber)
		_add_box("HallCapping", Vector3(width + 1.2, 0.9, depth + 1.2),
			centre + Vector3(0.0, height * 0.5 + 0.3, 0.0), rust_deep)
		_add_box("HallSill", Vector3(width + 1.0, 0.8, depth + 1.0),
			centre - Vector3(0.0, height * 0.5 + 0.2, 0.0), mill_scale)
		# Window grid on the outward face and the front face.
		for row in range(int(storeys) * 2):
			for column in range(3):
				var wy := centre.y - height * 0.5 + 2.6 + float(row) * 4.4
				var wz := cz - depth * 0.5 + 2.6 + float(column) * (depth - 5.2) * 0.5
				_add_box("HallWindow", Vector3(0.4, 1.9, 1.5),
					Vector3(centre.x + sx * (width * 0.5 + 0.1), wy, wz), window_lit)
			_add_box("HallWindowFront", Vector3(2.2, 1.9, 0.4),
				Vector3(centre.x, centre.y - height * 0.5 + 3.4 + float(row) * 4.4,
					cz + depth * 0.5 + 0.1), window_lit)
		# Ribs, so the cladding reads as boards on a frame.
		for rib in range(5):
			_add_box("HallRib", Vector3(0.5, height, 0.5),
				Vector3(centre.x - width * 0.5 + 0.5 + float(rib) * (width - 1.0) * 0.25,
					centre.y, cz + depth * 0.5 + 0.2), rust_deep)

# Company signage: hanging cloth banners and painted plate on the structure.
func _build_stack_signage(banner_cloth: Material, sign_plate: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var front_z := cz + STACK_HALF_EXTENT
	var emblem := _material(Color("ddd2c0"), 0.0, 0.8)

	var banners = [
		{"x": cx - 15.0, "level": 9.0, "lines": "KELLERWORKS", "sub": "MATERIALS\nMOVE\nCIVILIZATION\nRISES"},
		{"x": cx + 14.0, "level": 5.0, "lines": "", "sub": "PEOPLE\nPOWER\nPROGRESS"},
		{"x": cx - 4.0, "level": 12.0, "lines": "", "sub": "A HIGHER\nWORLD\nTOGETHER"},
	]
	for banner in banners:
		var bx: float = banner["x"]
		var top := float(banner["level"]) * STACK_LEVEL_HEIGHT - 0.8
		var height := 13.0
		var width := 5.0
		var centre := Vector3(bx, top - height * 0.5, front_z + 0.5)
		_add_box("Banner", Vector3(width, height, 0.12), centre, banner_cloth)
		_add_box("BannerRod", Vector3(width + 0.8, 0.22, 0.22),
			centre + Vector3(0.0, height * 0.5 + 0.2, 0.0), sign_plate)
		# Emblem: a canted bar cluster standing in for the company mark.
		for bar in range(2):
			var mark := _add_box("BannerMark", Vector3(2.4, 0.5, 0.06),
				centre + Vector3(0.0, height * 0.5 - 2.0, 0.09), emblem)
			mark.rotation = Vector3(0.0, 0.0, (0.7 if bar == 0 else -0.7))
		if String(banner["lines"]) != "":
			_add_sign_text(String(banner["lines"]),
				centre + Vector3(0.0, height * 0.5 - 4.1, 0.12), 0.0, 0.62, Color("efe5d4"))
		_add_sign_text(String(banner["sub"]),
			centre + Vector3(0.0, height * 0.5 - 7.4, 0.12), 0.0, 0.78, Color("e4d8c4"))

	# Painted plate high on the shaft, the biggest piece of lettering here.
	var plate_centre := Vector3(cx + 6.0, STACK_LEVEL_HEIGHT * 7.6, front_z + 0.45)
	_add_box("SignPlate", Vector3(11.0, 11.0, 0.3), plate_centre, sign_plate)
	for bar in range(2):
		var plate_mark := _add_box("SignPlateMark", Vector3(4.6, 0.9, 0.08),
			plate_centre + Vector3(0.0, 3.4, 0.2), emblem)
		plate_mark.rotation = Vector3(0.0, 0.0, (0.7 if bar == 0 else -0.7))
	_add_sign_text("HIGHER\nSTRONGER\nFURTHER", plate_centre + Vector3(0.0, -1.6, 0.25),
		0.0, 1.5, Color("ded2bd"))

	# Bay lettering down at the loading level, where the player starts.
	var bay_centre := Vector3(cx - 12.0, 6.4, front_z + 0.45)
	_add_box("BayPlate", Vector3(8.0, 9.0, 0.3), bay_centre, sign_plate)
	_add_sign_text("STACK 1", bay_centre + Vector3(0.0, 2.2, 0.25), 0.0, 2.1, Color("e8dcc6"))
	_add_sign_text("TO A HIGHER\nTOMORROW", bay_centre + Vector3(0.0, -1.8, 0.25), 0.0, 0.8,
		Color("cbbfa8"))

# Walkways striking out from the tower toward structures off in the weather.
func _build_stack_bridges(galvanised: Material, faded: Material, rust_deep: Material,
		mill_scale: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var spans = [
		{"y": STACK_LEVEL_HEIGHT * 6.0, "sx": 1.0, "length": 54.0},
		{"y": STACK_LEVEL_HEIGHT * 10.0, "sx": -1.0, "length": 46.0},
	]
	for span in spans:
		var y: float = span["y"]
		var sx: float = span["sx"]
		var length: float = span["length"]
		var from := Vector3(cx + sx * STACK_HALF_EXTENT, y, cz - 3.0)
		var to := from + Vector3(sx * length, -3.0, 0.0)

		_add_box("BridgeDeck", Vector3(length, 0.4, 4.4), (from + to) * 0.5, mill_scale)
		for rail_z in [-2.1, 2.1]:
			_add_box("BridgeRail", Vector3(length, 0.1, 0.1),
				(from + to) * 0.5 + Vector3(0.0, 1.15, rail_z), faded)
			for post in range(int(length / 4.0)):
				_add_box("BridgePost", Vector3(0.12, 1.2, 0.12),
					from + Vector3(sx * (2.0 + float(post) * 4.0), 0.6, rail_z), galvanised)
		# Under-truss, so the span looks like it could carry itself: level under
		# the level deck. (Its line once fell 3 m along the span with from-to,
		# so near the tower the webs stood 1.2 m up through the deck.)
		var under := Vector3(0.0, (from.y + to.y) * 0.5 - 0.2 - from.y, 0.0)
		var deck_from := from + under
		var deck_to := Vector3(to.x, deck_from.y, to.z)
		for web in range(int(length / 6.0)):
			var a := float(web) / (length / 6.0)
			var b := (float(web) + 1.0) / (length / 6.0)
			_add_strut("BridgeWeb", deck_from.lerp(deck_to, a),
				deck_from.lerp(deck_to, b) - Vector3(0.0, 2.4, 0.0), 0.3, rust_deep)
		_add_strut("BridgeChord", deck_from - Vector3(0.0, 2.4, 0.0), deck_to - Vector3(0.0, 2.4, 0.0),
			0.42, rust_deep)
		# A pylon out at the far end, implying the span lands somewhere.
		_add_box("BridgePylon", Vector3(3.0, y * 0.94, 3.0),
			to + Vector3(sx * 2.0, -y * 0.5, 0.0), rust_deep)

# Colour that isn't rust: verdigris copper pipe runs and lichen staining low on the columns where damp and shade let something
# green actually take hold. A decades-old working plant is never one colour.
func _build_stack_accents(verdigris: Material, lichen: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var front_z := cz + STACK_HALF_EXTENT

	# A verdigris pipe run climbing the front face, distinct from the oxidised
	# risers in the shaft -- copper service lines age to blue-green, not rust.
	# West of the centre: the east half of the face carries S1 and C1.
	for sx in [-1.0]:
		_add_cylinder("VerdigrisPipe", 0.4, STACK_LEVEL_HEIGHT * float(STACK_LEVEL_COUNT),
			Vector3(cx + sx * (STACK_HALF_EXTENT - 4.5),
				STACK_LEVEL_HEIGHT * float(STACK_LEVEL_COUNT) * 0.5, front_z + 0.9), verdigris)
		for level in range(1, STACK_LEVEL_COUNT + 1):
			_add_box("VerdigrisFlange", Vector3(1.0, 0.3, 1.0),
				Vector3(cx + sx * (STACK_HALF_EXTENT - 4.5), float(level) * STACK_LEVEL_HEIGHT - 1.2,
					front_z + 0.9), verdigris)

	# Lichen staining low on every column, on the shaded (south) face, and
	# streaking down from every deck's drip line -- damp industrial concrete
	# and iron are never actually clean at the base.
	for sx in [1.0, -1.0]:
		for sz in [1.0, -1.0]:
			_add_box("ColumnLichen", Vector3(STACK_COLUMN_SIZE + 0.1, 3.5, STACK_COLUMN_SIZE + 0.1),
				Vector3(cx + sx * STACK_HALF_EXTENT, 1.75, cz + sz * STACK_HALF_EXTENT), lichen)
	for level in range(1, 5):
		var y := float(level) * STACK_LEVEL_HEIGHT
		for streak in range(-3, 4):
			_add_box("DeckStreak", Vector3(0.35, 2.4, 0.1),
				Vector3(cx + float(streak) * 4.0, y - 1.2, cz - STACK_HALF_EXTENT - 0.15), lichen)

# Pipework, gearing, vents and lamps hung on the frame. None of this is
# collision or authority -- it is what makes the frame read as a working
# plant rather than a jungle gym.
func _build_stack_dressing(mill_scale: Material, oxidised: Material, rust_deep: Material,
		rust_bright: Material, galvanised: Material, faded: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var inner_half := STACK_HALF_EXTENT - STACK_DECK_BAND_DEPTH

	# Riser pipes running the full height in the corners of the shaft.
	for sx in [1.0, -1.0]:
		for sz in [1.0, -1.0]:
			var pipe_material: Material = oxidised if sx * sz > 0.0 else rust_bright
			_add_box("Riser", Vector3(0.7, STACK_LEVEL_HEIGHT * STACK_LEVEL_COUNT, 0.7),
				Vector3(cx + sx * (inner_half - 1.2),
					STACK_LEVEL_HEIGHT * STACK_LEVEL_COUNT * 0.5,
					cz + sz * (inner_half - 1.2)), pipe_material)
			for level in range(1, STACK_LEVEL_COUNT + 1):
				_add_box("RiserFlange", Vector3(1.1, 0.35, 1.1),
					Vector3(cx + sx * (inner_half - 1.2), float(level) * STACK_LEVEL_HEIGHT - 1.4,
						cz + sz * (inner_half - 1.2)), mill_scale)

	# Vent stacks, and lamp fittings throwing warm light into the frame.
	var lamp_material := _material(Color("4a3a24"), 0.3, 0.7, Color("ffb04d"), 3.0)
	for level in range(1, STACK_LEVEL_COUNT + 1):
		var y := float(level) * STACK_LEVEL_HEIGHT
		if level % 2 == 0:
			continue
		for sx in [1.0, -1.0]:
			_add_box("VentStack", Vector3(0.8, 2.4, 0.8),
				Vector3(cx + sx * (STACK_HALF_EXTENT - 2.2), y + 1.2,
					cz - STACK_HALF_EXTENT + 2.2), mill_scale)
			_vent_tops.append(Vector3(cx + sx * (STACK_HALF_EXTENT - 2.2), y + 2.45,
				cz - STACK_HALF_EXTENT + 2.2))
			_add_box("LampFitting", Vector3(0.5, 0.3, 0.7),
				Vector3(cx + sx * (inner_half - 0.6), y + 2.6, cz), lamp_material)
			var lamp := OmniLight3D.new()
			lamp.name = "StackLamp"
			lamp.position = Vector3(cx + sx * (inner_half - 0.6), y + 2.4, cz)
			lamp.light_color = Color(1.0, 0.72, 0.38)
			lamp.light_energy = 3.2
			lamp.omni_range = 16.0
			lamp.omni_attenuation = 1.5
			_light_rig.add_child(lamp)

	# Hazard striping on the outer edge beams at the lower, most-seen levels.
	for level in range(1, 4):
		var y := float(level) * STACK_LEVEL_HEIGHT
		for stripe in range(-7, 8):
			_add_box("EdgeStripe", Vector3(1.1, 0.5, 0.12),
				Vector3(cx + float(stripe) * 2.3, y - 0.7, cz + STACK_HALF_EXTENT + 0.32),
				faded if stripe % 2 == 0 else mill_scale)

func _build_tower_skin(mill_scale: Material, oxidised: Material, galvanised: Material, faded: Material, timber: Material) -> void:
	# Relief on the neighbouring mass, now set well back across the yard. It is
	# a second Kellerworks shaft receding into the weather, not a lid over the
	# frame the player climbs.
	var mass_x := -30.0
	var face_z := -290.0
	var lit_band := _material(Color("2a2521"), 0.1, 0.8, Color("e0a040"), 0.9)
	var lit_band_dim := _material(Color("242019"), 0.1, 0.8, Color("a86c22"), 0.5)
	for offset in [-38.0, -19.0, 0.0, 19.0, 38.0]:
		_add_box("FacePier", Vector3(6.0, 460.0, 3.0),
			Vector3(mass_x + offset, 230.0, face_z), oxidised)
	for level in range(40, 520, 16):
		_add_box("FaceBand", Vector3(90.0, 1.4, 2.0),
			Vector3(mass_x, float(level), face_z - 0.4), mill_scale)
	for offset in [-30.0, -10.0, 10.0, 30.0]:
		_add_box("FaceDuct", Vector3(3.0, 380.0, 3.0),
			Vector3(mass_x + offset, 200.0, face_z + 1.6), galvanised)
	for level in range(30, 360, 12):
		var band: Material = lit_band if (level / 12) % 3 != 0 else lit_band_dim
		_add_box("FloorLight", Vector3(80.0, 1.0, 0.6),
			Vector3(mass_x, float(level), face_z - 1.3), band)
	for level in range(380, 900, 34):
		_add_box("FloorLightHigh", Vector3(74.0, 0.9, 0.6),
			Vector3(mass_x, float(level), face_z - 1.3), lit_band_dim)
	for offset in [-28.0, -9.0, 9.0, 28.0]:
		_add_box("TimberCladding", Vector3(8.0, 44.0, 1.2),
			Vector3(mass_x + offset, 62.0, face_z + 0.8), timber)

	_build_stack_continuation(oxidised, rust_deepen(oxidised), mill_scale, galvanised, faded)

# The frame does not stop where the player's reach does. Above the climbable
# stack the same columns, decks and bracing carry on for a few hundred metres,
# thinning out as they go, and the environment fog takes them the rest of the
# way. This is what makes the structure read as a skyscraper rather than a
# gantry: there is always more of it above you.
func _build_stack_continuation(oxidised: Material, rust_deep: Material, mill_scale: Material,
		galvanised: Material, faded: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var base_level := STACK_LEVEL_COUNT
	var top_level := STACK_LEVEL_COUNT + 24

	for level in range(base_level, top_level):
		var base_y := float(level) * STACK_LEVEL_HEIGHT
		var mid_y := base_y + STACK_LEVEL_HEIGHT * 0.5
		# The shaft steps in as it rises, so the silhouette tapers.
		var shrink := 1.0 - float(level - base_level) / float(top_level - base_level) * 0.42
		var half := STACK_HALF_EXTENT * shrink

		for sx in [1.0, -1.0]:
			for sz in [1.0, -1.0]:
				_add_box("UpperColumn", Vector3(STACK_COLUMN_SIZE * shrink, STACK_LEVEL_HEIGHT,
					STACK_COLUMN_SIZE * shrink),
					Vector3(cx + sx * half, mid_y, cz + sz * half), oxidised)
		# A deck band every other level, and bracing on the faces between. Not
		# at the base level: the stack's own top deck (native) is there, and
		# a second slab 0.25 m proud of it is a floor on a floor.
		if level % 2 == 0 and level > base_level:
			for sz in [1.0, -1.0]:
				_add_box("UpperDeck", Vector3(half * 2.0, 0.5, 4.0),
					Vector3(cx, base_y, cz + sz * (half - 2.0)), mill_scale)
			for sx in [1.0, -1.0]:
				_add_box("UpperDeck", Vector3(4.0, 0.5, half * 2.0 - 8.0),
					Vector3(cx + sx * (half - 2.0), base_y, cz), mill_scale)
		var brace_length := sqrt(pow(STACK_LEVEL_HEIGHT, 2.0) + pow(half, 2.0))
		var brace_pitch := atan2(STACK_LEVEL_HEIGHT, half)
		for sz in [1.0, -1.0]:
			for direction in [1.0, -1.0]:
				var brace := _add_box("UpperBrace", Vector3(brace_length, 0.4, 0.4),
					Vector3(cx + direction * half * 0.5, mid_y, cz + sz * half), rust_deep)
				brace.rotation = Vector3(0.0, 0.0, direction * brace_pitch)
		if level % 3 == 0:
			_add_box("UpperLightBand", Vector3(half * 2.0, 0.7, 0.4),
				Vector3(cx, base_y + 1.2, cz + half + 0.3), faded)

func _build_yard(concrete: Material, faded: Material, tar: Material) -> void:
	for z in range(-130, 10, 14):
		_add_box("LaneStripe", Vector3(0.22, 0.02, 7.0), Vector3(-2.0, 0.012, float(z)), faded)
	for z in [-118.0, -60.0, -16.0]:
		_add_box("KerbRun", Vector3(86.0, 0.28, 0.5), Vector3(-6.0, 0.14, z), concrete)
	_add_box("StandingWater", Vector3(26.0, 0.02, 18.0), Vector3(-24.0, 0.021, -70.0), tar)
	_add_box("StandingWaterTwo", Vector3(18.0, 0.02, 12.0), Vector3(14.0, 0.021, -36.0), tar)
	_build_walkway(faded)

# The first cue (release plan item 13): the yard's painted pedestrian
# walkway, as any works has, from the start to S1's cage door. Two edge lines
# and chevrons pointing the way, broken where the kerbs cross it; paint only
# (solid_export.gd: WalkwayPaint).
const WALKWAY_X := 10.0
const WALKWAY_HALF_WIDTH := 1.4
const WALKWAY_RUNS := [Vector2(-27.0, -60.5), Vector2(-59.5, -117.5)]   # z from, to

func _build_walkway(paint: Material) -> void:
	for run: Vector2 in WALKWAY_RUNS:
		var length := absf(run.x - run.y)
		var mid_z := 0.5 * (run.x + run.y)
		for side: float in [-1.0, 1.0]:
			_add_box("WalkwayPaint", Vector3(0.12, 0.008, length),
				Vector3(WALKWAY_X + side * WALKWAY_HALF_WIDTH, 0.006, mid_z), paint)
		var z := run.x - 4.0
		while z > run.y + 1.0:
			# A chevron: two bars meeting ahead, toward the tower (-z).
			for side: float in [-1.0, 1.0]:
				var bar := _add_box("WalkwayPaint", Vector3(0.16, 0.008, 1.3),
					Vector3(WALKWAY_X + side * 0.42, 0.006, z + 0.45), paint)
				bar.rotation.y = side * 0.75
			z -= 9.0

func _build_lighting() -> void:
	# Sodium vapour: warm, low, and sourced from actual fittings on masts.
	for mast in [Vector3(-10.0, 0.0, -24.0), Vector3(-10.0, 0.0, -62.0),
			Vector3(-10.0, 0.0, -100.0), Vector3(22.0, 0.0, -44.0),
			Vector3(22.0, 0.0, -82.0), Vector3(40.0, 0.0, -104.0)]:
		_add_box("LampMast", Vector3(0.38, 11.0, 0.38), mast + Vector3(0.0, 5.5, 0.0),
			_material(Color("3a3835"), 0.7, 0.62))
		_add_box("LampHead", Vector3(1.5, 0.4, 0.8), mast + Vector3(0.6, 10.9, 0.0),
			_material(Color("5a4b2c"), 0.4, 0.7, Color("b06a1c"), 1.4))
		var lamp := OmniLight3D.new()
		lamp.name = "SodiumFlood"
		lamp.position = mast + Vector3(0.6, 10.6, 0.0)
		lamp.light_color = Color(1.0, 0.585, 0.225)
		lamp.light_energy = 3.5
		lamp.omni_range = 26.0
		lamp.omni_attenuation = 1.6
		_light_rig.add_child(lamp)

	for base_x in [-46.0, -16.0, 16.0, 46.0]:
		var base_flood := OmniLight3D.new()
		base_flood.name = "TowerFootFlood"
		base_flood.position = Vector3(base_x, 15.0, -132.0)
		base_flood.light_color = Color(1.0, 0.7, 0.42)
		base_flood.light_energy = 4.0
		base_flood.omni_range = 48.0
		base_flood.omni_attenuation = 1.3
		_light_rig.add_child(base_flood)

func _build_mountains_and_waterfall() -> void:
	# Non-collidable alpine backdrop (identity doc item 2). Peaks are cheap
	# cones -- a CylinderMesh with top_radius 0 -- with a lighter snow-cap cone
	# nested at the tip. Distance fade comes from the environment's aerial
	# perspective fog, not from manual colour tuning per peak.
	# Kept dark and desaturated: these are distant backdrop mass, and at the
	# old values they read as bright white blobs competing with the tower.
	var rock := _material(Color("2f353d"), 0.04, 0.92)
	var rock_far := _material(Color("414a55"), 0.02, 0.95)
	var snow := _material(Color("b9c2cc"), 0.0, 0.78)

	var near_peaks := [
		Vector3(-380.0, 0.0, -560.0), Vector3(-160.0, 0.0, -610.0),
		Vector3(110.0, 0.0, -630.0), Vector3(180.0, 0.0, -540.0),
		Vector3(380.0, 0.0, -580.0), Vector3(580.0, 0.0, -680.0),
	]
	var near_sizes := [520.0, 640.0, 700.0, 600.0, 560.0, 480.0]
	for index in near_peaks.size():
		var base: Vector3 = near_peaks[index]
		var peak_height: float = near_sizes[index]
		var radius := peak_height * MountainMesh.ASPECT
		_add_peak("MountainPeak", radius, peak_height, base, rock, Color("5a6067"), index,
			0.56 + 0.03 * float(index % 3))
		_add_cone("MountainSnowCap", radius * 0.34, peak_height * 0.3,
			base + Vector3(0.0, peak_height * 0.92, 0.0), snow).visible = false

	var far_peaks := [
		Vector3(-640.0, 0.0, -880.0), Vector3(-240.0, 0.0, -950.0),
		Vector3(260.0, 0.0, -930.0), Vector3(680.0, 0.0, -900.0),
	]
	for index in far_peaks.size():
		var base: Vector3 = far_peaks[index]
		var peak_height := 900.0
		var radius := peak_height * MountainMesh.ASPECT
		_add_peak("MountainRidgeFar", radius, peak_height, base, rock_far, Color("68707a"), index + 3, 0.52)
		_add_cone("MountainRidgeFarSnow", radius * 0.4, peak_height * 0.32,
			base + Vector3(0.0, peak_height * 0.9, 0.0), snow).visible = false

	_build_waterfall(Vector3(200.0, 0.0, -560.0))
	_build_foothill_forest()
	_build_basin()
	MountainMesh.draw($TowerPresentation, _peaks)

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
const BACK_RANGE_PEAKS := 22
const BACK_RANGE_CLEAR_M := 1450.0

func _build_basin() -> void:
	var meadow := _material(Color("3f4634"), 0.0, 0.96, Color.BLACK, 1.0, _bump_concrete)
	# A frame around the grade (x in [-240, 240], z in [-300, 180]), not a
	# sheet under it: two surfaces 0.3 m apart z-fight from any distance.
	var floor_y := VALLEY_FLOOR_TOP - 0.5
	for piece in [
			[Vector3(5200.0, 1.0, 2400.0), Vector3(0.0, floor_y, -1500.0)],
			[Vector3(5200.0, 1.0, 2320.0), Vector3(0.0, floor_y, 1340.0)],
			[Vector3(2360.0, 1.0, 480.0), Vector3(-1420.0, floor_y, -60.0)],
			[Vector3(2360.0, 1.0, 480.0), Vector3(1420.0, floor_y, -60.0)]]:
		_add_box("ValleyFloor", piece[0], piece[1], meadow)

	var rock := _material(Color("353b41"), 0.03, 0.93)
	var rock_warm := _material(Color("3d3934"), 0.03, 0.93)
	var snow := _material(Color("b9c2cc"), 0.0, 0.78)
	var rng := RandomNumberGenerator.new()
	rng.seed = 1100
	for index in BASIN_RIM_RIDGES:
		var angle := TAU * float(index) / float(BASIN_RIM_RIDGES) + rng.randf_range(-0.01, 0.01)
		var reach := BASIN_RIM_RADIUS + rng.randf_range(-15.0, 15.0)
		var height := rng.randf_range(280.0, 470.0)
		var radius := height * MountainMesh.ASPECT
		var base := BASIN_CENTER + Vector3(sin(angle) * reach, 0.0, cos(angle) * reach)
		_add_peak("RimRidge", radius, height, base, rock if index % 3 != 0 else rock_warm,
			Color("5d636a") if index % 3 != 0 else Color("67615a"), index, 0.7 if height > 400.0 else 0.0)
		if height > 400.0:
			_add_cone("RimRidgeSnow", radius * 0.3, height * 0.28,
				base + Vector3(0.0, height * 0.93, 0.0), snow).visible = false

	# Beyond the rim a higher range, out of reach: from the yard it stands over
	# the rim's saddles, so the skyline is layered rather than a row of teeth.
	# Drawn only (no solid: nothing can get there), each keeping its foot
	# outside the rim's own (BACK_RANGE_CLEAR_M from the centre).
	var back := RandomNumberGenerator.new()
	back.seed = 2207
	for index in BACK_RANGE_PEAKS:
		var angle := TAU * (float(index) + back.randf_range(-0.3, 0.3)) / float(BACK_RANGE_PEAKS)
		var reach := back.randf_range(1900.0, 2400.0)
		var height := back.randf_range(680.0, 1100.0)
		var radius := minf(height * MountainMesh.ASPECT, reach - BACK_RANGE_CLEAR_M)
		_peaks.append({"foot": BASIN_CENTER + Vector3(sin(angle) * reach, VALLEY_FLOOR_TOP - 2.0,
			cos(angle) * reach), "radius": radius, "height": height, "variant": index + 11,
			"rock": Color("56606e") if index % 2 == 0 else Color("5c5f68"),
			"snowline": 0.5 + 0.12 * fposmod(float(index) * 0.618, 1.0) if height > 820.0 else 0.0})

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

# Foliage scattered through the yard itself: away from every kernel/stack/
# plant footprint, so it reads as the site being slowly reclaimed rather than
# clipping through a wall. Two open bands exist by construction -- west of
# the tower and plant, and along the north/entrance edge -- and this stays
# inside them.
func _build_foliage() -> void:
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
	$TowerPresentation.add_child(instance)

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
	$TowerPresentation.add_child(mist)

func _build_kellerworks_signage(timber: Material, faded: Material) -> void:
	# Painted banners (identity doc items 1 and 7): real Label3D copy over a
	# backing plane, plus an original chevron-K mark. Non-collidable.
	var backing := _material(Color("4a231c"), 0.05, 0.85)
	var slogans := [
		["MATERIALS MOVE", "CIVILIZATION RISES"],
		["HIGHER  STRONGER", "FURTHER"],
		["PEOPLE  POWER", "PROGRESS"],
	]
	var banner_spots := [Vector3(-9.4, 0.0, -22.0), Vector3(21.4, 0.0, -42.0), Vector3(26.5, 0.0, -90.5)]
	for index in banner_spots.size():
		_add_banner(banner_spots[index], slogans[index], backing)

	# Tower-face wordmark: bigger backing, bigger text, high enough to be read
	# from the approach.
	_add_box("WordmarkBacking", Vector3(20.0, 5.0, 0.4), Vector3(0.0, 46.0, -142.3), backing)
	var wordmark := Label3D.new()
	wordmark.name = "KellerworksWordmark"
	wordmark.text = "KELLERWORKS"
	wordmark.font_size = 96
	wordmark.pixel_size = 0.018
	wordmark.modulate = Color(0.93, 0.89, 0.82)
	wordmark.billboard = BaseMaterial3D.BILLBOARD_DISABLED
	wordmark.position = Vector3(0.0, 46.0, -142.05)
	$TowerPresentation.add_child(wordmark)
	_add_chevron_mark(Vector3(0.0, 52.6, -142.1), 1.4, faded)
