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

# Graphics. A preset writes the four values under it; changing any of them
# afterwards makes the preset CUSTOM.
const QUALITY_NAMES := ["LOW", "MEDIUM", "HIGH", "ULTRA", "CUSTOM"]
const QUALITY_CUSTOM := 4
const QUALITY_PRESETS := [
	{"render_scale": 0.75, "shadow_quality": 1, "msaa": 0, "bloom": false},
	{"render_scale": 0.9, "shadow_quality": 2, "msaa": 0, "bloom": true},
	{"render_scale": 1.0, "shadow_quality": 3, "msaa": 1, "bloom": true},
	{"render_scale": 1.0, "shadow_quality": 4, "msaa": 2, "bloom": true},
]
const SHADOW_NAMES := ["OFF", "LOW", "MEDIUM", "HIGH", "ULTRA"]
const MSAA_NAMES := ["OFF", "2X MSAA", "4X MSAA"]
const FPS_CAPS := [0, 30, 60, 90, 120]
const FPS_CAP_NAMES := ["DISPLAY", "30", "60", "90", "120"]
const TIME_OF_DAY_NAMES := ["CYCLE", "DAWN", "NOON", "DUSK", "NIGHT"]
const TIME_OF_DAY_HOURS := [10.5, 6.4, 12.5, 18.3, 23.5]
# Where a (re)start puts the player: the native's InitialSpawn for each, -1
# for its default at grade. A playtest shortcut, not a save: every start is a
# fresh world.
const START_NAMES := ["GROUND", "154 M STAIR TOP", "220 M RING", "340 M PLATE", "374 M RING", "418 M CAGE"]
const START_SPAWNS := [-1, 24, 29, 32, 33, 34]

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
var fov := 82.0
var brightness := 1.0
var head_bob := true
var speed_fov := true
var time_of_day := 0
var day_minutes := 24.0
var master_volume := 0.8
var effects_volume := 1.0
var ambience_volume := 0.8
var interface_volume := 0.7
var start_at := 0
var persistent := true


# Writes a preset's values; CUSTOM leaves them as they are.
func apply_quality(index: int) -> void:
	quality = clampi(index, 0, QUALITY_CUSTOM)
	if quality == QUALITY_CUSTOM:
		return
	var preset: Dictionary = QUALITY_PRESETS[quality]
	render_scale = preset["render_scale"]
	shadow_quality = preset["shadow_quality"]
	msaa = preset["msaa"]
	bloom = preset["bloom"]


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
	fov = _read_range(file, "fov", fov, FOV_RANGE)
	brightness = _read_range(file, "brightness", brightness, BRIGHTNESS_RANGE)
	head_bob = _read_bool(file, "head_bob", head_bob)
	speed_fov = _read_bool(file, "speed_fov", speed_fov)
	time_of_day = _read_index(file, "time_of_day", time_of_day, TIME_OF_DAY_NAMES.size())
	day_minutes = _read_range(file, "day_minutes", day_minutes, DAY_MINUTES_RANGE)
	master_volume = _read_range(file, "master_volume", master_volume, VOLUME_RANGE)
	effects_volume = _read_range(file, "effects_volume", effects_volume, VOLUME_RANGE)
	ambience_volume = _read_range(file, "ambience_volume", ambience_volume, VOLUME_RANGE)
	interface_volume = _read_range(file, "interface_volume", interface_volume, VOLUME_RANGE)
	start_at = _read_index(file, "start_at", start_at, START_NAMES.size())


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
			"fov", "brightness", "head_bob", "speed_fov", "time_of_day", "day_minutes",
			"master_volume", "effects_volume", "ambience_volume", "interface_volume", "start_at"]:
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
