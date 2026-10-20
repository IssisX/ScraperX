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
	var test: RefCounted = load("res://tests/grapple_trip_input_case.gd").new()
	var result: Dictionary = test.begin(main)
	while not bool(result.done) and result.failures.is_empty():
		result = test.advance(90)
		await process_frame
	print("SCRAPERX_GRAPPLE_TRIP_INPUT %s checks=%d ticks=%d phase=%s touch=1 actual_native=1 initial_stage_only=1" %
		["PASS" if result.failures.is_empty() and result.done else "FAIL", result.checks, result.ticks, result.phase])
	main.queue_free()
	await process_frame
	quit(0 if result.failures.is_empty() and result.done else 1)
