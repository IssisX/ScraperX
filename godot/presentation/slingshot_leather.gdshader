shader_type spatial;
render_mode diffuse_burley, cull_disabled;
varying vec3 leather_position;
float grain_hash(vec3 p) {
	p = fract(p * 0.1031);
	p += dot(p, p.yzx + 19.19);
	return fract((p.x + p.y) * p.z);
}
void vertex() { leather_position = VERTEX; }
void fragment() {
	float grain = grain_hash(floor(leather_position * 320.0));
	float worn = 0.5 + 0.5 * sin(leather_position.x * 7.0 + leather_position.z * 11.0);
	ALBEDO = mix(vec3(0.075, 0.021, 0.009), vec3(0.245, 0.092, 0.032), grain * 0.34 + worn * 0.37);
	ROUGHNESS = 0.68 + grain * 0.22;
	NORMAL_MAP = vec3(0.5 + (grain - 0.5) * 0.12, 0.5 + (worn - 0.5) * 0.035, 1.0);
	NORMAL_MAP_DEPTH = 0.28;
}
