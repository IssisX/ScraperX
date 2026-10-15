extends Node3D
## A cloud layer partway up the tower: the height felt. From the yard it is a
## broken ceiling overhead; climbing, the puffs come level and drift past;
## above it, it lies below like a floor with the yard showing through.
##
## Presentation only. CLUSTERS clouds of LOBES soft, lit quads each (PUFFS in
## all, one MultiMesh), scattered in an annulus round the tower (kept CLEAR_M clear of its axis so no puff cuts the
## structure) between LAYER_LOW and LAYER_HIGH, each turned to face the camera
## and drifting slowly with the wind, wrapping round the annulus. A puff fades
## out as the eye comes into it, never a hard plane through the view.

const CLUSTERS := 42
const LOBES := 4
const PUFFS := CLUSTERS * LOBES
const LAYER_LOW := 455.0
const LAYER_HIGH := 505.0
const CENTER := Vector2(0.0, -150.0)   # the tower's axis (x, z)
const CLEAR_M := 75.0
const REACH_M := 950.0
const WIND := Vector2(1.6, 0.6)        # m/s, drift

var _mm: MultiMesh
var _at: Array[Vector3] = []
var _size: PackedFloat32Array = PackedFloat32Array()
var _shade: PackedFloat32Array = PackedFloat32Array()


func _ready() -> void:
	var rng := RandomNumberGenerator.new()
	rng.seed = 47011
	for c in CLUSTERS:
		var angle := rng.randf_range(0.0, TAU)
		# Denser near the tower, where the climber sees them pass.
		var reach := lerpf(CLEAR_M + 40.0, REACH_M, pow(rng.randf(), 1.4))
		var y := rng.randf_range(LAYER_LOW, LAYER_HIGH)
		var heart := Vector3(CENTER.x + cos(angle) * reach, y, CENTER.y + sin(angle) * reach)
		var scale := rng.randf_range(0.8, 1.5)
		for lobe in LOBES:
			# A cloud's lobes: spread along its length, the middle ones higher.
			var along := (float(lobe) - 1.5) * 38.0 * scale
			var lift := (8.0 if lobe in [1, 2] else 0.0) * scale + rng.randf_range(-4.0, 4.0)
			var turn := angle + PI * 0.5
			_at.append(heart + Vector3(cos(turn) * along, lift, sin(turn) * along) +
				Vector3(rng.randf_range(-12.0, 12.0), 0.0, rng.randf_range(-12.0, 12.0)))
			_size.append(rng.randf_range(80.0, 130.0) * scale * (1.15 if lobe in [1, 2] else 0.9))
			_shade.append(rng.randf_range(0.86, 1.0))
	_mm = MultiMesh.new()
	_mm.transform_format = MultiMesh.TRANSFORM_3D
	_mm.use_custom_data = true
	var quad := QuadMesh.new()
	quad.size = Vector2(1.0, 0.62)
	_mm.mesh = quad
	_mm.instance_count = PUFFS
	var instance := MultiMeshInstance3D.new()
	instance.name = "Clouds"
	instance.multimesh = _mm
	instance.material_override = _material()
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	instance.custom_aabb = AABB(Vector3(-REACH_M - 200.0, LAYER_LOW - 80.0, -REACH_M - 350.0),
		Vector3(2.0 * REACH_M + 400.0, LAYER_HIGH - LAYER_LOW + 160.0, 2.0 * REACH_M + 400.0))
	add_child(instance)


# Once per rendered frame: drift, wrap, and face the camera.
func update(delta: float, camera: Camera3D) -> void:
	var eye := camera.global_position
	for i in PUFFS:
		var p := _at[i] + Vector3(WIND.x, 0.0, WIND.y) * delta
		var flat := Vector2(p.x, p.z) - CENTER
		if flat.length() > REACH_M:
			# Out past the far edge: back in on the other side.
			flat = -flat.normalized() * (REACH_M - 1.0)
		elif flat.length() < CLEAR_M:
			flat = flat.normalized() * CLEAR_M
		p = Vector3(CENTER.x + flat.x, p.y, CENTER.y + flat.y)
		_at[i] = p
		var to_eye := eye - p
		var facing := Basis.looking_at(-to_eye.normalized() if to_eye.length() > 0.01 else Vector3.FORWARD,
			Vector3.UP)
		var size := _size[i]
		_mm.set_instance_transform(i, Transform3D(facing.scaled(Vector3(size, size, size)), p))
		_mm.set_instance_custom_data(i, Color(_shade[i], size, 0.0, 0.0))


func puff_count() -> int:
	return PUFFS


# The puff nearest `point`, and its distance: for a test to look at.
func nearest(point: Vector3) -> float:
	var best := INF
	for p in _at:
		best = minf(best, p.distance_to(point))
	return best


func _material() -> ShaderMaterial:
	var shader := Shader.new()
	shader.code = """
shader_type spatial;
render_mode cull_disabled, depth_draw_never, shadows_disabled;
varying float shade;
varying float near_fade;
void vertex() {
	shade = INSTANCE_CUSTOM.r;
	float size = INSTANCE_CUSTOM.g;
	vec3 centre = (MODELVIEW_MATRIX * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
	// Fade out as the eye comes within the puff, so it never cuts the view.
	near_fade = smoothstep(size * 0.25, size * 0.7, length(centre));
}
float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
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
	vec2 q = UV * 2.0 - 1.0;
	float r = length(q * vec2(1.0, 1.15));
	float body = 1.0 - smoothstep(0.0, 1.0, r);
	body *= body;
	float n = fbm(UV * 3.2 + shade * 23.0);
	float a = clamp(body * 1.6 + (n - 0.55) * 1.1 - 0.12, 0.0, 1.0);
	// A flat, shaded base and a sunlit top, as cumulus have.
	float base = mix(1.0, 0.72, smoothstep(0.35, 1.0, UV.y));
	ALBEDO = vec3(0.94, 0.95, 0.97) * shade * base;
	BACKLIGHT = vec3(0.3);
	ROUGHNESS = 1.0;
	SPECULAR = 0.0;
	ALPHA = a * 0.72 * near_fade;
}
"""
	var material := ShaderMaterial.new()
	material.shader = shader
	return material
