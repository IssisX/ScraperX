extends RefCounted

# Player comfort settings. None of this is simulation state: it only changes
# how input is read and how the interface draws, never what the native
# authority decides. CI and scripted UI runs construct it with persistent =
# false, so a developer's saved sensitivity can never leak into a proof run.

const PATH := "user://settings.cfg"
const SECTION := "interface"

const LOOK_SENSITIVITY_RANGE := Vector2(0.3, 2.5)
const STICK_SENSITIVITY_RANGE := Vector2(0.3, 2.5)
const TOUCH_SCALE_RANGE := Vector2(0.8, 1.3)
const GYRO_SENSITIVITY_RANGE := Vector2(0.5, 3.0)
const RENDER_SCALE_RANGE := Vector2(0.5, 1.0)
const FOV_RANGE := Vector2(65.0, 100.0)
const BRIGHTNESS_RANGE := Vector2(0.7, 1.4)
const DAY_MINUTES_RANGE := Vector2(8.0, 60.0)
const VOLUME_RANGE := Vector2(0.0, 1.0)
const VIEW_DISTANCE_RANGE := Vector2(500.0, 6000.0)
const RESTART_HEIGHT_RANGE := Vector2(0.0, 1600.0)
const RESTART_COORDINATE_RANGE := Vector2(-6000.0, 6000.0)
const RESTART_CLEARANCE_RANGE := Vector2(1.0, 100.0)

# Graphics. A preset writes its rendering and detail values; changing any
# afterwards makes the preset CUSTOM.
const QUALITY_NAMES := ["LOW", "MEDIUM", "HIGH", "ULTRA", "CUSTOM"]
const QUALITY_CUSTOM := 4
const QUALITY_PRESETS := [
	{"render_scale": 0.75, "shadow_quality": 1, "msaa": 0, "bloom": false, "detail_distance": 0, "view_distance": 1000.0},
	{"render_scale": 0.9, "shadow_quality": 2, "msaa": 0, "bloom": true, "detail_distance": 1, "view_distance": 1500.0},
	{"render_scale": 1.0, "shadow_quality": 3, "msaa": 1, "bloom": true, "detail_distance": 2, "view_distance": 2000.0},
	{"render_scale": 1.0, "shadow_quality": 4, "msaa": 2, "bloom": true, "detail_distance": 3, "view_distance": 6000.0},
]
const SHADOW_NAMES := ["OFF", "LOW", "MEDIUM", "HIGH", "ULTRA"]
const MSAA_NAMES := ["OFF", "2X MSAA", "4X MSAA"]
# Append new caps so saved indices from previous versions remain valid.
const FPS_CAPS := [0, 30, 60, 90, 120, 45]
const FPS_CAP_NAMES := ["UNCAPPED", "30", "60", "90", "120", "45"]
const DETAIL_DISTANCE_NAMES := ["LOW / 150 M", "MEDIUM / 350 M", "HIGH / 800 M", "FULL DETAIL"]
const DETAIL_DISTANCES := [150.0, 350.0, 800.0, 0.0]
const VSYNC_NAMES := ["ON", "OFF"]
const WINDOW_MODE_NAMES := ["WINDOWED", "FULLSCREEN"]
const RESTART_SIDE_NAMES := ["NORTH", "EAST", "SOUTH", "WEST"]
const RESTART_RING_HEIGHTS := [0.0, 11.0, 22.0, 33.0, 44.0, 55.0, 66.0, 77.0,
	88.0, 99.0, 110.0, 121.0, 132.0, 143.0, 154.0, 165.0, 176.0, 187.0, 198.0,
	209.0, 220.0, 231.0, 242.0, 253.0, 264.0, 275.0, 286.0, 297.0, 308.0, 319.0,
	330.0, 341.0, 352.0]
const RESTART_RING_NAMES := ["GRADE", "+11 M", "+22 M", "+33 M", "+44 M", "+55 M", "+66 M", "+77 M",
	"+88 M", "+99 M", "+110 M", "+121 M", "+132 M", "+143 M", "+154 M", "+165 M", "+176 M",
	"+187 M", "+198 M", "+209 M", "+220 M", "+231 M", "+242 M", "+253 M", "+264 M", "+275 M",
	"+286 M", "+297 M", "+308 M", "+319 M", "+330 M", "+341 M", "+352 M"]
# The player's native standing capsule centre is 0.9 m above a surface.
const STANDING_CENTER_HEIGHT := 0.9
const TOWER_CENTER := Vector3(0.0, 0.0, -150.0)
const TOWER_HALF_EXTENT := 26.0
const TIME_OF_DAY_NAMES := ["CYCLE", "DAWN", "NOON", "DUSK", "NIGHT"]
const TIME_OF_DAY_HOURS := [10.5, 6.4, 12.5, 18.3, 23.5]

var look_sensitivity := 1.0
var stick_sensitivity := 1.0
var invert_y := false
# Off unless the player asks: a device that turns the view when it moves is
# a surprise nobody should meet on first launch.
var gyro_aim := false
var gyro_sensitivity := 1.0
var vibration := true
var touch_scale := 1.0
var telemetry := false
var quality := 2
var render_scale := 1.0
var shadow_quality := 3
var msaa := 1
var bloom := true
var fps_cap := 0
var show_fps := false
var detail_distance := 2
var view_distance := 2000.0
var vsync := 0
var fov := 82.0
var brightness := 1.0
var launch_cinematics := true
var head_bob := true
var speed_fov := true
var time_of_day := 0
var day_minutes := 24.0
var window_mode := 0
var master_volume := 0.8
var effects_volume := 1.0
var ambience_volume := 0.8
var interface_volume := 0.7
var audio_muted := false
# Last playtest destination is saved for convenience, never applied on load.
var restart_height := 110.0
var restart_ring := 0
var restart_side := 0
var restart_clearance := 3.0
var restart_x := 16.0
var restart_y := 110.9
var restart_z := -174.5
var persistent := true


# Writes a preset's values; CUSTOM leaves them as they are.
func apply_quality(index: int) -> void:
	quality = clampi(index, 0, QUALITY_CUSTOM)
	if quality == QUALITY_CUSTOM:
		return
	var preset: Dictionary = QUALITY_PRESETS[quality]
	for key in preset:
		set(key, preset[key])


func tower_restart_position() -> Vector3:
	var radius := TOWER_HALF_EXTENT + restart_clearance
	var sides := [Vector3(0.0, 0.0, -1.0), Vector3(1.0, 0.0, 0.0),
		Vector3(0.0, 0.0, 1.0), Vector3(-1.0, 0.0, 0.0)]
	return TOWER_CENTER + sides[restart_side] * radius + Vector3(0.0, restart_height + STANDING_CENTER_HEIGHT, 0.0)


func exact_restart_position() -> Vector3:
	return Vector3(restart_x, restart_y, restart_z)


func ring_position() -> Vector3:
	if restart_ring == 0:
		return Vector3(6.0, 0.92, -58.0)
	# South deck band, clear of the tower's corner and centre columns.
	return Vector3(20.0, RESTART_RING_HEIGHTS[restart_ring] + 0.92, -128.0)


# The file is user-writable, so every value is parsed at this boundary:
# wrong type or out-of-range falls back to the default, never propagates.
func load_from_disk() -> void:
	if not persistent:
		return
	var file := ConfigFile.new()
	if file.load(PATH) != OK:
		return
	look_sensitivity = _read_range(file, "look_sensitivity", look_sensitivity, LOOK_SENSITIVITY_RANGE)
	stick_sensitivity = _read_range(file, "stick_sensitivity", stick_sensitivity, STICK_SENSITIVITY_RANGE)
	touch_scale = _read_range(file, "touch_scale", touch_scale, TOUCH_SCALE_RANGE)
	gyro_sensitivity = _read_range(file, "gyro_sensitivity", gyro_sensitivity, GYRO_SENSITIVITY_RANGE)
	invert_y = _read_bool(file, "invert_y", invert_y)
	gyro_aim = _read_bool(file, "gyro_aim", gyro_aim)
	vibration = _read_bool(file, "vibration", vibration)
	telemetry = _read_bool(file, "telemetry", telemetry)
	quality = _read_index(file, "quality", quality, QUALITY_NAMES.size())
	render_scale = _read_range(file, "render_scale", render_scale, RENDER_SCALE_RANGE)
	shadow_quality = _read_index(file, "shadow_quality", shadow_quality, SHADOW_NAMES.size())
	msaa = _read_index(file, "msaa", msaa, MSAA_NAMES.size())
	bloom = _read_bool(file, "bloom", bloom)
	fps_cap = _read_index(file, "fps_cap", fps_cap, FPS_CAPS.size())
	show_fps = _read_bool(file, "show_fps", show_fps)
	detail_distance = _read_index(file, "detail_distance", detail_distance, DETAIL_DISTANCE_NAMES.size())
	view_distance = _read_range(file, "view_distance", view_distance, VIEW_DISTANCE_RANGE)
	vsync = _read_index(file, "vsync", vsync, VSYNC_NAMES.size())
	fov = _read_range(file, "fov", fov, FOV_RANGE)
	brightness = _read_range(file, "brightness", brightness, BRIGHTNESS_RANGE)
	launch_cinematics = _read_bool(file, "launch_cinematics", launch_cinematics)
	head_bob = _read_bool(file, "head_bob", head_bob)
	speed_fov = _read_bool(file, "speed_fov", speed_fov)
	time_of_day = _read_index(file, "time_of_day", time_of_day, TIME_OF_DAY_NAMES.size())
	day_minutes = _read_range(file, "day_minutes", day_minutes, DAY_MINUTES_RANGE)
	window_mode = _read_index(file, "window_mode", window_mode, WINDOW_MODE_NAMES.size())
	master_volume = _read_range(file, "master_volume", master_volume, VOLUME_RANGE)
	effects_volume = _read_range(file, "effects_volume", effects_volume, VOLUME_RANGE)
	ambience_volume = _read_range(file, "ambience_volume", ambience_volume, VOLUME_RANGE)
	interface_volume = _read_range(file, "interface_volume", interface_volume, VOLUME_RANGE)
	audio_muted = _read_bool(file, "audio_muted", audio_muted)
	restart_height = _read_range(file, "restart_height", restart_height, RESTART_HEIGHT_RANGE)
	restart_ring = _read_index(file, "restart_ring", restart_ring, RESTART_RING_NAMES.size())
	restart_side = _read_index(file, "restart_side", restart_side, RESTART_SIDE_NAMES.size())
	restart_clearance = _read_range(file, "restart_clearance", restart_clearance, RESTART_CLEARANCE_RANGE)
	for key in ["restart_x", "restart_y", "restart_z"]:
		set(key, _read_range(file, key, get(key), RESTART_COORDINATE_RANGE))


func save_to_disk() -> void:
	if not persistent:
		return
	var file := ConfigFile.new()
	file.set_value(SECTION, "look_sensitivity", look_sensitivity)
	file.set_value(SECTION, "stick_sensitivity", stick_sensitivity)
	file.set_value(SECTION, "touch_scale", touch_scale)
	file.set_value(SECTION, "invert_y", invert_y)
	file.set_value(SECTION, "gyro_aim", gyro_aim)
	file.set_value(SECTION, "gyro_sensitivity", gyro_sensitivity)
	file.set_value(SECTION, "vibration", vibration)
	file.set_value(SECTION, "telemetry", telemetry)
	for key in ["quality", "render_scale", "shadow_quality", "msaa", "bloom", "fps_cap", "show_fps",
			"detail_distance", "view_distance", "vsync", "window_mode", "audio_muted",
			"fov", "brightness", "head_bob", "launch_cinematics", "speed_fov", "time_of_day", "day_minutes",
			"master_volume", "effects_volume", "ambience_volume", "interface_volume",
			"restart_ring", "restart_height", "restart_side", "restart_clearance", "restart_x", "restart_y", "restart_z"]:
		file.set_value(SECTION, key, get(key))
	var error := file.save(PATH)
	if error != OK:
		push_warning("SCRAPERX_SETTINGS_SAVE_FAILED code=%d path=%s" % [error, PATH])


func _read_range(file: ConfigFile, key: String, fallback: float, bounds: Vector2) -> float:
	var value: Variant = file.get_value(SECTION, key, fallback)
	if not (value is float or value is int):
		return fallback
	var number := float(value)
	if not is_finite(number):
		return fallback
	return clampf(number, bounds.x, bounds.y)


func _read_bool(file: ConfigFile, key: String, fallback: bool) -> bool:
	var value: Variant = file.get_value(SECTION, key, fallback)
	return value if value is bool else fallback


func _read_index(file: ConfigFile, key: String, fallback: int, count: int) -> int:
	var value: Variant = file.get_value(SECTION, key, fallback)
	if not value is int or value < 0 or value >= count:
		return fallback
	return value
