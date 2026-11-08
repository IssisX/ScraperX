extends Node3D
## The mist streak on the gust front (J-022 slice 2): the wind you can see.
##
## Above 396 m a gust front sweeps west off the tower's west face, one every
## 8 s, and the push follows it (src/sim/physics_world_movement.cpp). The
## streak is the front made visible: one tall, thin curtain of mist standing
## on the front's leading edge, from 396 m to 788 m over the west face's
## band, moving west with it at 5 m/s. Ahead of it, calm air; behind it, the
## full gust. The player climbing the west face sees the wall of mist come
## for him before the push arrives.
##
## Presentation only. Its x comes from the native every frame
## (get_wind_front_x_m); the band it spans is the sim's own (the upper wind's
## y and z bounds in src/sim/physics_world_tuning.hpp). No sim behavior
## changes: nothing here pushes, grips or decides anything.

# The front's band, as the sim has it (physics_world_tuning.hpp):
# kUpperWindLowY / kUpperWindHighY, kWindZoneNorthZ / kWindZoneSouthZ.
const LOW_Y := 396.0
const HIGH_Y := 788.0
const NORTH_Z := -166.0
const SOUTH_Z := -128.0
# The face the front leaves (kWindZoneX): the streak never stands east of it.
const FACE_X := -26.6

var _native: Object
var _front_x := FACE_X
var _sheet: MeshInstance3D


func setup(native: Object) -> void:
	_native = native


# The front's x as last read from the native: for a look test to stand by.
func front_x() -> float:
	return _front_x


# The curtain's center, for a look test to frame.
func centre() -> Vector3:
	return Vector3(_front_x, (LOW_Y + HIGH_Y) * 0.5, (NORTH_Z + SOUTH_Z) * 0.5)


func _ready() -> void:
	var quad := QuadMesh.new()
	# Wide across the face's band (z), tall over the front's whole height (y).
	quad.size = Vector2(SOUTH_Z - NORTH_Z, HIGH_Y - LOW_Y)
	_sheet = MeshInstance3D.new()
	_sheet.name = "MistStreak"
	_sheet.mesh = quad
	_sheet.material_override = _material()
	_sheet.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	# The quad faces +z; turned, it faces west (-x), its width along z.
	_sheet.rotation = Vector3(0.0, -PI * 0.5, 0.0)
	_sheet.position = centre()
	# No custom AABB: the quad's own bounds are exact, and they ride with
	# the node as its x follows the front.
	add_child(_sheet)


func _process(_delta: float) -> void:
	if _native == null or not _native.has_method("get_wind_front_x_m"):
		return
	_front_x = float(_native.get_wind_front_x_m())
	_sheet.position = centre()


func _material() -> ShaderMaterial:
	var shader := Shader.new()
	shader.code = """
shader_type spatial;
render_mode cull_disabled, depth_draw_never, shadows_disabled;
varying float near_fade;
void vertex() {
	// Fade out as the eye comes up to the sheet, so the front sweeping
	// through the camera never cuts the view with a hard plane.
	vec3 world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	near_fade = smoothstep(2.0, 8.0, abs(world.x - CAMERA_POSITION_WORLD.x));
}
// The same hash/noise/fbm as the cloud puffs (cloud_layer.gd).
float hash(vec2 p) {
	vec3 q = fract(vec3(p.xyx) * 0.1031);
	q += dot(q, q.yzx + 33.33);
	return fract((q.x + q.y) * q.z);
}
float noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x),
		mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}
float fbm(vec2 p) {
	float v = 0.0;
	float w = 0.5;
	for (int i = 0; i < 4; i++) {
		v += w * noise(p);
		p *= 2.03;
		w *= 0.5;
	}
	return v;
}
void fragment() {
	// Vertical streaks: fast variation across the band (z), slow along its
	// height (y), so the mist reads as columns blowing west, not blotches.
	// No sin(): at a phone's float precision sin of a large number is noise
	// of its own (cloud_layer.gd).
	float n = fbm(vec2(UV.x * 9.0, UV.y * 1.5));
	float body = clamp(0.35 + (n - 0.5) * 1.4, 0.0, 1.0);
	// Fade at the curtain's edges, so no hard rectangle ever shows. This
	// is read from UV here, in the fragment shader: UV read in vertex()
	// comes out (0, 0) on this mesh, which faded the whole curtain out.
	vec2 e = min(UV, 1.0 - UV);
	float edge_fade = smoothstep(0.0, 0.12, min(e.x, e.y));
	ALBEDO = vec3(0.93, 0.94, 0.97);
	BACKLIGHT = vec3(0.25);
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
	ALPHA = body * 0.55 * edge_fade * near_fade;
}
"""
	var material := ShaderMaterial.new()
	material.shader = shader
	return material
