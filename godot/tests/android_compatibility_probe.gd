extends Node

# Installed as an autoload only by the emulator export. This observer reads
# ordinary Main and native state. It never creates a world or sends input.
const PREFIX := "SCRAPERX_ANDROID_COMPAT "
var _elapsed := 0.0
var _sequence := 0
var _instance := ""
var _peak_upward_speed := 0.0
var _touch_events := 0

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	_instance = "%s-%s" % [OS.get_process_id(), Time.get_unix_time_from_system()]

func _input(event: InputEvent) -> void:
	if event is InputEventScreenTouch or event is InputEventScreenDrag:
		_touch_events += 1

func _point(control: Control, local: Vector2) -> Array:
	var point := control.get_viewport().get_screen_transform() * control.get_global_transform_with_canvas() * local
	return [point.x, point.y]

func _menu_buttons(node: Node, output: Array) -> void:
	if node is Button and node.is_visible_in_tree() and not node.disabled:
		output.append({"text": node.text, "point": _point(node, node.size * 0.5)})
	for child in node.get_children():
		_menu_buttons(child, output)

func _process(delta: float) -> void:
	var main: Node = get_tree().current_scene
	if main == null or not main.has_signal("play_ready"):
		return
	var native: Object = main.get("_native")
	if native != null:
		_peak_upward_speed = maxf(_peak_upward_speed, native.get_player_linear_velocity().y)
	_elapsed += delta
	if _elapsed < 0.4:
		return
	_elapsed = 0.0
	_sequence += 1
	var view := main.get_viewport()
	var touch: Control = main.get("_touch")
	var controls := {}
	if touch != null and touch.is_visible_in_tree():
		for id in touch._order:
			var button = touch._buttons[id]
			if button.shown and button.enabled:
				controls[String(id)] = {"point": _point(touch, button.center), "radius_canvas": button.radius}
	var buttons: Array = []
	var menu: Control = main.get("_pause_menu")
	if menu != null and menu.is_visible_in_tree():
		_menu_buttons(menu, buttons)
	var position: Vector3 = native.get_player_position() if native != null else Vector3.ZERO
	var velocity: Vector3 = native.get_player_linear_velocity() if native != null else Vector3.ZERO
	var landing: Dictionary = native.get_landing_state() if native != null else {}
	var stick: Array = _point(touch, touch.stick_home()) if touch != null else []
	var stick_up: Array = _point(touch, touch.stick_home() - Vector2(0, 100.0 * touch._u)) if touch != null else []
	var settings_path := "user://settings.cfg"
	print(PREFIX + JSON.stringify({
		"instance": _instance, "sequence": _sequence, "ready": bool(main.get("_play_ready")),
		"native_loaded": native != null, "paused": bool(main.get("_paused")),
		"tick": native.get_tick_index() if native != null else -1,
		"fixed_step_s": native.get_fixed_step_seconds() if native != null else 0,
		"position": [position.x, position.y, position.z], "velocity": [velocity.x, velocity.y, velocity.z],
		"peak_upward_speed": _peak_upward_speed, "touch_events": _touch_events,
		"jump_work_j": landing.get("landing_jump_work_j", 0.0),
		"crouched": native.is_player_crouched() if native != null else false,
		"window": [DisplayServer.window_get_size().x, DisplayServer.window_get_size().y],
		"canvas": [view.get_visible_rect().size.x, view.get_visible_rect().size.y],
		"safe_area": str(DisplayServer.get_display_safe_area()),
		"controls": controls, "stick": stick, "stick_up": stick_up, "menu_buttons": buttons,
		"settings_sha256": FileAccess.get_sha256(settings_path) if FileAccess.file_exists(settings_path) else "",
		"settings_path": ProjectSettings.globalize_path(settings_path),
		"source": preload("res://presentation/ui/build_info.gd").read_identity(),
		"android": OS.get_version(), "model": OS.get_model_name()
	}))
