extends Control

var _native: RefCounted
var _capture_path := ""
var _capture_scheduled := false
var _ci_mode := false

@onready var _status: Label = $Content/Columns/Telemetry/StatusRow/Status
@onready var _status_dot: ColorRect = $Content/Columns/Telemetry/StatusRow/StatusDot
@onready var _tick_label: Label = $Content/Columns/Telemetry/TickLabel
@onready var _tick_progress: ProgressBar = $Content/Columns/Telemetry/TickProgress
@onready var _time_value: Label = $Content/Columns/Telemetry/Metrics/TimeValue
@onready var _step_value: Label = $Content/Columns/Telemetry/Metrics/StepValue


func _ready() -> void:
	for argument in OS.get_cmdline_user_args():
		if argument == "--ci":
			_ci_mode = true
		elif argument.begins_with("--capture="):
			_capture_path = argument.trim_prefix("--capture=")

	if not ClassDB.class_exists("ScraperXSimulation"):
		_status.text = "NATIVE EXTENSION FAILED"
		_status_dot.color = Color("ef5b5b")
		push_error("SCRAPERX_EXTENSION_LOAD_FAILED")
		get_tree().quit(20)
		return

	_native = ScraperXSimulation.new()
	_step_value.text = "%.3f ms" % (_native.get_fixed_step_seconds() * 1000.0)
	print("SCRAPERX_EXTENSION_LOADED api=4.7 authority=scraperx_sim")
	_render_snapshot()


func _process(delta: float) -> void:
	if _native == null:
		return

	var steps_advanced := int(_native.advance_frame(delta))
	if steps_advanced < 0:
		_status.text = "NATIVE INPUT REJECTED"
		_status_dot.color = Color("ef5b5b")
		get_tree().quit(21)
		return

	_render_snapshot()
	var tick := int(_native.get_tick_index())

	if tick >= 8 and not _capture_path.is_empty() and not _capture_scheduled:
		_capture_scheduled = true
		RenderingServer.frame_post_draw.connect(_capture_frame, CONNECT_ONE_SHOT)
	elif tick >= 8 and _ci_mode and _capture_path.is_empty():
		print("SCRAPERX_RUNTIME_PROOF ticks=%d sim_seconds=%.6f" % [tick, _native.get_simulation_time_seconds()])
		get_tree().quit(0)


func _render_snapshot() -> void:
	var tick := int(_native.get_tick_index())
	_tick_label.text = "TICK %08d" % tick
	_tick_progress.value = tick % 90
	_time_value.text = "%.3f s" % _native.get_simulation_time_seconds()


func _capture_frame() -> void:
	var target := _capture_path
	DirAccess.make_dir_recursive_absolute(target.get_base_dir())
	var image := get_viewport().get_texture().get_image()
	var error := image.save_png(target)
	if error != OK:
		push_error("SCRAPERX_SCREENSHOT_FAILED code=%d path=%s" % [error, target])
		get_tree().quit(22)
		return

	print("SCRAPERX_SCREENSHOT_SAVED=%s" % target)
	print("SCRAPERX_RUNTIME_PROOF ticks=%d sim_seconds=%.6f" % [
		_native.get_tick_index(),
		_native.get_simulation_time_seconds(),
	])
	_capture_path = ""
	get_tree().quit(0)

