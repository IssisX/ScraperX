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
var persistent := true


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
