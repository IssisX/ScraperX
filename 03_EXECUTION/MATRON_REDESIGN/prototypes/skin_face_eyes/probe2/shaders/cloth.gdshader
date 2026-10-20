shader_type spatial;
render_mode cull_disabled;
// Starched cotton: off-white, a fine weave, grime in the folds; a stain that spreads with exposure.
uniform float exposure = 0.0;
uniform int cut_mode = 0;          // 1: cap (keep above the cap line), 2: band (keep a strip just above it)
uniform float band_w = 0.13;       // dm
varying vec3 o_pos;
float h31(vec3 p) { p = fract(p * 0.3183099 + 0.1); p *= 17.0; return fract(p.x * p.y * p.z * (p.x + p.y + p.z)); }
float vn3(vec3 x) {
	vec3 i = floor(x); vec3 f = fract(x); f = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix(h31(i), h31(i + vec3(1, 0, 0)), f.x), mix(h31(i + vec3(0, 1, 0)), h31(i + vec3(1, 1, 0)), f.x), f.y),
		mix(mix(h31(i + vec3(0, 0, 1)), h31(i + vec3(1, 0, 1)), f.x), mix(h31(i + vec3(0, 1, 1)), h31(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}
void vertex() { o_pos = VERTEX; }
// (cull_disabled for the cap shells: a discarded edge must not show the inside)
void fragment() {
	// cap line in dm, from object metres (origin at the eyes): y_dm - (7.85 + (z_dm - 1.05) * 0.55)
	float cl = 10.0 * o_pos.y - 5.5 * o_pos.z - 0.3185 + 0.025 * sin(atan(o_pos.x, o_pos.z) * 9.0);
	if (cut_mode == 1 && cl < 0.0) discard;
	if (cut_mode == 2 && (cl < 0.0 || cl > band_w)) discard;
	float n = vn3(o_pos * 60.0) * 0.5 + vn3(o_pos * 160.0) * 0.3 + vn3(o_pos * 15.0) * 0.2;
	vec3 c = vec3(0.80, 0.79, 0.75) * (0.9 + 0.12 * n);
	float stain = smoothstep(0.55, 0.75, vn3(o_pos * 9.0) + exposure * 0.3) * 0.5;
	c = mix(c, vec3(0.62, 0.55, 0.42), stain);
	if (cut_mode == 2) c *= 0.93 + 0.07 * smoothstep(0.0, band_w, cl);   // the fold catches less light at its lip
	ALBEDO = c;
	ROUGHNESS = 0.93;
	SPECULAR = 0.2;
	float fade = 1.0 - smoothstep(0.0004, 0.0012, length(fwidth(o_pos)));   // no weave moire at a distance
	float w = sin(o_pos.x * 2600.0) * sin(o_pos.y * 2600.0) * fade;
	NORMAL_MAP = normalize(vec3(0.5 + 0.08 * w, 0.5 + 0.08 * (vn3(o_pos * 300.0) - 0.5), 1.0));
}
