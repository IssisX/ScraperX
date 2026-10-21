extends SceneTree

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	root.size = Vector2i(1280, 900)
	root.disable_3d = true
	var main: Node = load("res://main.tscn").instantiate()
	main.prepare_initial_world = false
	main._settings_persistent = false
	main._force_touch = true
	root.add_child(main)
	main.set_process(false)
	var test: RefCounted = load("res://tests/grapple_toolbelt_input_case.gd").new()
	var result: Dictionary = test.run(main)
	print("GRAPPLE_TOOLBELT_INPUT %s checks=%d rise_m=%.3f actual_native=1 touch=1" %
		["PASS" if result.failures.is_empty() else "FAIL", result.checks, result.rise_m])
	main.queue_free()
	await process_frame
	quit(0 if result.failures.is_empty() else 1)
