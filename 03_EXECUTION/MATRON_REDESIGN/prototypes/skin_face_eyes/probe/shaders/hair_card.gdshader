shader_type spatial;
render_mode cull_disabled, depth_prepass_alpha;
uniform sampler2D tex : source_color, filter_linear_mipmap;
uniform vec3 tint : source_color = vec3(0.6);
uniform float thin = 0.5;
void fragment() {
	vec4 t = texture(tex, UV);
	ALBEDO = tint;
	ALPHA = clamp(t.a * thin, 0.0, 1.0);
	ROUGHNESS = 0.55;
	SPECULAR = 0.3;
}
