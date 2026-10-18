extends SceneTree

const MainScene := preload("res://main.tscn")
const Report := preload("res://presentation/ui/performance_report.gd")
var failed := false

func check(ok: bool, message: String) -> void:
	if not ok:
		failed = true
		push_error("PERFORMANCE_MAIN FAIL " + message)

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var main := MainScene.instantiate()
	root.add_child(main)
	# Use the actual startup handoff before exercising the real native call.
	if not main._play_ready:
		await main.play_ready
	main.set_process(false)
	# Reset only diagnostic history; the real native body and inputs stay live.
	# The first real call seeds a wall boundary; the second completes an interval.
	check(main._native.has_method("get_last_advance_process_cpu_usec"), "actual native bridge exposes process CPU observation")
	if not main._native.has_method("get_last_advance_process_cpu_usec"):
		main.queue_free()
		quit(1)
		return
	# Consume only the startup-discard frame before the measured 90 Hz ticks.
	if main._discard_startup_delta:
		main._process(1.0 / 90.0)
	main._performance_report = Report.new()
	main._process(1.0 / 90.0)
	main._process(1.0 / 90.0)
	var captured: Dictionary = main._performance_report.summary()
	check(captured.sample_count == 1, "real Main cadence enters the bounded report after first boundary")
	check(captured.native_wall_ms_max > 0.0, "native timing comes from the actual advance call")
	var cpu_usec: int = main._native.get_last_advance_process_cpu_usec()
	if cpu_usec >= 0:
		check(cpu_usec > 0 and captured.native_process_cpu_samples == 1, "one actual 90 Hz tick supplies a positive process CPU observation")
		check(is_equal_approx(captured.native_process_cpu_ms_max, float(cpu_usec) / 1000.0), "report captures the real bridge process CPU value")
	else:
		check(captured.native_process_cpu_samples == 0, "unsupported host CPU clock stays unavailable")
	check(captured.frame_ms_max >= captured.native_wall_ms_max, "real frame interval includes its native advance wall time")
	check(is_equal_approx(captured.height, main._native.get_player_position().y), "report reads real player height")
	main._open_pause()
	check(paused, "normal pause freezes the scene tree")
	check(main._pause_menu.performance_report.sample_count == 1, "pause receives the captured window")
	check(main._pause_menu.performance_report_text.contains("Native advance WALL"), "copied text labels real wall timing")
	var copied: String = main._pause_menu.performance_report_text
	check(copied.contains("Process CPU ms p50 / p95 / max") if cpu_usec >= 0 else copied.contains("Process CPU: Unavailable"), "copied text labels actual CPU availability")
	check(copied.contains("all app threads during the native call") and copied.contains("can exceed WALL") and copied.contains("Neither timing isolates physics or measures GPU cost"), "copied text preserves process CPU scope")
	var label := main._pause_menu.find_child("PerformanceSummary", true, false) as Label
	check(label != null and label.text.contains("Frame p95"), "pause exposes readable frame evidence")
	var capture := OS.get_environment("SCRAPERX_REPORT_CAPTURE")
	if not capture.is_empty():
		await RenderingServer.frame_post_draw
		check(root.get_texture().get_image().save_png(capture) == OK, "pause layout capture saved")
	main._resume()
	main._process(1.0 / 90.0)
	check(main._performance_report.summary().sample_count == 1, "resume excludes exactly its pause-gap frame")
	main._process(1.0 / 90.0)
	check(main._performance_report.summary().sample_count == 2, "ordinary recording resumes immediately")
	main.queue_free()
	await process_frame
	print("PERFORMANCE_MAIN failures=" + str(int(failed)))
	quit(1 if failed else 0)
