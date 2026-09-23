extends Node
# Time of day for the presentation. One shadowed DirectionalLight3D is the sun
# by day and the moon by night (both are near zero at the swap, so it cannot
# be seen); a second, unshadowed light is the sky's bounce. The sky shader
# draws whichever body the light currently is. Presentation only: the native
# simulation has no notion of time of day, and nothing here feeds it.
#
# The light is moved in steps (STEP_SECONDS), not every frame: a directional
# shadow re-fits its cascades whenever the light turns, so a continuously
# turning sun makes every shadow edge crawl, and each turn also rebuilds the
# sky's radiance map. At the default day length a step is 0.125 degrees.

const STEP_SECONDS := 0.5
# Noon elevation: an alpine summer sun, high but never overhead.
const NOON_ELEVATION_DEG := 58.0
const SUN_ENERGY := 2.4
const MOON_ENERGY := 0.55
const FILL_DAY_ENERGY := 0.55
const FILL_NIGHT_ENERGY := 0.2
const DAY_EXPOSURE := 1.42  # main.tscn's own tonemap_exposure
const NIGHT_EXPOSURE := 2.3

var hour := 10.5
# Real minutes per 24 in-game hours; 0 holds the current hour.
var day_minutes := 24.0

var _sun: DirectionalLight3D
var _fill: DirectionalLight3D
var _environment: Environment
var _sky_material: ShaderMaterial
var _step_clock := 0.0


func setup(sun: DirectionalLight3D, fill: DirectionalLight3D, environment: Environment) -> void:
	_sun = sun
	_fill = fill
	_environment = environment
	_sky_material = ShaderMaterial.new()
	_sky_material.shader = load("res://presentation/sky.gdshader")
	var sky := Sky.new()
	sky.sky_material = _sky_material
	# Ambient light is taken from the sky (AMBIENT_SOURCE_SKY). A small
	# radiance map, rebuilt only when the light steps, keeps that cheap.
	sky.radiance_size = Sky.RADIANCE_SIZE_64
	sky.process_mode = Sky.PROCESS_MODE_QUALITY
	_environment.sky = sky
	_environment.background_mode = Environment.BG_SKY
	# The height fog is for the air between the eye and the far slopes. Left
	# at full strength on the sky it washed the dome to one flat haze and hid
	# the cloud deck (observed).
	_environment.fog_sky_affect = 0.2
	# Ambient yes, specular no: the rough asphalt and concrete mirrored the
	# bright dome and read near-white (observed); the scene is lit by the sun,
	# the fill, the sky ambient and its own lamps.
	_environment.reflected_light_source = Environment.REFLECTION_SOURCE_DISABLED
	_apply()


func set_hour(value: float) -> void:
	hour = fposmod(value, 24.0)
	_apply()


func _process(delta: float) -> void:
	if day_minutes <= 0.0 or _sun == null:
		return
	hour = fposmod(hour + delta * 24.0 / (day_minutes * 60.0), 24.0)
	_step_clock += delta
	if _step_clock >= STEP_SECONDS:
		_step_clock = 0.0
		_apply()


# Direction toward the sun: rises in the east (+x), crosses the south (+z)
# at NOON_ELEVATION_DEG, sets in the west.
static func sun_direction(at_hour: float) -> Vector3:
	var angle := (at_hour - 12.0) / 24.0 * TAU
	var tilt := deg_to_rad(NOON_ELEVATION_DEG)
	return Vector3(-sin(angle), cos(angle) * sin(tilt), cos(angle) * cos(tilt)).normalized()


func _apply() -> void:
	if _sun == null:
		return
	var sun_dir := sun_direction(hour)
	var is_moon := sun_dir.y < 0.0
	var body_dir := -sun_dir if is_moon else sun_dir
	# Light travels away from the body it comes from.
	_sun.global_basis = Basis.looking_at(-body_dir, Vector3.UP)
	var rise := smoothstep(-0.02, 0.16, body_dir.y)
	var day := smoothstep(-0.10, 0.22, sun_dir.y)
	var dusk := (1.0 - smoothstep(0.05, 0.38, sun_dir.y)) * smoothstep(-0.12, 0.0, sun_dir.y)
	if is_moon:
		_sun.light_color = Color(0.62, 0.72, 0.92)
		_sun.light_energy = MOON_ENERGY * rise
	else:
		var warm := Color(1.0, 0.60, 0.34)
		var white := Color(1.0, 0.93, 0.84)
		_sun.light_color = warm.lerp(white, smoothstep(0.05, 0.45, sun_dir.y))
		_sun.light_energy = SUN_ENERGY * rise
	_sun.shadow_enabled = _sun.light_energy > 0.01
	_fill.light_energy = lerpf(FILL_NIGHT_ENERGY, FILL_DAY_ENERGY, day)
	_fill.light_color = Color(0.30, 0.36, 0.52).lerp(Color(0.58, 0.68, 0.82), day)
	_sky_material.set_shader_parameter(&"day_factor", day)
	_sky_material.set_shader_parameter(&"dusk_factor", dusk)
	_sky_material.set_shader_parameter(&"light_is_moon", is_moon)
	# Fog is the air between the eye and the far slopes: it takes the colour
	# of the horizon it is lit against.
	var horizon_day := Color(0.60, 0.66, 0.73)
	var horizon_night := Color(0.08, 0.10, 0.155)
	var fog := horizon_night.lerp(horizon_day, day).lerp(Color(0.78, 0.55, 0.40), dusk * 0.4)
	_environment.fog_light_color = fog
	_environment.fog_light_energy = lerpf(0.25, 0.4, day)
	# Eyes adapt: night is dark, not black -- the yard stays readable by
	# moonlight and its own lamps.
	_environment.tonemap_exposure = lerpf(NIGHT_EXPOSURE, DAY_EXPOSURE, day)
