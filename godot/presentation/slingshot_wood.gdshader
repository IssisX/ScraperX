shader_type spatial;
render_mode diffuse_burley;

// Object-space grain follows the laminated timbers. No imported texture is
// required, so desktop and Fold display the same warm, worn oak surface.
uniform vec4 timber_light : source_color = vec4(0.55, 0.29, 0.105, 1.0);
uniform vec4 timber_dark : source_color = vec4(0.18, 0.068, 0.023, 1.0);
uniform float grain_scale = 1.0;
uniform bool part_grain_uv = false;
varying vec3 timber_position;

float hash(vec3 p) {
	p = fract(p * 0.1031);
	p += dot(p, p.yzx + 33.33);
	return fract((p.x + p.y) * p.z);
}

float noise3(vec3 p) {
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash(i), hash(i + vec3(1,0,0)), f.x),
		mix(hash(i + vec3(0,1,0)), hash(i + vec3(1,1,0)), f.x), f.y),
		mix(mix(hash(i + vec3(0,0,1)), hash(i + vec3(1,0,1)), f.x),
		mix(hash(i + vec3(0,1,1)), hash(i + vec3(1,1,1)), f.x), f.y), f.z);
}

void vertex() {
	// The rebuilt bevel mesh keeps each brace's local coordinates in UVs,
	// so grain follows that piece even when the native local pose is angled.
	timber_position = (part_grain_uv ? vec3(UV.x, UV2.x, UV.y) : VERTEX) * grain_scale;
}

void fragment() {
	vec3 p = timber_position;
	float warp = noise3(p * vec3(3.8, 3.8, 0.38));
	float rings = sin(p.x * 62.0 + p.y * 27.0 + warp * 8.0);
	float fibers = noise3(p * vec3(220.0, 120.0, 1.5));
	float pores = noise3(p * vec3(380.0, 240.0, 12.0));
	float grain = smoothstep(-0.82, 0.94, rings) * 0.29 + fibers * 0.44 + warp * 0.27;
	ALBEDO = mix(timber_dark.rgb, timber_light.rgb, grain);
	// Broad satin highlights make the chamfer and natural grain legible.
	ROUGHNESS = clamp(0.67 + fibers * 0.2 - pores * 0.08, 0.58, 0.88);
	METALLIC = 0.0;
	NORMAL_MAP = vec3(0.5 + (fibers - 0.5) * 0.07, 0.5 + (pores - 0.5) * 0.04, 1.0);
	NORMAL_MAP_DEPTH = 0.22;
}
