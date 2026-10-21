extends RefCounted

# Builds and mirrors the authored worksite machines. Native state is read only.
const WorldGeometry := preload("res://presentation/world_geometry.gd")
const StackWorldBuilder := preload("res://presentation/stack_world_builder.gd")
# AS-001 B00 intake rise. Every figure here mirrors a kIntake* constant in
# src/sim/simulation/fixture_constants.hpp -- what is drawn and what is collided with are the
# same geometry, so a player who can see a route can walk it.
const INTAKE_BAY_FRONT_Z := -110.5
const INTAKE_BAY_BACK_Z := -122.5
const INTAKE_BAY_HALF_X := 10.0
const INTAKE_BAY_WALL_HEIGHT := 5.0
const INTAKE_BAY_WALL_THICKNESS := 0.6
const INTAKE_THROAT_HALF_WIDTH := 1.3
const INTAKE_THROAT_HEIGHT := 2.5
const INTAKE_DOG_HALF_WIDTH := 1.04
const INTAKE_DOG_HALF_HEIGHT := 1.19
const INTAKE_DOG_CENTER_Y := 1.25
const INTAKE_JIB_MAST_Z := -100.2
const INTAKE_BOOM_LENGTH := 12.0
const INTAKE_BOOM_HEIGHT := 11.5
const INTAKE_PACK_SIZE := Vector3(2.2, 1.8, 2.3)
const INTAKE_OVERWEIGHT_STAND := Vector3(7.0, 0.0, -98.0)
const INTAKE_BELT_X := 6.0
const INTAKE_BELT_TOP_Y := 1.38
const INTAKE_PENDANT_X := 3.0
const INTAKE_PENDANT_Z := -100.0
const INTAKE_STAIR_Z := -118.0
const INTAKE_STAIR_LANE_OFFSET := 2.0
const INTAKE_STAIR_FLIGHT_COUNT := 6
const INTAKE_STAIR_FLIGHT_RISE := 4.0
const INTAKE_STAIR_HALF_RUN := 7.0
const INTAKE_STAIR_WIDTH := 3.6
const INTAKE_STAIR_LANDING_X := 8.0
const INTAKE_HANDOFF_Y := 24.0
const INTAKE_HANDOFF_CENTER_X := -6.0
const INTAKE_HANDOFF_SOUTH_Z := -107.7
const INTAKE_HANDOFF_NORTH_Z := -117.5
const INTAKE_SKIN_RUNG_RISE := 1.6
const INTAKE_SKIN_RUNG_COUNT := 15
const INTAKE_SKIN_RUNG_HALF_Z := 1.0
const WATER_SCREW_LOW := Vector3(-18.0, 0.0, -98.0)
const WATER_SCREW_UPPER := Vector3(-18.0, 5.5, -107.526279)
const WATER_SCREW_STATION := Vector3(-14.0, 0.0, -96.5)
const WATER_SCREW_LENGTH := 11.0
const WATER_SCREW_TANK_FLOOR_Y := 5.0
# AS-002 Legal Forty (Ascent Atlas §6, band B00's 24-40 m leftover). Surface
# heights read off fixture_constants.hpp's own build-time derivation (handoff
# tread-surface offset stacked with two more 8.000 m rises); geometry
# literals read off the kLegalForty*/kIntake* constants there directly.
const LEGAL_FORTY_HANDOFF_SURFACE_Y := 24.1872
const LEGAL_FORTY_MID_LANDING_SURFACE_Y := 32.1872
const LEGAL_FORTY_HALL_DECK_SURFACE_Y := 40.1872
const LEGAL_FORTY_FLIGHT_HALF_LENGTH := 8.0
const LEGAL_FORTY_FLIGHT_HALF_WIDTH := 0.90
const LEGAL_FORTY_FLIGHT_RISE := 8.0
const LEGAL_FORTY_HINGE_X := 7.856
const LEGAL_FORTY_HINGE_Z := -112.5
const LEGAL_FORTY_STOWED_THETA := 0.139626 # 8 deg
const LEGAL_FORTY_SWING_BRACKET_LOCAL_X := -7.0 # local, relative to the body centre
const LEGAL_FORTY_SWING_BRACKET_LOCAL_Y := -0.18
const LEGAL_FORTY_SWING_SHEAVE_HEIGHT := 4.0
const LEGAL_FORTY_MID_LANDING_X := 9.350
const LEGAL_FORTY_MID_LANDING_HALF_X := 1.05
const LEGAL_FORTY_MID_LANDING_Z := -115.80
const LEGAL_FORTY_MID_LANDING_HALF_Z := 5.20
const LEGAL_FORTY_UPPER_FLIGHT_X := 2.422
const LEGAL_FORTY_UPPER_FLIGHT_Z := -117.0
const LEGAL_FORTY_HALL_DECK_HALF_X := 10.0
const LEGAL_FORTY_HALL_DECK_Z := -115.1
const LEGAL_FORTY_HALL_DECK_HALF_Z := 7.40
const LEGAL_FORTY_HALL_DECK_HALF_THICKNESS := 0.20
const LEGAL_FORTY_WELL_X := -1.65
const LEGAL_FORTY_WELL_HALF_X := 3.15
const LEGAL_FORTY_WELL_Z := -117.0
const LEGAL_FORTY_WELL_HALF_Z := 2.20
const LEGAL_FORTY_SKIN_RUNG_FIRST := 16
const LEGAL_FORTY_SKIN_RUNG_LAST := 20
const LEGAL_FORTY_SKIN_RUNG_FIRST_Z := -110.0
const LEGAL_FORTY_SKIN_RUNG_STEP_Z := 2.0
const LEGAL_FORTY_SKIN_RUNG_JOG_X := -0.8
const LEGAL_FORTY_SKIN_WALKWAY_MIN_X := -5.8
const LEGAL_FORTY_SKIN_WALKWAY_MAX_X := 8.30
const LEGAL_FORTY_SKIN_WALKWAY_Z := -119.0
const LEGAL_FORTY_SKIN_WALKWAY_HALF_Z := 2.00
const LEGAL_FORTY_CW_CRADLE_HALF_X := 1.50
const LEGAL_FORTY_CW_CRADLE_HALF_Y := 0.90  # native: clears the B00 belt
const LEGAL_FORTY_CW_CRADLE_HALF_Z := 1.20
const LEGAL_FORTY_CW_CRADLE_BUILD_Y := 5.20 - LEGAL_FORTY_CW_CRADLE_HALF_Y  # top face at 5.20
const LEGAL_FORTY_CW_CRADLE_GUIDE_HALF_X := 0.35
const LEGAL_FORTY_CW_CRADLE_GUIDE_HALF_Y := 4.0
const LEGAL_FORTY_CW_CRADLE_GUIDE_HALF_Z := 0.35
const LEGAL_FORTY_CW_CRADLE_BEARING := 0.80
# AS-003 MOD-HOOK5-RACK, mirrored from the kHook5* constants in
# simulation.cpp: a cage locked by its height beside the belt's north stroke,
# its door held shut by a bar lying in its swing, the hook block on a rack.
const HOOK5_MIN_X := 8.0
const HOOK5_MAX_X := 12.0
const HOOK5_MIN_Z := -88.0
const HOOK5_MAX_Z := -84.0
const HOOK5_TOP_Y := 4.45
const HOOK5_WALL_TOP_Y := 4.15
const HOOK5_WALL := 0.30
const HOOK5_BUTTRESS_MAX_X := 9.4
const HOOK5_HATCH_MIN_X := 9.7
const HOOK5_HATCH_MAX_X := 10.9
const HOOK5_HATCH_MIN_Z := HOOK5_MIN_Z + 1.4
const HOOK5_HATCH_MAX_Z := HOOK5_MIN_Z + 3.0
const HOOK5_DOORWAY_MIN_X := 9.85
const HOOK5_DOORWAY_MAX_X := 11.25
const HOOK5_DOORWAY_TOP_Y := 2.20
const HOOK5_DOOR_HINGE_X := 10.05
const HOOK5_DOOR_Z := HOOK5_MIN_Z + 0.15
const HOOK5_DOOR_HALF_LENGTH := 0.57
const HOOK5_DOOR_HALF_HEIGHT := 1.06
const HOOK5_DOOR_HALF_THICKNESS := 0.12
const HOOK5_DOOR_CENTER_Y := 1.12
const HOOK5_BAR_HALF_LENGTH := 1.025
const HOOK5_BAR_HALF_SECTION := 0.09
const HOOK5_BAR_CENTER_Y := 1.00
const HOOK5_BAR_CENTER_Z := HOOK5_MIN_Z + 0.40
const HOOK5_BRACKET_HALF_X := 0.10
const HOOK5_BRACKET_XS := [9.60, 11.45]
const HOOK5_BLOCK_HALF := 0.30
const HOOK5_BLOCK_HALF_Y := 0.25
const HOOK5_RACK_TOP_Y := 0.60
const HOOK5_BLOCK_SEAT_X := 11.35
const HOOK5_BLOCK_SEAT_Z := HOOK5_MAX_Z - HOOK5_WALL - 0.45
# Reference mass flow for the plume, kg/s. The native orifice peaks near this, so
# the ratio below is a real fraction of a real flow, not a tuned animation curve.
const PLUME_REFERENCE_FLOW := 0.75
# Mirrors scraperx::sim kMachineCyclePeriodSeconds. The tower-face gear motif is
# decorative -- it owns no state and is never queried -- but its rotation is a
# real function of the native machine_cycle_phase_seconds, not a free-running
# clock, so it reads as the visible face of the actual plant. Governing Law 26:
# it must never be mistaken for a second physics authority.
const KELLERWORKS_CYCLE_PERIOD_SECONDS := 26.0

var _scene_root: Node3D
var _geometry: WorldGeometry
var _light_rig: Node3D
var _stack_world: StackWorldBuilder
var _regression_scene: bool
var _scoop_meshes: Array[MeshInstance3D] = []
var _scoop_locals: Array[Vector3] = []
var _ballast_mesh: MeshInstance3D
var _tipper_mesh: Node3D
var _valve_mesh: Node3D
var _treadle_mesh: Node3D
var _treadle_cable_a: Node3D
var _treadle_cable_b: Node3D
var _jib_boom_mesh: Node3D
var _jib_hook_mesh: MeshInstance3D
var _jib_crate_mesh: MeshInstance3D
var _jib_capacity_load_mesh: MeshInstance3D
var _jib_hoist_cable: Node3D
var _needle_beam_mesh: MeshInstance3D
var _needle_hoist_cable: Node3D
var _sump_grate_mesh: MeshInstance3D
var _intake_boom_pivot: Node3D
var _intake_hook_mesh: MeshInstance3D
var _intake_pack_mesh: MeshInstance3D
var _intake_overweight_mesh: MeshInstance3D
var _intake_dog_pivot: Node3D
var _intake_hoist_cable: Node3D
var _intake_belt_mesh: MeshInstance3D
var _water_screw_root: Node3D
var _water_screw_rotor: Node3D
var _water_screw_basin_water: MeshInstance3D
var _water_screw_tank_water: MeshInstance3D
var _legal_forty_swing_flight_pivot: Node3D
var _legal_forty_cradle_mesh: MeshInstance3D
var _legal_forty_rope_flight: Node3D
var _legal_forty_rope_cradle: Node3D
var _legal_forty_sheave_a := Vector3.ZERO
var _legal_forty_sheave_b := Vector3.ZERO
var _hook5_door_pivot: Node3D
var _hook5_bar_node: Node3D
var _hook5_block_node: Node3D
var _sump_grate_safe_material: Material
var _sump_grate_hazard_material: Material
var _lift_mesh: MeshInstance3D
var _counterweight_mesh: MeshInstance3D
var _translating_support_mesh: MeshInstance3D
var _rotating_support_mesh: MeshInstance3D
var _moving_ledge_mesh: MeshInstance3D
var _rope_mesh: MeshInstance3D
var _plume: CPUParticles3D
var _plume_material: StandardMaterial3D
var _fire_box: OmniLight3D
var _vent_light: OmniLight3D
var _gear_pivot: Node3D
var _drum_pivot: Node3D
var _crane_boom: Node3D
var _crane_hook: Node3D
var _crane_crate: MeshInstance3D


func _init(geometry: WorldGeometry, light_rig: Node3D, stack_world: StackWorldBuilder, regression_scene: bool, scene_root: Node3D) -> void:
	_scene_root = scene_root
	_geometry = geometry
	_light_rig = light_rig
	_stack_world = stack_world
	_regression_scene = regression_scene


func mirror(_native: Object, _valve: float, flow: float) -> void:
	if _translating_support_mesh != null:
		_translating_support_mesh.position = _native.get_translating_support_position()
	if _rotating_support_mesh != null:
		_rotating_support_mesh.position = _native.get_rotating_support_position()
		_rotating_support_mesh.rotation = Vector3(0.0, float(_native.get_rotating_support_yaw_radians()), 0.0)
	if _moving_ledge_mesh != null:
		_moving_ledge_mesh.position = _native.get_moving_ledge_position()

	var scoop_origin: Vector3 = _native.get_hoist_scoop_position()
	var scoop_tilt := float(_native.get_hoist_scoop_tilt_radians())
	var scoop_basis := Basis(Vector3(0.0, 0.0, 1.0), scoop_tilt)
	for index in _scoop_meshes.size():
		var mesh := _scoop_meshes[index]
		mesh.position = scoop_origin + scoop_basis * _scoop_locals[index]
		mesh.rotation = Vector3(0.0, 0.0, scoop_tilt)

	if _ballast_mesh != null:
		_ballast_mesh.position = _native.get_ballast_position()
	if _tipper_mesh != null:
		_tipper_mesh.position = _native.get_tipper_position()
		_tipper_mesh.rotation = Vector3(0.0, 0.0, float(_native.get_tipper_angle_radians()))
	if _valve_mesh != null:
		_valve_mesh.rotation = Vector3(0.0, 0.0, float(_native.get_valve_lever_angle_radians()))
	if _treadle_mesh != null:
		var treadle_angle := float(_native.get_treadle_angle_radians())
		_treadle_mesh.rotation = Vector3(0.0, 0.0, treadle_angle)
		# The catwalk-side cable pays out as the pedal swings, exactly as the
		# native pulley sees it.
		var cable_anchor := Vector3(15.7, 9.09, -106.0)
		cable_anchor.y -= sin(treadle_angle) * 0.70
		_span_cable(_treadle_cable_a, cable_anchor, Vector3(15.7, 11.09, -105.0))
	if _lift_mesh != null:
		_lift_mesh.position = _native.get_lift_platform_position()
	if _counterweight_mesh != null:
		_counterweight_mesh.position = _native.get_counterweight_position()

	if _jib_boom_mesh != null:
		_jib_boom_mesh.rotation = Vector3(0.0, float(_native.get_jib_boom_angle_radians()), 0.0)
	if _jib_hook_mesh != null:
		_jib_hook_mesh.position = _native.get_jib_hook_position()
	if _jib_crate_mesh != null:
		_jib_crate_mesh.position = _native.get_jib_crate_position()
	if _jib_capacity_load_mesh != null:
		_jib_capacity_load_mesh.position = _native.get_jib_capacity_stand_load_position()
	if _jib_hoist_cable != null:
		_span_cable(_jib_hoist_cable, _native.get_jib_hook_position() + Vector3(0.0, 0.15, 0.0),
			Vector3(200.0, 5.0, 0.0) + Vector3(6.0, 0.0, 0.0).rotated(
				Vector3.UP, float(_native.get_jib_boom_angle_radians())))

	if _needle_beam_mesh != null:
		_needle_beam_mesh.position = _native.get_needle_position()
	if _needle_hoist_cable != null:
		_span_cable(_needle_hoist_cable,
			_native.get_needle_position() + Vector3(0.0, 0.18, 0.0), Vector3(200.0, 7.0, -16.0))

	if _sump_grate_mesh != null:
		var grate_safe := bool(_native.is_grate_safe())
		_sump_grate_mesh.mesh.material = (
			_sump_grate_safe_material if grate_safe else _sump_grate_hazard_material)

	if _intake_boom_pivot != null:
		_intake_boom_pivot.rotation = Vector3(
			0.0, float(_native.get_intake_boom_angle_radians()), 0.0)
	if _intake_hook_mesh != null:
		_intake_hook_mesh.position = _native.get_intake_hook_position()
	if _intake_pack_mesh != null:
		_intake_pack_mesh.position = _native.get_intake_pack_position()
	if _intake_overweight_mesh != null:
		_intake_overweight_mesh.position = _native.get_intake_overweight_pack_position()
	if _intake_dog_pivot != null:
		_intake_dog_pivot.rotation = Vector3(
			0.0, float(_native.get_intake_dog_angle_radians()), 0.0)
	if _intake_hoist_cable != null:
		var boom_yaw := float(_native.get_intake_boom_angle_radians())
		var boom_tip := Vector3(0.0, INTAKE_BOOM_HEIGHT, INTAKE_JIB_MAST_Z) + Vector3(
			0.0, 0.0, -INTAKE_BOOM_LENGTH).rotated(Vector3.UP, boom_yaw)
		_span_cable(_intake_hoist_cable,
			_native.get_intake_hook_position() + Vector3(0.0, 0.2, 0.0), boom_tip)
	if _intake_belt_mesh != null:
		# The belt is native-kinematic; its stroke is authoritative, so the
		# mesh follows the body rather than running its own animation.
		var belt_phase: float = sin(0.40 * float(_native.get_simulation_time_seconds()))
		_intake_belt_mesh.position = Vector3(INTAKE_BELT_X, INTAKE_BELT_TOP_Y - 0.18,
			-96.0 + 9.0 * belt_phase)

	# AS-002 Legal Forty. The pivot sits at the hinge itself (built once,
	# above); only its own rotation is native-driven, exactly like the dog
	# plate and the yard jib boom. travel runs 0 (stowed) .. ~0.9076
	# (deployed); simulation.cpp's own build comment: the body is built at
	# stowed_phi = pi/2 - theta_stowed, and departing the stowed limit is a
	# NEGATIVE rotation about +Z, so the live angle is stowed_phi - travel.
	if _legal_forty_swing_flight_pivot != null:
		var legal_forty_travel := float(_native.get_legal_forty_swing_travel_radians())
		var stowed_phi := PI * 0.5 - LEGAL_FORTY_STOWED_THETA
		_legal_forty_swing_flight_pivot.rotation = Vector3(0.0, 0.0, stowed_phi - legal_forty_travel)
	if _legal_forty_cradle_mesh != null:
		_legal_forty_cradle_mesh.position = _native.get_legal_forty_cradle_position()
	if _legal_forty_rope_flight != null and _legal_forty_swing_flight_pivot != null:
		var bracket_local := Vector3(
			LEGAL_FORTY_SWING_BRACKET_LOCAL_X - LEGAL_FORTY_FLIGHT_HALF_LENGTH,
			LEGAL_FORTY_SWING_BRACKET_LOCAL_Y, 0.0)
		var bracket_world: Vector3 = (
			_legal_forty_swing_flight_pivot.global_transform * bracket_local)
		_span_cable(_legal_forty_rope_flight, bracket_world, _legal_forty_sheave_a)
	if _legal_forty_rope_cradle != null:
		var cradle_top: Vector3 = _native.get_legal_forty_cradle_position() + Vector3(
			0.0, LEGAL_FORTY_CW_CRADLE_HALF_Y, 0.0)
		_span_cable(_legal_forty_rope_cradle, cradle_top, _legal_forty_sheave_b)

	# AS-003. The door hangs on a -Y hinge natively, so a positive angle swings
	# its free end inward (+z): a negative turn about Godot's +Y. The bar and
	# the block are free bodies, posed from their own native transforms.
	if _hook5_door_pivot != null:
		_hook5_door_pivot.rotation = Vector3(0.0,
			-float(_native.get_hook5_door_angle_radians()), 0.0)
	if _hook5_bar_node != null:
		_hook5_bar_node.transform = Transform3D(Basis(_native.get_hook5_bar_rotation()),
			_native.get_hook5_bar_position())
	if _hook5_block_node != null:
		_hook5_block_node.transform = Transform3D(Basis(_native.get_hook5_block_rotation()),
			_native.get_hook5_block_position())

	if _rope_mesh != null:
		var from: Vector3 = _native.get_tipper_position() + Vector3(3.0, -0.2, 0.0).rotated(
			Vector3(0.0, 0.0, 1.0), float(_native.get_tipper_angle_radians()))
		var to := Vector3(29.6, 7.2, -93.0) + Vector3(-1.6, 0.0, 0.0).rotated(
			Vector3(0.0, 0.0, 1.0), float(_native.get_valve_lever_angle_radians()))
		var span := to - from
		var length := span.length()
		if length > 0.05:
			_rope_mesh.position = from + span * 0.5
			_rope_mesh.look_at_from_position(from + span * 0.5, to, Vector3.UP, true)
			_rope_mesh.scale = Vector3(1.0, 1.0, length)

	# The plume is the only "simulated-looking" effect in this file, and it is a
	# pure function of the native orifice mass flow. Shut the valve and it stops:
	# it has no clock of its own.
	if _plume != null:
		var flow_ratio := clampf(flow / PLUME_REFERENCE_FLOW, 0.0, 1.0)
		_plume.emitting = flow > 0.0005
		_plume.initial_velocity_min = 1.5 + 6.0 * flow_ratio
		_plume.initial_velocity_max = 3.0 + 15.0 * flow_ratio
		_plume.scale_amount_min = 0.9 + 1.4 * flow_ratio
		_plume.scale_amount_max = 1.8 + 3.6 * flow_ratio
		if _plume_material != null:
			_plume_material.albedo_color = Color(0.80, 0.78, 0.74, 0.05 + 0.30 * flow_ratio)
	if _vent_light != null:
		_vent_light.light_energy = 1.2 + 9.0 * clampf(flow / PLUME_REFERENCE_FLOW, 0.0, 1.0)
	if _fire_box != null:
		var charge := clampf(float(_native.get_vessel_pressure_pa()) / 4.6e5, 0.0, 1.0)
		_fire_box.light_energy = 1.6 + 5.4 * (1.0 - charge)

	# Decorative face gear/drum: driven one-way from the real cycle phase so it
	# reads as the plant's visible mechanism. It owns no state and is never
	# read back -- freezing machine_cycle_phase_seconds freezes it too.
	var phase := float(_native.get_machine_cycle_phase_seconds())
	var gear_angle := (phase / KELLERWORKS_CYCLE_PERIOD_SECONDS) * TAU
	if _gear_pivot != null:
		_gear_pivot.rotation = Vector3(0.0, 0.0, gear_angle)
	if _drum_pivot != null:
		_drum_pivot.rotation = Vector3(gear_angle * 1.6, 0.0, 0.0)


func update_ambient_dressing(_native: Object, _ambient_clock: float) -> void:
	# Clock-driven crane sway only. This is ambient presentation, not rigging or
	# an interactive lift. Playable freight is owned separately by native
	# simulation (currently the pipe-loaded bridge); nothing here is queried by
	# the physics, route or mission systems.
	if _crane_boom == null:
		return
	var sway := sin(_ambient_clock * 0.18) * 0.035
	_crane_boom.rotation = Vector3(0.0, 0.0, sway)
	if _crane_hook != null:
		_crane_hook.position.y = -13.0 + sin(_ambient_clock * 0.5) * 0.25
	# Stack gearing turns on the real native machine phase, like the yard gear
	# motif -- a visible face of an authoritative value, never its own clock.
	var phase := float(_native.get_machine_cycle_phase_seconds()) if _native != null else 0.0
	_stack_world.update_machine_phase(phase, KELLERWORKS_CYCLE_PERIOD_SECONDS)


# AS-001: MOD-APRON, MOD-INTAKE-BELT, CAP-PENDANT, MOD-YARD-JIB, the 4 t pack,
# MOD-DOG-A, MOD-STAIR-A, the +24 m handoff, and MOD-SKIN-LADDER-S. This is the
# bottom of the real ascent: the player starts on the apron and the first 24 m
# of the climb is won by moving freight, not by finding a ladder.
#
# Bodies that the native simulation owns get a mesh here and nothing else --
# their transforms come from _render_snapshot(). Everything static mirrors the
# native half-extents doubled.
func build_intake_rise(concrete: Material, mill_scale: Material, oxidised: Material,
		rust_deep: Material, rust_bright: Material, galvanised: Material,
		faded: Material, hazard: Material, timber: Material) -> void:
	var bay := Node3D.new()
	bay.name = "IntakeBay"
	_geometry.presentation_root.add_child(bay)

	var bay_depth := INTAKE_BAY_FRONT_Z - INTAKE_BAY_BACK_Z
	var bay_center_z := (INTAKE_BAY_FRONT_Z + INTAKE_BAY_BACK_Z) * 0.5
	var wall_mid_y := INTAKE_BAY_WALL_HEIGHT * 0.5

	_geometry.add_box_to("BayBack", Vector3(INTAKE_BAY_HALF_X * 2.0, INTAKE_BAY_WALL_HEIGHT,
		INTAKE_BAY_WALL_THICKNESS), Vector3(0.0, wall_mid_y, INTAKE_BAY_BACK_Z),
		concrete, bay)
	for sx in [1.0, -1.0]:
		_geometry.add_box_to("BaySide%d" % int(sx), Vector3(INTAKE_BAY_WALL_THICKNESS,
			INTAKE_BAY_WALL_HEIGHT, bay_depth),
			Vector3(sx * INTAKE_BAY_HALF_X, wall_mid_y, bay_center_z), concrete, bay)

	var jamb_width := INTAKE_BAY_HALF_X - INTAKE_THROAT_HALF_WIDTH
	for sx in [1.0, -1.0]:
		_geometry.add_box_to("BayJamb%d" % int(sx), Vector3(jamb_width, INTAKE_BAY_WALL_HEIGHT,
			INTAKE_BAY_WALL_THICKNESS),
			Vector3(sx * (INTAKE_THROAT_HALF_WIDTH + jamb_width * 0.5), wall_mid_y,
				INTAKE_BAY_FRONT_Z), concrete, bay)
	var lintel_height := INTAKE_BAY_WALL_HEIGHT - INTAKE_THROAT_HEIGHT
	_geometry.add_box_to("BayLintel", Vector3(INTAKE_THROAT_HALF_WIDTH * 2.0, lintel_height,
		INTAKE_BAY_WALL_THICKNESS),
		Vector3(0.0, INTAKE_THROAT_HEIGHT + lintel_height * 0.5, INTAKE_BAY_FRONT_Z),
		concrete, bay)

	# MOD-DOG-A. The pivot sits on the hinge line so the plate swings exactly
	# as the native hinge does; _render_snapshot drives its yaw.
	_intake_dog_pivot = Node3D.new()
	_intake_dog_pivot.name = "IntakeDog"
	_intake_dog_pivot.position = Vector3(-INTAKE_DOG_HALF_WIDTH, INTAKE_DOG_CENTER_Y,
		INTAKE_BAY_FRONT_Z)
	bay.add_child(_intake_dog_pivot)
	_geometry.add_box_to("DogPlate", Vector3(INTAKE_DOG_HALF_WIDTH * 2.0,
		INTAKE_DOG_HALF_HEIGHT * 2.0, 0.4),
		Vector3(INTAKE_DOG_HALF_WIDTH, 0.0, 0.0), hazard, _intake_dog_pivot)
	for rib in range(3):
		_geometry.add_box_to("DogRib%d" % rib, Vector3(0.14, INTAKE_DOG_HALF_HEIGHT * 2.0, 0.5),
			Vector3(0.5 + float(rib) * 0.7, 0.0, 0.0), mill_scale, _intake_dog_pivot)

	# MOD-INTAKE-BELT and the CAP-PENDANT catwalk.
	_intake_belt_mesh = _geometry.add_box_to("IntakeBelt", Vector3(4.0, 0.36, 12.0),
		Vector3(INTAKE_BELT_X, INTAKE_BELT_TOP_Y - 0.18, -96.0), galvanised, bay)
	for slat in range(9):
		_geometry.add_box_to("BeltSlat%d" % slat, Vector3(4.2, 0.1, 0.5),
			Vector3(0.0, 0.22, -5.0 + float(slat) * 1.25), mill_scale, _intake_belt_mesh)
	_geometry.add_box_to("PendantCatwalk", Vector3(3.2, 0.36, 6.0),
		Vector3(INTAKE_PENDANT_X, INTAKE_BELT_TOP_Y - 0.18, INTAKE_PENDANT_Z),
		galvanised, bay)
	var ramp := _geometry.add_box_to("PendantRamp", Vector3(3.2, 0.3, 4.24),
		Vector3(INTAKE_PENDANT_X, INTAKE_BELT_TOP_Y * 0.5, INTAKE_PENDANT_Z + 5.0),
		galvanised, bay)
	ramp.rotation = Vector3(atan2(INTAKE_BELT_TOP_Y, 4.0), 0.0, 0.0)
	_geometry.add_box_to("PendantStand", Vector3(0.4, 1.1, 0.4),
		Vector3(INTAKE_PENDANT_X - 1.0, INTAKE_BELT_TOP_Y + 0.55, INTAKE_PENDANT_Z),
		mill_scale, bay)
	_geometry.add_box_to("PendantBox", Vector3(0.5, 0.6, 0.3),
		Vector3(INTAKE_PENDANT_X - 1.0, INTAKE_BELT_TOP_Y + 1.35, INTAKE_PENDANT_Z),
		faded, bay)

	# MOD-YARD-JIB: 12 m boom, 11.5 m under the boom, 5 t SWL.
	_geometry.add_box_to("YardJibMast", Vector3(0.9, INTAKE_BOOM_HEIGHT, 0.9),
		Vector3(0.0, INTAKE_BOOM_HEIGHT * 0.5, INTAKE_JIB_MAST_Z), oxidised, bay)
	for brace in range(4):
		var brace_y := 2.0 + float(brace) * 2.6
		_geometry.add_strut("MastBrace%d" % brace,
			Vector3(0.0, brace_y, INTAKE_JIB_MAST_Z),
			Vector3(2.4, brace_y - 2.2, INTAKE_JIB_MAST_Z + 2.4), 0.16, rust_deep, bay)
	_intake_boom_pivot = Node3D.new()
	_intake_boom_pivot.name = "YardJibBoom"
	_intake_boom_pivot.position = Vector3(0.0, INTAKE_BOOM_HEIGHT, INTAKE_JIB_MAST_Z)
	bay.add_child(_intake_boom_pivot)
	_geometry.add_box_to("BoomSpine", Vector3(0.44, 0.44, INTAKE_BOOM_LENGTH),
		Vector3(0.0, 0.0, -INTAKE_BOOM_LENGTH * 0.5), oxidised, _intake_boom_pivot)
	for lattice in range(7):
		var lz := -1.0 - float(lattice) * 1.6
		_geometry.add_strut("BoomLattice%d" % lattice, Vector3(0.0, 0.28, lz),
			Vector3(0.0, -0.28, lz - 1.6), 0.09, mill_scale, _intake_boom_pivot)
	_geometry.add_box_to("BoomTail", Vector3(0.5, 0.5, 3.0), Vector3(0.0, 0.0, 2.0),
		rust_bright, _intake_boom_pivot)
	_geometry.add_box_to("BoomBallast", Vector3(1.4, 1.0, 1.4), Vector3(0.0, -0.2, 3.4),
		mill_scale, _intake_boom_pivot)

	_intake_hoist_cable = Node3D.new()
	_intake_hoist_cable.name = "IntakeHoistCable"
	bay.add_child(_intake_hoist_cable)
	var cable := BoxMesh.new()
	cable.size = Vector3(0.07, 1.0, 0.07)
	cable.material = mill_scale
	var cable_mesh := MeshInstance3D.new()
	cable_mesh.name = "Span"
	cable_mesh.set_meta(&"part", "Span")
	cable_mesh.mesh = cable
	_intake_hoist_cable.add_child(cable_mesh)

	_intake_hook_mesh = _geometry.add_box_to("IntakeHook", Vector3(0.4, 0.4, 0.4),
		Vector3(0.0, 2.0, -112.2), galvanised, bay)
	_intake_pack_mesh = _geometry.add_box_to("IntakePack", INTAKE_PACK_SIZE,
		Vector3(0.0, 0.9, -112.2), timber, bay)
	_geometry.add_box_to("PackBand", Vector3(2.3, 0.14, 2.4), Vector3(0.0, 0.5, 0.0),
		mill_scale, _intake_pack_mesh)
	_geometry.add_box_to("PackBandUpper", Vector3(2.3, 0.14, 2.4), Vector3(0.0, -0.5, 0.0),
		mill_scale, _intake_pack_mesh)

	# The 9 t proof load on its own stand: rated force, permanently commanded
	# up, permanently stalled.
	_geometry.add_box_to("OverweightMast", Vector3(0.7, 9.0, 0.7),
		INTAKE_OVERWEIGHT_STAND + Vector3(0.0, 4.5, 0.0), oxidised, bay)
	_intake_overweight_mesh = _geometry.add_box_to("OverweightPack", INTAKE_PACK_SIZE,
		INTAKE_OVERWEIGHT_STAND + Vector3(0.0, 0.9, 0.0), rust_deep, bay)

	# MOD-STAIR-A: six flights in two lanes, landings at each turn.
	var pitch := atan2(INTAKE_STAIR_FLIGHT_RISE, INTAKE_STAIR_HALF_RUN * 2.0)
	var flight_length := sqrt(pow(INTAKE_STAIR_HALF_RUN * 2.0, 2.0)
		+ pow(INTAKE_STAIR_FLIGHT_RISE, 2.0))
	var tread_surface := 0.18 / cos(pitch)
	for flight in range(INTAKE_STAIR_FLIGHT_COUNT):
		var base_y := float(flight) * INTAKE_STAIR_FLIGHT_RISE
		var side := 1.0 if flight % 2 == 0 else -1.0
		var lane_z := INTAKE_STAIR_Z + side * INTAKE_STAIR_LANE_OFFSET
		var slab := _geometry.add_box_to("StairFlight%d" % flight,
			Vector3(flight_length, 0.36, INTAKE_STAIR_WIDTH),
			Vector3(0.0, base_y + INTAKE_STAIR_FLIGHT_RISE * 0.5, lane_z),
			mill_scale, bay, side * pitch)
		# Treads drawn on the slab, so what you see and what you stand on agree.
		for tread in range(11):
			var along := -flight_length * 0.5 + 0.7 + float(tread) * 1.3
			_geometry.add_box_to("Tread%d" % tread, Vector3(1.0, 0.1, INTAKE_STAIR_WIDTH - 0.2),
				Vector3(along, 0.22, 0.0), galvanised, slab)
		for rail in [1.0, -1.0]:
			_geometry.add_box_to("StairRail%d" % int(rail), Vector3(flight_length, 0.08, 0.08),
				Vector3(0.0, 1.05, rail * (INTAKE_STAIR_WIDTH * 0.5 - 0.1)), faded, slab)
	for flight in range(INTAKE_STAIR_FLIGHT_COUNT + 1):
		var base_y := float(flight) * INTAKE_STAIR_FLIGHT_RISE
		var side := 1.0 if flight % 2 == 0 else -1.0
		_geometry.add_box_to("StairLanding%d" % flight, Vector3(2.0, 0.36,
			(INTAKE_STAIR_LANE_OFFSET + INTAKE_STAIR_WIDTH * 0.5) * 2.0),
			Vector3(-side * INTAKE_STAIR_LANDING_X, base_y + tread_surface - 0.18,
				INTAKE_STAIR_Z), galvanised, bay)

	# The +24 m handoff. Both braids arrive here.
	var handoff_depth := INTAKE_HANDOFF_SOUTH_Z - INTAKE_HANDOFF_NORTH_Z
	_geometry.add_box_to("IntakeHandoff", Vector3(8.0, 0.4, handoff_depth),
		Vector3(INTAKE_HANDOFF_CENTER_X, INTAKE_HANDOFF_Y + tread_surface - 0.2,
			(INTAKE_HANDOFF_SOUTH_Z + INTAKE_HANDOFF_NORTH_Z) * 0.5), timber, bay)

	# MOD-SKIN-LADDER-S: the exposed bypass, stepping north up the apron.
	var head_z := INTAKE_HANDOFF_SOUTH_Z + INTAKE_SKIN_RUNG_HALF_Z
	for rung in range(1, INTAKE_SKIN_RUNG_COUNT + 1):
		var top_y := float(rung) * INTAKE_SKIN_RUNG_RISE
		var rung_z := head_z + 2.0 * INTAKE_SKIN_RUNG_HALF_Z * float(
			INTAKE_SKIN_RUNG_COUNT - rung)
		_geometry.add_box_to("SkinRung%d" % rung, Vector3(2.0, 1.0, INTAKE_SKIN_RUNG_HALF_Z * 2.0),
			Vector3(INTAKE_HANDOFF_CENTER_X, top_y - 0.5, rung_z), rust_deep, bay)
		_geometry.add_box_to("SkinRungPlate%d" % rung, Vector3(2.1, 0.08, 1.9),
			Vector3(INTAKE_HANDOFF_CENTER_X, top_y + 0.02, rung_z), galvanised, bay)
		if rung % 3 == 0:
			_geometry.add_strut("SkinStay%d" % rung,
				Vector3(INTAKE_HANDOFF_CENTER_X - 1.0, top_y, rung_z),
				Vector3(INTAKE_HANDOFF_CENTER_X - 2.6, top_y - 3.0, rung_z + 1.2),
				0.12, oxidised, bay)


# Ground Archimedes screw. Native state owns rotation and both water
# inventories; this assembly only presents that state and does not get exported
# into the static-solid table a second time.
func build_water_screw(concrete: Material, mill_scale: Material, oxidised: Material,
		galvanised: Material, faded: Material, hazard: Material) -> void:
	_water_screw_root = Node3D.new()
	_water_screw_root.name = "GroundWaterScrew"
	_scene_root.add_child(_water_screw_root)

	var water := StandardMaterial3D.new()
	water.albedo_color = Color(0.08, 0.28, 0.34, 0.76)
	water.roughness = 0.18
	water.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA

	# 2 m² lower basin, with enough freeboard for the 2.4 m³ initial inventory.
	_geometry.add_box_to("BasinSouth", Vector3(3.2, 1.3, 0.20), Vector3(-18.0, 0.65, -96.4),
		concrete, _water_screw_root)
	_geometry.add_box_to("BasinNorth", Vector3(3.2, 1.3, 0.20), Vector3(-18.0, 0.65, -98.6),
		concrete, _water_screw_root)
	for sx in [-1.0, 1.0]:
		_geometry.add_box_to("BasinSide", Vector3(0.20, 1.3, 2.0),
			Vector3(-18.0 + sx * 1.1, 0.65, -97.5), concrete, _water_screw_root)
	_water_screw_basin_water = _geometry.add_box_to("BasinWater", Vector3(2.0, 1.0, 1.0),
		Vector3(-18.0, 0.5, -97.5), water, _water_screw_root)

	var axis := Node3D.new()
	axis.name = "ScrewAxis"
	axis.position = (WATER_SCREW_LOW + WATER_SCREW_UPPER) * 0.5
	_water_screw_root.add_child(axis)
	axis.look_at(WATER_SCREW_UPPER, Vector3.UP)

	# Open trough, side rails, shaft and a helical sequence of flights.
	_geometry.add_box_to("TroughBed", Vector3(1.45, 0.18, WATER_SCREW_LENGTH),
		Vector3(0.0, -0.42, 0.0), galvanised, axis)
	for side in [-1.0, 1.0]:
		_geometry.add_box_to("TroughRail", Vector3(0.16, 0.72, WATER_SCREW_LENGTH),
			Vector3(side * 0.62, -0.08, 0.0), oxidised, axis)
	_water_screw_rotor = Node3D.new()
	_water_screw_rotor.name = "FlightedRotor"
	axis.add_child(_water_screw_rotor)
	_geometry.add_box_to("Shaft", Vector3(0.22, 0.22, WATER_SCREW_LENGTH),
		Vector3.ZERO, mill_scale, _water_screw_rotor)
	for flight in 37:
		var z := -5.4 + float(flight) * 0.30
		var blade := _geometry.add_box_to("Flight", Vector3(1.28, 0.10, 0.22),
			Vector3(0.0, 0.0, z), galvanised, _water_screw_rotor)
		blade.rotation.z = float(flight) * PI

	# Three braced trestles make the machine's load path readable.
	for t in [0.22, 0.50, 0.78]:
		var p := WATER_SCREW_LOW.lerp(WATER_SCREW_UPPER, t)
		_geometry.add_strut("TrestleL", Vector3(p.x - 1.2, 0.0, p.z),
			p + Vector3(-0.55, -0.25, 0.0), 0.16, oxidised, _water_screw_root)
		_geometry.add_strut("TrestleR", Vector3(p.x + 1.2, 0.0, p.z),
			p + Vector3(0.55, -0.25, 0.0), 0.16, oxidised, _water_screw_root)
		_geometry.add_strut("TrestleTie", Vector3(p.x - 1.15, maxf(0.2, p.y * 0.45), p.z),
			Vector3(p.x + 1.15, maxf(0.2, p.y * 0.45), p.z),
			0.12, mill_scale, _water_screw_root)

	# Reducer, motor and bearing blocks explain where finite shaft work enters.
	_geometry.add_box_to("Reducer", Vector3(1.5, 1.1, 1.2),
		WATER_SCREW_LOW + Vector3(0.0, 0.65, 1.0), faded, _water_screw_root)
	var motor := _geometry.add_cylinder("DriveMotor", 0.45, 1.5,
		WATER_SCREW_LOW + Vector3(1.1, 0.60, 1.0), mill_scale, _water_screw_root)
	motor.rotation.z = deg_to_rad(90.0)
	for p in [WATER_SCREW_LOW, WATER_SCREW_UPPER]:
		_geometry.add_box_to("BearingPedestal", Vector3(1.5, 0.8, 0.9),
			p + Vector3(0.0, 0.15, 0.0), oxidised, _water_screw_root)

	# Independently supported 2 m³ tank at the screw's output port.
	_geometry.add_box_to("TankDeck", Vector3(4.4, 0.30, 4.4), Vector3(-18.0, 4.85, -108.2),
		mill_scale, _water_screw_root)
	for sx in [-1.0, 1.0]:
		for sz in [-1.0, 1.0]:
			_geometry.add_box_to("TankLeg", Vector3(0.28, 4.7, 0.28),
				Vector3(-18.0 + sx * 1.75, 2.35, -108.2 + sz * 1.75),
				oxidised, _water_screw_root)
	for side in [-1.0, 1.0]:
		_geometry.add_box_to("TankSide", Vector3(0.24, 0.9, 2.3),
			Vector3(-18.0 + side * 1.12, 5.35, -108.2), galvanised, _water_screw_root)
		_geometry.add_box_to("TankEnd", Vector3(2.3, 0.9, 0.24),
			Vector3(-18.0, 5.35, -108.2 + side * 1.12), galvanised, _water_screw_root)
	_water_screw_tank_water = _geometry.add_box_to("TankWater", Vector3(2.0, 1.0, 2.0),
		Vector3(-18.0, WATER_SCREW_TANK_FLOOR_Y, -108.2), water, _water_screw_root)
	_geometry.add_box_to("ScrewControlPedestal", Vector3(0.64, 1.7, 0.64),
		WATER_SCREW_STATION + Vector3(0.0, 0.85, 0.0), hazard, _water_screw_root)
	_geometry.add_box_to("ScrewControlHead", Vector3(0.90, 0.55, 0.45),
		WATER_SCREW_STATION + Vector3(0.0, 1.75, 0.0), faded, _water_screw_root)
	_geometry.add_strut("PowerConduit", WATER_SCREW_STATION + Vector3(0.0, 0.35, 0.0),
		WATER_SCREW_LOW + Vector3(1.0, 0.35, 1.0), 0.09, mill_scale, _water_screw_root)


func mirror_water_screw(_native: Object) -> void:
	if _water_screw_rotor != null:
		_water_screw_rotor.rotation.z = float(_native.get_water_screw_shaft_angle_radians())
	if _water_screw_basin_water != null:
		var basin_depth := clampf(float(_native.get_water_screw_basin_volume_m3()) / 2.0, 0.0, 1.25)
		_water_screw_basin_water.visible = basin_depth > 0.002
		_water_screw_basin_water.scale.y = maxf(0.002, basin_depth)
		_water_screw_basin_water.position.y = basin_depth * 0.5
	if _water_screw_tank_water != null:
		var tank_depth := clampf(float(_native.get_water_screw_tank_volume_m3()) / 4.0, 0.0, 0.5)
		_water_screw_tank_water.visible = tank_depth > 0.002
		_water_screw_tank_water.scale.y = maxf(0.002, tank_depth)
		_water_screw_tank_water.position.y = WATER_SCREW_TANK_FLOOR_Y + tank_depth * 0.5


# AS-002 Legal Forty: the counterweighted-bascule flight and cradle that
# open band B00's own 24-40 m leftover, plus the static run above it --
# mid-landing, upper flight, MOD-HALL-DECK and its well, and the SKIN
# ladder's own continuation. Geometry mirrors simulation.cpp's own
# build_legal_forty one-for-one; this function only draws it.
func build_legal_forty(concrete: Material, mill_scale: Material, oxidised: Material,
		rust_deep: Material, rust_bright: Material, galvanised: Material,
		faded: Material, hazard: Material, timber: Material) -> void:
	var forty := Node3D.new()
	forty.name = "LegalForty"
	_geometry.presentation_root.add_child(forty)

	# ---- MOD-STAIR-A-SWING: the dynamic bascule flight, its rotation driven
	# by mirror every frame -- the pivot sits at the hinge itself,
	# exactly like IntakeDog and YardJibBoom above, so only its own rotation
	# needs updating; the slab (and everything hung on it) follows for free.
	_legal_forty_swing_flight_pivot = Node3D.new()
	_legal_forty_swing_flight_pivot.name = "SwingFlightPivot"
	_legal_forty_swing_flight_pivot.position = Vector3(
		LEGAL_FORTY_HINGE_X, LEGAL_FORTY_MID_LANDING_SURFACE_Y, LEGAL_FORTY_HINGE_Z)
	forty.add_child(_legal_forty_swing_flight_pivot)
	var flight_length := LEGAL_FORTY_FLIGHT_HALF_LENGTH * 2.0
	var flight_slab := _geometry.add_box_to("SwingFlightSlab",
		Vector3(flight_length, 0.36, LEGAL_FORTY_FLIGHT_HALF_WIDTH * 2.0),
		Vector3(-LEGAL_FORTY_FLIGHT_HALF_LENGTH, 0.0, 0.0), mill_scale,
		_legal_forty_swing_flight_pivot)
	for tread in range(6):
		var along := -flight_length + 1.4 + float(tread) * 2.4
		_geometry.add_box_to("SwingTread%d" % tread,
			Vector3(1.0, 0.1, LEGAL_FORTY_FLIGHT_HALF_WIDTH * 2.0 - 0.2),
			Vector3(along, 0.22, 0.0), galvanised, flight_slab)
	for rail in [1.0, -1.0]:
		_geometry.add_box_to("SwingRail%d" % int(rail), Vector3(flight_length, 0.08, 0.08),
			Vector3(0.0, 1.05, rail * (LEGAL_FORTY_FLIGHT_HALF_WIDTH - 0.1)), faded, flight_slab)

	# ---- MOD-CW-CRADLE: dynamic car on a free vertical slider, plus its
	# guide mast -- cradle_x/z read off the same yard-jib bearing formula
	# simulation.cpp uses (same boom, same mast, MOD-YARD-JIB's own).
	# kIntakeJibMastX is kIntakeBayCenterX (0.0) natively -- the yard jib's
	# own mast sits on the bay's own centreline.
	var cradle_x := 0.0 + INTAKE_BOOM_LENGTH * sin(LEGAL_FORTY_CW_CRADLE_BEARING)
	var cradle_z := INTAKE_JIB_MAST_Z - INTAKE_BOOM_LENGTH * cos(LEGAL_FORTY_CW_CRADLE_BEARING)
	var guide_mast_z := cradle_z + 2.0
	_geometry.add_box_to("CradleGuideMast",
		Vector3(LEGAL_FORTY_CW_CRADLE_GUIDE_HALF_X * 2.0, LEGAL_FORTY_CW_CRADLE_GUIDE_HALF_Y * 2.0,
			LEGAL_FORTY_CW_CRADLE_GUIDE_HALF_Z * 2.0),
		Vector3(cradle_x, LEGAL_FORTY_CW_CRADLE_GUIDE_HALF_Y, guide_mast_z), oxidised, forty)

	# The car: a solid block, exactly the native body. The pack rides on its
	# top face. _render_snapshot drives it from the live cradle every frame.
	_legal_forty_cradle_mesh = _geometry.add_box_to("CwCradleCar",
		Vector3(LEGAL_FORTY_CW_CRADLE_HALF_X * 2.0, LEGAL_FORTY_CW_CRADLE_HALF_Y * 2.0,
			LEGAL_FORTY_CW_CRADLE_HALF_Z * 2.0),
		Vector3(cradle_x, LEGAL_FORTY_CW_CRADLE_BUILD_Y, cradle_z), rust_deep, forty)

	# ---- The rope: two segments over two fixed sheaves, matching the
	# native pulley's own two runs (exactly like the treadle's own cable
	# above) -- mirror spans both every frame from the live flight
	# bracket and cradle top.
	_legal_forty_sheave_a = Vector3(LEGAL_FORTY_HINGE_X,
		LEGAL_FORTY_MID_LANDING_SURFACE_Y + LEGAL_FORTY_SWING_SHEAVE_HEIGHT, LEGAL_FORTY_HINGE_Z)
	_legal_forty_sheave_b = Vector3(cradle_x,
		LEGAL_FORTY_MID_LANDING_SURFACE_Y + LEGAL_FORTY_SWING_SHEAVE_HEIGHT, cradle_z)
	_geometry.add_box_to("SheaveA", Vector3(0.3, 0.3, 0.3), _legal_forty_sheave_a, oxidised, forty)
	_geometry.add_box_to("SheaveB", Vector3(0.3, 0.3, 0.3), _legal_forty_sheave_b, oxidised, forty)
	_legal_forty_rope_flight = _geometry.add_box_to("RopeFlightSide", Vector3(0.05, 0.05, 1.0),
		Vector3.ZERO, mill_scale, forty)
	_legal_forty_rope_cradle = _geometry.add_box_to("RopeCradleSide", Vector3(0.05, 0.05, 1.0),
		Vector3.ZERO, mill_scale, forty)

	# ---- MOD-STAIR-A upper flight (static) + mid-landing --------------------
	var upper_flight_pitch := asin(LEGAL_FORTY_FLIGHT_RISE / flight_length)
	_geometry.add_box_to("UpperFlightSlab",
		Vector3(flight_length, 0.36, LEGAL_FORTY_FLIGHT_HALF_WIDTH * 2.0),
		Vector3(LEGAL_FORTY_UPPER_FLIGHT_X,
			LEGAL_FORTY_MID_LANDING_SURFACE_Y + LEGAL_FORTY_FLIGHT_RISE * 0.5,
			LEGAL_FORTY_UPPER_FLIGHT_Z),
		mill_scale, forty, -upper_flight_pitch)
	_geometry.add_box_to("MidLanding",
		Vector3(LEGAL_FORTY_MID_LANDING_HALF_X * 2.0, 0.36, LEGAL_FORTY_MID_LANDING_HALF_Z * 2.0),
		Vector3(LEGAL_FORTY_MID_LANDING_X, LEGAL_FORTY_MID_LANDING_SURFACE_Y - 0.18,
			LEGAL_FORTY_MID_LANDING_Z),
		galvanised, forty)

	# ---- MOD-HALL-DECK, four strips tiling the deck minus its well ----------
	var well_min_x := LEGAL_FORTY_WELL_X - LEGAL_FORTY_WELL_HALF_X
	var well_max_x := LEGAL_FORTY_WELL_X + LEGAL_FORTY_WELL_HALF_X
	var well_min_z := LEGAL_FORTY_WELL_Z - LEGAL_FORTY_WELL_HALF_Z
	var well_max_z := LEGAL_FORTY_WELL_Z + LEGAL_FORTY_WELL_HALF_Z
	var deck_min_x := -LEGAL_FORTY_HALL_DECK_HALF_X
	var deck_max_x := LEGAL_FORTY_HALL_DECK_HALF_X
	var deck_min_z := LEGAL_FORTY_HALL_DECK_Z - LEGAL_FORTY_HALL_DECK_HALF_Z
	var deck_max_z := LEGAL_FORTY_HALL_DECK_Z + LEGAL_FORTY_HALL_DECK_HALF_Z
	var deck_center_y := LEGAL_FORTY_HALL_DECK_SURFACE_Y - LEGAL_FORTY_HALL_DECK_HALF_THICKNESS
	var south_cz := (well_max_z + deck_max_z) * 0.5
	var south_hz := (deck_max_z - well_max_z) * 0.5
	_geometry.add_box_to("HallDeckSouth", Vector3(LEGAL_FORTY_HALL_DECK_HALF_X * 2.0,
		LEGAL_FORTY_HALL_DECK_HALF_THICKNESS * 2.0, south_hz * 2.0),
		Vector3(0.0, deck_center_y, south_cz), timber, forty)
	var north_cz := (deck_min_z + well_min_z) * 0.5
	var north_hz := (well_min_z - deck_min_z) * 0.5
	_geometry.add_box_to("HallDeckNorth", Vector3(LEGAL_FORTY_HALL_DECK_HALF_X * 2.0,
		LEGAL_FORTY_HALL_DECK_HALF_THICKNESS * 2.0, north_hz * 2.0),
		Vector3(0.0, deck_center_y, north_cz), timber, forty)
	var west_cx := (deck_min_x + well_min_x) * 0.5
	var west_hx := (well_min_x - deck_min_x) * 0.5
	_geometry.add_box_to("HallDeckWest", Vector3(west_hx * 2.0, LEGAL_FORTY_HALL_DECK_HALF_THICKNESS * 2.0,
		LEGAL_FORTY_WELL_HALF_Z * 2.0),
		Vector3(west_cx, deck_center_y, LEGAL_FORTY_WELL_Z), timber, forty)
	var east_cx := (well_max_x + deck_max_x) * 0.5
	var east_hx := (deck_max_x - well_max_x) * 0.5
	_geometry.add_box_to("HallDeckEast", Vector3(east_hx * 2.0, LEGAL_FORTY_HALL_DECK_HALF_THICKNESS * 2.0,
		LEGAL_FORTY_WELL_HALF_Z * 2.0),
		Vector3(east_cx, deck_center_y, LEGAL_FORTY_WELL_Z), timber, forty)

	# ---- MOD-SKIN-LADDER-S continuation, rungs 16-20, and its walkway -------
	for rung in range(LEGAL_FORTY_SKIN_RUNG_FIRST, LEGAL_FORTY_SKIN_RUNG_LAST + 1):
		var top_y := float(rung) * INTAKE_SKIN_RUNG_RISE
		var rung_z := LEGAL_FORTY_SKIN_RUNG_FIRST_Z - LEGAL_FORTY_SKIN_RUNG_STEP_Z * float(
			rung - LEGAL_FORTY_SKIN_RUNG_FIRST)
		var rung_x := INTAKE_HANDOFF_CENTER_X + (LEGAL_FORTY_SKIN_RUNG_JOG_X
			if rung > LEGAL_FORTY_SKIN_RUNG_FIRST else 0.0)
		_geometry.add_box_to("SkinRung%d" % rung, Vector3(2.0, 1.0, INTAKE_SKIN_RUNG_HALF_Z * 2.0),
			Vector3(rung_x, top_y - 0.5, rung_z), rust_deep, forty)
		_geometry.add_box_to("SkinRungPlate%d" % rung, Vector3(2.1, 0.08, 1.9),
			Vector3(rung_x, top_y + 0.02, rung_z), galvanised, forty)
		if rung % 3 == 0:
			_geometry.add_strut("SkinStay%d" % rung,
				Vector3(rung_x - 1.0, top_y, rung_z),
				Vector3(rung_x - 2.6, top_y - 3.0, rung_z + 1.2),
				0.12, oxidised, forty)
	var walk_cx := (LEGAL_FORTY_SKIN_WALKWAY_MIN_X + LEGAL_FORTY_SKIN_WALKWAY_MAX_X) * 0.5
	var walk_hx := (LEGAL_FORTY_SKIN_WALKWAY_MAX_X - LEGAL_FORTY_SKIN_WALKWAY_MIN_X) * 0.5
	_geometry.add_box_to("SkinWalkway", Vector3(walk_hx * 2.0, 0.36, LEGAL_FORTY_SKIN_WALKWAY_HALF_Z * 2.0),
		Vector3(walk_cx, LEGAL_FORTY_MID_LANDING_SURFACE_Y - 0.18, LEGAL_FORTY_SKIN_WALKWAY_Z),
		mill_scale, forty)


# AS-003: MOD-HOOK5-RACK. Natively the cage's walls are solid 0.30 m slabs;
# here they are a steel frame barred inside that thickness, so everything
# visible stays inside what is solid while the hook block on its rack, the bar
# across the door and the hatch in the roof all read from outside -- a lock
# has to be seen to be picked. The buttress the belt reaches is solid, its
# grab edge painted. The door, the bar and the block are native bodies that
# mirror drives every frame.
func build_hook5_rack(concrete: Material, mill_scale: Material, oxidised: Material,
		galvanised: Material, faded: Material, hazard: Material) -> void:
	var cage := Node3D.new()
	cage.name = "Hook5Cage"
	_geometry.presentation_root.add_child(cage)
	var mid_z := (HOOK5_MIN_Z + HOOK5_MAX_Z) * 0.5
	var span_z := HOOK5_MAX_Z - HOOK5_MIN_Z

	_geometry.add_box_to("Hook5Buttress", Vector3(HOOK5_BUTTRESS_MAX_X - HOOK5_MIN_X, HOOK5_TOP_Y, span_z),
		Vector3((HOOK5_MIN_X + HOOK5_BUTTRESS_MAX_X) * 0.5, HOOK5_TOP_Y * 0.5, mid_z),
		concrete, cage)
	_geometry.add_box_to("EdgeStripe", Vector3(0.02, 0.18, span_z),
		Vector3(HOOK5_MIN_X - 0.01, HOOK5_TOP_Y - 0.09, mid_z), hazard, cage)

	# North and east walls, barred; the south wall's two jambs and the header.
	_add_barred_wall(cage, HOOK5_BUTTRESS_MAX_X, HOOK5_MAX_X, HOOK5_MAX_Z - HOOK5_WALL,
		HOOK5_MAX_Z, HOOK5_WALL_TOP_Y, mill_scale, oxidised)
	_add_barred_wall(cage, HOOK5_MAX_X - HOOK5_WALL, HOOK5_MAX_X, HOOK5_MIN_Z,
		HOOK5_MAX_Z - HOOK5_WALL, HOOK5_WALL_TOP_Y, mill_scale, oxidised)
	for jamb in [[HOOK5_BUTTRESS_MAX_X, HOOK5_DOORWAY_MIN_X],
			[HOOK5_DOORWAY_MAX_X, HOOK5_MAX_X - HOOK5_WALL]]:
		_geometry.add_box_to("Hook5Post", Vector3(jamb[1] - jamb[0], HOOK5_WALL_TOP_Y, HOOK5_WALL),
			Vector3((jamb[0] + jamb[1]) * 0.5, HOOK5_WALL_TOP_Y * 0.5,
				HOOK5_MIN_Z + HOOK5_WALL * 0.5), mill_scale, cage)
	var header_height := HOOK5_WALL_TOP_Y - HOOK5_DOORWAY_TOP_Y
	_geometry.add_box_to("Hook5Header",
		Vector3(HOOK5_DOORWAY_MAX_X - HOOK5_DOORWAY_MIN_X, header_height, HOOK5_WALL),
		Vector3((HOOK5_DOORWAY_MIN_X + HOOK5_DOORWAY_MAX_X) * 0.5,
			HOOK5_DOORWAY_TOP_Y + header_height * 0.5, HOOK5_MIN_Z + HOOK5_WALL * 0.5),
		galvanised, cage)
	_geometry.add_sign_text("HOOK 5", Vector3((HOOK5_DOORWAY_MIN_X + HOOK5_DOORWAY_MAX_X) * 0.5,
		HOOK5_DOORWAY_TOP_Y + header_height * 0.5, HOOK5_MIN_Z - 0.03), PI, 0.55,
		Color("d8b04a"), cage)

	# The roof: four plates round the hatch, the hatch's rim painted.
	var roof_y := (HOOK5_WALL_TOP_Y + HOOK5_TOP_Y) * 0.5
	var roof_h := HOOK5_TOP_Y - HOOK5_WALL_TOP_Y
	for plate in [
			[HOOK5_BUTTRESS_MAX_X, HOOK5_HATCH_MIN_X, HOOK5_MIN_Z, HOOK5_MAX_Z],
			[HOOK5_HATCH_MAX_X, HOOK5_MAX_X, HOOK5_MIN_Z, HOOK5_MAX_Z],
			[HOOK5_HATCH_MIN_X, HOOK5_HATCH_MAX_X, HOOK5_HATCH_MAX_Z, HOOK5_MAX_Z],
			[HOOK5_HATCH_MIN_X, HOOK5_HATCH_MAX_X, HOOK5_MIN_Z, HOOK5_HATCH_MIN_Z]]:
		_geometry.add_box_to("Hook5Roof", Vector3(plate[1] - plate[0], roof_h, plate[3] - plate[2]),
			Vector3((plate[0] + plate[1]) * 0.5, roof_y, (plate[2] + plate[3]) * 0.5),
			galvanised, cage)
	for rim_x in [HOOK5_HATCH_MIN_X - 0.06, HOOK5_HATCH_MAX_X + 0.06]:
		_geometry.add_box_to("EdgeStripe", Vector3(0.12, 0.01, HOOK5_HATCH_MAX_Z - HOOK5_HATCH_MIN_Z),
			Vector3(rim_x, HOOK5_TOP_Y + 0.005, (HOOK5_HATCH_MIN_Z + HOOK5_HATCH_MAX_Z) * 0.5),
			hazard, cage)
	for rim_z in [HOOK5_HATCH_MIN_Z - 0.06, HOOK5_HATCH_MAX_Z + 0.06]:
		_geometry.add_box_to("EdgeStripe", Vector3(HOOK5_HATCH_MAX_X - HOOK5_HATCH_MIN_X + 0.24, 0.01, 0.12),
			Vector3((HOOK5_HATCH_MIN_X + HOOK5_HATCH_MAX_X) * 0.5, HOOK5_TOP_Y + 0.005, rim_z),
			hazard, cage)

	# The bar's two brackets (a ledge under each end and a stop on its north
	# face), and the block's rack in the north-east corner: the native boxes.
	var bar_bottom := HOOK5_BAR_CENTER_Y - HOOK5_BAR_HALF_SECTION
	var bar_north := HOOK5_BAR_CENTER_Z + HOOK5_BAR_HALF_SECTION
	var wall_inner_z := HOOK5_MIN_Z + HOOK5_WALL
	for bracket_x in HOOK5_BRACKET_XS:
		_geometry.add_box_to("Hook5Bracket", Vector3(HOOK5_BRACKET_HALF_X * 2.0, 0.08,
			bar_north + 0.10 - wall_inner_z),
			Vector3(bracket_x, bar_bottom - 0.04, (wall_inner_z + bar_north + 0.10) * 0.5),
			oxidised, cage)
		var stop_top := HOOK5_BAR_CENTER_Y + 0.02
		_geometry.add_box_to("Hook5Bracket", Vector3(HOOK5_BRACKET_HALF_X * 2.0,
			stop_top - (bar_bottom - 0.06), 0.10),
			Vector3(bracket_x, (stop_top + bar_bottom - 0.06) * 0.5, bar_north + 0.05),
			oxidised, cage)
	_geometry.add_box_to("Hook5Rack", Vector3(HOOK5_MAX_X - HOOK5_WALL - 10.95, HOOK5_RACK_TOP_Y, 0.9),
		Vector3((10.95 + HOOK5_MAX_X - HOOK5_WALL) * 0.5, HOOK5_RACK_TOP_Y * 0.5,
			HOOK5_BLOCK_SEAT_Z), mill_scale, cage)

	# MOD-HOOK5-DOOR: a barred leaf on its hinge, swung by the native angle.
	_hook5_door_pivot = Node3D.new()
	_hook5_door_pivot.name = "Hook5DoorPivot"
	_hook5_door_pivot.position = Vector3(HOOK5_DOOR_HINGE_X, HOOK5_DOOR_CENTER_Y, HOOK5_DOOR_Z)
	cage.add_child(_hook5_door_pivot)
	var leaf_length := HOOK5_DOOR_HALF_LENGTH * 2.0
	var leaf_height := HOOK5_DOOR_HALF_HEIGHT * 2.0
	var leaf_depth := HOOK5_DOOR_HALF_THICKNESS * 2.0
	for side in [0.07, leaf_length - 0.07]:
		_geometry.add_box_to("Hook5DoorFrame", Vector3(0.14, leaf_height, leaf_depth),
			Vector3(side, 0.0, 0.0), mill_scale, _hook5_door_pivot)
	for edge in [-HOOK5_DOOR_HALF_HEIGHT + 0.07, HOOK5_DOOR_HALF_HEIGHT - 0.07]:
		_geometry.add_box_to("Hook5DoorFrame", Vector3(leaf_length, 0.14, leaf_depth),
			Vector3(HOOK5_DOOR_HALF_LENGTH, edge, 0.0), mill_scale, _hook5_door_pivot)
	for bar_index in 5:
		_geometry.add_box_to("Hook5DoorGrille", Vector3(0.05, leaf_height - 0.28, 0.05),
			Vector3(0.23 + float(bar_index) * 0.18, 0.0, 0.0), oxidised, _hook5_door_pivot)

	# MOD-HOOK5-BAR and the hook block (CAP-HOOK5): posed from native.
	_hook5_bar_node = Node3D.new()
	_hook5_bar_node.name = "Hook5BarBody"
	cage.add_child(_hook5_bar_node)
	_geometry.add_box_to("Hook5BarBeam", Vector3(HOOK5_BAR_HALF_LENGTH * 2.0, HOOK5_BAR_HALF_SECTION * 2.0,
		HOOK5_BAR_HALF_SECTION * 2.0), Vector3.ZERO, hazard, _hook5_bar_node)
	_hook5_block_node = Node3D.new()
	_hook5_block_node.name = "Hook5BlockBody"
	cage.add_child(_hook5_block_node)
	_geometry.add_box_to("Hook5Block", Vector3(HOOK5_BLOCK_HALF * 2.0, HOOK5_BLOCK_HALF_Y * 2.0,
		HOOK5_BLOCK_HALF * 2.0), Vector3.ZERO, faded, _hook5_block_node)
	_geometry.add_box_to("Hook5BlockBand", Vector3(HOOK5_BLOCK_HALF * 2.0 + 0.02, 0.12,
		HOOK5_BLOCK_HALF * 2.0 + 0.02), Vector3(0.0, -0.04, 0.0), mill_scale, _hook5_block_node)

	var lamp := OmniLight3D.new()
	lamp.name = "Hook5Lamp"
	lamp.position = Vector3(10.9, HOOK5_WALL_TOP_Y - 0.4, HOOK5_BLOCK_SEAT_Z - 0.6)
	lamp.light_color = Color(1.0, 0.78, 0.5)
	lamp.light_energy = 1.4
	lamp.omni_range = 5.0
	cage.add_child(lamp)


# A cage wall drawn as a frame barred inside the native slab x in [x0, x1],
# z in [z0, z1], up to y1: posts at its ends, a rail top and bottom, and bars
# every 0.18 m between -- none of it outside the slab.
func _add_barred_wall(parent: Node3D, x0: float, x1: float, z0: float, z1: float, y1: float,
		frame: Material, bars: Material) -> void:
	var along_x := (x1 - x0) >= (z1 - z0)
	var length := (x1 - x0) if along_x else (z1 - z0)
	var depth := (z1 - z0) if along_x else (x1 - x0)
	var centre := Vector3((x0 + x1) * 0.5, y1 * 0.5, (z0 + z1) * 0.5)
	var axis := Vector3.RIGHT if along_x else Vector3.BACK
	var lateral := Vector3(0.0, 0.0, depth) if along_x else Vector3(depth, 0.0, 0.0)
	for end in [-1.0, 1.0]:
		var post_size := axis * 0.2 + lateral + Vector3(0.0, y1, 0.0)
		_geometry.add_box_to("Hook5Post", post_size, centre + axis * end * (length * 0.5 - 0.1), frame,
			parent)
	for rail_y in [0.1, y1 - 0.1]:
		_geometry.add_box_to("Hook5Rail", axis * length + lateral + Vector3(0.0, 0.2, 0.0),
			Vector3(centre.x, rail_y, centre.z), frame, parent)
	var count := int(floor((length - 0.4) / 0.18))
	for index in count:
		var along := (float(index) - float(count - 1) * 0.5) * 0.18
		_geometry.add_box_to("Hook5Grille", Vector3(0.05, y1 - 0.4, 0.05), centre + axis * along, bars,
			parent)


func build_yard(concrete: Material, mill_scale: Material,
		faded: Material, tar: Material) -> void:
	for z in range(-130, 10, 14):
		_geometry.add_box("LaneStripe", Vector3(0.22, 0.02, 7.0), Vector3(-2.0, 0.012, float(z)), faded)
	for z in [-118.0, -60.0, -16.0]:
		_geometry.add_box("KerbRun", Vector3(86.0, 0.28, 0.5), Vector3(-6.0, 0.14, z), concrete)
	for x in [-44.0, -20.0]:
		for z in [-124.0, -86.0, -40.0]:
			_geometry.add_box("YardCrate", Vector3(4.4, 3.0, 4.4), Vector3(x, 1.5, z), mill_scale)
	_geometry.add_box("StandingWater", Vector3(26.0, 0.02, 18.0), Vector3(-24.0, 0.021, -70.0), tar)
	_geometry.add_box("StandingWaterTwo", Vector3(18.0, 0.02, 12.0), Vector3(14.0, 0.021, -36.0), tar)


func build_legacy_fixtures(mill_scale: Material, galvanised: Material,
		hazard: Material, faded: Material) -> void:
	# The WO-001..003 traversal fixtures, re-sited as the loading dock the player
	# starts beside. Native sizes, unchanged.
	_translating_support_mesh = _geometry.add_box("NativeTranslatingSupport", Vector3(5.5, 0.5, 5.5), Vector3(0.0, 0.25, 8.0), hazard)
	_rotating_support_mesh = _geometry.add_box("NativeRotatingSupport", Vector3(6.0, 0.5, 6.0), Vector3(-8.0, 0.25, 0.0), galvanised)
	_geometry.add_box("NativeVaultRail", Vector3(0.44, 0.95, 5.0), Vector3(5.0, 0.475, -6.0), faded)
	_geometry.add_box("NativeMantleLedge", Vector3(4.0, 1.55, 4.0), Vector3(11.0, 0.775, -6.0), mill_scale)
	_geometry.add_box("NativeHangLedge", Vector3(5.0, 3.6, 5.0), Vector3(11.0, 1.8, 4.0), mill_scale)
	_moving_ledge_mesh = _geometry.add_box("NativeMovingLedge", Vector3(4.0, 3.6, 4.0), Vector3(9.0, 1.8, 12.5), galvanised)
	_geometry.add_box("NativeBlockedLedge", Vector3(3.0, 1.55, 3.0), Vector3(-6.0, 0.775, -8.0), mill_scale)
	_geometry.add_box("NativeBlockedCanopy", Vector3(4.4, 0.3, 4.4), Vector3(-6.0, 2.7, -8.0), faded)
	# Crouch fixture (kCrawl* in simulation.cpp): a hazard-striped beam across a
	# 4 m lane, its underside 1.45 m up, on two posts. Crouch to pass under.
	_geometry.add_box("NativeCrawlBeam", Vector3(4.0, 0.4, 0.6), Vector3(-2.0, 1.65, -13.0), hazard)
	for post_x in [-4.15, 0.15]:
		_geometry.add_box("NativeCrawlPost", Vector3(0.3, 1.85, 0.3), Vector3(post_x, 0.925, -13.0), mill_scale)


func build_plant(mill_scale: Material, oxidised: Material, galvanised: Material,
		hazard: Material, faded: Material) -> void:
	# Static plant structure, mirroring the native bodies.
	_geometry.add_box("TipperPylon", Vector3(1.1, 2.9, 2.8), Vector3(29.0, 1.45, -96.0), mill_scale)
	_geometry.add_box("ValvePylon", Vector3(0.6, 7.2, 0.6), Vector3(29.6, 3.6, -92.25), galvanised)
	_geometry.add_box("HoistMast", Vector3(0.8, 12.4, 0.8), Vector3(35.9, 6.2, -96.0), mill_scale)
	_geometry.add_box("Vessel", Vector3(3.4, 4.6, 3.4), Vector3(30.5, 2.3, -101.5), oxidised)
	_geometry.add_box("LiftMast", Vector3(0.8, 11.2, 0.8), Vector3(13.0, 5.6, -100.0), mill_scale)
	_geometry.add_box("Catwalk", Vector3(5.2, 0.28, 18.0), Vector3(16.4, 8.55, -112.0), galvanised)
	_geometry.add_box("AccessStepOne", Vector3(1.8, 1.25, 1.6), Vector3(27.0, 0.625, -94.05), mill_scale)
	_geometry.add_box("AccessStepTwo", Vector3(1.8, 2.5, 1.6), Vector3(28.3, 1.25, -94.05), mill_scale)
	# Starts at the second step's east face; over the step it left 1.0 m of headroom.
	_geometry.add_box("AccessLanding", Vector3(2.0, 0.3, 1.6), Vector3(30.2, 3.65, -94.05), galvanised)
	_geometry.add_box("ReturnBasin", Vector3(2.4, 0.28, 3.0), Vector3(31.82, 1.84, -96.0), oxidised, -0.20)
	for guard_z in [-97.55, -94.45]:
		_geometry.add_box("BasinGuard", Vector3(2.8, 0.9, 0.24), Vector3(31.82, 2.20, guard_z), faded)

	# Catwalk handrail and sheave head: dressing, deliberately dimmer.
	for rail_x in [13.9, 18.9]:
		_geometry.add_box("CatwalkRail", Vector3(0.1, 1.1, 18.0), Vector3(rail_x, 9.2, -112.0), galvanised)
	_geometry.add_box("SheaveHead", Vector3(6.4, 0.5, 1.2), Vector3(15.0, 10.4, -100.0), mill_scale)

	# Moving machine parts, each mirroring one authoritative native body.
	_scoop_locals = [
		Vector3(0.0, -0.03, 0.0), Vector3(1.42, 0.70, 0.0),
		Vector3(0.0, 0.70, -1.42), Vector3(0.0, 0.70, 1.42)]
	var scoop_sizes := [
		Vector3(2.6, 0.06, 2.6), Vector3(0.24, 1.4, 2.6),
		Vector3(2.6, 1.4, 0.24), Vector3(2.6, 1.4, 0.24)]
	for index in 4:
		_scoop_meshes.append(_geometry.add_box("HoistScoop", scoop_sizes[index],
			Vector3(34.0, 0.03, -96.0) + _scoop_locals[index], oxidised))

	_ballast_mesh = _geometry.add_box("Ballast", Vector3(0.84, 0.84, 0.84), Vector3(34.6, 1.0, -96.0), mill_scale)

	_tipper_mesh = Node3D.new()
	_tipper_mesh.name = "Tipper"
	_geometry.presentation_root.add_child(_tipper_mesh)
	_geometry.add_box_to("TipperBeam", Vector3(7.2, 0.4, 2.2), Vector3.ZERO, hazard, _tipper_mesh)
	_geometry.add_box_to("TipperBallast", Vector3(1.0, 1.0, 1.0), Vector3(-3.95, -0.30, 0.0), mill_scale, _tipper_mesh)

	_valve_mesh = Node3D.new()
	_valve_mesh.name = "ValveLever"
	_valve_mesh.position = Vector3(29.6, 7.2, -93.0)
	_geometry.presentation_root.add_child(_valve_mesh)
	_geometry.add_box_to("ValveArm", Vector3(1.6, 0.26, 0.4), Vector3(-0.80, 0.0, 0.0), galvanised, _valve_mesh)
	_geometry.add_box_to("ValveWeight", Vector3(0.68, 0.68, 0.68), Vector3(0.62, 0.0, 0.0), mill_scale, _valve_mesh)

	_lift_mesh = _geometry.add_box("LiftPlatform", Vector3(4.6, 0.32, 4.6), Vector3(16.4, 1.2, -100.0), galvanised)
	_counterweight_mesh = _geometry.add_box("Counterweight", Vector3(1.0, 1.8, 1.0), Vector3(11.6, 7.4, -100.0), mill_scale)

	var rope_material := _geometry.material(Color("2b2621"), 0.5, 0.7)
	_rope_mesh = _geometry.add_box("Rope", Vector3(0.09, 0.09, 1.0), Vector3(30.0, 5.0, -95.0), rope_material)

	build_treadle(galvanised, mill_scale, hazard, rope_material)
	build_kernel_jib(galvanised, mill_scale, hazard)
	build_kernel_needle(galvanised, mill_scale)
	build_kernel_sump(mill_scale)
	build_plume()


# WO-010. The plant's human-scale control and the cable run that proves what it
# is wired to. The cable is drawn between the same two sheave points the native
# PulleyConstraint uses, so what the player sees spanning the yard is the actual
# linkage, not a decorative wire.
func build_treadle(galvanised: StandardMaterial3D, mill_scale: StandardMaterial3D,
		hazard: StandardMaterial3D, cable_material: StandardMaterial3D) -> void:
	_geometry.add_box("TreadlePylon", Vector3(0.48, 0.31, 0.68), Vector3(16.4, 8.845, -106.0), mill_scale)

	_treadle_mesh = Node3D.new()
	_treadle_mesh.name = "Treadle"
	_treadle_mesh.position = Vector3(16.4, 9.09, -106.0)
	_geometry.presentation_root.add_child(_treadle_mesh)
	_geometry.add_box_to("TreadlePlate", Vector3(1.5, 0.08, 1.1), Vector3(-0.75, 0.0, 0.0), hazard,
		_treadle_mesh)
	_geometry.add_box_to("TreadleWeight", Vector3(0.64, 0.64, 0.64), Vector3(0.55, 0.42, 0.0), mill_scale,
		_treadle_mesh)

	_geometry.add_box("TreadleSheaveMast", Vector3(0.2, 2.0, 0.2), Vector3(15.7, 10.3, -105.0), galvanised)
	_geometry.add_box("ValveSheaveMast", Vector3(0.2, 2.4, 0.2), Vector3(29.85, 8.4, -92.0), galvanised)

	# Two cable segments, matching the native pulley's two runs.
	_treadle_cable_a = _geometry.add_box("TreadleCableA", Vector3(0.06, 0.06, 1.0), Vector3.ZERO,
		cable_material)
	_treadle_cable_b = _geometry.add_box("TreadleCableB", Vector3(0.06, 0.06, 1.0), Vector3.ZERO,
		cable_material)
	_span_cable(_treadle_cable_a, Vector3(15.7, 9.09, -106.0), Vector3(15.7, 11.09, -105.0))
	_span_cable(_treadle_cable_b, Vector3(29.85, 7.4, -93.0), Vector3(29.85, 9.6, -92.0))


func _span_cable(node: Node3D, from: Vector3, to: Vector3) -> void:
	if node == null:
		return
	var delta := to - from
	var length := delta.length()
	if length < 0.001:
		return
	node.position = from + delta * 0.5
	node.scale = Vector3(1.0, 1.0, length)
	node.look_at(to, Vector3.UP if absf(delta.normalized().y) < 0.99 else Vector3.RIGHT)


# WO-011. Ascent Atlas v1.0 kernel: KX-JIB + KX-CRATE. Sited well clear of the
# Kellerworks yard -- the atlas kernel is its own bounded proof volume (atlas
# section 9), not band content, so it is not staged inside the tower approach.
func build_kernel_jib(galvanised: StandardMaterial3D, mill_scale: StandardMaterial3D,
		hazard: StandardMaterial3D) -> void:
	var kernel_deck := _geometry.material(Color("55524a"), 0.05, 0.9)
	_geometry.add_box("KernelDeck", Vector3(20.0, 0.6, 20.0), Vector3(200.0, -0.3, 0.0), kernel_deck)
	_geometry.add_box("JibMast", Vector3(0.7, 5.0, 0.7), Vector3(200.0, 2.5, 0.0), mill_scale)

	_jib_boom_mesh = Node3D.new()
	_jib_boom_mesh.name = "JibBoom"
	_jib_boom_mesh.position = Vector3(200.0, 5.0, 0.0)
	_geometry.presentation_root.add_child(_jib_boom_mesh)
	_geometry.add_box_to("JibBoomBeam", Vector3(6.0, 0.3, 0.3), Vector3(3.0, 0.0, 0.0), galvanised,
		_jib_boom_mesh)

	_jib_hook_mesh = _geometry.add_box("JibHook", Vector3(0.3, 0.3, 0.3), Vector3(206.0, 1.65, 0.0), hazard)
	_jib_crate_mesh = _geometry.add_box("KernelCrate", Vector3(1.5, 1.5, 1.5), Vector3(206.0, 0.75, 0.0),
		mill_scale)
	_jib_hoist_cable = _geometry.add_box("JibHoistCable", Vector3(0.05, 0.05, 1.0), Vector3.ZERO,
		_geometry.material(Color("2b2621"), 0.5, 0.7))

	# Capacity-proving stand: fixed, no slew, permanently overweight. A real,
	# visible part of the yard, not a hidden test fixture -- it proves the
	# rated winch force is real whether or not anyone is watching.
	_geometry.add_box("CapacityStandMast", Vector3(0.6, 4.0, 0.6), Vector3(208.0, 2.0, 6.0), mill_scale)
	_jib_capacity_load_mesh = _geometry.add_box("CapacityStandLoad", Vector3(1.2, 1.2, 1.2),
		Vector3(208.0, 0.6, 6.0), hazard)


# WO-012. Ascent Atlas v1.0 kernel: KX-NEEDLE + KX-POCKETS. Each pier is a
# main block plus a lower notch: the seated beam's top sits flush with the
# pier top (a real recessed pocket, not a shelf the beam sits proud on --
# this locomotion has no step-up assist, confirmed by direct observation),
# so its underside needs somewhere to go that is not solid pier.
func build_kernel_needle(galvanised: StandardMaterial3D, mill_scale: StandardMaterial3D) -> void:
	_geometry.add_box("NeedlePierApproachMain", Vector3(3.9, 4.0, 3.2), Vector3(195.35, 2.0, -16.0),
		mill_scale)
	_geometry.add_box("NeedlePierApproachNotch", Vector3(1.1, 3.64, 3.2), Vector3(197.85, 1.82, -16.0),
		mill_scale)
	_geometry.add_box("NeedlePierFarMain", Vector3(3.9, 4.0, 3.2), Vector3(204.65, 2.0, -16.0), mill_scale)
	_geometry.add_box("NeedlePierFarNotch", Vector3(1.1, 3.64, 3.2), Vector3(202.15, 1.82, -16.0), mill_scale)

	_geometry.add_box("NeedleHoistMast", Vector3(0.7, 7.0, 0.7), Vector3(200.0, 3.5, -13.4), mill_scale)
	_geometry.add_box("NeedleHoistHead", Vector3(0.8, 0.4, 0.8), Vector3(200.0, 7.0, -16.0), mill_scale)

	_needle_beam_mesh = _geometry.add_box("NeedleBeam", Vector3(5.0, 0.36, 1.0), Vector3(200.0, 6.2, -16.0),
		galvanised)
	_needle_hoist_cable = _geometry.add_box("NeedleHoistCable", Vector3(0.05, 0.05, 1.0), Vector3.ZERO,
		_geometry.material(Color("2b2621"), 0.5, 0.7))


# WO-013. Ascent Atlas v1.0 kernel: KX-SUMP + KX-GRATE. Fixed decking flanks
# one grate panel; the panel's own tint amplifies the real derived predicate
# (hazard amber wet, mill-scale grey safe) read back from native every frame
# -- it never decides the predicate, only displays it.
func build_kernel_sump(mill_scale: StandardMaterial3D) -> void:
	_geometry.add_box("SumpApproachDeck", Vector3(3.0, 0.3, 3.0), Vector3(197.0, 2.85, 16.0), mill_scale)
	_geometry.add_box("SumpFarDeck", Vector3(3.0, 0.3, 3.0), Vector3(203.0, 2.85, 16.0), mill_scale)
	_geometry.add_box("SumpApproachLeg", Vector3(0.5, 3.0, 0.5), Vector3(197.0, 1.5, 16.0), mill_scale)
	_geometry.add_box("SumpFarLeg", Vector3(0.5, 3.0, 0.5), Vector3(203.0, 1.5, 16.0), mill_scale)

	_sump_grate_safe_material = mill_scale
	_sump_grate_hazard_material = _geometry.material(Color("6b4a1c"), 0.2, 0.75, Color("c98a2c"), 0.8)
	_sump_grate_mesh = _geometry.add_box("SumpGrate", Vector3(3.0, 0.3, 3.0), Vector3(200.0, 2.85, 16.0),
		_sump_grate_hazard_material)


# A soft radial falloff for every billboard particle in the scene. Without
# one, an untextured quad renders as a hard-edged square, and a plume reads as
# a drift of grey boxes rather than steam.
func _smoke_texture() -> GradientTexture2D:
	var gradient := Gradient.new()
	gradient.set_color(0, Color(1.0, 1.0, 1.0, 1.0))
	gradient.set_color(1, Color(1.0, 1.0, 1.0, 0.0))
	var texture := GradientTexture2D.new()
	texture.gradient = gradient
	texture.width = 64
	texture.height = 64
	texture.fill = GradientTexture2D.FILL_RADIAL
	texture.fill_from = Vector2(0.5, 0.5)
	texture.fill_to = Vector2(1.0, 0.5)
	return texture


func build_plume() -> void:
	_plume = CPUParticles3D.new()
	_plume.name = "VentPlume"
	_plume.position = Vector3(30.5, 5.1, -101.5)
	# Kept deliberately sparse and thin: at the old 160 x 2.2 m billboards this
	# vent stacked into an opaque white wall whenever it sat between the eye
	# and the tower, swallowing the whole frame behind it.
	_plume.amount = 60
	_plume.lifetime = 2.6
	_plume.direction = Vector3(0.15, 1.0, 0.0)
	_plume.spread = 16.0
	_plume.gravity = Vector3(0.6, 1.1, 0.0)
	_plume.damping_min = 0.5
	_plume.damping_max = 1.4
	_plume.emission_shape = CPUParticles3D.EMISSION_SHAPE_SPHERE
	_plume.emission_sphere_radius = 0.45
	_plume.scale_amount_min = 0.5
	_plume.scale_amount_max = 1.1
	_plume.emitting = false

	var quad := QuadMesh.new()
	quad.size = Vector2(1.2, 1.2)
	_plume_material = StandardMaterial3D.new()
	_plume_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	_plume_material.shading_mode = BaseMaterial3D.SHADING_MODE_PER_PIXEL
	_plume_material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	_plume_material.albedo_color = Color(0.82, 0.80, 0.76, 0.10)
	_plume_material.albedo_texture = _smoke_texture()
	_plume_material.roughness = 1.0
	_plume_material.disable_receive_shadows = false
	quad.material = _plume_material
	_plume.mesh = quad
	_geometry.presentation_root.add_child(_plume)

	# Stack plumes on the tower itself, high enough to shear the mass before the
	# crown. These are weather, not simulation, and are not claimed otherwise.
	# High stack plume, kept well above the climbable frame and much smaller
	# than before: at the old scale these billboards swallowed the structure.
	for stack in [Vector3(-34.0, 320.0, -168.0), Vector3(22.0, 392.0, -172.0)]:
		var haze := CPUParticles3D.new()
		haze.name = "StackPlume"
		haze.position = stack
		haze.amount = 26
		haze.lifetime = 20.0
		haze.direction = Vector3(0.8, 0.6, 0.0)
		haze.spread = 22.0
		haze.gravity = Vector3(3.2, 1.6, 0.0)
		haze.scale_amount_min = 8.0
		haze.scale_amount_max = 20.0
		haze.emitting = true
		var stack_quad := QuadMesh.new()
		stack_quad.size = Vector2(2.0, 2.0)
		var stack_material := StandardMaterial3D.new()
		stack_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		stack_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		stack_material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
		stack_material.albedo_color = Color(0.66, 0.64, 0.62, 0.16)
		stack_material.albedo_texture = _smoke_texture()
		stack_quad.material = stack_material
		haze.mesh = stack_quad
		_geometry.presentation_root.add_child(haze)

	# Working steam venting from the frame itself, at a human scale -- the
	# references are full of small, sharp vents, not fog banks.
	for level in range(2, _stack_world.get_level_count(), 3):
		var vent_y := float(level) * StackWorldBuilder.STACK_LEVEL_HEIGHT + 1.6
		var vent := CPUParticles3D.new()
		vent.name = "FrameVent"
		vent.position = Vector3(StackWorldBuilder.STACK_CENTER.x + (StackWorldBuilder.STACK_HALF_EXTENT - 2.2) * (1.0 if level % 2 == 0 else -1.0),
			vent_y, StackWorldBuilder.STACK_CENTER.z - StackWorldBuilder.STACK_HALF_EXTENT + 2.2)
		vent.amount = 10
		vent.lifetime = 2.2
		vent.direction = Vector3(0.3, 1.0, 0.0)
		vent.spread = 14.0
		vent.gravity = Vector3(0.4, 1.6, 0.0)
		vent.emission_shape = CPUParticles3D.EMISSION_SHAPE_SPHERE
		vent.emission_sphere_radius = 0.3
		vent.scale_amount_min = 0.8
		vent.scale_amount_max = 2.6
		vent.emitting = true
		var vent_quad := QuadMesh.new()
		vent_quad.size = Vector2(1.6, 1.6)
		var vent_material := StandardMaterial3D.new()
		vent_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		vent_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		vent_material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
		vent_material.albedo_color = Color(0.90, 0.89, 0.86, 0.22)
		vent_material.albedo_texture = _smoke_texture()
		vent_quad.material = vent_material
		vent.mesh = vent_quad
		_geometry.presentation_root.add_child(vent)


func build_lighting() -> void:
	# Sodium vapour: warm, low, and sourced from actual fittings on masts.
	for mast in [Vector3(-10.0, 0.0, -24.0), Vector3(-10.0, 0.0, -62.0),
			Vector3(-10.0, 0.0, -100.0), Vector3(22.0, 0.0, -44.0),
			Vector3(22.0, 0.0, -82.0), Vector3(40.0, 0.0, -104.0)]:
		_geometry.add_box("LampMast", Vector3(0.38, 11.0, 0.38), mast + Vector3(0.0, 5.5, 0.0),
			_geometry.material(Color("3a3835"), 0.7, 0.62))
		_geometry.add_box("LampHead", Vector3(1.5, 0.4, 0.8), mast + Vector3(0.6, 10.9, 0.0),
			_geometry.material(Color("5a4b2c"), 0.4, 0.7, Color("b06a1c"), 1.4))
		var lamp := OmniLight3D.new()
		lamp.name = "SodiumFlood"
		lamp.position = mast + Vector3(0.6, 10.6, 0.0)
		lamp.light_color = Color(1.0, 0.585, 0.225)
		lamp.light_energy = 4.5
		lamp.omni_range = 26.0
		lamp.omni_attenuation = 1.6
		_light_rig.add_child(lamp)

	for base_x in [-46.0, -16.0, 16.0, 46.0]:
		var base_flood := OmniLight3D.new()
		base_flood.name = "TowerFootFlood"
		base_flood.position = Vector3(base_x, 15.0, -132.0)
		base_flood.light_color = Color(1.0, 0.7, 0.42)
		base_flood.light_energy = 5.0
		base_flood.omni_range = 48.0
		base_flood.omni_attenuation = 1.3
		_light_rig.add_child(base_flood)

	if not _regression_scene:
		return
	_fire_box = OmniLight3D.new()
	_fire_box.name = "FireBox"
	_fire_box.position = Vector3(30.5, 1.2, -100.0)
	_fire_box.light_color = Color(1.0, 0.42, 0.14)
	_fire_box.light_energy = 2.4
	_fire_box.omni_range = 16.0
	_light_rig.add_child(_fire_box)

	_vent_light = OmniLight3D.new()
	_vent_light.name = "VentGlow"
	_vent_light.position = Vector3(30.5, 5.3, -101.5)
	_vent_light.light_color = Color(0.86, 0.82, 0.76)
	_vent_light.light_energy = 1.2
	_vent_light.omni_range = 22.0
	_light_rig.add_child(_vent_light)


func build_gear_motif(mill_scale: Material, oxidised: Material) -> void:
	# Decorative exposed gear + wound drum on the tower face (identity doc item
	# 5). Rotation is applied in mirror from the real machine phase.
	_gear_pivot = Node3D.new()
	_gear_pivot.name = "FaceGearPivot"
	_gear_pivot.set_meta(&"solid_disc", Vector2(3.9 + 0.53, 0.7))
	_gear_pivot.set_meta(&"part", "FaceGear")
	_gear_pivot.position = Vector3(-30.0, 58.0, -142.4)
	_geometry.presentation_root.add_child(_gear_pivot)
	var hub := CylinderMesh.new()
	hub.top_radius = 3.6
	hub.bottom_radius = 3.6
	hub.height = 0.7
	hub.radial_segments = 20
	var hub_instance := MeshInstance3D.new()
	hub_instance.set_meta(&"part", "GearMotifHub")
	hub_instance.mesh = hub
	hub_instance.material_override = mill_scale
	hub_instance.rotation = Vector3(deg_to_rad(90.0), 0.0, 0.0)
	_gear_pivot.add_child(hub_instance)
	for tooth_index in 12:
		var angle := float(tooth_index) / 12.0 * TAU
		var tooth := _geometry.add_box_to("GearTooth", Vector3(0.75, 0.75, 0.7), Vector3.ZERO, mill_scale, _gear_pivot)
		tooth.position = Vector3(cos(angle), sin(angle), 0.0) * 3.9
		tooth.rotation = Vector3(0.0, 0.0, angle)

	_drum_pivot = Node3D.new()
	_drum_pivot.name = "FaceDrumPivot"
	_drum_pivot.position = Vector3(-19.0, 58.0, -142.4)
	_geometry.presentation_root.add_child(_drum_pivot)
	var drum := CylinderMesh.new()
	drum.top_radius = 1.9
	drum.bottom_radius = 1.9
	drum.height = 3.2
	drum.radial_segments = 14
	var drum_instance := MeshInstance3D.new()
	drum_instance.set_meta(&"part", "FaceDrum")
	drum_instance.mesh = drum
	drum_instance.material_override = oxidised
	drum_instance.rotation = Vector3(0.0, 0.0, deg_to_rad(90.0))
	_drum_pivot.add_child(drum_instance)


func build_crane(mill_scale: Material, hazard: Material) -> void:
	# Scale-telegraphing crane (identity doc item 6). Sway is applied in
	# update_ambient_dressing from a wall-clock, never from native state --
	# it is explicitly not simulated rigging.
	var mast_base := Vector3(58.0, 0.0, -108.0)
	_geometry.add_box("CraneMast", Vector3(1.1, 26.0, 1.1), mast_base + Vector3(0.0, 13.0, 0.0), hazard)

	_crane_boom = Node3D.new()
	_crane_boom.name = "CraneBoom"
	_crane_boom.position = mast_base + Vector3(0.0, 25.0, 0.0)
	_geometry.presentation_root.add_child(_crane_boom)
	_geometry.add_box_to("CraneBoomArm", Vector3(24.0, 0.9, 0.9), Vector3(-11.0, 0.6, 0.0), mill_scale, _crane_boom)
	_geometry.add_box_to("CraneCounterArm", Vector3(6.0, 0.9, 0.9), Vector3(4.0, 0.6, 0.0), mill_scale, _crane_boom)
	_geometry.add_box_to("CraneCounterweight", Vector3(2.4, 2.0, 2.4), Vector3(7.4, -0.4, 0.0), mill_scale, _crane_boom)

	_crane_hook = Node3D.new()
	_crane_hook.name = "CraneHook"
	_crane_hook.position = Vector3(-20.0, -13.0, 0.0)
	_crane_boom.add_child(_crane_hook)
	_geometry.add_box_to("CraneCable", Vector3(0.08, 12.0, 0.08), Vector3(0.0, 6.0, 0.0), hazard, _crane_hook)
	_crane_crate = _geometry.add_box_to("CraneCrate", Vector3(2.6, 2.0, 2.6), Vector3.ZERO, mill_scale, _crane_hook)
