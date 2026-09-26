extends SceneTree

# Real main scene plus the native extension. This catches visual stepping,
# abrupt camera-response changes, and settings that fail to quiet the view.

func _initialize() -> void:
	_run.call_deferred()


func _check(condition: bool, message: String) -> bool:
	if condition:
		return true
	push_error("SCRAPERX_PARKOUR_CAMERA FAIL " + message)
	quit(31)
	return false


func _run() -> void:
	var main = load("res://main.tscn").instantiate()
	root.add_child(main)
	main.set_process(false)
	if not _check(main._native != null, "native extension loads"):
		return

	main._head_bob_on = false
	main._yaw = 0.0
	var native = main._native
	native.set_move_input(1.0, 0.0)
	native.set_facing(1.0, 0.0)
	native.advance_frame(native.get_fixed_step_seconds())
	native.advance_frame(native.get_fixed_step_seconds() * 0.5)
	main._render_snapshot(0.0)
	var visual: Vector3 = native.get_player_render_position()
	var physical: Vector3 = native.get_player_position()
	if not _check(absf(visual.x - physical.x) > 0.00001,
			"visual pose sits between fixed physics ticks"):
		return
	if not _check(absf(main._camera.position.x - visual.x) < 0.0001,
			"camera consumes the visual pose"):
		return

	main._head_bob_on = true
	main._speed_fov_on = true
	main._apply_camera_feel(Vector3.ZERO, Vector3(5.5, 0.0, 0.0), true, false, 1.0 / 60.0)
	if not _check(main._camera.fov > 82.1 and main._camera.fov < 85.5,
			"running FOV eases in rather than stepping to its maximum"):
		return
	for frame in range(20):
		main._apply_camera_feel(Vector3.ZERO, Vector3(5.5, 0.0, 0.0), true, false, 1.0 / 60.0)
	if not _check(absf(main._camera.rotation.z) > 0.003 and
			absf(main._camera.rotation.z) < 0.035,
			"sideways running gains a restrained camera lean"):
		return

	main._cam_bob_phase = PI * 0.5
	main._apply_camera_feel(Vector3.ZERO, Vector3.ZERO, false, false, 1.0 / 60.0)
	if not _check(main._camera.position.y > 0.625,
			"running head motion fades out after takeoff"):
		return

	for frame in range(180):
		main._apply_camera_feel(Vector3.ZERO, Vector3.ZERO, true, false, 1.0 / 60.0)
	if not _check(absf(main._camera.fov - 82.0) < 0.001 and
			absf(main._camera.rotation.z) < 0.001 and
			absf(main._camera.position.y - 0.62) < 0.001,
			"stationary view returns to its baseline"):
		return

	main._apply_camera_feel(Vector3.ZERO, Vector3(5.5, 0.0, 0.0), true, false, 0.25)
	main._head_bob_on = false
	main._speed_fov_on = false
	main._apply_camera_feel(Vector3.ZERO, Vector3(5.5, 0.0, 0.0), true, false, 1.0 / 60.0)
	if not _check(absf(main._camera.fov - 82.0) < 0.001 and
			absf(main._camera.rotation.z) < 0.001,
			"motion-comfort toggles quiet the view immediately"):
		return
	print("SCRAPERX_PARKOUR_CAMERA PASS")
	quit(0)
