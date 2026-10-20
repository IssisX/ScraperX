shader_type spatial;
render_mode cull_disabled, depth_prepass_alpha;
// Grey stray hairs: ribbons fading to nothing at the tips and edges.
uniform vec3 tint : source_color = vec3(0.70, 0.68, 0.64);
void fragment() {
	float edge = 1.0 - pow(abs(UV.x * 2.0 - 1.0), 2.0);
	float tip = 1.0 - smoothstep(0.6, 1.0, UV.y);
	ALBEDO = tint * (0.85 + 0.15 * sin(UV.y * 40.0));
	ALPHA = clamp(edge * tip * 0.85, 0.0, 1.0);
	ROUGHNESS = 0.42;
	SPECULAR = 0.45;
}
