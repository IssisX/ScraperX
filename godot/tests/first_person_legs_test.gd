extends SceneTree

# Supplied native observations exercise presentation only; no Jolt runtime claim.
# godot --headless --path godot --script res://tests/first_person_legs_test.gd
const Legs := preload("res://presentation/first_person_legs.gd")
var _checks := 0
var _failed := false

func _initialize() -> void:
	_run.call_deferred()

func _check(ok: bool, label: String) -> void:
	_checks += 1
	if not ok:
		_failed = true
		push_error("SCRAPERX_LEGS FAIL " + label)

func _sample() -> Dictionary:
	return {"native_tick": 100, "render_position": Vector3(0, 0.9, 0),
		"facing": Vector3.FORWARD, "player_grounded": true, "player_crouched": false,
		"operating": &"", "dead": false, "foot_push_active": true,
		"foot_push_start_count": 1, "foot_push_source_contact_tick": 99,
		"foot_push_support_entity_id": 42, "foot_push_world_anchor": Vector3(0.115, 0, 0),
		"foot_push_world_normal": Vector3.UP}

func _run() -> void:
	var rig := Legs.new()
	root.add_child(rig)
	var material := StandardMaterial3D.new()
	rig.build(material, material, material)
	_check(rig.get_child_count() == 11, "hip garment plus ten leg surfaces")
	_check(not rig.visible and rig.planted_foot_index() == -1, "build starts cleared")
	for child in rig.get_children():
		_check(child is MeshInstance3D and child.cast_shadow == GeometryInstance3D.SHADOW_CASTING_SETTING_OFF,
			"mesh only with shadows disabled")
	_check(rig._legs[0].upper.mesh == rig._legs[1].upper.mesh, "shared meshes")
	_check(rig._legs[0].upper.mesh is ArrayMesh and rig._legs[0].lower.mesh is ArrayMesh
		and rig._legs[0].knee.mesh is ArrayMesh and rig._legs[0].sole.mesh is ArrayMesh,
		"shaped workwear replaces tube, sphere and box primitives")
	_check(rig._legs[0].upper.material_override == rig._legs[1].upper.material_override
		and rig._legs[0].upper.material_override == rig._pelvis.material_override,
		"one shared fabric resource joins both legs and hips")
	_check(rig._legs[0].upper.material_override.vertex_color_use_as_albedo,
		"cloth seam and reinforcement shades reach the shared material")
	_check(absf(rig._legs[0].sole.mesh.get_aabb().position.y + 0.0125) < 0.00001,
		"beveled sole retains the actual native contact plane")
	var boot_mesh: Mesh = rig._legs[0].boot.mesh
	_check(boot_mesh is ArrayMesh and boot_mesh.get_surface_count() == 1,
		"rounded work boot retains one surface")
	_check(boot_mesh == rig._legs[1].boot.mesh, "rounded boot resource shared")
	var boot_vertices: PackedVector3Array = boot_mesh.surface_get_arrays(0)[Mesh.ARRAY_VERTEX]
	var has_toe_chamfer := false
	for vertex in boot_vertices:
		if vertex.z < -0.11 and absf(vertex.x) > 0.03 and absf(vertex.x) < 0.07:
			has_toe_chamfer = true
	_check(has_toe_chamfer, "toe outline has chamfered corners")
	var pose := _sample()
	pose.foot_push_active = false
	pose.bob_phase = PI
	rig.update_legs(pose, Transform3D.IDENTITY, 0.016)
	var idle_left: Vector3 = rig._legs[0].point
	var idle_right: Vector3 = rig._legs[1].point
	pose.velocity = Vector3(0, 0, -3.5)
	rig.update_legs(pose, Transform3D.IDENTITY, 0.016)
	var walk_offset: Vector3 = rig._legs[0].point - idle_left
	_check(walk_offset.z < -0.1 and (rig._legs[1].point - idle_right).z > 0.1,
		"grounded moving feet stride in opposite phases")
	var repeated: Vector3 = rig._legs[0].point
	rig.update_legs(pose, Transform3D.IDENTITY, 0.08)
	_check(rig._legs[0].point == repeated, "stride uses supplied phase without its own clock")
	pose.bob_phase = 0.0
	rig.update_legs(pose, Transform3D.IDENTITY, 0.016)
	_check(rig._legs[0].point.y > idle_left.y + 0.04, "swing foot lifts modestly")
	pose.player_crouched = true
	pose.render_position.y = 0.6
	pose.bob_phase = PI
	rig.update_legs(pose, Transform3D.IDENTITY, 0.016)
	_check(absf(rig._legs[0].point.z + 0.035) < absf(walk_offset.z), "crouch stride shortens")
	pose.player_grounded = false
	pose.player_crouched = false
	pose.render_position.y = 0.9
	rig.update_legs(pose, Transform3D.IDENTITY, 0.016)
	_check(rig._legs[0].point.y > idle_left.y + 0.15 and rig.planted_foot_index() == -1,
		"airborne unloaded feet tuck without a plant")
	rig.reset()
	var state := _sample()
	state.velocity = Vector3(0, 0, -3.5)
	state.bob_phase = 1.7
	rig.update_legs(state, Transform3D.IDENTITY, 0.016)
	_check(rig.planted_foot_index() == 1, "generation selects presentation side")
	_check(rig.planted_foot_position().distance_to(state.foot_push_world_anchor) < 0.00001, "floor sole exact")
	_check(rig.planted_foot_normal().distance_to(Vector3.UP) < 0.00001, "floor sole normal")
	_check((rig._legs[1].upper.global_transform * Vector3(0, 0.5, 0)).y >
		(rig._legs[1].upper.global_transform * Vector3(0, -0.5, 0)).y,
		"trouser proximal end faces the hip")
	state.player_grounded = false
	rig.update_legs(state, Transform3D.IDENTITY, 0.016)
	_check(rig.planted_foot_position().distance_to(state.foot_push_world_anchor) < 0.00001,
		"active sole overrides airborne tuck as well as walking stride")
	state.player_grounded = true
	var camera := Transform3D(Basis.from_euler(Vector3(1.2, 2.1, 0.1)), Vector3(90, 80, 70))
	for i in 4:
		rig.update_legs(state, camera, 0.016)
	_check(rig.planted_foot_index() == 1, "repeated sample never alternates")
	_check(rig.planted_foot_position().distance_to(state.foot_push_world_anchor) < 0.00001, "free look cannot move plant")
	state.foot_push_world_anchor += Vector3(0.04, 0.07, -0.02)
	rig.update_legs(state, camera, 0.016)
	_check(rig.planted_foot_position().distance_to(state.foot_push_world_anchor) < 0.00001, "moving support follows exact supplied point")
	state.foot_push_world_normal = Vector3.BACK
	state.foot_push_world_anchor = Vector3(0.115, 0.4, -0.4)
	rig.update_legs(state, camera, 0.016)
	_check(rig.planted_foot_position().distance_to(state.foot_push_world_anchor) < 0.00001, "wall sole exact")
	_check(rig.planted_foot_normal().distance_to(Vector3.BACK) < 0.00001, "wall sole faces real normal")
	_check(rig._legs[1].point == state.foot_push_world_anchor, "wall anchor never clamped by IK")
	state.foot_push_start_count = 2
	state.foot_push_world_anchor.x = -0.115
	rig.update_legs(state, camera, 0.016)
	_check(rig.planted_foot_index() == 0 and not rig._legs[1].planted, "new generation clears old plant immediately")
	state.foot_push_start_count = 0
	rig.update_legs(state, camera, 0.016)
	_check(rig.planted_foot_index() == 0, "zero motor count can retain supplied passive contact")
	state.foot_push_active = false
	state.velocity = Vector3.ZERO
	rig.update_legs(state, camera, 0.016)
	_check(rig.planted_foot_index() == -1 and not rig._legs[0].planted, "release clears planted status immediately")
	_check(rig._legs[0].returning, "release starts unloaded return")
	for i in 8:
		rig.update_legs(state, camera, 0.016)
	var neutral := Vector3(-0.115, 0, -0.035)
	_check(not rig._legs[0].returning and rig._legs[0].point.distance_to(neutral) < 0.00001,
		"unloaded foot returns in 0.12 seconds")
	for invalid in [
		{"foot_push_support_entity_id": 0}, {"foot_push_source_contact_tick": 0},
		{"foot_push_source_contact_tick": 101}, {"foot_push_world_normal": Vector3.ZERO},
		{"foot_push_world_normal": Vector3(NAN, 0, 0)},
		{"foot_push_world_normal": Vector3(1.0e30, 1.0e30, 0)},
		{"foot_push_world_anchor": Vector3.INF}, {"foot_push_world_anchor": Vector3(20, 20, 20)}]:
		state = _sample()
		state.merge(invalid, true)
		rig.update_legs(state, camera, 0.016)
		_check(rig.planted_foot_index() == -1, "invalid or unreachable contact cannot plant " + str(invalid))
	state = _sample()
	state.render_position = Vector3(0, 0.6, 0)
	state.player_crouched = true
	state.crouch_visual_01 = 0.25
	rig.update_legs(state, camera, 0.016)
	var lowering_hip := rig._pelvis.global_position.y
	_check(rig.planted_foot_position().distance_to(state.foot_push_world_anchor) < 0.00001,
		"camera-shared crouch transition preserves the material point")
	state.crouch_visual_01 = 1.0
	rig.update_legs(state, camera, 0.016)
	_check(rig.planted_foot_index() == 1, "crouched floor contact reachable")
	_check(lowering_hip > rig._pelvis.global_position.y + 0.15,
		"hip lowers using the supplied camera crouch phase")
	# A transformed parent must not offset the supplied world material point.
	rig.position = Vector3(2, 3, 4)
	rig.rotation.y = 0.7
	rig.update_legs(state, camera, 0.016)
	_check(rig.planted_foot_position().distance_to(state.foot_push_world_anchor) < 0.00001, "world anchor under transformed rig")
	for station in [&"forklift", &"yard_crane", &"camera_station"]:
		state = _sample()
		state.operating = station
		rig.update_legs(state, camera, 0.016)
		_check(not rig.visible and rig.planted_foot_index() == -1, "station hides and clears " + String(station))
	state = _sample()
	state.operating = &"grappling_gun"
	rig.update_legs(state, camera, 0.016)
	_check(rig.visible and rig.planted_foot_index() == 1, "portable grapple retains body view")
	state.dead = true
	rig.update_legs(state, camera, 0.016)
	_check(not rig.visible and rig.planted_foot_index() == -1, "death clears rig")
	state = _sample()
	rig.update_legs(state, camera, 0.016)
	state.native_tick = 50
	rig.update_legs(state, camera, 0.016)
	_check(not rig.visible and rig.planted_foot_index() == -1, "rewind clears retained pose")
	state = _sample()
	rig.update_legs(state, camera, 0.016)
	rig.reset()
	_check(not rig.visible and rig.planted_foot_index() == -1 and not rig._legs[0].initialized,
		"explicit reset clears retained pose")
	print("SCRAPERX_LEGS ", "FAIL" if _failed else "PASS", " checks=", _checks,
		" supplied observations only; no native runtime or device visual claim")
	rig.free()
	quit(1 if _failed else 0)
