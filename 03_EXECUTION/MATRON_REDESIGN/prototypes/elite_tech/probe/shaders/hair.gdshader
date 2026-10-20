shader_type spatial;
//@QUALITY@
#ifndef HAIR_MODE
#define HAIR_MODE 1
#endif
// HAIR_MODE 0 alpha scissor, 1 hashed alpha written by hand (Wyman & McGuire
// 2017, object-space, pixel-scaled cells; the built-in ALPHA_HASH_SCALE renders
// opaque in gl_compatibility), 2 alpha to coverage (needs MSAA), 3 blended
// with a depth pre-pass.
#if HAIR_MODE == 3
render_mode cull_disabled, depth_prepass_alpha;
#else
render_mode cull_disabled;
#endif
#ifndef KK
#define KK 1
#endif
uniform sampler2D hair_tex : source_color, filter_linear_mipmap_anisotropic, repeat_disable;
uniform vec3 tint : source_color = vec3(0.78, 0.76, 0.72);
uniform float thin = 1.0;            // alpha scale: hair loss
uniform float shift1 = -0.08;
uniform float shift2 = 0.12;
uniform float exp1 = 140.0;
uniform float exp2 = 22.0;
uniform vec3 spec1 : source_color = vec3(0.9, 0.9, 0.86);
uniform float spec_scale = 0.18;
uniform float hash_scale = 1.0;
varying vec3 v_t;
varying vec3 v_obj;

float hash13(vec3 p) {
	p = fract(p * 0.1031);
	p += dot(p, p.zyx + 31.32);
	return fract((p.x + p.y) * p.z);
}
float hashed_alpha_threshold(vec3 obj) {
	// Wyman & McGuire, "Hashed Alpha Testing" (2017), the isotropic version
	float max_deriv = max(length(dFdx(obj)), length(dFdy(obj)));
	float pix = 1.0 / max(hash_scale * max_deriv, 1e-6);
	vec2 lp = vec2(floor(log2(pix)), fract(log2(pix)));
	vec2 sc = vec2(exp2(lp.x), exp2(lp.x + 1.0));
	float a = hash13(floor(sc.x * obj));
	float b = hash13(floor(sc.y * obj));
	return clamp(mix(a, b, lp.y), 0.01, 0.99);
}

void vertex() { v_obj = VERTEX; }

void fragment() {
	vec4 c = texture(hair_tex, UV);
	float lum = dot(c.rgb, vec3(0.3, 0.59, 0.11));
	// the CC0 texture is brown; take its value only and grey it
	ALBEDO = tint * (0.35 + 0.9 * lum);
	// hair loss: whole strands (columns of the card texture) drop out below thin
	float strand = fract(sin(floor(UV.x * 260.0) * 12.9898) * 43758.5453);
	float a = c.a * step(strand, thin);
	ALPHA = a;
#if HAIR_MODE == 0
	ALPHA_SCISSOR_THRESHOLD = 0.5;
#elif HAIR_MODE == 1
	if (a < hashed_alpha_threshold(v_obj)) { discard; }
#elif HAIR_MODE == 2
	ALPHA_SCISSOR_THRESHOLD = 0.5;
	ALPHA_ANTIALIASING_EDGE = 0.3;
	ALPHA_TEXTURE_COORDINATE = UV * vec2(textureSize(hair_tex, 0));
#endif
	v_t = normalize(TANGENT);
	ROUGHNESS = 0.6;
	BACKLIGHT = vec3(0.35, 0.33, 0.3);
}

void light() {
#if KK == 1
	vec3 T = normalize(v_t);
	vec3 H = normalize(LIGHT + VIEW);
	float tl = dot(T, LIGHT);
	// Kajiya-Kay diffuse, wrapped with N.L so the cards do not glow flat
	float kd = sqrt(max(1.0 - tl * tl, 0.0));
	float wrapd = clamp((dot(NORMAL, LIGHT) + 0.5) / 1.5, 0.0, 1.0);
	DIFFUSE_LIGHT += mix(wrapd, kd, 0.5) * ATTENUATION * LIGHT_COLOR / PI;
	// two shifted lobes (Scheuermann): a white primary, a tinted secondary
	vec3 t1 = normalize(T + NORMAL * shift1);
	vec3 t2 = normalize(T + NORMAL * shift2);
	float th1 = dot(t1, H);
	float th2 = dot(t2, H);
	float s1 = pow(sqrt(max(1.0 - th1 * th1, 0.0)), exp1);
	float s2 = pow(sqrt(max(1.0 - th2 * th2, 0.0)), exp2);
	float vis = clamp(dot(NORMAL, LIGHT) + 0.6, 0.0, 1.0);
	SPECULAR_LIGHT += (spec1 * s1 + ALBEDO * s2 * 0.6) * spec_scale * vis * ATTENUATION * LIGHT_COLOR / PI;
#else
	DIFFUSE_LIGHT += clamp(dot(NORMAL, LIGHT), 0.0, 1.0) * ATTENUATION * LIGHT_COLOR / PI;
#endif
}
