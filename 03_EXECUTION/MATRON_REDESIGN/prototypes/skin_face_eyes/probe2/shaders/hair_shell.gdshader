shader_type spatial;
render_mode cull_disabled, alpha_to_coverage_and_one;
// Thin white hair combed back to a bun, as 10 offset shells of the scalp (fur-shell technique,
// Lengyel et al. 2001). Strands are meridians converging on the bun; each shell keeps fewer of
// them (shorter/flatter strands drop out). Kajiya-Kay highlights along the strand direction.
// COLOR.r = shell height 0..1, COLOR.g = local thickness, COLOR.b = 1 for the bun.
uniform sampler2D maskB_tex : filter_linear_mipmap, repeat_disable;   // A: hair density (stage2)
uniform vec3 bun_centre;
uniform vec3 bun_axis = vec3(0.0, 0.0, -1.0);
uniform vec3 head_centre;
uniform vec3 hair_col : source_color = vec3(0.80, 0.78, 0.72);
uniform float strands = 3200.0;
uniform float density_scale = 1.0;
varying vec3 o_pos;
varying vec3 v_t;
varying float v_layer;
varying float v_bun;
float h11(float p) { return fract(sin(p * 127.1) * 43758.5453); }
float vn1(float x) { float i = floor(x); float f = fract(x); f = f * f * (3.0 - 2.0 * f); return mix(h11(i), h11(i + 1.0), f); }
void vertex() {
	o_pos = VERTEX; v_layer = COLOR.r; v_bun = COLOR.b;
	vec3 f;
	if (COLOR.b > 0.5) {
		// the bun: hair wound around its axis
		vec3 q = VERTEX - bun_centre;
		f = normalize(cross(bun_axis, q) + bun_axis * 0.15);
	} else {
		vec3 to = bun_centre - VERTEX;
		f = to - NORMAL * dot(to, NORMAL);
	}
	f = normalize(f - NORMAL * dot(f, NORMAL));
	v_t = normalize((MODELVIEW_MATRIX * vec4(f, 0.0)).xyz);
}
void fragment() {
	float dens = v_bun > 0.5 ? 0.95 : texture(maskB_tex, UV).a * density_scale;
	vec3 a = normalize(bun_centre - head_centre);
	vec3 e1 = normalize(cross(a, vec3(1.0, 0.0, 0.0)));
	vec3 e2 = cross(a, e1);
	vec3 q = o_pos - head_centre;
	float phi = atan(dot(q, e2), dot(q, e1));
	float along = dot(q, a);
	if (v_bun > 0.5) {
		vec3 qb = o_pos - bun_centre;
		phi = dot(qb, bun_axis) * 60.0; along = atan(dot(qb, e2), dot(qb, e1)) * 0.02;
	}
	// strand coordinate: gently wavy meridians. Hairs gather into comb-tracked clumps; between the
	// clumps the scalp shows (thin old hair combed back flat).
	float wav = 2.0 * vn1(along * 40.0 + phi * 5.0) + 6.0 * vn1(along * 9.0 + phi * 1.7);
	float uc = phi / TAU * strands * 0.045 + wav * 0.05 + 0.6 * vn1(along * 14.0 + phi * 3.0);   // clump coordinate (~4 mm comb tracks)
	float cid = floor(uc);
	float cc = abs(fract(uc) - 0.5);
	float cw = 0.30 + 0.15 * h11(cid * 2.3);                           // clump half-width
	float aac = fwidth(uc) + 1e-4;
	float clump = mix(0.35, 1.0, 1.0 - smoothstep(cw - 0.12 - aac, cw + aac, cc));
	float u = phi / TAU * strands + wav;
	float id = floor(u);
	float cu = abs(fract(u) - 0.5);
	float aa = fwidth(u) * 0.8 + 1e-4;
	float line = 1.0 - smoothstep(0.32 - aa, 0.32 + aa, cu);
	line = mix(line, 1.0, clamp(aa * 1.6, 0.0, 1.0) * 0.6);           // far away: strands blur to a sheet
	// hair lies flat: near the hairline only the low shells carry it, deeper in it builds up
	float height = clamp(dens * 1.6, 0.0, 1.0) * mix(0.55, 1.0, h11(id * 3.11));
	float present = step(v_layer, height) * step(h11(id * 7.7 + 0.3), dens * 1.25 + 0.1);
	// looking along the scalp you see through many hairs: the gaps close toward the silhouette
	float graze = 1.0 - smoothstep(0.08, 0.45, abs(dot(NORMAL, VIEW)));
	line = mix(line, 1.0, graze * 0.85);
	clump = mix(clump, 1.0, graze);
	float alpha = line * present * mix(clump, 1.0, 0.25 * v_layer);
	alpha *= step(0.04, dens);
	// silhouette: the upper shells fade at grazing angles (no bristle fringe)
	alpha *= mix(1.0, smoothstep(0.03, 0.2, abs(dot(NORMAL, VIEW))), v_layer * v_layer);
	alpha = max(alpha, step(v_layer, 0.1) * dens * 0.4 * clump);
	ALPHA = alpha;
	ALPHA_SCISSOR_THRESHOLD = 0.5;
	float tone = 0.80 + 0.25 * h11(id * 9.1);
	vec3 c = hair_col * tone;
	c = mix(c, vec3(0.42, 0.40, 0.37), step(0.93, h11(id * 4.3)));   // a few dark grey hairs left
	c *= mix(0.72, 1.0, v_layer);                                      // self-shadow toward the scalp
	ALBEDO = c;
	ROUGHNESS = 0.45;
	SPECULAR = 0.35;
}
void light() {
	float ndl = dot(NORMAL, LIGHT);
	DIFFUSE_LIGHT += clamp(ndl * 0.75 + 0.25, 0.0, 1.0) * ATTENUATION * LIGHT_COLOR / PI;
	vec3 h = normalize(LIGHT + VIEW);
	float th = dot(v_t, h);
	float s1 = pow(max(sqrt(max(1.0 - (th + 0.08) * (th + 0.08), 0.0)), 0.0), 90.0);
	float s2 = pow(max(sqrt(max(1.0 - (th - 0.12) * (th - 0.12), 0.0)), 0.0), 20.0);
	float vis = clamp(ndl + 0.3, 0.0, 1.0);
	SPECULAR_LIGHT += (vec3(0.22) * s1 + ALBEDO * 0.25 * s2) * vis * ATTENUATION * LIGHT_COLOR;
}
