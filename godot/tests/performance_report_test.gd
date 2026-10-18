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
	var stamp := 1000000
	report.record_frame(0.133333, 250000, 1, 1, 1, 0.0, 0.0, stamp)
	check(report.summary().sample_count == 0, "first call establishes wall boundary without startup time")
	check(report.summary().native_process_cpu_samples == 0, "empty report has no CPU observations")
	stamp += 250000
	report.record_frame(0.133333, 250000, 1, 1, 1, 0.0, 0.0, stamp)
	var s: Dictionary = report.summary()
	check(s.sample_count == 1 and s.frame_ms_p50 == 250.0 and s.fps == 4.0,
		"capped process delta cannot hide 250ms real cadence or claim 7.5 FPS")
	check(s.native_process_cpu_samples == 0 and s.native_process_cpu_ms_max == 0.0, "old callers retain wall frames without fabricated CPU samples")
	check(report.copy_text({}).contains("Process CPU: Unavailable"), "missing CPU is explicitly unavailable in copied text")
	var zero_cpu: RefCounted = load(path).new()
	zero_cpu.record_frame(0.01, 100, 1, 0, 0, 0.0, 0.0, stamp, 0)
	zero_cpu.record_frame(0.01, 100, 1, 0, 0, 0.0, 0.0, stamp + 10000, 0)
	var zero_summary: Dictionary = zero_cpu.summary()
	check(zero_summary.native_process_cpu_samples == 1 and zero_summary.native_process_cpu_ms_p50 == 0.0 and zero_summary.native_process_cpu_ms_p95 == 0.0 and zero_summary.native_process_cpu_ms_max == 0.0, "observed zero CPU remains a valid zero quantile")
	check(zero_cpu.copy_text({}).contains("(1 samples)") and not zero_cpu.copy_text({}).contains("Unavailable"), "observed zero CPU is available in copied text")
	# Reset only diagnostic history for a known nearest-rank window.
	report = load(path).new()
	report.record_frame(0.0, 0, 0, 0, 0, 0.0, 0.0, stamp)
	for n in range(1, 21):
		stamp += n * 1000
		report.record_frame(0.133333, n * 100, 0, n, n * 10, 12.0, -30.0, stamp, n * 2000)
	s = report.summary()
	check(s.sample_count == 20 and is_equal_approx(s.elapsed_seconds, 0.21), "zero-step render frames count toward wall time")
	check(s.frame_ms_p50 == 10.0 and s.frame_ms_p95 == 19.0 and s.frame_ms_max == 20.0, "known nearest-rank frame quantiles")
	check(is_equal_approx(s.native_wall_ms_p95, 1.9), "native wall microseconds converted to milliseconds")
	check(s.native_process_cpu_samples == 20 and s.native_process_cpu_ms_p50 == 20.0 and s.native_process_cpu_ms_p95 == 38.0 and s.native_process_cpu_ms_max == 40.0, "CPU quantiles use observed microseconds and can exceed frame/native WALL")
	stamp += 4000000
	report.record_frame(0.133333, 2000000, 1, 99, 999, 14.0, 40.0, stamp)
	s = report.summary()
	check(s.frame_ms_max == 4000.0 and s.native_wall_ms_max == 2000.0, "actual stalls survive capped process delta")
	report.set_paused(true)
	stamp += 60000000
	report.record_frame(60.0, 0, 0, 0, 0, 0.0, 0.0, stamp, 9000000)
	report.set_paused(false)
	stamp += 60000000
	report.record_frame(60.0, 0, 0, 0, 0, 0.0, 0.0, stamp, 9000000)
	stamp += 16000
	report.record_frame(0.016, 0, 0, 3, 4, 15.0, -10.0, stamp, 0)
	s = report.summary()
	check(s.sample_count == 22 and is_equal_approx(s.elapsed_seconds, 4.226), "explicit pause and resume gap excluded; zero-native frame retained")
	check(s.native_process_cpu_samples == 21 and s.native_process_cpu_ms_p50 == 20.0 and s.native_process_cpu_ms_max == 40.0, "valid zero CPU retained; unavailable, paused and resume-gap observations excluded")
	report.skip_next_frame()
	stamp += 80000000
	report.record_frame(80.0, 0, 0, 0, 0, 0.0, 0.0, stamp, 9000000)
	check(report.summary().sample_count == 22, "manual quiesce gap excluded")
	stamp += 10000
	report.record_frame(NAN, 0, 0, 0, 0, 0.0, 0.0, stamp)
	stamp += 10000
	report.record_frame(-1.0, 0, 0, 0, 0, 0.0, 0.0, stamp)
	report.record_frame(0.01, 0, 0, 0, 0, 0.0, 0.0, -2, 9000000)
	stamp += 10000
	report.record_frame(0.01, 0, 0, 0, 0, 0.0, 0.0, stamp)
	report.record_frame(0.01, 0, 0, 0, 0, 0.0, 0.0, stamp)
	stamp -= 1000
	report.record_frame(0.01, 0, 0, 0, 0, 0.0, 0.0, stamp)
	check(report.summary().sample_count == 22, "invalid data, negative, equal and reversed timestamps cannot fabricate samples")
	stamp += 10000
	report.record_frame(0.133333, 0, 0, 0, 0, 0.0, 0.0, stamp, -20)
	s = report.summary()
	check(s.sample_count == 23 and is_equal_approx(s.elapsed_seconds, 4.236), "rebase resumes without admitting rejected intervals")
	check(s.native_process_cpu_samples == 21 and s.native_process_cpu_ms_max == 40.0, "negative CPU leaves valid wall frame intact; invalid timestamp cannot admit CPU")
	var ring: RefCounted = load(path).new()
	ring.record_frame(0.0, 0, 0, 0, 0, 0.0, 0.0, stamp)
	for n in 600:
		stamp += 10000 if n < 88 else 20000
		ring.record_frame(0.133333, 100, 0, n, n * 2, float(n), -5.0, stamp, 9000000 if n < 88 else (200 if n % 2 == 0 else -1))
	s = ring.summary()
	check(s.sample_count == 512 and is_equal_approx(s.elapsed_seconds, 10.24), "overflow retains only recent wall window")
	check(s.frame_ms_p50 == 20.0 and s.frame_ms_p95 == 20.0 and s.frame_ms_max == 20.0 and s.draw_calls_last == 599 and s.draw_calls_max == 599, "overflow preserves recent quantiles and counters")
	check(s.height == 599.0 and s.pitch_degrees == -5.0, "recent camera context")
	check(s.native_process_cpu_samples == 256 and s.native_process_cpu_ms_p50 == 0.2 and s.native_process_cpu_ms_p95 == 0.2 and s.native_process_cpu_ms_max == 0.2, "ring overwrite removes stale CPU values and counts only available retained observations")
	var text: String = report.copy_text({"source_sha": "abcdef", "version_code": 42})
	check(text.contains("abcdef") and text.contains("42") and text.contains("WALL") and text.contains("previous completed render frame"), "copied report identifies build and measurement boundaries")
	check(text.contains("Process CPU ms p50 / p95 / max") and text.contains("(21 samples)"), "copied report labels observed CPU quantiles and sample count")
	check(text.contains("all app threads during the native call") and text.contains("can exceed WALL") and text.contains("Neither timing isolates physics or measures GPU cost"), "copied CPU scope prevents exclusive physics or GPU attribution")
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
