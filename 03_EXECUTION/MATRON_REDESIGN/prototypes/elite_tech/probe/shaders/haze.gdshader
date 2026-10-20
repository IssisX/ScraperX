shader_type spatial;
render_mode unshaded, cull_disabled, depth_draw_never, blend_mix;
// Screen and depth texture reads in gl_compatibility (probe): a quad around
// her head bends what is behind it (the air near her is wrong) and fades
// where it meets geometry (soft intersection from the depth texture).
uniform sampler2D screen_tex : hint_screen_texture, filter_linear_mipmap;
uniform sampler2D depth_tex : hint_depth_texture, filter_nearest;
uniform float strength = 0.012;
uniform float t = 0.0;
uniform float soft = 0.15;
void fragment() {
	vec2 c = UV - 0.5;
	float r = length(c);
	float ring = smoothstep(0.5, 0.15, r);
	vec2 off = c / max(r, 1e-3) * sin(r * 40.0 - t * 3.0) * strength * ring;
	vec3 bg = textureLod(screen_tex, SCREEN_UV + off, 1.5 * ring).rgb;
	float d = texture(depth_tex, SCREEN_UV).r;
	vec4 ndc = vec4(SCREEN_UV * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
	vec4 vpos = INV_PROJECTION_MATRIX * ndc;
	float scene_z = -vpos.z / vpos.w;
	float frag_z = -VERTEX.z;
	float fade = clamp((scene_z - frag_z) / soft, 0.0, 1.0);
	ALBEDO = bg * vec3(0.92, 0.95, 1.0);
	ALPHA = ring * fade;
}
