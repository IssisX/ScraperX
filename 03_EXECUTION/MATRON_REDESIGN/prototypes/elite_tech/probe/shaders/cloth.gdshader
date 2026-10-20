shader_type spatial;
render_mode cull_disabled;
// Cotton/wool sheen for gl_compatibility (probe): Charlie sheen distribution
// (Estevez & Kulla 2017) with the Neubelt visibility term, plus wrapped
// diffuse; a fibre grain from a procedural weave.
uniform vec3 base : source_color = vec3(0.86, 0.84, 0.78);
uniform vec3 sheen_col : source_color = vec3(0.9, 0.9, 0.88);
uniform float sheen_rough = 0.45;
uniform float sheen_amt = 1.0;       // 0 = plain lambert (comparison)
uniform float grime = 0.0;
varying vec3 o_pos;
float h21(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
void vertex() { o_pos = VERTEX; }
void fragment() {
	vec2 w = UV * vec2(260.0, 260.0);
	float weave = 0.92 + 0.08 * (abs(sin(w.x)) * abs(sin(w.y)));
	ALBEDO = base * weave * mix(1.0, 0.55, grime * smoothstep(0.2, 0.8, h21(floor(UV * 30.0))));
	ROUGHNESS = 0.95;
	SPECULAR = 0.1;
}
void light() {
	float nl = dot(NORMAL, LIGHT);
	DIFFUSE_LIGHT += clamp((nl + 0.3) / 1.3, 0.0, 1.0) * ATTENUATION * LIGHT_COLOR / PI;
	vec3 h = normalize(LIGHT + VIEW);
	float nh = clamp(dot(NORMAL, h), 0.0, 1.0);
	float sin2 = max(1.0 - nh * nh, 0.0078125);
	float inv = 1.0 / sheen_rough;
	float D = (2.0 + inv) * pow(sin2, inv * 0.5) / (2.0 * PI);
	float nv = clamp(dot(NORMAL, VIEW), 0.0, 1.0);
	float l = clamp(nl, 0.0, 1.0);
	float V = 1.0 / (4.0 * (l + nv - l * nv) + 1e-4);
	SPECULAR_LIGHT += sheen_col * D * V * l * sheen_amt * ATTENUATION * LIGHT_COLOR;
}
