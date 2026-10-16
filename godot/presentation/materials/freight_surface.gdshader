shader_type spatial;
render_mode diffuse_burley, specular_schlick_ggx;

uniform vec4 base_color : source_color = vec4(0.5, 0.5, 0.5, 1.0);
uniform bool painted = false;
uniform float base_metallic : hint_range(0.0, 1.0) = 1.0;
uniform float base_roughness : hint_range(0.3, 0.95) = 0.56;
varying vec3 body_position;
varying vec3 body_normal;

void vertex() {
	// KitView already batches native part transforms into rigid-body space.
	// Coordinates therefore travel with the machine, even on cooked hulls.
	body_position = VERTEX;
	body_normal = NORMAL;
}

float hash_cell(vec3 p) {
	p = fract(p * vec3(0.1031, 0.1030, 0.0973));
	p += dot(p, p.yxz + 33.33);
	return fract((p.x + p.y) * p.z);
}

float mottling(vec3 p) {
	vec3 cell = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(hash_cell(cell), hash_cell(cell + vec3(1,0,0)), f.x),
		mix(hash_cell(cell + vec3(0,1,0)), hash_cell(cell + vec3(1,1,0)), f.x), f.y),
		mix(mix(hash_cell(cell + vec3(0,0,1)), hash_cell(cell + vec3(1,0,1)), f.x),
		mix(hash_cell(cell + vec3(0,1,1)), hash_cell(cell + vec3(1,1,1)), f.x), f.y), f.z);
}

void fragment() {
	vec3 n = abs(normalize(body_normal));
	vec2 p = n.y > max(n.x, n.z) ? body_position.xz :
		(n.x > n.z ? body_position.zy : body_position.xy);
	// Meter-scaled rubbed patches and sparse millimeter scratches. Derivative
	// filtering suppresses fine lines beyond the close cab/boarding view.
	float patina = mottling(body_position * 7.0);
	vec2 cell = floor(p * vec2(85.0, 15.0));
	float seed = hash_cell(vec3(cell, 2.0));
	float line_distance = abs(fract(p.x * 85.0) - .5);
	float aa = max(fwidth(p.x * 85.0), .025);
	float scratch = (1.0 - smoothstep(.025, .025 + aa, line_distance)) * step(.975, seed);
	float rubbed = smoothstep(.66, .85, patina);
	vec3 color = base_color.rgb * mix(.94, 1.03, patina);
	float metallic = base_metallic;
	if (painted) {
		float exposed = max(scratch * .28, rubbed * .20);
		color = mix(color, vec3(.19, .21, .22), exposed);
		metallic = exposed;
	} else {
		color = mix(color, color * 1.15, scratch);
	}
	ALBEDO = color;
	METALLIC = metallic;
	ROUGHNESS = clamp(base_roughness + (patina - .5) * .16 - scratch * .12, .32, .95);
}
