extends "res://tests/grappling_gun_test.gd"

# Keep every ordered native tick; render once per diagnostic batch.
func _step(count: int = 1, delta: float = STEP) -> void:
	for i in count:
		_main._process(delta)
	await process_frame

func _status_row() -> Array:
	var panel: Dictionary = _main._station_panel()
	for row in panel.get("rows", []):
		if row[0] == "STATUS":
			return row
	return []

func _capture(label: String) -> void:
	await process_frame
	await RenderingServer.frame_post_draw
	var directory := OS.get_environment("SCRAPERX_STATUS_CAPTURE_DIR")
	if not directory.is_empty():
		root.get_texture().get_image().save_png(directory.path_join(label + ".png"))

func _run() -> void:
	root.size = Vector2i(864, 742)
	_main = load("res://main.tscn").instantiate()
	_main._settings_persistent = false
	root.add_child(_main)
	_main.set_process(false)
	_main._router.capture_mouse = false
	_native = _main._native
	_check(_native != null and not _main._ci_mode, "shipping Main with actual native bridge")
	_touch(root.get_visible_rect().size * Vector2(.78, .35), true, 7)
	_touch(root.get_visible_rect().size * Vector2(.78, .35), false, 7)
	await _nearby_recovery_action()
	var failed: Dictionary = _state()
	var row := _status_row()
	_check(failed.last_failure == "HOOK OUT OF REACH" and row.size() == 3 \
		and row[1] == failed.last_failure and row[2] == 2,
		"actual refused recovery is visible as the native failure with hazard tone")
	var tick: int = _native.get_tick_index()
	var stock: int = failed.cartridges_remaining
	for i in 4:
		_main._process(0.0)
		_check(_status_row() == row, "zero-tick read preserves visible native refusal")
	_check(_native.get_tick_index() == tick and int(_state().cartridges_remaining) == stock,
		"displaying refusal never advances physics or spends another cartridge")
	await _capture("recovery-refused")
	if not _tap(&"grapple"):
		quit(1)
		return
	await _step(8)
	_check(not bool(_state().equipped) and _main._station_panel().is_empty(), "stowing clears gun panel normally")
	if not _tap(&"grapple"):
		quit(1)
		return
	await _step(8)
	var restored: Dictionary = _state()
	row = _status_row()
	_check(restored.last_failure.is_empty() and row.size() == 3 and row[1] == restored.state_name and row[2] == 0,
		"native-cleared failure displays current line state without stale warning")
	await _capture("redrawn-clear")
	print("GRAPPLE_STATUS ", "FAIL" if _failed else "PASS", " checks=", _checks,
		" actual_refusal=HOOK_OUT_OF_REACH phone_input=1 installed_device=0")
	quit(1 if _failed else 0)
