shader_type spatial;
// Probe: pre-integrated skin in a custom light(), a fragment->light varying,
// screen-space curvature, a back-scatter term, and an exposure-driven burn
// mask on one side. GL Compatibility.
uniform sampler2D skin_lut : source_color, filter_linear, repeat_disable;
uniform vec3 albedo_col : source_color = vec3(0.80, 0.66, 0.58);
uniform float exposure : hint_range(0.0, 1.0) = 0.0;
varying float curv;
varying vec3 obj_pos;
varying float wet;
void vertex() { obj_pos = VERTEX; }
void fragment() {
	vec3 n = normalize(NORMAL);
	float c = length(fwidth(n)) / max(length(fwidth(VERTEX)), 1e-5);
	curv = clamp(c * 0.012, 0.0, 1.0);
	// The burn: the right cheek and temple (her side that held the source).
	float burn = smoothstep(0.02, -0.06, obj_pos.x - 0.0) * smoothstep(0.0, 0.12, obj_pos.z + 0.02);
	burn *= smoothstep(0.0, 0.6, exposure);
	vec3 erythema = vec3(0.78, 0.34, 0.30);
	ALBEDO = mix(albedo_col, erythema, burn * 0.7);
	wet = burn * smoothstep(0.4, 0.8, exposure);
	ROUGHNESS = mix(0.58, 0.22, wet);
	SPECULAR = 0.4;
}
void light() {
	float ndl = dot(NORMAL, LIGHT);
	vec3 sss = texture(skin_lut, vec2(ndl * 0.5 + 0.5, curv)).rgb;
	DIFFUSE_LIGHT += sss * ATTENUATION * LIGHT_COLOR / PI;
	float back = pow(clamp(dot(VIEW, -LIGHT), 0.0, 1.0), 6.0) * (1.0 - clamp(ndl, 0.0, 1.0));
	DIFFUSE_LIGHT += vec3(0.95, 0.22, 0.12) * back * ATTENUATION * LIGHT_COLOR * 0.06;
	vec3 h = normalize(VIEW + LIGHT);
	float sp = pow(clamp(dot(NORMAL, h), 0.0, 1.0), mix(40.0, 160.0, wet)) * mix(0.18, 0.9, wet);
	SPECULAR_LIGHT += sp * ATTENUATION * LIGHT_COLOR * clamp(ndl, 0.0, 1.0);
}
