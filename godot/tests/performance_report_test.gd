extends SceneTree

const Pause := preload("res://presentation/ui/pause_menu.gd")
const Settings := preload("res://presentation/ui/settings_store.gd")
class ClipboardBoundaryMenu extends Pause:
	var copied := ""
	func _copy_performance_report() -> void:
		copied = performance_report_text

var failed := false

func check(ok: bool, message: String) -> void:
	if not ok:
		failed = true
		push_error("PERFORMANCE_REPORT FAIL " + message)

func _initialize() -> void:
	_run.call_deferred()

func _run() -> void:
	var path := "res://presentation/ui/performance_report.gd"
	check(ResourceLoader.exists(path), "recorder is available")
	if not ResourceLoader.exists(path):
		quit(1)
		return
	var report: RefCounted = load(path).new()
	for n in range(1, 21):
		report.record_frame(float(n) / 1000.0, n * 100, 0, n, n * 10, 12.0, -30.0)
	var s: Dictionary = report.summary()
	check(s.sample_count == 20 and is_equal_approx(s.elapsed_seconds, 0.21), "zero-step render frames count toward elapsed time")
	check(s.frame_ms_p50 == 10.0 and s.frame_ms_p95 == 19.0 and s.frame_ms_max == 20.0, "known nearest-rank frame quantiles")
	check(is_equal_approx(s.native_wall_ms_p95, 1.9), "native wall microseconds converted to milliseconds")
	report.record_frame(4.0, 2000000, 1, 99, 999, 14.0, 40.0)
	s = report.summary()
	check(s.frame_ms_max == 4000.0 and s.native_wall_ms_max == 2000.0, "actual stalls are retained")
	report.set_paused(true)
	report.record_frame(60.0, 0, 0, 0, 0, 0.0, 0.0)
	report.set_paused(false)
	report.record_frame(60.0, 0, 0, 0, 0, 0.0, 0.0)
	report.record_frame(0.016, 100, 0, 3, 4, 15.0, -10.0)
	check(report.summary().sample_count == 22, "explicit pause and resume gap excluded once")
	report.skip_next_frame()
	report.record_frame(80.0, 0, 0, 0, 0, 0.0, 0.0)
	check(report.summary().sample_count == 22, "manual pause gap exclusion")
	report.record_frame(NAN, 0, 0, 0, 0, 0.0, 0.0)
	report.record_frame(-1.0, 0, 0, 0, 0, 0.0, 0.0)
	check(report.summary().sample_count == 22, "invalid timings do not corrupt report")
	var ring: RefCounted = load(path).new()
	for n in 600:
		ring.record_frame(0.01 if n < 88 else 0.02, 100, 0, n, n * 2, float(n), -5.0)
	s = ring.summary()
	check(s.sample_count == 512 and is_equal_approx(s.elapsed_seconds, 10.24), "overflow retains only recent window")
	check(s.frame_ms_p50 == 20.0 and s.draw_calls_last == 599 and s.draw_calls_max == 599, "overflow preserves recent counters")
	check(s.height == 599.0 and s.pitch_degrees == -5.0, "recent camera context")
	var text: String = report.copy_text({"source_sha": "abcdef", "version_code": 42})
	check(text.contains("abcdef") and text.contains("42") and text.contains("WALL") and text.contains("previous completed render frame"), "copied report identifies build and measurement boundaries")
	check(text.length() < 4096, "report remains bounded")
	var menu := ClipboardBoundaryMenu.new()
	menu.settings = Settings.new()
	menu.performance_report = report.summary()
	menu.performance_report_text = text
	root.add_child(menu)
	menu.open(0, "TEST")
	await process_frame
	var label := menu.find_child("PerformanceSummary", true, false) as Label
	check(label != null and label.is_visible_in_tree() and label.text.contains("p95"), "pause displays readable report")
	var button := menu.find_child("CopyPerformanceReport", true, false) as Button
	check(button != null and button.focus_mode == Control.FOCUS_ALL and not button.is_visible_in_tree(), "copy control belongs to a navigable settings page")
	menu._show_page(Pause.PAGE_GRAPHICS)
	check(button.is_visible_in_tree() and button.get_parent().get_parent() is ScrollContainer, "copy action participates in existing settings scrolling")
	# Replace only the unsupported headless OS clipboard boundary.
	check(menu.copied.is_empty(), "opening pause and settings never copies eagerly")
	check(button.pressed.get_connections().size() == 1, "copy is connected to explicit press")
	button.pressed.emit()
	check(menu.copied == text, "explicit button press copies the requested snapshot")
	menu.open(0, "AGAIN")
	check(menu.current_page() == Pause.PAGE_NONE, "reopening preserves normal pause navigation")
	menu.free()
	print("PERFORMANCE_REPORT failures=" + str(int(failed)))
	quit(1 if failed else 0)
