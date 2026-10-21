extends SceneTree

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	root.size = Vector2i(1280, 900)
	root.disable_3d = true
	var checks := 0
	var heard := 0
	var failures: Array = []
	for choice in [&"sent_to_dev", &"ari_first"]:
		var main: Node = load("res://main.tscn").instantiate()
		main.prepare_initial_world = false
		main._settings_persistent = false
		main._force_touch = true
		root.add_child(main)
		main.set_process(false)
		var test: RefCounted = load("res://tests/story_handover_input_case.gd").new()
		test.requested_disposition = choice
		var result: Dictionary = test.begin(main)
		while not result.done and result.failures.is_empty():
			result = test.advance()
			await process_frame
		checks += int(result.checks)
		heard += result.heard.size() + result.decision_heard.size() + result.ari_heard.size()
		failures.append_array(result.failures)
		if not result.done and result.failures.is_empty():
			failures.append("story decision did not complete: " + str(choice))
		main.queue_free()
		await process_frame
		if not failures.is_empty():
			break
	print("SCRAPERX_STORY_HANDOVER_INPUT %s checks=%d actual_native=1 touch=1 choices=2 explicit_receiver_stages_per_choice=2 heard=%d failures=%s" %
		["PASS" if failures.is_empty() else "FAIL", checks, heard, str(failures)])
	quit(0 if failures.is_empty() else 1)
