extends RefCounted

# Authored tower dressing; native physics owns every gameplay fact.
const WorldGeometry := preload("res://presentation/world_geometry.gd")
# The stack. Mirrors the kStack* constants in simulation.cpp exactly -- these
# are the native collision sizes, so what is drawn is what you stand on.
const STACK_CENTER := Vector3(0.0, 0.0, -150.0)
const STACK_HALF_EXTENT := 26.0
const STACK_LEVEL_HEIGHT := 11.0
const STACK_LEVEL_COUNT := 32
const REGRESSION_STACK_LEVEL_COUNT := 14
const STACK_DECK_THICKNESS := 0.5
const STACK_DECK_BAND_DEPTH := 9.0
const STACK_COLUMN_SIZE := 1.6

var _geometry: WorldGeometry
var _light_rig: Node3D
var _regression_scene: bool
var _stack_gears: Array[Node3D] = []


func _init(geometry: WorldGeometry, light_rig: Node3D, regression_scene: bool) -> void:
	_geometry = geometry
	_light_rig = light_rig
	_regression_scene = regression_scene


func update_machine_phase(phase: float, period_seconds: float) -> void:
	for index in _stack_gears.size():
		var direction := 1.0 if index % 2 == 0 else -1.0
		_stack_gears[index].rotation = Vector3(0.0, 0.0,
			direction * phase * TAU / period_seconds)


# The stack: the tower's climbable lower section. Continuous deck rings and
# columns mirror real native collision one-for-one; bracing, rails, pipework
# and lamps are dressing hung on that frame. The rejected fourteen-flight
# bypass is absent, so ascent comes from authored mechanisms and parkour.
func get_level_count() -> int:
	return REGRESSION_STACK_LEVEL_COUNT if _regression_scene else STACK_LEVEL_COUNT


func build(mill_scale: Material, oxidised: Material, rust_deep: Material,
		rust_bright: Material, galvanised: Material, faded: Material, timber: Material) -> void:
	var band_center := STACK_HALF_EXTENT - STACK_DECK_BAND_DEPTH * 0.5
	var inner_half := STACK_HALF_EXTENT - STACK_DECK_BAND_DEPTH
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var level_count := get_level_count()

	for level in range(1, level_count + 1):
		var deck_y := float(level) * STACK_LEVEL_HEIGHT
		var slab_y := deck_y - STACK_DECK_THICKNESS * 0.5
		var deck_material: Material = galvanised if level % 2 == 1 else mill_scale

		for sz in [1.0, -1.0]:
			_geometry.add_box("StackDeck", Vector3(STACK_HALF_EXTENT * 2.0, STACK_DECK_THICKNESS,
				STACK_DECK_BAND_DEPTH), Vector3(cx, slab_y, cz + sz * band_center), deck_material)
		for sx in [1.0, -1.0]:
			_geometry.add_box("StackDeck", Vector3(STACK_DECK_BAND_DEPTH, STACK_DECK_THICKNESS,
				inner_half * 2.0), Vector3(cx + sx * band_center, slab_y, cz), deck_material)

		# Edge beams around the shaft and the outer face: the structure reads
		# as fabricated plate girders, not floating slabs.
		for sz in [1.0, -1.0]:
			_geometry.add_box("ShaftEdgeBeam", Vector3(inner_half * 2.0, 0.9, 0.5),
				Vector3(cx, deck_y - 0.55, cz + sz * inner_half), oxidised)
			_geometry.add_box("OuterEdgeBeam", Vector3(STACK_HALF_EXTENT * 2.0, 1.1, 0.6),
				Vector3(cx, deck_y - 0.7, cz + sz * STACK_HALF_EXTENT), rust_deep)
		for sx in [1.0, -1.0]:
			_geometry.add_box("ShaftEdgeBeam", Vector3(0.5, 0.9, inner_half * 2.0),
				Vector3(cx + sx * inner_half, deck_y - 0.55, cz), oxidised)
			_geometry.add_box("OuterEdgeBeam", Vector3(0.6, 1.1, STACK_HALF_EXTENT * 2.0),
				Vector3(cx + sx * STACK_HALF_EXTENT, deck_y - 0.7, cz), rust_deep)

		# Handrails around the open shaft -- the safety line you walk beside.
		for sz in [1.0, -1.0]:
			_geometry.add_box("ShaftRail", Vector3(inner_half * 2.0, 0.08, 0.08),
				Vector3(cx, deck_y + 1.05, cz + sz * inner_half), galvanised)
		for sx in [1.0, -1.0]:
			_geometry.add_box("ShaftRail", Vector3(0.08, 0.08, inner_half * 2.0),
				Vector3(cx + sx * inner_half, deck_y + 1.05, cz), galvanised)
		for post_x in [-inner_half, -inner_half * 0.5, 0.0, inner_half * 0.5, inner_half]:
			for sz in [1.0, -1.0]:
				_geometry.add_box("ShaftPost", Vector3(0.09, 1.1, 0.09),
					Vector3(cx + post_x, deck_y + 0.55, cz + sz * inner_half), galvanised)

		# Timber decking planks laid continuously over each receiving band.
		for plank in range(-2, 3):
			for sz in [1.0, -1.0]:
				var plank_z := band_center + float(plank) * 1.35
				_geometry.add_box("DeckPlank", Vector3(STACK_HALF_EXTENT * 2.0 - 2.0, 0.08, 1.1),
					Vector3(cx, deck_y + 0.05, cz + sz * plank_z), timber)

	# Columns, and the diagonal bracing that makes a frame a frame.
	for level in range(0, level_count):
		var base_y := float(level) * STACK_LEVEL_HEIGHT
		var mid_y := base_y + STACK_LEVEL_HEIGHT * 0.5
		var brace_length := sqrt(pow(STACK_LEVEL_HEIGHT, 2.0) + pow(STACK_HALF_EXTENT, 2.0))
		var brace_pitch := atan2(STACK_LEVEL_HEIGHT, STACK_HALF_EXTENT)

		for sx in [1.0, -1.0]:
			for sz in [1.0, -1.0]:
				_geometry.add_box("StackColumn", Vector3(STACK_COLUMN_SIZE, STACK_LEVEL_HEIGHT,
					STACK_COLUMN_SIZE), Vector3(cx + sx * STACK_HALF_EXTENT, mid_y,
					cz + sz * STACK_HALF_EXTENT), rust_deep)
			_geometry.add_box("StackColumn", Vector3(STACK_COLUMN_SIZE, STACK_LEVEL_HEIGHT,
				STACK_COLUMN_SIZE), Vector3(cx + sx * STACK_HALF_EXTENT, mid_y, cz), rust_deep)
			_geometry.add_box("StackColumn", Vector3(STACK_COLUMN_SIZE, STACK_LEVEL_HEIGHT,
				STACK_COLUMN_SIZE), Vector3(cx, mid_y, cz + sx * STACK_HALF_EXTENT), rust_deep)

		# Cross bracing on all four outer faces.
		for sz in [1.0, -1.0]:
			for direction in [1.0, -1.0]:
				var brace := _geometry.add_box("StackBrace", Vector3(brace_length, 0.45, 0.45),
					Vector3(cx + direction * STACK_HALF_EXTENT * 0.5, mid_y,
						cz + sz * STACK_HALF_EXTENT), oxidised)
				brace.rotation = Vector3(0.0, 0.0, direction * brace_pitch)
		for sx in [1.0, -1.0]:
			for direction in [1.0, -1.0]:
				var brace_z := _geometry.add_box("StackBrace", Vector3(0.45, 0.45, brace_length),
					Vector3(cx + sx * STACK_HALF_EXTENT, mid_y,
						cz + direction * STACK_HALF_EXTENT * 0.5), oxidised)
				brace_z.rotation = Vector3(-direction * brace_pitch, 0.0, 0.0)

	# No ordinary stair or ramp geometry is authored here.

	# The normal scene keeps only the collision-honest frame and authored ascent routes.
	if not _regression_scene:
		_build_stack_footings(rust_deep, oxidised, mill_scale)
		_build_stack_interior_dressing(oxidised, rust_deep, rust_bright, mill_scale, galvanised)
		return
	_build_stack_dressing(mill_scale, oxidised, rust_deep, rust_bright, galvanised, faded)
	_build_stack_megastructure(mill_scale, oxidised, rust_deep, rust_bright, galvanised,
		faded, timber)


# Small maintenance traces make the hollow frame read as a place people have
# worked in. These stay high or tight to the inside columns; the player's routes
# and all consequential collision remain owned by the native world.
func _build_stack_interior_dressing(oxidised: Material, rust_deep: Material,
		rust_bright: Material, mill_scale: Material, galvanised: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var inner_half := STACK_HALF_EXTENT - STACK_DECK_BAND_DEPTH
	var levels := mini(get_level_count(), 10)
	var run_height := float(levels) * STACK_LEVEL_HEIGHT - 2.0
	var pipe_rust := _geometry.material(Color("80513a"), 0.48, 0.82)
	var cable_dark := _geometry.material(Color("332c27"), 0.34, 0.93)
	var warning_amber := _geometry.material(Color("c28b35"), 0.34, 0.68)
	var lamp_glow := _geometry.material(Color("d7a75c"), 0.14, 0.55, Color("ffb74f"), 1.15)
	var guard := _geometry.material(Color("4b463c"), 0.66, 0.72)

	# Four process risers follow the inner column lines. Rust bands and flanges
	# break their long runs into believable serviced sections.
	for sx_value in [-1.0, 1.0]:
		var sx: float = float(sx_value)
		for sz_value in [-1.0, 1.0]:
			var sz: float = float(sz_value)
			var pipe_x: float = cx + sx * (inner_half - 1.35)
			var pipe_z: float = cz + sz * (inner_half - 1.35)
			_geometry.add_cylinder("InteriorProcessRiser", 0.22, run_height,
				Vector3(pipe_x, run_height * 0.5 + 1.0, pipe_z), pipe_rust if sx == sz else oxidised)
			_geometry.add_cylinder("InteriorReturnLine", 0.075, run_height,
				Vector3(pipe_x + sx * 0.43, run_height * 0.5 + 1.0, pipe_z), mill_scale)
			for level in range(1, levels):
				var collar_y := float(level) * STACK_LEVEL_HEIGHT - 0.9
				_geometry.add_cylinder("RiserCoupling", 0.31, 0.24,
					Vector3(pipe_x, collar_y, pipe_z), rust_deep)
				_geometry.add_box("RiserBracket", Vector3(0.82, 0.10, 0.12),
					Vector3(pipe_x + sx * 0.22, collar_y - 0.36, pipe_z), galvanised)

	# Short slack loops hang just below selected cross members. Segmented curves
	# give the heavy cable a soft belly instead of a ruler-straight appearance.
	for level in [2, 5, 8]:
		var y := float(level) * STACK_LEVEL_HEIGHT - 1.0
		var start := Vector3(cx - inner_half + 1.7, y, cz - inner_half + 0.18)
		var finish := Vector3(cx - inner_half + 7.3, y, cz - inner_half + 0.18)
		_draw_sagging_service_cable(start, finish, 1.1, cable_dark, 8)
		_draw_sagging_service_cable(start + Vector3(0.2, 0.0, 0.02),
			finish + Vector3(0.2, 0.0, 0.02), 0.95, pipe_rust, 8)

	# A few amber cages and warning chevrons make the first storeys legible at
	# human scale, while the far side of each bay stays open to the huge shaft.
	for level in [1, 6]:
		var deck_y := float(level) * STACK_LEVEL_HEIGHT
		var grate_at := Vector3(cx - inner_half + 2.2, deck_y + 0.055,
			cz - inner_half - STACK_DECK_BAND_DEPTH * 0.5)
		_geometry.add_box("ServiceCatwalkGrate", Vector3(2.8, 0.08, 1.05), grate_at, rust_deep)
		for slot in range(9):
			_geometry.add_box("CatwalkGrateBar", Vector3(0.045, 0.025, 0.97),
				grate_at + Vector3(-1.2 + float(slot) * 0.3, 0.03, 0.0), galvanised)
	for level in [1, 3, 6, 9]:
		var sx := -1.0 if level % 2 == 1 else 1.0
		var sz := -1.0
		var at := Vector3(cx + sx * (inner_half - 0.65),
			float(level) * STACK_LEVEL_HEIGHT + 3.8,
			cz + sz * (inner_half - 1.0))
		var housing := _geometry.add_box("CagedServiceLamp", Vector3(0.54, 0.34, 0.8), at, lamp_glow)
		housing.rotation.y = PI * 0.25
		for offset in [-0.27, 0.27]:
			_geometry.add_box("LampGuard", Vector3(0.07, 0.48, 0.92), at + Vector3(offset, 0.0, 0.0), guard)
		var work_light := OmniLight3D.new()
		work_light.name = "WarmServiceLight"
		work_light.position = at + Vector3(0.0, -0.1, 0.15)
		work_light.light_color = Color("ffbf6e")
		work_light.light_energy = 0.8
		work_light.omni_range = 8.0
		work_light.shadow_enabled = false
		_geometry.presentation_root.add_child(work_light)
		if level == 1 or level == 6:
			var plate_at := at + Vector3(-sx * 0.7, -1.3, 0.0)
			_geometry.add_box("CableHazardPlate", Vector3(1.25, 0.72, 0.09), plate_at, warning_amber)
			for stripe in range(3):
				var mark := _geometry.add_box("HazardSlash", Vector3(0.10, 0.7, 0.11),
					plate_at + Vector3(-0.34 + float(stripe) * 0.34, 0.0, -0.06), rust_deep)
				mark.rotation.z = -0.45
				# Old handwheels at the base of each tagged service point show
				# this is still maintained, even where the steel has gone red.
			var wheel := MeshInstance3D.new()
			wheel.name = "RustyValveWheel"
			wheel.set_meta(&"part", "RustyValveWheel")
			var wheel_mesh := TorusMesh.new()
			wheel_mesh.inner_radius = 0.27
			wheel_mesh.outer_radius = 0.34
			wheel_mesh.rings = 8
			wheel_mesh.ring_segments = 12
			wheel.mesh = wheel_mesh
			wheel.material_override = rust_bright
			wheel.position = plate_at + Vector3(-sx * 0.28, -0.72, 0.03)
			wheel.rotation.x = PI * 0.5
			var pipe_at := Vector3(cx + sx * (inner_half - 1.35), wheel.position.y,
				cz + sz * (inner_half - 1.35))
			_geometry.add_strut("ValveServiceBranch", pipe_at, wheel.position, 0.10, pipe_rust)
			_geometry.add_box("ValveServiceHousing", Vector3(0.32, 0.32, 0.32),
				wheel.position + Vector3(0.0, 0.0, 0.20), rust_deep)
			_geometry.presentation_root.add_child(wheel)

	# One visibly tired junction spits a sparse handful of sparks. It is a small
	# lighting cue high in the bay, not an interactive hazard or route mechanic.
	var spark_point := Vector3(cx - inner_half + 1.65, STACK_LEVEL_HEIGHT * 2.45,
		cz - inner_half + 1.4)
	var spark_clip := _geometry.add_box("DamagedCableClip", Vector3(0.46, 0.22, 0.32),
		spark_point, rust_deep)
	spark_clip.rotation.z = 0.3
	var sparks := GPUParticles3D.new()
	sparks.name = "OccasionalServiceSparks"
	sparks.position = spark_point + Vector3(0.0, -0.16, 0.0)
	sparks.amount = 3
	sparks.lifetime = 0.28
	sparks.one_shot = true
	sparks.emitting = true
	sparks.explosiveness = 1.0
	sparks.randomness = 0.65
	var spark_process := ParticleProcessMaterial.new()
	spark_process.direction = Vector3(0.15, -0.35, 0.1)
	spark_process.spread = 150.0
	spark_process.initial_velocity_min = 0.35
	spark_process.initial_velocity_max = 1.25
	spark_process.gravity = Vector3(0.0, -3.5, 0.0)
	spark_process.scale_min = 0.5
	spark_process.scale_max = 1.2
	sparks.process_material = spark_process
	var spark_mesh := SphereMesh.new()
	spark_mesh.radius = 0.035
	spark_mesh.height = 0.07
	var spark_mat := _geometry.material(Color("ffe0a0"), 0.0, 0.45, Color("ff9a32"), 4.0)
	spark_mesh.material = spark_mat
	sparks.draw_pass_1 = spark_mesh
	_geometry.presentation_root.add_child(sparks)
	var spark_cycle := Timer.new()
	spark_cycle.name = "ServiceSparkCycle"
	spark_cycle.wait_time = 4.2
	spark_cycle.autostart = true
	spark_cycle.timeout.connect(func(): sparks.restart())
	_geometry.presentation_root.add_child(spark_cycle)


func _draw_sagging_service_cable(from: Vector3, to: Vector3, sag: float,
		material: Material, segments: int) -> void:
	# End collars intersect the visible/native shaft edge beam; the slack
	# service cable is decorative, never an alternate gameplay support.
	for endpoint in [from, to]:
		_geometry.add_box("SaggingServiceCable", Vector3(0.18, 0.12, 0.12),
			endpoint, material)
	var previous := from
	for segment in range(1, segments + 1):
		var t := float(segment) / float(segments)
		var point := from.lerp(to, t) + Vector3(0.0, -sin(PI * t) * sag, 0.0)
		_geometry.add_strut("SaggingServiceCable", previous, point, 0.055, material)
		previous = point


# Everything that makes the frame read as one vast working plant rather than
# a repeated scaffold: splayed footings, clad machine halls with lit windows,
# exposed gearing, lift cages on their guide rails, jib cranes with loads
# hanging off them, company signage, and walkways striking out into the air.
# None of it is collision or authority -- it is filler in the honest sense,
# structure whose job is scale and density.
func _build_stack_megastructure(mill_scale: Material, oxidised: Material, rust_deep: Material,
		rust_bright: Material, galvanised: Material, faded: Material, timber: Material) -> void:
	var banner_cloth := _geometry.material(Color("5e2220"), 0.0, 0.95)
	var crane_yellow := _geometry.material(Color("b8862a"), 0.26, 0.58)
	var sign_plate := _geometry.material(Color("46423b"), 0.2, 0.86)
	var window_lit := _geometry.material(Color("2a2118"), 0.1, 0.7, Color("ffb45c"), 2.8)
	var cage_yellow := _geometry.material(Color("94701f"), 0.3, 0.6)

	_build_stack_footings(rust_deep, oxidised, mill_scale)
	_build_stack_halls(timber, mill_scale, rust_deep, window_lit)
	_build_stack_gearworks(rust_deep, oxidised, mill_scale)
	_build_stack_lifts(cage_yellow, galvanised, mill_scale, window_lit)
	_build_stack_jibs(crane_yellow, mill_scale, timber, galvanised)
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
			_geometry.add_strut("Buttress", foot, head, 2.4, rust_deep)
			_geometry.add_box("ButtressFoot", Vector3(6.0, 2.6, 6.0),
				foot + Vector3(0.0, 1.3, 0.0), mill_scale)
			# Secondary tie back into the frame, one storey down.
			var tie_foot := foot.lerp(head, 0.42)
			var tie_head := Vector3(cx + sx * STACK_HALF_EXTENT, STACK_LEVEL_HEIGHT,
				cz + sz * STACK_HALF_EXTENT)
			_geometry.add_strut("ButtressTie", tie_foot, tie_head, 1.1, oxidised)


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

		_geometry.add_box("MachineHall", Vector3(width, height, depth), centre, timber)
		_geometry.add_box("HallCapping", Vector3(width + 1.2, 0.9, depth + 1.2),
			centre + Vector3(0.0, height * 0.5 + 0.3, 0.0), rust_deep)
		_geometry.add_box("HallSill", Vector3(width + 1.0, 0.8, depth + 1.0),
			centre - Vector3(0.0, height * 0.5 + 0.2, 0.0), mill_scale)
		# Window grid on the outward face and the front face.
		for row in range(int(storeys) * 2):
			for column in range(3):
				var wy := centre.y - height * 0.5 + 2.6 + float(row) * 4.4
				var wz := cz - depth * 0.5 + 2.6 + float(column) * (depth - 5.2) * 0.5
				_geometry.add_box("HallWindow", Vector3(0.4, 1.9, 1.5),
					Vector3(centre.x + sx * (width * 0.5 + 0.1), wy, wz), window_lit)
			_geometry.add_box("HallWindowFront", Vector3(2.2, 1.9, 0.4),
				Vector3(centre.x, centre.y - height * 0.5 + 3.4 + float(row) * 4.4,
					cz + depth * 0.5 + 0.1), window_lit)
		# Ribs, so the cladding reads as boards on a frame.
		for rib in range(5):
			_geometry.add_box("HallRib", Vector3(0.5, height, 0.5),
				Vector3(centre.x - width * 0.5 + 0.5 + float(rib) * (width - 1.0) * 0.25,
					centre.y, cz + depth * 0.5 + 0.2), rust_deep)


# Exposed gearing on the front face, big enough to read from the yard.
func _build_stack_gearworks(rust_deep: Material, oxidised: Material, mill_scale: Material) -> void:
	var cx := STACK_CENTER.x
	var front_z := STACK_CENTER.z + STACK_HALF_EXTENT
	var specs = [
		{"at": Vector3(cx - 10.0, STACK_LEVEL_HEIGHT * 2.4, front_z + 1.4), "r": 7.4, "t": 22},
		{"at": Vector3(cx + 2.0, STACK_LEVEL_HEIGHT * 3.6, front_z + 1.0), "r": 4.6, "t": 16},
		{"at": Vector3(cx - 6.0, STACK_LEVEL_HEIGHT * 6.3, front_z + 1.4), "r": 6.2, "t": 20},
		{"at": Vector3(cx + 8.0, STACK_LEVEL_HEIGHT * 9.4, front_z + 1.2), "r": 5.4, "t": 18},
	]
	for index in specs.size():
		var spec = specs[index]
		var gear_at: Vector3 = spec["at"]
		var gear_radius: float = spec["r"]
		var gear_teeth: int = spec["t"]
		var gear := _geometry.add_gear("StackGearwheel", gear_at, gear_radius, gear_teeth,
			rust_deep, oxidised)
		_stack_gears.append(gear)
		# The shaft it turns on, driven back into the frame.
		var shaft := _geometry.add_cylinder("GearShaft", gear_radius * 0.16, 3.2,
			gear_at - Vector3(0.0, 0.0, 1.6), mill_scale)
		shaft.rotation = Vector3(PI * 0.5, 0.0, 0.0)

	# Winch drums with cable wound on them, paired with the gearing.
	for drum in [Vector3(cx + 9.0, STACK_LEVEL_HEIGHT * 4.5, front_z - 1.0),
			Vector3(cx - 11.0, STACK_LEVEL_HEIGHT * 8.4, front_z - 1.0)]:
		var barrel := _geometry.add_cylinder("WinchDrum", 1.9, 7.0, drum, mill_scale)
		barrel.rotation = Vector3(0.0, 0.0, PI * 0.5)
		for band in range(7):
			var ring := _geometry.add_cylinder("DrumCable", 2.05, 0.5,
				drum + Vector3(-2.6 + float(band) * 0.9, 0.0, 0.0), rust_deep)
			ring.rotation = Vector3(0.0, 0.0, PI * 0.5)
		_geometry.add_box("DrumHousing", Vector3(1.4, 3.4, 3.4), drum + Vector3(4.4, 0.0, 0.0), rust_deep)


# Lift cages running in guide rails up the front of the shaft.
func _build_stack_lifts(cage_yellow: Material, galvanised: Material, mill_scale: Material,
		window_lit: Material) -> void:
	var cz := STACK_CENTER.z
	var front_z := cz + STACK_HALF_EXTENT
	var top_y := STACK_LEVEL_HEIGHT * float(get_level_count())

	for shaft_index in range(2):
		var lift_x := STACK_CENTER.x + (6.5 if shaft_index == 0 else -14.5)
		# Paired guide rails, full height.
		for rail in [-1.6, 1.6]:
			_geometry.add_box("LiftGuide", Vector3(0.45, top_y, 0.45),
				Vector3(lift_x + rail, top_y * 0.5, front_z + 1.1), galvanised)
		_geometry.add_box("LiftHead", Vector3(5.4, 2.2, 3.2), Vector3(lift_x, top_y + 1.0, front_z + 1.1),
			mill_scale)

		var cage_y: float = STACK_LEVEL_HEIGHT * (4.5 if shaft_index == 0 else 8.5)
		# Hoist rope from the head down to the cage.
		_geometry.add_box("LiftRope", Vector3(0.12, top_y - cage_y, 0.12),
			Vector3(lift_x, (top_y + cage_y) * 0.5, front_z + 1.1), mill_scale)

		var cage := Node3D.new()
		cage.name = "LiftCage"
		cage.position = Vector3(lift_x, cage_y, front_z + 1.1)
		_geometry.presentation_root.add_child(cage)
		_geometry.add_box_to("CageFloor", Vector3(4.0, 0.3, 3.0), Vector3(0.0, -1.7, 0.0), mill_scale, cage)
		_geometry.add_box_to("CageRoof", Vector3(4.0, 0.3, 3.0), Vector3(0.0, 1.7, 0.0), cage_yellow, cage)
		for corner_x in [-1.85, 1.85]:
			for corner_z in [-1.35, 1.35]:
				_geometry.add_box_to("CagePost", Vector3(0.24, 3.4, 0.24),
					Vector3(corner_x, 0.0, corner_z), cage_yellow, cage)
		_geometry.add_box_to("CageBack", Vector3(4.0, 3.0, 0.16), Vector3(0.0, 0.0, -1.4),
			galvanised, cage)
		_geometry.add_box_to("CageLamp", Vector3(1.4, 0.3, 1.0), Vector3(0.0, 1.35, 0.0), window_lit, cage)
		_geometry.add_sign_text(str(shaft_index + 3), Vector3(0.0, 0.4, 1.45), 0.0, 1.1,
			Color("f2e6cf"), cage)

		var cage_light := OmniLight3D.new()
		cage_light.name = "CageLight"
		cage_light.position = cage.position
		cage_light.light_color = Color(1.0, 0.74, 0.42)
		cage_light.light_energy = 3.0
		cage_light.omni_range = 13.0
		_light_rig.add_child(cage_light)


# Jib cranes reaching off the tower with loads on the hook, at three heights.
func _build_stack_jibs(crane_yellow: Material, mill_scale: Material, timber: Material,
		galvanised: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var jibs = [
		{"y": STACK_LEVEL_HEIGHT * 5.0, "sx": 1.0, "reach": 26.0, "drop": 13.0},
		{"y": STACK_LEVEL_HEIGHT * 9.0, "sx": -1.0, "reach": 22.0, "drop": 17.0},
		{"y": STACK_LEVEL_HEIGHT * 12.5, "sx": 1.0, "reach": 19.0, "drop": 11.0},
	]
	for jib in jibs:
		var y: float = jib["y"]
		var sx: float = jib["sx"]
		var reach: float = jib["reach"]
		var drop: float = jib["drop"]
		var root := Vector3(cx + sx * (STACK_HALF_EXTENT - 1.0), y, cz + 4.0)
		var tip := Vector3(cx + sx * (STACK_HALF_EXTENT + reach), y + reach * 0.42, cz + 9.0)
		var mast_top := root + Vector3(0.0, 11.0, 0.0)

		# Boom as a shallow lattice: two chords and the zigzag between them.
		_geometry.add_strut("JibChord", root, tip, 1.05, crane_yellow)
		var chord_offset := Vector3(0.0, 1.5, 0.0)
		_geometry.add_strut("JibChord", root + chord_offset, tip + chord_offset, 0.8, crane_yellow)
		for web in range(7):
			var a := float(web) / 7.0
			var b := (float(web) + 1.0) / 7.0
			_geometry.add_strut("JibWeb", root.lerp(tip, a) + chord_offset, root.lerp(tip, b), 0.4,
				crane_yellow)
		# A-frame mast and the tie back to the boom tip.
		_geometry.add_strut("JibMast", root, mast_top, 1.2, crane_yellow)
		_geometry.add_strut("JibStay", mast_top, tip, 0.35, mill_scale)
		_geometry.add_strut("JibBackStay", mast_top,
			Vector3(cx - sx * (STACK_HALF_EXTENT - 2.0), y + 2.0, cz), 0.35, mill_scale)

		# Hook rope and the crate hanging on it.
		var hook := tip - Vector3(0.0, drop, 0.0)
		_geometry.add_box("JibRope", Vector3(0.14, drop, 0.14), (tip + hook) * 0.5, mill_scale)
		_geometry.add_box("JibBlock", Vector3(1.0, 0.9, 1.0), hook + Vector3(0.0, 0.5, 0.0), mill_scale)
		var crate_size := 3.6
		_geometry.add_box("JibLoad", Vector3(crate_size, crate_size, crate_size),
			hook - Vector3(0.0, crate_size * 0.5, 0.0), timber)
		for edge in [-1.0, 1.0]:
			_geometry.add_box("JibLoadBand", Vector3(crate_size + 0.2, 0.34, 0.34),
				hook + Vector3(0.0, -crate_size * 0.5, edge * crate_size * 0.5), galvanised)
		# Slings from the block out to the crate corners.
		for corner_x in [-1.0, 1.0]:
			for corner_z in [-1.0, 1.0]:
				_geometry.add_strut("JibSling", hook + Vector3(0.0, 0.5, 0.0),
					hook + Vector3(corner_x * crate_size * 0.5, 0.0, corner_z * crate_size * 0.5),
					0.1, mill_scale)


# Company signage: hanging cloth banners and painted plate on the structure.
func _build_stack_signage(banner_cloth: Material, sign_plate: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var front_z := cz + STACK_HALF_EXTENT
	var emblem := _geometry.material(Color("ddd2c0"), 0.0, 0.8)

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
		_geometry.add_box("Banner", Vector3(width, height, 0.12), centre, banner_cloth)
		_geometry.add_box("BannerRod", Vector3(width + 0.8, 0.22, 0.22),
			centre + Vector3(0.0, height * 0.5 + 0.2, 0.0), sign_plate)
		# Emblem: a canted bar cluster standing in for the company mark.
		for bar in range(2):
			var mark := _geometry.add_box("BannerMark", Vector3(2.4, 0.5, 0.06),
				centre + Vector3(0.0, height * 0.5 - 2.0, 0.09), emblem)
			mark.rotation = Vector3(0.0, 0.0, (0.7 if bar == 0 else -0.7))
		if String(banner["lines"]) != "":
			_geometry.add_sign_text(String(banner["lines"]),
				centre + Vector3(0.0, height * 0.5 - 4.1, 0.12), 0.0, 0.62, Color("efe5d4"))
		_geometry.add_sign_text(String(banner["sub"]),
			centre + Vector3(0.0, height * 0.5 - 7.4, 0.12), 0.0, 0.78, Color("e4d8c4"))

	# Painted plate high on the shaft, the biggest piece of lettering here.
	var plate_centre := Vector3(cx + 6.0, STACK_LEVEL_HEIGHT * 7.6, front_z + 0.45)
	_geometry.add_box("SignPlate", Vector3(11.0, 11.0, 0.3), plate_centre, sign_plate)
	for bar in range(2):
		var plate_mark := _geometry.add_box("SignPlateMark", Vector3(4.6, 0.9, 0.08),
			plate_centre + Vector3(0.0, 3.4, 0.2), emblem)
		plate_mark.rotation = Vector3(0.0, 0.0, (0.7 if bar == 0 else -0.7))
	_geometry.add_sign_text("HIGHER\nSTRONGER\nFURTHER", plate_centre + Vector3(0.0, -1.6, 0.25),
		0.0, 1.5, Color("ded2bd"))

	# Bay lettering down at the loading level, where the player starts.
	var bay_centre := Vector3(cx - 12.0, 6.4, front_z + 0.45)
	_geometry.add_box("BayPlate", Vector3(8.0, 9.0, 0.3), bay_centre, sign_plate)
	_geometry.add_sign_text("LIFT A", bay_centre + Vector3(0.0, 2.2, 0.25), 0.0, 2.1, Color("e8dcc6"))
	_geometry.add_sign_text("TO A HIGHER\nTOMORROW", bay_centre + Vector3(0.0, -1.8, 0.25), 0.0, 0.8,
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

		_geometry.add_box("BridgeDeck", Vector3(length, 0.4, 4.4), (from + to) * 0.5, mill_scale)
		for rail_z in [-2.1, 2.1]:
			_geometry.add_box("BridgeRail", Vector3(length, 0.1, 0.1),
				(from + to) * 0.5 + Vector3(0.0, 1.15, rail_z), faded)
			for post in range(int(length / 4.0)):
				_geometry.add_box("BridgePost", Vector3(0.12, 1.2, 0.12),
					from + Vector3(sx * (2.0 + float(post) * 4.0), 0.6, rail_z), galvanised)
		# Under-truss, so the span looks like it could carry itself.
		for web in range(int(length / 6.0)):
			var a := float(web) / (length / 6.0)
			var b := (float(web) + 1.0) / (length / 6.0)
			_geometry.add_strut("BridgeWeb", from.lerp(to, a) - Vector3(0.0, 0.2, 0.0),
				from.lerp(to, b) - Vector3(0.0, 2.6, 0.0), 0.3, rust_deep)
		_geometry.add_strut("BridgeChord", from - Vector3(0.0, 2.6, 0.0), to - Vector3(0.0, 2.6, 0.0),
			0.42, rust_deep)
		# A pylon out at the far end, implying the span lands somewhere.
		_geometry.add_box("BridgePylon", Vector3(3.0, y * 0.94, 3.0),
			to + Vector3(sx * 2.0, -y * 0.5, 0.0), rust_deep)


# Colour that isn't rust: verdigris copper pipe runs, blue-painted machinery
# boxes (the ordinary paint colour for real industrial valve gear), and
# lichen staining low on the columns where damp and shade let something
# green actually take hold. A decades-old working plant is never one colour.
func build_accents(verdigris: Material, machine_blue: Material,
		lichen: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var front_z := cz + STACK_HALF_EXTENT

	# A verdigris pipe run climbing the front face, distinct from the oxidised
	# risers in the shaft -- copper service lines age to blue-green, not rust.
	for sx in [1.0]:
		_geometry.add_cylinder("VerdigrisPipe", 0.4, STACK_LEVEL_HEIGHT * float(get_level_count()),
			Vector3(cx + sx * (STACK_HALF_EXTENT - 4.5),
				STACK_LEVEL_HEIGHT * float(get_level_count()) * 0.5, front_z + 0.9), verdigris)
		for level in range(1, get_level_count() + 1):
			_geometry.add_box("VerdigrisFlange", Vector3(1.0, 0.3, 1.0),
				Vector3(cx + sx * (STACK_HALF_EXTENT - 4.5), float(level) * STACK_LEVEL_HEIGHT - 1.2,
					front_z + 0.9), verdigris)

	# Painted machine-blue valve boxes and gauge housings at working levels.
	for level in [2, 5, 8, 11]:
		var y := float(level) * STACK_LEVEL_HEIGHT + 2.0
		var box := _geometry.add_box("ValveHousing", Vector3(1.8, 1.4, 1.2),
			Vector3(cx - STACK_HALF_EXTENT + 3.5, y, front_z - 2.0), machine_blue)
		_geometry.add_cylinder("ValveWheel", 0.55, 0.22,
			box.position + Vector3(0.0, 0.0, 0.75), machine_blue).rotation = Vector3(PI * 0.5, 0.0, 0.0)

	# Lichen staining low on every column, on the shaded (south) face, and
	# streaking down from every deck's drip line -- damp industrial concrete
	# and iron are never actually clean at the base.
	for sx in [1.0, -1.0]:
		for sz in [1.0, -1.0]:
			_geometry.add_box("ColumnLichen", Vector3(STACK_COLUMN_SIZE + 0.1, 3.5, STACK_COLUMN_SIZE + 0.1),
				Vector3(cx + sx * STACK_HALF_EXTENT, 1.75, cz + sz * STACK_HALF_EXTENT), lichen)
	for level in range(1, 5):
		var y := float(level) * STACK_LEVEL_HEIGHT
		for streak in range(-3, 4):
			_geometry.add_box("DeckStreak", Vector3(0.35, 2.4, 0.1),
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
			_geometry.add_box("Riser", Vector3(0.7, STACK_LEVEL_HEIGHT * get_level_count(), 0.7),
				Vector3(cx + sx * (inner_half - 1.2),
					STACK_LEVEL_HEIGHT * get_level_count() * 0.5,
					cz + sz * (inner_half - 1.2)), pipe_material)
			for level in range(1, get_level_count() + 1):
				_geometry.add_box("RiserFlange", Vector3(1.1, 0.35, 1.1),
					Vector3(cx + sx * (inner_half - 1.2), float(level) * STACK_LEVEL_HEIGHT - 1.4,
						cz + sz * (inner_half - 1.2)), mill_scale)

	# Drive gearing on alternating levels, big enough to read from the yard.
	for level in range(1, get_level_count()):
		if level % 2 == 0:
			continue
		var gear_y := float(level) * STACK_LEVEL_HEIGHT + 3.4
		var gear := Node3D.new()
		gear.name = "StackGear"
		gear.set_meta(&"solid_disc", Vector2(2.6 + 0.5, 0.8))
		gear.set_meta(&"part", "StackGear")
		gear.position = Vector3(cx - inner_half + 1.0, gear_y, cz - STACK_HALF_EXTENT + 1.2)
		_geometry.presentation_root.add_child(gear)
		_geometry.add_box_to("GearHub", Vector3(1.0, 1.0, 0.8), Vector3.ZERO, rust_deep, gear)
		for tooth in range(12):
			var angle := TAU * float(tooth) / 12.0
			_geometry.add_box_to("GearTooth", Vector3(0.7, 0.7, 0.7),
				Vector3(cos(angle) * 2.6, sin(angle) * 2.6, 0.0), oxidised, gear)
		_stack_gears.append(gear)

	# Vent stacks that the plume system can sit on later, plus lamp fittings
	# throwing warm light into the frame.
	var lamp_material := _geometry.material(Color("4a3a24"), 0.3, 0.7, Color("ffb04d"), 3.0)
	for level in range(1, get_level_count() + 1):
		var y := float(level) * STACK_LEVEL_HEIGHT
		if level % 2 == 0:
			continue
		for sx in [1.0, -1.0]:
			_geometry.add_box("VentStack", Vector3(0.8, 2.4, 0.8),
				Vector3(cx + sx * (STACK_HALF_EXTENT - 2.2), y + 1.2,
					cz - STACK_HALF_EXTENT + 2.2), mill_scale)
			_geometry.add_box("LampFitting", Vector3(0.5, 0.3, 0.7),
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
			_geometry.add_box("EdgeStripe", Vector3(1.1, 0.5, 0.12),
				Vector3(cx + float(stripe) * 2.3, y - 0.7, cz + STACK_HALF_EXTENT + 0.32),
				faded if stripe % 2 == 0 else mill_scale)


func build_tower_skin(mill_scale: Material, oxidised: Material, galvanised: Material, faded: Material, timber: Material) -> void:
	# Relief on the neighbouring mass, now set well back across the yard. It is
	# a second Kellerworks shaft receding into the weather, not a lid over the
	# frame the player climbs.
	var mass_x := -30.0
	var face_z := -290.0
	var lit_band := _geometry.material(Color("2a2521"), 0.1, 0.8, Color("e0a040"), 0.9)
	var lit_band_dim := _geometry.material(Color("242019"), 0.1, 0.8, Color("a86c22"), 0.5)
	for offset in [-38.0, -19.0, 0.0, 19.0, 38.0]:
		_geometry.add_box("FacePier", Vector3(6.0, 460.0, 3.0),
			Vector3(mass_x + offset, 230.0, face_z), oxidised)
	for level in range(40, 520, 16):
		_geometry.add_box("FaceBand", Vector3(90.0, 1.4, 2.0),
			Vector3(mass_x, float(level), face_z - 0.4), mill_scale)
	for offset in [-30.0, -10.0, 10.0, 30.0]:
		_geometry.add_box("FaceDuct", Vector3(3.0, 380.0, 3.0),
			Vector3(mass_x + offset, 200.0, face_z + 1.6), galvanised)
	for level in range(30, 360, 12):
		var band: Material = lit_band if (level / 12) % 3 != 0 else lit_band_dim
		_geometry.add_box("FloorLight", Vector3(80.0, 1.0, 0.6),
			Vector3(mass_x, float(level), face_z - 1.3), band)
	for level in range(380, 900, 34):
		_geometry.add_box("FloorLightHigh", Vector3(74.0, 0.9, 0.6),
			Vector3(mass_x, float(level), face_z - 1.3), lit_band_dim)
	for offset in [-28.0, -9.0, 9.0, 28.0]:
		_geometry.add_box("TimberCladding", Vector3(8.0, 44.0, 1.2),
			Vector3(mass_x + offset, 62.0, face_z + 0.8), timber)

	build_continuation(oxidised, rust_deepen(oxidised), mill_scale, galvanised, faded)


# The frame does not stop where the player's reach does. Above the climbable
# stack the same columns, decks and bracing carry on for a few hundred metres,
# thinning out as they go, and the environment fog takes them the rest of the
# way. This is what makes the structure read as a skyscraper rather than a
# gantry: there is always more of it above you.
func build_continuation(oxidised: Material, rust_deep: Material, mill_scale: Material,
		galvanised: Material, faded: Material) -> void:
	var cx := STACK_CENTER.x
	var cz := STACK_CENTER.z
	var base_level := get_level_count()
	var top_level := get_level_count() + 24

	for level in range(base_level, top_level):
		var base_y := float(level) * STACK_LEVEL_HEIGHT
		var mid_y := base_y + STACK_LEVEL_HEIGHT * 0.5
		# The shaft steps in as it rises, so the silhouette tapers.
		var shrink := 1.0 - float(level - base_level) / float(top_level - base_level) * 0.42
		var half := STACK_HALF_EXTENT * shrink

		for sx in [1.0, -1.0]:
			for sz in [1.0, -1.0]:
				_geometry.add_box("UpperColumn", Vector3(STACK_COLUMN_SIZE * shrink, STACK_LEVEL_HEIGHT,
					STACK_COLUMN_SIZE * shrink),
					Vector3(cx + sx * half, mid_y, cz + sz * half), oxidised)
		# A deck band every other level, and bracing on the faces between. Not
		# at the base level: the stack's own top deck (native) is there, and
		# a second slab 0.25 m proud of it is a floor on a floor.
		if level % 2 == 0 and level > base_level:
			for sz in [1.0, -1.0]:
				_geometry.add_box("UpperDeck", Vector3(half * 2.0, 0.5, 4.0),
					Vector3(cx, base_y, cz + sz * (half - 2.0)), mill_scale)
			for sx in [1.0, -1.0]:
				_geometry.add_box("UpperDeck", Vector3(4.0, 0.5, half * 2.0 - 8.0),
					Vector3(cx + sx * (half - 2.0), base_y, cz), mill_scale)
		var brace_length := sqrt(pow(STACK_LEVEL_HEIGHT, 2.0) + pow(half, 2.0))
		var brace_pitch := atan2(STACK_LEVEL_HEIGHT, half)
		for sz in [1.0, -1.0]:
			for direction in [1.0, -1.0]:
				var brace := _geometry.add_box("UpperBrace", Vector3(brace_length, 0.4, 0.4),
					Vector3(cx + direction * half * 0.5, mid_y, cz + sz * half), rust_deep)
				brace.rotation = Vector3(0.0, 0.0, direction * brace_pitch)
		if level % 3 == 0:
			_geometry.add_box("UpperLightBand", Vector3(half * 2.0, 0.7, 0.4),
				Vector3(cx, base_y + 1.2, cz + half + 0.3), faded)


# Slightly darker variant of a base material, for members that should sit back
# a value step without defining a whole new palette entry.
func rust_deepen(source: Material) -> Material:
	var base := source as StandardMaterial3D
	if base == null:
		return source
	return _geometry.material(base.albedo_color.darkened(0.35), base.metallic, base.roughness)
