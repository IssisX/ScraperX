shader_type spatial;
render_mode diffuse_burley, specular_schlick_ggx;

// Meter-scaled poured concrete on the existing solid shaft. Surface staining
// and flush construction joints add no relief, windows or implied footholds.
varying vec3 shaft_position;
varying vec3 shaft_normal;

void vertex() {
	shaft_position = VERTEX;
	shaft_normal = NORMAL;
}

float cell_hash(vec2 p) {
	p = fract(p * vec2(0.1031, 0.1030));
	p += dot(p, p.yx + 33.33);
	return fract((p.x + p.y) * p.x);
}

float concrete_noise(vec2 p) {
	vec2 cell = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(cell_hash(cell), cell_hash(cell + vec2(1.0, 0.0)), f.x),
		mix(cell_hash(cell + vec2(0.0, 1.0)), cell_hash(cell + vec2(1.0)), f.x), f.y);
}

float filtered_joint(float coordinate, float spacing, float width) {
	float distance_to_joint = abs(mod(coordinate + spacing * 0.5, spacing) - spacing * 0.5);
	float footprint = max(fwidth(coordinate), 0.002);
	// Subpixel joints disappear rather than crawling during a tower pan.
	float coverage = 1.0 - smoothstep(width, width + footprint, distance_to_joint);
	return coverage * min(1.0, width / footprint);
}

void fragment() {
	vec3 n = abs(shaft_normal);
	vec2 p = n.y > max(n.x, n.z) ? shaft_position.xz :
		(n.x > n.z ? shaft_position.zy : shaft_position.xy);
	// The eleven-meter pour lifts remain readable from the receiving yard;
	// smaller formwork seams filter out naturally at the same distance.
	float bay = floor(p.x / 7.6);
	float lift = floor((p.y + 800.0) / 11.0);
	float pour = cell_hash(vec2(bay, lift));
	float cloud = concrete_noise(p * vec2(0.14, 0.055));
	float runoff = concrete_noise(p * vec2(0.85, 0.025));
	float streak = smoothstep(0.48, 0.86, runoff);
	float age = smoothstep(0.2, 0.9, cloud);
	vec3 color = mix(vec3(0.17, 0.185, 0.19), vec3(0.265, 0.26, 0.235), pour * 0.5 + cloud * 0.5);
	color *= 1.0 - streak * (0.14 + 0.12 * age);
	float lift_joint = filtered_joint(p.y + 800.0, 11.0, 0.055);
	float panel_joint = max(filtered_joint(p.x, 2.4, 0.012), filtered_joint(p.y + 800.0, 1.2, 0.012));
	color *= 1.0 - max(lift_joint * 0.38, panel_joint * 0.18);
	ALBEDO = color;
	METALLIC = 0.0;
	ROUGHNESS = 0.94;
	SPECULAR = 0.18;
}
