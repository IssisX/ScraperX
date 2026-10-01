extends SceneTree

const Response := preload("res://presentation/landing_camera_response.gd")
var _checks := 0
var _main: Node3D


func _initialize() -> void:
	_run.call_deferred()


func _check(condition: bool, message: String) -> bool:
	_checks += 1
	if condition:
		return true
	push_error("SCRAPERX_LANDING_CAMERA FAIL " + message)
	quit(43)
	return false


func _response_checks() -> bool:
	var baseline := {"landing_count": 0, "landing_balance": 1.0}
	var impact := {"landing_count": 1, "landing_normal_speed_mps": 5.0,
		"landing_approach_energy_j": 1200.0, "landing_tangent_energy_j": 150.0,
		"landing_slip_velocity": Vector3(3.0, 0.0, -1.0),
		"landing_balance": 0.25, "landing_recovering": true}
	var unchanged := impact.duplicate(true)
	var one := Response.new()
	var divided := Response.new()
	one.update(baseline, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	divided.update(baseline, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	one.update(impact, 0.08, true, Vector3.RIGHT, Vector3.FORWARD)
	for i in 4:
		divided.update(impact, 0.02, true, Vector3.RIGHT, Vector3.FORWARD)
	if not _check(one.translation.distance_to(divided.translation) < 0.000001 \
			and one.rotation.distance_to(divided.rotation) < 0.000001,
			"critical damping is invariant to splitting a rendering frame"):
		return false
	if not _check(one.translation.y < -0.01 and absf(one.rotation.y) > 0.0001 \
			and impact == unchanged, "normal impact compresses, actual slip sways, native state stays read-only"):
		return false
	one.update(impact, 0.016, false, Vector3.RIGHT, Vector3.FORWARD)
	if not _check(one.translation == Vector3.ZERO and one.rotation == Vector2.ZERO,
			"motion comfort clears lens movement immediately"):
		return false
	impact.landing_recovering = false
	impact.landing_balance = 1.0
	impact.landing_slip_velocity = Vector3.ZERO
	for i in 120:
		divided.update(impact, 1.0 / 60.0, true, Vector3.RIGHT, Vector3.FORWARD)
	if not _check(divided.translation == Vector3.ZERO and divided.rotation == Vector2.ZERO,
			"completed footing recovery settles exactly to the true eye"):
		return false
	divided.reset()
	divided.update(impact, 0.1, true, Vector3.RIGHT, Vector3.FORWARD)
	return _check(divided.translation == Vector3.ZERO and divided.rotation == Vector2.ZERO,
		"restart seeds current event count without replaying a historical impact")


func _drop(height: float, move_x: float) -> Dictionary:
	var native: Object = _main._native
	if not _check(bool(native.debug_restart_at(Vector3(6.0, height, -25.0))), "clear grade-drop staging"):
		return {}
	_main._landing_camera.reset()
	native.set_move_input(move_x, 0.0)
	var dt := float(native.get_fixed_step_seconds())
	_main._render_snapshot(0.0)
	var before := int(native.get_landing_state()["landing_count"])
	var witness: Dictionary = {}
	var deepest := 0.0
	for tick in 240:
		native.advance_frame(dt)
		var physical_position: Vector3 = native.get_player_position()
		var physical_velocity: Vector3 = native.get_player_linear_velocity()
		_main._render_snapshot(dt)
		if not _check(native.get_player_position() == physical_position \
				and native.get_player_linear_velocity() == physical_velocity,
				"render feedback never changes the physical player"):
			return {}
		deepest = minf(deepest, _main._landing_camera.translation.y)
		var contact: Dictionary = native.get_landing_state()
		if int(contact["landing_count"]) > before and witness.is_empty():
			witness = contact.duplicate(true)
		if not witness.is_empty() and tick > 120:
			break
	native.set_move_input(0.0, 0.0)
	if not _check(not witness.is_empty() and float(witness["landing_approach_energy_j"]) > 0.0 \
			and float(witness["landing_normal_speed_mps"]) > 0.0 \
			and int(witness["landing_support_entity_id"]) > 0,
			"ordinary grade contact reports its own approach energy and real support"):
		return {}
	if not _check(deepest < -0.001 and deepest > -0.25,
			"ordinary native landing drives bounded knee compression"):
		return {}
	witness["observed_camera_dip_m"] = deepest
	return witness


func _run() -> void:
	if not _response_checks():
		return
	if "--landing-camera-unit-only" in OS.get_cmdline_user_args():
		print("SCRAPERX_LANDING_CAMERA_UNIT PASS checks=%d" % _checks)
		quit(0)
		return
	_main = load("res://main.tscn").instantiate()
	root.add_child(_main)
	_main.set_process(false)
	if not _check(_main._native != null and _main._native.has_method("get_landing_state"),
			"shipping native landing bridge is loaded"):
		return
	_main._settings.head_bob = true
	_main._head_bob_on = true
	var low := _drop(1.4, 0.0)
	if low.is_empty():
		return
	var high := _drop(4.0, 1.0)
	if high.is_empty():
		return
	if not _check(float(high["landing_approach_energy_j"]) > float(low["landing_approach_energy_j"]) \
			and float(high["observed_camera_dip_m"]) < float(low["observed_camera_dip_m"]),
			"a larger real fall carries more contact energy and compresses the lens farther"):
		return
	_main._head_bob_on = false
	_main._render_snapshot(1.0 / 60.0)
	if not _check(_main._landing_camera.translation == Vector3.ZERO \
			and _main._landing_camera.rotation == Vector2.ZERO,
			"the shipping camera honors head motion OFF"):
		return
	print("SCRAPERX_LANDING_CAMERA PASS checks=%d low_j=%.2f high_j=%.2f low_dip_m=%.4f high_dip_m=%.4f" % [
		_checks, float(low["landing_approach_energy_j"]), float(high["landing_approach_energy_j"]),
		float(low["observed_camera_dip_m"]), float(high["observed_camera_dip_m"])])
	_main.queue_free()
	await process_frame
	quit(0)
