extends CanvasLayer

signal prepared(native: Object, steps: int)
signal failed(message: String)
signal retry_requested
signal cancel_requested

const UiStyle := preload("res://presentation/ui/ui_style.gd")
const MAX_STEPS := 900
const ESTIMATED_STEPS := 405

class PreparationState extends RefCounted:
	var mutex := Mutex.new()
	var cancelled := false
	var paused := false
	var steps := 0

var _state := PreparationState.new()
var _thread := Thread.new()
var _native: RefCounted
var _started := false
var _finished := false
var _cancel_emitted := false
var _status: Label
var _progress: ProgressBar
var _retry: Button
var _cancel: Button
var _backdrop: ColorRect
var _center: CenterContainer
var _panel: PanelContainer
var _column: VBoxContainer
var _title: Label
var _plate: StyleBoxFlat

func _ready() -> void:
	layer = 100
	process_mode = Node.PROCESS_MODE_ALWAYS
	_build_panel()
	_layout_panel()
	get_viewport().size_changed.connect(_layout_panel)
	set_process(false)

# The caller relinquishes all native access until prepared is emitted after join.
# A retry uses a fresh panel and fresh native instance; start is single-use.
func start(native: Object) -> Error:
	if _started:
		return ERR_ALREADY_IN_USE
	_started = true
	if not is_inside_tree() or not native is RefCounted:
		_show_failure("The yard could not be prepared. Please retry.")
		return ERR_INVALID_PARAMETER
	for method in [&"get_fixed_step_seconds", &"advance_frame", &"is_initial_world_settled"]:
		if not native.has_method(method):
			_show_failure("The yard preparation interface is unavailable. Please retry.")
			return ERR_INVALID_PARAMETER
	var fixed_dt := float(native.get_fixed_step_seconds())
	if not is_finite(fixed_dt) or absf(fixed_dt - 1.0 / 90.0) > 0.00000001:
		_show_failure("The yard preparation clock is unavailable. Please retry.")
		return ERR_INVALID_PARAMETER
	_native = native
	var error := _start_worker(_prepare.bind(_state, _native, fixed_dt))
	if error != OK:
		_native = null
		_show_failure("The preparation worker could not start. Please retry.")
		return error
	set_process(true)
	return OK

# Narrow launch boundary permits a deterministic failed-start lifecycle test.
func _start_worker(work: Callable) -> Error:
	return _thread.start(work)

static func _prepare(state: PreparationState, native: RefCounted, fixed_dt: float) -> Dictionary:
	var steps := 0
	while steps <= MAX_STEPS:
		state.mutex.lock()
		var cancelled := state.cancelled
		var paused := state.paused
		state.mutex.unlock()
		if cancelled:
			return {"status": "cancelled", "steps": steps}
		if paused:
			OS.delay_msec(10)
			continue
		# This worker exclusively owns native. No scene-tree or UI calls here.
		if bool(native.is_initial_world_settled()):
			return {"status": "prepared", "steps": steps}
		if steps == MAX_STEPS:
			break
		if int(native.advance_frame(fixed_dt)) != 1:
			return {"status": "failed", "message": "The yard preparation step failed. Please retry.", "steps": steps}
		steps += 1
		state.mutex.lock()
		state.steps = steps
		state.mutex.unlock()
	return {"status": "failed", "message": "The yard has not settled yet. Please retry.", "steps": steps}

func progress_steps() -> int:
	_state.mutex.lock()
	var steps := _state.steps
	_state.mutex.unlock()
	return steps

func _process(_delta: float) -> void:
	if _finished or not _thread.is_started():
		return
	_progress.value = minf(95.0, float(progress_steps()) * 95.0 / ESTIMATED_STEPS)
	if _thread.is_alive():
		return
	var result: Dictionary = _thread.wait_to_finish()
	_finished = true
	set_process(false)
	# Cancellation may race completion; it must never become successful entry.
	_state.mutex.lock()
	var cancelled := _state.cancelled
	_state.mutex.unlock()
	var native := _native
	_native = null
	if cancelled or result.get("status") == "cancelled":
		_status.text = "Preparation cancelled"
		return
	if result.get("status") == "prepared":
		_progress.value = 100.0
		prepared.emit(native, int(result["steps"]))
	else:
		_show_failure(str(result.get("message", "The yard could not be prepared. Please retry.")))

func _show_failure(message: String) -> void:
	_finished = true
	if _status != null:
		_status.text = message
		_retry.show()
	failed.emit(message)

func _request_cancel() -> void:
	_state.mutex.lock()
	_state.cancelled = true
	_state.mutex.unlock()
	if not _cancel_emitted:
		_cancel_emitted = true
		_cancel.disabled = true
		cancel_requested.emit()

func _notification(what: int) -> void:
	if what == NOTIFICATION_APPLICATION_PAUSED or what == NOTIFICATION_APPLICATION_RESUMED:
		_state.mutex.lock()
		_state.paused = what == NOTIFICATION_APPLICATION_PAUSED
		_state.mutex.unlock()

func _exit_tree() -> void:
	_state.mutex.lock()
	_state.cancelled = true
	_state.mutex.unlock()
	# Teardown is the sole blocking join: cancellation waits at most one native
	# tick (or a paused-worker sleep), before releasing the owned native object.
	if _thread.is_started():
		_thread.wait_to_finish()
	_native = null

func _build_panel() -> void:
	_backdrop = ColorRect.new()
	var backdrop := _backdrop
	backdrop.color = UiStyle.INK
	backdrop.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	add_child(backdrop)
	_center = CenterContainer.new()
	var center := _center
	center.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	backdrop.add_child(center)
	_panel = PanelContainer.new()
	var panel := _panel
	_plate = StyleBoxFlat.new()
	var plate := _plate
	plate.bg_color = Color("191612")
	plate.border_color = UiStyle.AMBER_DEEP
	plate.set_border_width_all(2)
	plate.set_content_margin_all(24)
	panel.add_theme_stylebox_override("panel", plate)
	center.add_child(panel)
	_column = VBoxContainer.new()
	var column := _column
	column.add_theme_constant_override("separation", 18)
	panel.add_child(column)
	_title = Label.new()
	var title := _title
	title.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	title.text = "Preparing the yard"
	title.add_theme_font_override("font", UiStyle.font_heavy())
	title.add_theme_color_override("font_color", UiStyle.AMBER)
	title.add_theme_font_size_override("font_size", 22)
	column.add_child(title)
	_status = Label.new()
	_status.text = "Getting the worksite ready."
	_status.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
	_status.add_theme_color_override("font_color", UiStyle.PAPER)
	column.add_child(_status)
	_progress = ProgressBar.new()
	_progress.show_percentage = false
	_progress.custom_minimum_size.y = 8
	column.add_child(_progress)
	_retry = Button.new()
	_retry.text = "Retry"
	_retry.hide()
	_retry.pressed.connect(func() -> void: retry_requested.emit())
	column.add_child(_retry)
	_cancel = Button.new()
	_cancel.text = "Cancel"
	_cancel.pressed.connect(_request_cancel)
	column.add_child(_cancel)

func _layout_panel() -> void:
	var unit := UiStyle.unit(_backdrop)
	var safe := UiStyle.safe_rect(_backdrop)
	_center.set_anchors_and_offsets_preset(Control.PRESET_TOP_LEFT)
	_center.position = safe.position
	_center.size = safe.size
	_panel.custom_minimum_size.x = minf(960.0 * unit, maxf(1.0, safe.size.x - 64.0 * unit))
	_plate.set_content_margin_all(maxf(8.0, 32.0 * unit))
	_column.add_theme_constant_override("separation", maxi(8, roundi(28.0 * unit)))
	_title.add_theme_font_size_override("font_size", clampi(roundi(52.0 * unit), 18, 72))
	_status.add_theme_font_size_override("font_size", clampi(roundi(28.0 * unit), 12, 40))
	_progress.custom_minimum_size.y = maxf(6.0, 12.0 * unit)
	for button in [_retry, _cancel]:
		button.custom_minimum_size.y = maxf(32.0, 96.0 * unit)
		button.add_theme_font_override("font", UiStyle.font_label())
		button.add_theme_font_size_override("font_size", clampi(roundi(28.0 * unit), 12, 40))
