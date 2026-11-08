shader_type spatial;
// Her hair (matron_look.gd): each ribbon is a lock of a few fine hairs
// (hair_a.png, tools/body/build_matron_body.py hair_map: R the hair's shade,
// G the order hairs drop out in, A coverage), its color the lock's own
// (vertex colors). The hairs' soft edges are blended over a depth prepass of
// their solid middles, so a lock reads as hairs, never as a hard-cut strip.
// Lit along the hair (Kajiya-Kay): a bright narrow shine shifted toward the
// tip and a dimmer one in the hair's own color shifted toward the root, the
// way light runs along a strand instead of off a card. Works in the
// gl_compatibility renderer.
render_mode cull_disabled, depth_prepass_alpha;

uniform sampler2D strands : filter_linear_mipmap, repeat_disable;
// The world's porous level (matron_look.gd POROUS_SPECULAR): the sky's sheen
// on a rough surface would read milky.
uniform float porous_specular = 0.22;
uniform float roughness : hint_range(0.0, 1.0) = 0.6;
// The exposure's work: the share of her hairs gone (0 none), whole hairs at a
// time in their own order, so what is left stays fine hairs.
uniform float thin : hint_range(0.0, 1.0) = 0.0;
uniform vec3 shine_color : source_color = vec3(1.0, 0.96, 0.9);
uniform float shine = 0.16;
uniform float shine_power = 80.0;
uniform float shine_shift = 0.1;
uniform float shine2 = 0.22;
uniform float shine2_power = 20.0;
uniform float shine2_shift = -0.12;
// hair_a.png's width, for how many of its texels one pixel covers.
uniform float map_width = 64.0;

varying vec3 hair_dir;   // along the hair, root to tip, in view space


float along_shine(vec3 t, vec3 h, float power) {
	float th = dot(t, h);
	return pow(sqrt(max(0.0, 1.0 - th * th)), power);
}


void fragment() {
	vec4 m = texture(strands, UV);
	// Far off, a lock is a pixel or two and its mipmap averages hairs with the
	// gaps between them: give back the coverage those hairs had.
	float texels = max(fwidth(UV.x) * map_width, 1.0);
	float a = min(1.0, m.a * mix(1.0, 1.8, smoothstep(2.0, 16.0, texels)));
	a *= smoothstep(thin, thin + 0.06, m.g + 0.07);
	ALPHA = a;
	ALBEDO = COLOR.rgb * m.r;
	SPECULAR = porous_specular;
	ROUGHNESS = roughness;
	// The normal is the lock's place round her head (lit as a volume); a
	// double-sided ribbon's back face must not turn it inward.
	if (!FRONT_FACING) {
		NORMAL = -NORMAL;
	}
	// Along the hair: the surface direction in which u holds still, pointed
	// the way v (root to tip) grows.
	vec3 dp1 = dFdx(VERTEX);
	vec3 dp2 = dFdy(VERTEX);
	vec2 d1 = dFdx(UV);
	vec2 d2 = dFdy(UV);
	vec3 dir = dp1 * d2.x - dp2 * d1.x;
	float dv = d1.y * d2.x - d2.y * d1.x;
	dir *= dv < 0.0 ? -1.0 : 1.0;
	hair_dir = length(dir) > 1e-9 ? normalize(dir) : normalize(BINORMAL);
}


void light() {
	float ndl = dot(NORMAL, LIGHT);
	// Light passes into a lock of hair: wrapped, so its far side is not black.
	DIFFUSE_LIGHT += clamp((ndl + 0.3) / 1.3, 0.0, 1.0) * ATTENUATION * LIGHT_COLOR / PI;
	vec3 h = normalize(LIGHT + VIEW);
	vec3 t = hair_dir;
	float s1 = along_shine(normalize(t + NORMAL * shine_shift), h, shine_power);
	float s2 = along_shine(normalize(t + NORMAL * shine2_shift), h, shine2_power);
	float facing = smoothstep(-0.1, 0.4, ndl);
	SPECULAR_LIGHT += (shine_color * shine * s1 + ALBEDO * shine2 * s2) * facing * ATTENUATION * LIGHT_COLOR / PI * SPECULAR_AMOUNT;
}
