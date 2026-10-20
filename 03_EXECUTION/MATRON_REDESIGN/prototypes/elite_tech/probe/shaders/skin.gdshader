shader_type spatial;
render_mode cull_back;
// Elite skin for gl_compatibility (probe). Quality is chosen by a #define the
// loader inserts on the next line (QUALITY 0 low, 1 medium, 2 high).
//@QUALITY@
#ifndef QUALITY
#define QUALITY 2
#endif

uniform sampler2D albedo_tex : source_color, filter_linear_mipmap_anisotropic, repeat_disable;
uniform sampler2D skin_lut : source_color, filter_linear, repeat_disable;
uniform sampler2D pores_n : hint_normal, filter_linear_mipmap, repeat_enable;
uniform float pore_tiling = 36.0;
uniform float pore_depth : hint_range(0.0, 2.0) = 0.55;
uniform vec3 tint : source_color = vec3(1.0);
uniform float rough_a = 0.46;   // the tight, oily lobe
uniform float rough_b = 0.82;   // the broad, dusty lobe
uniform float lobe_mix = 0.78;
uniform float spec_scale = 0.8;
uniform vec3 scatter_col : source_color = vec3(0.86, 0.46, 0.34);
uniform float trans_strength = 0.75;
uniform float exposure : hint_range(0.0, 1.0) = 0.0;
uniform float light_wrap = 0.12;   // the bulb is a sphere, not a point: its terminator wraps
// micro-motion, all on simulation time so it repeats exactly
uniform float sim_time = 0.0;
uniform float pulse_hz = 1.15;
uniform float pulse_amp = 0.0;     // metres along the normal at the carotid
uniform float tremor_amp = 0.0;    // metres, lips and chin
uniform float tremor_hz = 9.0;
uniform float breath = 0.0;        // 0..1, the chest's breath value
uniform float stillness = 0.0;     // 1 = the too-still window: everything but the pulse freezes

varying float v_curv;
varying float v_ao;
varying float v_thick;
varying vec3 v_rest;
varying vec3 v_geo_n;
varying float v_burn;

float hash11(float n) { return fract(sin(n) * 43758.5453); }
float noise1(float x) {
	float i = floor(x);
	float f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(hash11(i), hash11(i + 1.0), f) * 2.0 - 1.0;
}

void vertex() {
	// CUSTOM0: rest-pose position (xyz) so masks stay glued to the skin when
	// bones and blend shapes move it; w = the carotid (pulse) mask.
	v_rest = CUSTOM0.xyz;
	v_curv = COLOR.r;
	v_ao = COLOR.g;
	v_thick = COLOR.b;
	float t = sim_time;
	// The pulse: a sharp double beat (lub-dub) under the jaw.
	float ph = fract(t * pulse_hz);
	float beat = exp(-pow((ph - 0.08) * 22.0, 2.0)) + 0.55 * exp(-pow((ph - 0.25) * 22.0, 2.0));
	VERTEX += NORMAL * pulse_amp * beat * CUSTOM0.w;
	// Breath: the collar and the cords of the neck rise and tighten.
	float collar = smoothstep(0.56, 0.47, v_rest.y);
	float live = 1.0 - stillness;
	VERTEX.y += breath * 0.005 * collar * live;
	// Tremor: lips and chin, smooth noise at 8-12 Hz.
	float mouth = exp(-pow(length((v_rest - vec3(0.0, 0.60, 0.165)) * vec3(1.0, 1.4, 1.0)) / 0.035, 2.0));
	float tr = noise1(t * tremor_hz) * 0.7 + noise1(t * tremor_hz * 1.7 + 13.0) * 0.3;
	VERTEX.x += tremor_amp * tr * mouth * live;
	VERTEX.y += tremor_amp * 0.6 * noise1(t * tremor_hz * 1.3 + 5.0) * mouth * live;
	// The burn climbs the neck from the left breast pocket as exposure rises.
	// (local and slow: at full exposure it reaches the left jaw line, never the whole face)
	float dp = distance(v_rest * vec3(1.0, 0.8, 1.0), vec3(0.085, 0.40 * 0.8, 0.10));
	v_burn = smoothstep(0.03 + exposure * 0.15, exposure * 0.09, dp) * step(0.02, exposure);
}

void fragment() {
	vec3 alb = texture(albedo_tex, UV).rgb * tint;
	// exposure drains the blood out of all of her: greyer, not redder
	float lum = dot(alb, vec3(0.30, 0.59, 0.11));
	alb = mix(alb, lum * vec3(0.93, 0.92, 0.90), exposure * 0.55);
	// the burn: dusky erythema, then a wet weeping ring, then grey-violet dead skin
	float b = v_burn;
	alb = mix(alb, alb * vec3(0.92, 0.70, 0.74), smoothstep(0.0, 0.35, b));
	alb = mix(alb, alb * vec3(1.02, 0.93, 0.80), smoothstep(0.45, 0.7, b) * 0.5);
	alb = mix(alb, vec3(0.30, 0.27, 0.29), smoothstep(0.75, 0.95, b) * 0.8);
	ALBEDO = alb;
	v_geo_n = NORMAL;
#if QUALITY >= 1
	NORMAL_MAP = texture(pores_n, UV * pore_tiling).rgb;
	NORMAL_MAP_DEPTH = pore_depth * (1.0 - 0.7 * smoothstep(0.6, 0.9, b));
#endif
	ROUGHNESS = mix(rough_a, 0.16, smoothstep(0.45, 0.7, b) * (1.0 - smoothstep(0.8, 0.95, b)));
	AO = v_ao;
	AO_LIGHT_AFFECT = 0.5;
}

float d_ggx(float nh, float r) {
	float a = r * r;
	float a2 = a * a;
	float d = nh * nh * (a2 - 1.0) + 1.0;
	return a2 / (PI * d * d);
}

void light() {
	float ndl = (dot(NORMAL, LIGHT) + light_wrap) / (1.0 + light_wrap);
	float c = clamp(v_curv, 0.03, 0.97);
#if QUALITY >= 2
	// three taps: red reads the blurred (geometric) normal, blue the detailed one
	float ndl_g = (dot(normalize(v_geo_n), LIGHT) + light_wrap) / (1.0 + light_wrap);
	vec3 d;
	d.r = texture(skin_lut, vec2(ndl_g * 0.5 + 0.5, c)).r;
	d.g = texture(skin_lut, vec2(mix(ndl_g, ndl, 0.6) * 0.5 + 0.5, c)).g;
	d.b = texture(skin_lut, vec2(ndl * 0.5 + 0.5, c)).b;
#elif QUALITY == 1
	vec3 d = texture(skin_lut, vec2(ndl * 0.5 + 0.5, c)).rgb;
#else
	vec3 d = vec3(clamp((ndl + 0.2) / 1.2, 0.0, 1.0));
#endif
	DIFFUSE_LIGHT += d * ATTENUATION * LIGHT_COLOR / PI;
#if QUALITY >= 2
	// light through thin parts: ears, nostril wings, eyelids
	float thin = clamp(1.0 - v_thick, 0.0, 1.0);
	float tr = pow(clamp(dot(VIEW, -LIGHT) * 0.5 + 0.5, 0.0, 1.0), 4.0) * thin * thin;
	DIFFUSE_LIGHT += scatter_col * tr * trans_strength * ATTENUATION * LIGHT_COLOR / PI;
#endif
	vec3 h = normalize(VIEW + LIGHT);
	float nh = clamp(dot(NORMAL, h), 0.0, 1.0);
	float nl = clamp(ndl, 0.0, 1.0);
	float nv = clamp(dot(NORMAL, VIEW), 0.001, 1.0);
	float f = 0.028 + 0.972 * pow(1.0 - clamp(dot(h, VIEW), 0.0, 1.0), 5.0);
#if QUALITY >= 2
	float dd = mix(d_ggx(nh, rough_b), d_ggx(nh, rough_a), lobe_mix);
#else
	float dd = d_ggx(nh, rough_a);
#endif
	float k = 0.25;
	float vis = 0.25 / ((nl * (1.0 - k) + k) * (nv * (1.0 - k) + k));
	SPECULAR_LIGHT += dd * f * vis * nl * spec_scale * v_ao * ATTENUATION * LIGHT_COLOR;
}
