shader_type spatial;
render_mode unshaded, blend_add, depth_draw_never, cull_back, skip_vertex_transform;
// The tear film's mirror image of the world, drawn after the lid-occlusion strip (which darkens
// the diffuse eyeball, but must not darken a reflection). Same eyeball mesh, 0.15 mm larger.
uniform vec3 env_sky = vec3(0.0);
uniform vec3 env_band = vec3(0.0);
uniform vec3 env_ground = vec3(0.0);
uniform float strength = 1.0;
uniform float wet = 1.0;
varying vec3 o_pos;
void vertex() {
	o_pos = VERTEX;
	VERTEX = (MODELVIEW_MATRIX * vec4(VERTEX * 1.012, 1.0)).xyz;
	NORMAL = normalize((MODELVIEW_MATRIX * vec4(NORMAL, 0.0)).xyz);
}
void fragment() {
	vec3 dp = normalize(o_pos);
	vec3 rv = reflect(-VIEW, NORMAL);
	vec3 rw = (INV_VIEW_MATRIX * vec4(rv, 0.0)).xyz;
	float fres = 0.025 + 0.975 * pow(1.0 - clamp(dot(NORMAL, VIEW), 0.0, 1.0), 5.0);
	float el = rw.y + 0.03 * sin(rw.x * 41.0);
	vec3 envc = mix(env_ground, env_band, smoothstep(-0.45, -0.30, el));
	// CIE overcast sky: the zenith is three times as bright as the horizon (Moon & Spencer 1942)
	vec3 sky = env_sky * (1.0 + 2.0 * clamp(el, 0.0, 1.0)) / 3.0 * 2.0;
	envc = mix(envc, sky, smoothstep(-0.10, 0.02, el));
	// the reflected world is broken by the lashes at the top edge
	float lash = 0.65 + 0.35 * step(0.5, fract(atan(dp.x, dp.y) * 30.0)) ;
	float top = smoothstep(0.62, 0.40, dp.y);
	ALBEDO = envc * fres * strength * wet * mix(lash, 1.0, smoothstep(0.45, 0.25, dp.y)) * top;
}
