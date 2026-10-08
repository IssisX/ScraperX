extends SceneTree

const Dust := preload("res://presentation/reclaim_dust.gd")
var _checks := 0
var _failures := 0


func _initialize() -> void:
	_run.call_deferred()


func _check(condition: bool, message: String) -> void:
	_checks += 1
	if not condition:
		_failures += 1
		push_error("SCRAPERX_RECLAIM_DUST FAIL " + message)


func _active(dust: Node3D) -> int:
	var count := 0
	for particles in dust.get_children():
		if particles.visible:
			count += 1
	return count


func _impact(state: Dictionary, count: int, position: Vector3, speed: float) -> void:
	state["rubble_impact_count"] = count
	state["rubble_impact_position"] = position
	state["rubble_impact_speed_mps"] = speed


func _run() -> void:
	var dust := Dust.new()
	root.add_child(dust)
	dust.setup()
	dust.setup()
	_check(dust.get_child_count() == 3 and _active(dust) == 0,
		"idempotent fixed pool starts quietly")
	for particles in dust.get_children():
		_check(particles is CPUParticles3D and particles.one_shot and not particles.emitting
			and particles.amount <= 32 and particles.lifetime < 1.0
			and particles.gravity.y < 0.0 and particles.damping_min > 0.0
			and not particles.local_coords, "bounded gravity/drag powder has no collision nodes")
		var material: StandardMaterial3D = particles.mesh.material
		_check(material.shading_mode == BaseMaterial3D.SHADING_MODE_PER_PIXEL
			and material.transparency == BaseMaterial3D.TRANSPARENCY_ALPHA
			and material.billboard_mode == BaseMaterial3D.BILLBOARD_ENABLED
			and material.vertex_color_use_as_albedo
			and particles.color_ramp.get_color(particles.color_ramp.get_point_count() - 1).a == 0.0,
			"lit alpha billboards fade smoothly")
	var state := {"rubble_impact_count": 7, "rubble_impact_position": Vector3.ONE,
		"rubble_impact_speed_mps": 8.0, "rubble_break_serial": 4,
		"rubble_break_valid": true, "rubble_break_position": Vector3.ONE,
		"rubble_break_force_n": 12000.0, "rubble_break_torque_nm": 2400.0,
		"player_position": Vector3.ZERO}
	var original := state.duplicate(true)
	dust.update_from_native(state, 0.0)
	dust.update_from_native(state, 1.0)
	_check(_active(dust) == 0 and state == original, "first historical receipts are quiet and read-only")
	_impact(state, 8, Vector3(1.0, 0.0, 0.0), 2.0)
	dust.update_from_native(state, 0.0)
	_check(_active(dust) == 1 and dust.get_child(0).global_position == state["rubble_impact_position"]
		and is_equal_approx(dust.get_child(0).initial_velocity_max, 0.32),
		"new native impact emits at contact with speed derived from closing speed")
	dust.update_from_native(state, 0.01)
	_check(_active(dust) == 1, "a repeated native receipt cannot emit twice")
	_impact(state, 9, Vector3(2.0, 0.0, 0.0), 3.0)
	dust.update_from_native(state, 0.03)
	_impact(state, 10, Vector3(3.0, 0.0, 0.0), 9.0)
	dust.update_from_native(state, 0.03)
	_impact(state, 11, Vector3(4.0, 0.0, 0.0), 4.0)
	dust.update_from_native(state, 0.03)
	_check(_active(dust) == 1, "contact storm obeys emission cooldown")
	dust.update_from_native(state, 0.03)
	_check(_active(dust) == 2 and dust.get_child(1).global_position == Vector3(3.0, 0.0, 0.0),
		"only strongest pending contact survives coalescing")
	state["rubble_break_serial"] = 5
	state["rubble_break_position"] = Vector3(5.0, 0.0, 0.0)
	dust.update_from_native(state, 0.13)
	_check(_active(dust) == 3 and dust.get_child(2).global_position == state["rubble_break_position"],
		"a valid native fracture uses the same finite pool")
	_impact(state, 12, Vector3(6.0, 0.0, 0.0), 6.0)
	dust.update_from_native(state, 0.13)
	_check(_active(dust) == 3 and dust.get_child_count() == 3,
		"pool saturation cannot allocate another emitter")
	dust.update_from_native(state, 0.25)
	_check(dust._pending.is_empty(), "a saturated pending event expires instead of replaying late")
	state["rubble_impact_count"] = 0
	state["rubble_break_serial"] = 0
	dust.update_from_native(state, 0.0)
	_check(_active(dust) == 0 and dust._pending.is_empty(), "decreasing receipt clears and baselines both streams")
	state["rubble_break_serial"] = 1
	state["rubble_break_valid"] = false
	dust.update_from_native(state, 0.13)
	state["rubble_break_valid"] = true
	dust.update_from_native(state, 0.13)
	_check(_active(dust) == 0, "invalid fracture consumes its receipt and cannot later replay")
	_impact(state, 1, Vector3.ZERO, 0.2)
	dust.update_from_native(state, 0.13)
	_check(_active(dust) == 0, "minor resting contact produces no cloud")
	_impact(state, 2, Vector3(100.0, 0.0, 0.0), 9.0)
	dust.update_from_native(state, 0.13)
	state["player_position"] = Vector3(100.0, 0.0, 0.0)
	dust.update_from_native(state, 0.13)
	_check(_active(dust) == 0, "distant contact is culled and consumed")
	state["player_position"] = Vector3.ZERO
	_impact(state, 3, Vector3.ZERO, 9.0)
	dust.update_from_native(state, 0.13)
	dust.set_reduced_motion(true)
	_check(_active(dust) == 0, "reduced motion immediately suppresses active dust")
	_impact(state, 4, Vector3.ZERO, 9.0)
	dust.update_from_native(state, 0.13)
	dust.set_reduced_motion(false)
	dust.update_from_native(state, 0.13)
	_check(_active(dust) == 0, "comfort suppression consumes events without replay on reenable")
	dust.reset()
	dust.update_from_native(state, 0.13)
	_check(_active(dust) == 0, "explicit restart quietly baselines current receipt")
	_impact(state, 5, Vector3.ZERO, 9.0)
	dust.update_from_native(state, 0.13)
	_check(_active(dust) == 1, "pool restarts after suppression and reset")
	state["player_position"] = Vector3(100.0, 0.0, 0.0)
	dust.update_from_native(state, 0.01)
	_check(_active(dust) == 0, "leaving the event culls active billboards immediately")
	var camera := Camera3D.new()
	root.add_child(camera)
	camera.current = true
	state.erase("player_position")
	_impact(state, 6, Vector3.ZERO, 9.0)
	dust.update_from_native(state, 0.13)
	_check(_active(dust) == 1, "active viewport camera supplies optional distance-cull fallback")
	await create_timer(1.0).timeout
	_check(_active(dust) == 0 and not true in dust._busy,
		"real one-shot completion releases pool slots without spawning more nodes")
	dust.free()
	camera.free()
	print("SCRAPERX_RECLAIM_DUST checks=%d failures=%d" % [_checks, _failures])
	quit(0 if _failures == 0 else 49)
