shader_type spatial;
render_mode blend_mix, depth_draw_never, cull_disabled;
// The wet meniscus along the lower lid: a thin, glassy strip that catches the light.
uniform float wet = 1.0;
void fragment() {
	ALBEDO = vec3(0.75, 0.62, 0.60);
	ALPHA = clamp(0.45 * COLOR.a * wet, 0.0, 1.0);
	ROUGHNESS = 0.03;
	SPECULAR = 1.0;
}
