extends SceneTree

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	root.disable_3d = true
	var result: Dictionary = await preload("res://tests/story_handover_identity_render_case.gd").new().run(self)
	for failure in result.failures:
		push_error(failure)
	print("STORY_HANDOVER_IDENTITY_RENDER %s checks=%d failures=%d" % ["PASS" if result.failures.is_empty() else "FAIL", result.checks, result.failures.size()])
	quit(0 if result.failures.is_empty() else 1)
