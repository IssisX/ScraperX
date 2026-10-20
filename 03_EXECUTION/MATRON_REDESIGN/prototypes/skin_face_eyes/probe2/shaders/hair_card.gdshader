shader_type spatial;
render_mode cull_disabled, depth_prepass_alpha;
uniform sampler2D tex : source_color, filter_linear_mipmap;
uniform vec3 tint : source_color = vec3(0.6);
uniform float thin = 0.5;
uniform float sparse = 0.0;   // old brows: hairs missing in patches
float hh(vec2 p) { return fract(sin(dot(p, vec2(41.3, 289.1))) * 43758.5453); }
void fragment() {
	vec4 t = texture(tex, UV);
	ALBEDO = tint;
	// old brows: only the strongest hairs of the card survive, at mixed greys
	float a = smoothstep(sparse, 1.0, t.a);
	ALPHA = clamp(a * thin, 0.0, 1.0);
	ALBEDO = tint * mix(0.8, 1.15, t.r);
	ROUGHNESS = 0.55;
	SPECULAR = 0.3;
}
