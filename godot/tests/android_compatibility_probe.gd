extends Node

# Installed as an autoload only by the emulator export. This observer reads
# ordinary Main and native state. It never creates a world or sends input.
const PREFIX := "SCRAPERX_ANDROID_COMPAT "
const PART_PREFIX := "SCRAPERX_ANDROID_COMPAT_PART "
const UiStyle := preload("res://presentation/ui/ui_style.gd")
var _elapsed := 0.0
var _sequence := 0
var _instance := ""
var _peak_upward_speed := 0.0
var _touch_events := 0
var _input_trace := {"events": 0, "drags": 0, "max_move": 0.0,
	"nonzero_native_ticks": 0, "world_move": [0.0, 0.0], "history": []}

func record_touch(event: InputEvent, touch: Control) -> void:
	_input_trace.events += 1
	if event is InputEventScreenDrag:
		_input_trace.drags += 1
	_input_trace.max_move = maxf(_input_trace.max_move, touch.move_vector.length())
	_input_trace.history.append({"event": "drag" if event is InputEventScreenDrag else
		("down" if event.pressed else "up"), "at": [event.position.x, event.position.y],
		"usec": Time.get_ticks_usec(), "move": [touch.move_vector.x, touch.move_vector.y],
		"frame": Engine.get_process_frames(), "stick_owner": touch._stick_index})
	if _input_trace.history.size() > 12:
		_input_trace.history.pop_front()

func record_step(world_move: Vector2, tick: int, steps: int) -> void:
	_input_trace.world_move = [world_move.x, world_move.y]
	_input_trace.tick = tick
	if world_move.length() > 0.01:
		_input_trace.nonzero_native_ticks += maxi(0, steps)

func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	_instance = "%s-%s" % [OS.get_process_id(), Time.get_unix_time_from_system()]

func _input(event: InputEvent) -> void:
	if event is InputEventScreenTouch or event is InputEventScreenDrag:
		_touch_events += 1

func _point(control: Control, local: Vector2) -> Array:
	var point := control.get_viewport().get_screen_transform() * control.get_global_transform_with_canvas() * local
	# Pixels inside the game window. The adb driver adds Android's measured
	# window origin when the OS letterboxes or positions that window.
	return [roundi(point.x), roundi(point.y)]

func _xyz(point: Vector3) -> Array:
	return [point.x, point.y, point.z]

func _publish(state: Dictionary) -> void:
	# Godot's Android logger truncates a single print at 1023 bytes. Encode
	# small binary chunks so even Unicode menu text cannot exceed that limit.
	var payload := JSON.stringify(state).to_utf8_buffer()
	var count := ceili(float(payload.size()) / 512.0)
	for part in count:
		print(PART_PREFIX + JSON.stringify({"instance": _instance,
			"sequence": _sequence, "part": part, "count": count,
			"data": Marshalls.raw_to_base64(payload.slice(part * 512, (part + 1) * 512))}))

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
	if _elapsed < 0.1:
		return
	_elapsed = 0.0
	_sequence += 1
	var view := main.get_viewport()
	var touch: Control = main.get("_touch")
	var controls := {}
	if touch != null and touch.is_visible_in_tree() and not bool(main.get("_paused")):
		for id in touch._order:
			var button = touch._buttons[id]
			if button.shown and button.enabled and button.appear >= 0.2:
				var hit: float = button.radius * touch.HIT_SLOP
				controls[String(id)] = {"point": _point(touch, button.center),
					"min": _point(touch, button.center - Vector2.ONE * hit),
					"max": _point(touch, button.center + Vector2.ONE * hit)}
	var buttons: Array = []
	var menu: Control = main.get("_pause_menu")
	if menu != null and menu.is_visible_in_tree():
		_menu_buttons(menu, buttons)
	var position: Vector3 = native.get_player_position() if native != null else Vector3.ZERO
	var velocity: Vector3 = native.get_player_linear_velocity() if native != null else Vector3.ZERO
	var landing: Dictionary = native.get_landing_state() if native != null else {}
	var forklift: Dictionary = native.get_forklift_state() if native != null else {}
	var crane: Dictionary = native.get_yard_crane_state() if native != null else {}
	var crane_lines: Array = crane.get("lines", [])
	var crane_load: Vector3 = crane_lines[0].get("to", Vector3.ZERO) if not crane_lines.is_empty() else Vector3.ZERO
	var stick: Array = _point(touch, touch.stick_home()) if touch != null else []
	var stick_up: Array = _point(touch, touch.stick_home() - Vector2(0, 100.0 * touch._u)) if touch != null else []
	# Expected bounds come from today's Android/viewport state, independently
	# of the layout's cached rectangle. A stale layout must be able to fail.
	var current_safe: Rect2 = UiStyle.safe_rect(touch) if touch != null else Rect2()
	var safe_min: Array = _point(touch, current_safe.position) if touch != null else []
	var safe_max: Array = _point(touch, current_safe.end) if touch != null else []
	var look: Vector2 = touch._safe.position + touch._safe.size * Vector2(0.85, 0.30) if touch != null else Vector2.ZERO
	var settings_path := "user://settings.cfg"
	_input_trace["owned"] = touch != null and (touch._stick_index != -1 or touch._look_index != -1)
	if touch != null:
		for button in touch._buttons.values():
			_input_trace.owned = _input_trace.owned or button.index != -1
	_publish({
		"instance": _instance, "sequence": _sequence, "ready": bool(main.get("_play_ready")),
		"native_loaded": native != null, "paused": bool(main.get("_paused")),
		"tick": native.get_tick_index() if native != null else -1,
		"fixed_step_s": native.get_fixed_step_seconds() if native != null else 0,
		"position": [position.x, position.y, position.z], "velocity": [velocity.x, velocity.y, velocity.z],
		"peak_upward_speed": _peak_upward_speed, "touch_events": _touch_events,
		"input_trace": _input_trace,
		"jump_work_j": landing.get("landing_jump_work_j", 0.0),
		"crouched": native.is_player_crouched() if native != null else false,
		"grounded": native.is_player_grounded() if native != null else false,
		"support": native.get_support_entity_id() if native != null else 0,
		"operating": main.get("_operating"),
		"forklift": {"reachable": forklift.get("controls_reachable", false),
			"station": _xyz(forklift.get("station_position", Vector3.ZERO)),
			"lift_m": forklift.get("lift_position_m", 0), "tilt_rad": forklift.get("tilt_angle_rad", 0),
			"lift_cmd": forklift.get("lift_target_mps", 0), "tilt_cmd": forklift.get("tilt_target_rad_s", 0),
			"work_j": forklift.get("positive_work_j", 0), "battery_j": forklift.get("battery_j", 0)},
		"crane": {"reachable": crane.get("controls_reachable", false),
			"station": _xyz(crane.get("station_position", Vector3.ZERO)),
			"hoist_m": crane.get("hoist_length_m", 0), "work_j": crane.get("hoist_work_j", 0),
			"hoist_cmd": crane.get("hoist_target_mps", 0), "slew_cmd": crane.get("slew_target_rad_s", 0),
			"trolley_cmd": crane.get("trolley_target_mps", 0), "source_used_j": crane.get("hoist_source_used_j", 0),
			"connected": crane.get("hoist_connected", false), "failure": crane.get("failure", 0),
			"load_point": _xyz(crane_load)},
		"window": [DisplayServer.window_get_size().x, DisplayServer.window_get_size().y],
		"canvas": [view.get_visible_rect().size.x, view.get_visible_rect().size.y],
		"safe_area": str(DisplayServer.get_display_safe_area()),
		"touch_safe_screen": [safe_min, safe_max], "look": _point(touch, look) if touch != null else [],
		"yaw": main.get("_yaw"), "pitch": main.get("_pitch"),
		"controls": controls, "stick": stick, "stick_up": stick_up, "menu_buttons": buttons,
		"settings_sha256": FileAccess.get_sha256(settings_path) if FileAccess.file_exists(settings_path) else "",
		"settings_path": ProjectSettings.globalize_path(settings_path),
		"source": preload("res://presentation/ui/build_info.gd").read_identity(),
		"android": OS.get_version(), "model": OS.get_model_name()
	})
