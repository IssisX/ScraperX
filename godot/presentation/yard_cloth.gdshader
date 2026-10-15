shader_type spatial;
render_mode cull_disabled;
uniform float phase = 0.0;
uniform float motion = 1.0;
void vertex() {
	float free_edge = UV.y * UV.y;
	VERTEX.y += motion * free_edge * (0.06 * sin(phase * 2.1 + UV.y * 5.0) + 0.025 * sin(phase * 3.7 + UV.x * 4.0));
}
void fragment() {
	float stripe = step(0.5, fract(UV.y * 4.0 + UV.x));
	ALBEDO = mix(vec3(0.66, 0.32, 0.12), vec3(0.78, 0.67, 0.38), stripe * 0.55);
	ROUGHNESS = 0.96;
}
