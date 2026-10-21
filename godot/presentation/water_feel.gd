extends Node

# Water, as the player meets it (sim physics_world_water.cpp). Read from the
# native's get_water_state(); nothing here decides wet or dry.
#   - the entry: a splash thrown up where the body went in, sized by the
#     falling speed, its sound, and (main_feel.gd) the haptic;
#   - afloat: the strokes heard as the body swims;
#   - under the surface: the view tinted and fogged murky green-brown, and
#     the effects and ambience low-passed (audio_director.set_underwater).
# UNTESTED in Godot on this branch (no editor binary was available); the
# GDScript follows the file's neighbours.

const TINT := Color(0.09, 0.16, 0.13, 0.0)
const TINT_ALPHA := 0.62
const FOG_COLOR := Color(0.07, 0.12, 0.1)
const FOG_DENSITY := 0.35

var _entries := -1
var _strokes := -1
var _splash: CPUParticles3D
var _overlay: ColorRect
var _layer: CanvasLayer
var _under := 0.0
var _fog_saved := {}
var splashes_shown := 0   # counted for the UI tests

func _ready() -> void:
	_splash = CPUParticles3D.new()
	_splash.emitting = false
	_splash.one_shot = true
	_splash.amount = 96
	_splash.lifetime = 1.1
	_splash.explosiveness = 0.92
	_splash.direction = Vector3.UP
	_splash.spread = 38.0
	_splash.initial_velocity_min = 2.5
	_splash.initial_velocity_max = 6.0
	_splash.gravity = Vector3(0.0, -9.81, 0.0)
	_splash.scale_amount_min = 0.04
	_splash.scale_amount_max = 0.11
	_splash.emission_shape = CPUParticles3D.EMISSION_SHAPE_RING
	_splash.emission_ring_axis = Vector3.UP
	_splash.emission_ring_radius = 0.45
	_splash.emission_ring_inner_radius = 0.2
	_splash.emission_ring_height = 0.05
	var mesh := SphereMesh.new()
	mesh.radius = 0.5
	mesh.height = 1.0
	mesh.radial_segments = 6
	mesh.rings = 3
	var material := StandardMaterial3D.new()
	material.albedo_color = Color(0.78, 0.84, 0.82, 0.75)
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	mesh.material = material
	_splash.mesh = mesh
	add_child(_splash)
	_layer = CanvasLayer.new()
	_layer.layer = 1
	add_child(_layer)
	_overlay = ColorRect.new()
	_overlay.color = TINT
	_overlay.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_overlay.set_anchors_preset(Control.PRESET_FULL_RECT)
	_layer.add_child(_overlay)


# Returns the entry's falling speed (m/s) on the frame the body went in, for
# the haptic; 0 otherwise.
func update_view(water: Dictionary, camera: Camera3D, audio: Node, delta: float) -> float:
	var entered := 0.0
	var entries := int(water.get("entries", 0))
	if _entries >= 0 and entries > _entries:
		entered = maxf(float(water.get("entry_speed", 0.0)), 0.5)
		var at: Vector3 = water.get("position", Vector3.ZERO)
		at.y = float(water.get("surface_y", at.y))
		_splash.global_position = at
		_splash.initial_velocity_max = lerpf(3.0, 7.5, clampf(entered / 12.0, 0.0, 1.0))
		_splash.restart()
		_splash.emitting = true
		splashes_shown += 1
		if audio != null and audio.has_method(&"water_splash"):
			audio.water_splash(at, entered)
	_entries = entries
	var strokes := int(water.get("kicks", 0))
	if _strokes >= 0 and strokes > _strokes and audio != null and audio.has_method(&"swim_stroke"):
		audio.swim_stroke(water.get("position", Vector3.ZERO))
	_strokes = strokes
	# The eye, not the body's centre, decides under: the camera's height
	# against the surface (the native says whether the head is under).
	var eye_under := bool(water.get("in_water", false)) and camera != null and \
		camera.global_position.y < float(water.get("surface_y", -1.0e9))
	eye_under = eye_under or bool(water.get("head_under", false))
	_under = move_toward(_under, 1.0 if eye_under else 0.0, delta * 5.0)
	_overlay.color = Color(TINT.r, TINT.g, TINT.b, TINT_ALPHA * _under)
	_overlay.visible = _under > 0.001
	_apply_fog(camera)
	if audio != null and audio.has_method(&"set_underwater"):
		audio.set_underwater(eye_under, delta)
	return entered


# Murk under the surface: the world's fog drawn in close and green-brown,
# given back exactly as it was on the way out.
func _apply_fog(camera: Camera3D) -> void:
	if camera == null:
		return
	var world := camera.get_world_3d()
	if world == null or world.environment == null:
		return
	var env := world.environment
	if _under > 0.001:
		if _fog_saved.is_empty():
			_fog_saved = {"enabled": env.fog_enabled, "color": env.fog_light_color,
				"density": env.fog_density}
		env.fog_enabled = true
		env.fog_light_color = (_fog_saved["color"] as Color).lerp(FOG_COLOR, _under)
		env.fog_density = lerpf(float(_fog_saved["density"]), FOG_DENSITY, _under)
	elif not _fog_saved.is_empty():
		env.fog_enabled = bool(_fog_saved["enabled"])
		env.fog_light_color = _fog_saved["color"]
		env.fog_density = float(_fog_saved["density"])
		_fog_saved = {}
