shader_type spatial;
// An old woman's eye for the Compatibility renderer: refracted (parallax) iris under a wet
// cornea, an old person's small pupil, arcus senilis (the grey ring old corneas get), a
// yellowed sclera with veins and pinguecula, a milky cataract option, a bleed option.
uniform float eye_radius = 0.0146;
uniform float iris_angle = 0.50;          // half-angle of the limbus (11.5 mm iris on a 24 mm eye)
uniform float pupil_size = 0.27;          // fraction of the iris radius (old eyes: small pupils)
uniform float cloud : hint_range(0.0, 1.0) = 0.0;
uniform float arcus : hint_range(0.0, 1.0) = 0.75;
uniform float vein_amt = 0.5;
uniform float bleed : hint_range(0.0, 1.0) = 0.0;
uniform float seed = 1.0;
uniform int eye_debug = 0;
uniform vec3 iris_outer : source_color = vec3(0.47, 0.53, 0.56);
uniform vec3 iris_inner : source_color = vec3(0.47, 0.44, 0.37);
uniform vec3 sclera_col : source_color = vec3(0.82, 0.79, 0.73);
uniform vec3 env_sky = vec3(0.0);      // linear, HDR: an overcast sky is several times brighter than the face
uniform vec3 env_ground = vec3(0.0);
uniform vec3 env_band = vec3(0.0);     // the skyline: cranes, the core, the rest of the deck
varying vec3 o_pos;
varying vec3 o_cam;
varying float v_rough;
varying float v_sclera;

float h21(vec2 p) { p = fract(p * vec2(123.34, 456.21) + seed); p += dot(p, p + 45.32); return fract(p.x * p.y); }
float vn(vec2 p) {
	vec2 i = floor(p); vec2 f = fract(p); f = f * f * (3.0 - 2.0 * f);
	return mix(mix(h21(i), h21(i + vec2(1, 0)), f.x), mix(h21(i + vec2(0, 1)), h21(i + vec2(1, 1)), f.x), f.y);
}
float fbm2(vec2 p) { float s = 0.0; float a = 0.5; for (int i = 0; i < 4; i++) { s += a * vn(p); p *= 2.03; a *= 0.5; } return s; }

void vertex() {
	o_pos = VERTEX;
	o_cam = (inverse(MODEL_MATRIX) * vec4(INV_VIEW_MATRIX[3].xyz, 1.0)).xyz;
}

void fragment() {
	float R = eye_radius;
	vec3 dp = normalize(o_pos);
	float th = acos(clamp(dp.z, -1.0, 1.0));
	float ph = atan(dp.y, dp.x);
	// ---- iris through the cornea: refract the view ray, hit the iris plane ----
	vec3 rd = normalize(o_pos - o_cam);
	vec3 rr = refract(rd, dp, 1.0 / 1.376);
	float z_iris = R * cos(iris_angle) - 0.0004;
	float t = (z_iris - o_pos.z) / min(rr.z, -0.0001);
	vec3 hit = o_pos + rr * t;
	vec2 ip = hit.xy / (R * sin(iris_angle));
	float rho = length(ip);
	float ang = atan(ip.y, ip.x);
	// fibres: radial streaks, a jagged collarette, crypts, a dark limbal ring
	float fib = fbm2(vec2(ang * 9.0, rho * 2.5)) * 0.6 + vn(vec2(ang * 38.0, rho * 4.0)) * 0.4;
	float coll_r = 0.47 + 0.05 * (vn(vec2(ang * 7.0, 1.0)) - 0.5);
	float inner = 1.0 - smoothstep(coll_r - 0.03, coll_r + 0.03, rho);
	vec3 iris = mix(iris_outer, iris_inner, inner * 0.85);
	iris *= 0.72 + 0.55 * fib;
	iris = mix(iris, iris * 1.25, smoothstep(0.03, 0.0, abs(rho - coll_r)) * 0.6);
	float crypt = smoothstep(0.68, 0.8, fbm2(vec2(ang * 5.0, rho * 6.0) + 3.0)) * smoothstep(0.5, 0.6, rho) * (1.0 - smoothstep(0.85, 0.9, rho));
	iris *= 1.0 - 0.55 * crypt;
	iris *= mix(1.0, 0.35, smoothstep(0.78, 1.0, rho));       // limbal ring
	float pr = pupil_size * (1.0 + 0.04 * (vn(vec2(ang * 6.0, 0.0)) - 0.5));
	float pupil = 1.0 - smoothstep(pr - 0.015, pr + 0.01, rho);
	vec3 milky = vec3(0.58, 0.60, 0.60);
	vec3 pupil_col = mix(vec3(0.004), milky * 0.85, cloud);
	vec3 front = mix(iris, pupil_col, pupil);
	front = mix(front, iris * vec3(0.55, 0.4, 0.3), smoothstep(pr + 0.04, pr, rho) * (1.0 - pupil) * 0.6);   // pupillary ruff
	front = mix(front, mix(front, milky, 0.55), cloud * (1.0 - smoothstep(0.6, 1.0, rho)));             // cataract haze
	// ---- sclera ----
	float s = smoothstep(iris_angle - 0.02, iris_angle + 0.035, th);
	vec3 scl = sclera_col;
	scl *= 0.92 + 0.1 * fbm2(vec2(ph * 3.0, th * 6.0) + 7.0);
	float periph = smoothstep(iris_angle + 0.05, iris_angle + 0.7, th);
	float v1 = 1.0 - abs(sin(ph * 7.0 + fbm2(vec2(th * 5.0, ph * 2.0)) * 5.0));
	float v2 = 1.0 - abs(sin(ph * 17.0 + fbm2(vec2(th * 9.0, ph * 4.0) + 2.0) * 7.0));
	float veins = (smoothstep(0.965, 0.995, v1) + 0.6 * smoothstep(0.975, 0.997, v2)) * periph;
	veins *= smoothstep(0.35, 0.6, fbm2(vec2(ph * 2.0, th * 3.0) + 11.0));
	scl = mix(scl, vec3(0.62, 0.16, 0.13), clamp(veins * vein_amt, 0.0, 0.9));
	scl = mix(scl, scl * vec3(0.95, 0.75, 0.72), periph * vein_amt * 0.5);                   // general pink flush
	float ping = exp(-pow((th - iris_angle - 0.14) / 0.08, 2.0)) * (exp(-pow(ph / 0.35, 2.0)) + exp(-pow((abs(ph) - PI) / 0.35, 2.0)));
	scl = mix(scl, vec3(0.78, 0.70, 0.45), ping * 0.45);
	float bl = smoothstep(0.45, 0.6, fbm2(vec2(ph * 1.5, th * 2.5) + 21.0)) * smoothstep(iris_angle + 0.04, iris_angle + 0.12, th)
		* exp(-pow((ph - 0.3) / 0.9, 2.0));
	scl = mix(scl, vec3(0.45, 0.03, 0.03), bl * bleed);
	vec3 col = mix(front, scl, s);
	col = mix(col, vec3(0.32, 0.33, 0.33), smoothstep(0.05, 0.0, abs(th - iris_angle)) * 0.35);   // limbus blur
	// arcus senilis: a grey-white ring just inside the limbus, a clear band at the very edge
	float ar = smoothstep(iris_angle * 0.82, iris_angle * 0.9, th) * (1.0 - smoothstep(iris_angle * 0.95, iris_angle * 0.98, th));
	col = mix(col, vec3(0.78, 0.78, 0.74), ar * arcus * 0.6);
	ALBEDO = col;
	v_sclera = s;
	v_rough = mix(0.11, 0.16, s);   // a real bulb is not a point: widened so the catchlight survives at phone pixel size
	ROUGHNESS = v_rough;
	SPECULAR = 0.5;
	vec3 rv = reflect(-VIEW, NORMAL);
	vec3 rw = (INV_VIEW_MATRIX * vec4(rv, 0.0)).xyz;
	float fres = 0.04 + 0.96 * pow(1.0 - clamp(dot(NORMAL, VIEW), 0.0, 1.0), 5.0);
	// what the wet eye mirrors: sky above a dark skyline band, the deck below
	float el = rw.y + 0.04 * sin(rw.x * 23.0) * step(0.0, rw.y);
	// 154 m up, the deck is only near and below: just under the horizon the eye still mirrors the
	// hazy city and sky (env_band), the deck only for steep downward directions
	vec3 envc = mix(env_ground, env_band, smoothstep(-0.45, -0.30, el));
	envc = mix(envc, env_sky, smoothstep(-0.10, 0.02, el + 0.03 * sin(rw.x * 41.0)));
	// the upper lid and lashes shade the top of the eyeball from the sky
	float lid = 1.0 - smoothstep(0.40, 0.62, dp.y);
	float on_cornea = 1.0 - s;
	EMISSION = envc * fres * lid * mix(0.35, 1.0, on_cornea) * 0.0;   // moved to cornea.gdshader (drawn after the lid occlusion)
	vec3 nw = (INV_VIEW_MATRIX * vec4(NORMAL, 0.0)).xyz;
	vec3 vw = (INV_VIEW_MATRIX * vec4(VIEW, 0.0)).xyz;
	if (eye_debug == 2) { ALBEDO = vec3(0.0); EMISSION = nw * 0.5 + 0.5; }
	if (eye_debug == 3) { ALBEDO = vec3(0.0); EMISSION = vw * 0.5 + 0.5; }
	if (eye_debug == 4) { ALBEDO = vec3(0.0); EMISSION = el > 0.0 ? vec3(0.0, 1.0, 0.0) : (el > -0.3 ? vec3(1.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0)); EMISSION *= 0.3 + 0.7 * lid; if (fres > 0.2) EMISSION = vec3(0.0, 0.0, 1.0); }
	if (eye_debug == 1) { ALBEDO = vec3(0.0); EMISSION = vec3(clamp(rw.y * 3.0 + 0.5, 0.0, 1.0), fres * 5.0, lid); }
}

void light() {
	float ndl = dot(NORMAL, LIGHT);
	float wrap = mix(0.0, 0.25, v_sclera);
	DIFFUSE_LIGHT += clamp((ndl + wrap) / (1.0 + wrap), 0.0, 1.0) * ATTENUATION * LIGHT_COLOR / PI;
	vec3 h = normalize(VIEW + LIGHT);
	float a = v_rough * v_rough; float a2 = a * a;
	float ndh = clamp(dot(NORMAL, h), 0.0, 1.0);
	float d = ndh * ndh * (a2 - 1.0) + 1.0;
	float D = a2 / (PI * d * d);
	float F = 0.04 + 0.96 * pow(1.0 - clamp(dot(h, VIEW), 0.0, 1.0), 5.0);
	float nl = clamp(ndl, 0.0, 1.0); float nv = clamp(dot(NORMAL, VIEW), 0.001, 1.0);
	SPECULAR_LIGHT += vec3(min(D * F * 0.25 / max(nv, 0.2), 60.0)) * nl * ATTENUATION * LIGHT_COLOR;
}
