extends SceneTree

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	root.size = Vector2i(1280,900)
	root.disable_3d = true
	var main: Node = load("res://main.tscn").instantiate()
	main.prepare_initial_world = false
	main._settings_persistent = false
	main._force_touch = true
	root.add_child(main)
	main.set_process(false)
	var trial: RefCounted = load("res://tests/yard_worker_input_case.gd").new()
	var result: Dictionary = trial.begin(main)
	while not result.done and result.failures.is_empty():
		result = trial.advance()
		await process_frame
	print("SCRAPERX_YARD_WORKER_INPUT %s checks=%d actual_native=1 touch=1 staged=0" %
		["PASS" if result.done and result.failures.is_empty() else "FAIL",result.checks])
	main.queue_free()
	await process_frame
	quit(0 if result.done and result.failures.is_empty() else 1)
