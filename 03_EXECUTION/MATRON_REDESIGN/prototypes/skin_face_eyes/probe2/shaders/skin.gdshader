shader_type spatial;
// The Matron's skin, for Godot's Compatibility renderer (no SSS/SSAO/SSR).
//  - pre-integrated diffusion LUT by curvature (Penner 2011, d'Eon profile), sampled per colour
//    channel with three normals (red softest), so light bleeds red only at the terminator
//  - dual-lobe GGX specular (sharp oily + broad), cavity-occluded
//  - translucency through thin parts (ears, nostril rims, lids) from baked thickness
//  - vertex colour: R ambient occlusion, G curvature row, B thinness
//  - radiation exposure as a local story driven by a baked dose field from the left breast pocket
uniform sampler2D albedo_tex : source_color, filter_linear_mipmap_anisotropic, repeat_disable;
uniform sampler2D normal_tex : filter_linear_mipmap_anisotropic, repeat_disable;
uniform sampler2D maskA_tex : filter_linear_mipmap, repeat_disable;   // R dose, G veins, B oil, A cavity
uniform sampler2D maskB_tex : filter_linear_mipmap, repeat_disable;   // R flakes, G petechiae, B wet, A hair
uniform sampler2D skin_lut : filter_linear, repeat_disable;
uniform float normal_strength = 1.2;
uniform float exposure : hint_range(0.0, 1.0) = 0.0;
uniform float wrongness : hint_range(0.0, 1.0) = 0.0;
uniform float translucency = 1.0;
uniform vec3 scatter_tint = vec3(1.0, 0.28, 0.16);
uniform float red_wrap = 0.04;
uniform float spec_scale = 1.0;
uniform float detail_r = 0.35;   // old thin skin scatters less: the red channel keeps more of the wrinkles
uniform float detail_g = 0.7;
uniform float lobe_mix = 0.85;   // weight of the sharp lobe
uniform float flat_mode = 0.0;      // 1: no maps (for A/B renders)
uniform int debug_view = 0;
uniform float ao_min = 0.55;
uniform float cav_diffuse = 0.0;   // micro-occlusion: creases and pores darken the diffuse a little too
uniform float lut_mix = 1.0;
uniform float wrap_all = 0.0;      // a broad source (the overcast sky patch) spreads the terminator
uniform float lut_sat = 0.6;       // old, thin, pale skin: less red bleed than the young-face profile
uniform float amb_spec = 1.0;

varying vec3 n_r;
varying vec3 n_g;
varying float v_curv;
varying float v_thin;
varying float v_rough;
varying float v_cav;
varying float v_wet;

vec3 lin(vec3 c) { return c * c; }

void fragment() {
	vec4 vc = COLOR;
	vec3 alb = texture(albedo_tex, UV).rgb;
	vec4 ma = texture(maskA_tex, UV);
	vec4 mb = texture(maskB_tex, UV);
	vec3 nt = texture(normal_tex, UV).xyz * 2.0 - 1.0;
	float nstr = normal_strength;
	float rough = mix(0.60, 0.42, ma.b);            // dry old skin; oilier T-zone
	rough = mix(rough, 0.22, mb.b);                 // wet lid rims, lip inner edge
	float cav = mix(0.35, 1.0, smoothstep(0.25, 0.6, ma.a));
	float spec = 0.25;   // ambient/sky reflection strength (F0 ~0.02); direct specular is in light()
	// ---------------- the source's work: a burn that climbs from the pocket ----------------
	float eff = ma.r + 1.05 * (exposure - 1.0);     // dose so far: the contour sweeps outward with time
	float front_noise = (mb.r - 0.5) * 0.08;
	float ery = smoothstep(0.30, 0.42, eff + front_noise);
	float dry = smoothstep(0.50, 0.56, eff + front_noise * 1.5);
	float moist = smoothstep(0.64, 0.70, eff + front_noise);
	float margin = ery * (1.0 - smoothstep(0.42, 0.52, eff));
	alb *= mix(vec3(1.0), vec3(0.98, 0.62, 0.66), ery);                 // pink-red, not orange
	alb *= mix(vec3(1.0), vec3(0.86, 0.74, 0.70), margin * 0.8);        // dusky, hyperpigmented edge
	float scale = dry * (1.0 - moist) * smoothstep(0.45, 0.75, mb.r);
	alb = mix(alb, vec3(0.80, 0.74, 0.68), scale * 0.55);                // dry white scale
	rough = mix(rough, 0.95, scale);
	nstr += scale * 2.0;
	alb = mix(alb, vec3(0.62, 0.36, 0.30), moist * 0.75);               // raw, weeping
	alb = mix(alb, vec3(0.72, 0.62, 0.40), moist * smoothstep(0.6, 0.9, mb.r) * 0.35); // serous crust
	rough = mix(rough, 0.16, moist);
	spec = mix(spec, 0.55, moist);
	float pet = mb.g * smoothstep(0.45, 0.65, exposure) * (0.4 + 0.6 * smoothstep(0.1, 0.4, ma.r));
	alb = mix(alb, vec3(0.30, 0.04, 0.05), pet * 0.85);
	float vein = ma.g * (0.25 + 0.75 * smoothstep(0.25, 0.8, exposure));
	alb *= mix(vec3(1.0), vec3(0.80, 0.78, 0.92), vein * 0.4);
	// ---------------- wrongness: waxy, clammy, a shade too grey ----------------
	float lum = dot(alb, vec3(0.3, 0.59, 0.11));
	alb = mix(alb, vec3(lum) * vec3(0.96, 0.98, 1.02), 0.22 * wrongness);
	rough -= 0.1 * wrongness;
	if (flat_mode > 0.5) { alb = vec3(0.62, 0.48, 0.40); nstr = 0.0; cav = 1.0; rough = 0.5; }
	nt.xy *= nstr;
	vec3 ng = normalize(NORMAL);
	vec3 nd = normalize(TANGENT * nt.x + BINORMAL * nt.y + ng * max(nt.z, 0.05));
	NORMAL = nd;
	n_r = normalize(mix(ng, nd, detail_r));
	n_g = normalize(mix(ng, nd, detail_g));
	v_curv = vc.g;
	v_thin = vc.b * (1.0 + 0.6 * wrongness);
	v_rough = clamp(rough, 0.08, 1.0);
	v_cav = cav;
	v_wet = moist;
	ALBEDO = alb * mix(1.0 - cav_diffuse, 1.0, smoothstep(0.2, 0.55, ma.a));
	ROUGHNESS = v_rough;
	SPECULAR = spec * cav * amb_spec;
	AO = mix(ao_min, 1.0, vc.r);
	AO_LIGHT_AFFECT = 0.15;
	if (debug_view == 1) { ALBEDO = alb; }
	if (debug_view == 2) { ALBEDO = vec3(0.0); EMISSION = (nd * 0.5 + 0.5); }
	if (debug_view == 3) { ALBEDO = vec3(0.0); EMISSION = vec3(vc.r); }
	if (debug_view == 4) { ALBEDO = vec3(0.0); EMISSION = vec3(vc.g); }
	if (debug_view == 5) { ALBEDO = vec3(0.0); EMISSION = vec3(vc.b); }
	if (debug_view == 7) { ALBEDO = vec3(0.0); EMISSION = alb; }
	if (debug_view == 6) { ALBEDO = vec3(0.0); EMISSION = vec3(ma.r, ery, moist); }
}

float D_ggx(float ndh, float a) {
	float a2 = a * a; float d = ndh * ndh * (a2 - 1.0) + 1.0;
	return a2 / (PI * d * d);
}

void light() {
	float ndl_b = (dot(NORMAL, LIGHT) + wrap_all) / (1.0 + wrap_all);
	float ndl_g = (dot(n_g, LIGHT) + wrap_all) / (1.0 + wrap_all);
	float ndl_r = (dot(n_r, LIGHT) + wrap_all) / (1.0 + wrap_all);
	float row = 1.0 - v_curv;
	vec3 d;
	d.r = texture(skin_lut, vec2(clamp((ndl_r + red_wrap) / (1.0 + red_wrap), -1.0, 1.0) * 0.5 + 0.5, row)).r;
	d.g = texture(skin_lut, vec2(ndl_g * 0.5 + 0.5, row)).g;
	d.b = texture(skin_lut, vec2(ndl_b * 0.5 + 0.5, row)).b;
	d *= d;    // LUT is sqrt-encoded
	float dl = dot(d, vec3(0.3, 0.55, 0.15));
	d = mix(vec3(dl), d, lut_sat);
	d = mix(vec3(clamp(ndl_b, 0.0, 1.0)), d, lut_mix);
	DIFFUSE_LIGHT += d * ATTENUATION * LIGHT_COLOR / PI;
	// light through thin flesh, seen from the far side
	vec3 ht = normalize(LIGHT + NORMAL * 0.35);
	float through = pow(clamp(dot(VIEW, -ht), 0.0, 1.0), 3.0) + 0.25 * clamp(-ndl_g, 0.0, 1.0);
	DIFFUSE_LIGHT += scatter_tint * through * v_thin * v_thin * translucency * ATTENUATION * LIGHT_COLOR / PI;
	// dual-lobe specular, Schlick fresnel F0 0.028 (skin)
	vec3 h = normalize(VIEW + LIGHT);
	float ndh = clamp(dot(NORMAL, h), 0.0, 1.0);
	float ndl = clamp(ndl_b, 0.0, 1.0);
	float ndv = clamp(dot(NORMAL, VIEW), 0.001, 1.0);
	float a1 = max(v_rough * 0.62, 0.12); a1 *= a1;      // sharp lobe (sebum film)
	float a2 = min(1.0, v_rough * 1.05); a2 *= a2;      // broad lobe (the stratum corneum)
	float D = mix(D_ggx(ndh, a2), D_ggx(ndh, a1), lobe_mix);
	float k = (v_rough + 1.0) * (v_rough + 1.0) / 8.0;
	float G = (ndl / (ndl * (1.0 - k) + k)) * (ndv / (ndv * (1.0 - k) + k));
	float F = 0.028 + (1.0 - 0.028) * pow(1.0 - clamp(dot(h, VIEW), 0.0, 1.0), 5.0);
	F = mix(F, 0.05, v_wet);
	float s = D * G * F / max(4.0 * ndl * ndv, 0.001) * ndl;
	SPECULAR_LIGHT += vec3(s) * v_cav * spec_scale * ATTENUATION * LIGHT_COLOR;
}
