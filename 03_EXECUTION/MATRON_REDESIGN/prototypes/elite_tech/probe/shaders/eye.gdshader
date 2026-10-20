shader_type spatial;
render_mode cull_back;
// Wet eye for gl_compatibility (probe): refracted (parallax) iris under a
// bulged cornea, limbal ring, veined yellowed sclera, a pupil size uniform,
// a fake environment reflection, and a cloud (cataract) layer.
// The eyeball's local +Z is the gaze.
uniform float eye_r = 0.0159;
uniform float iris_r = 0.0074;
uniform float pupil = 0.32;          // pupil radius / iris radius
uniform float cornea_h = 0.0026;     // cornea apex height above the iris plane
uniform vec3 iris_col : source_color = vec3(0.40, 0.43, 0.40);
uniform vec3 iris_col2 : source_color = vec3(0.55, 0.47, 0.30);
uniform vec3 sclera_col : source_color = vec3(0.84, 0.78, 0.66);
uniform float bloodshot : hint_range(0.0, 1.0) = 0.25;
uniform float milky : hint_range(0.0, 1.0) = 0.0;
uniform float refraction = 1.0;      // 0 = flat painted iris (for comparison)
uniform float env_strength = 0.35;
uniform float lid_shadow = 0.6;

varying vec3 o_pos;
varying vec3 o_cam;

float h21(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float n21(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(h21(i), h21(i + vec2(1, 0)), f.x), mix(h21(i + vec2(0, 1)), h21(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) {
	float s = 0.0;
	float a = 0.5;
	for (int k = 0; k < 4; k++) { s += a * n21(p); p *= 2.03; a *= 0.5; }
	return s;
}

void vertex() {
	o_pos = VERTEX;
	o_cam = (inverse(MODEL_MATRIX) * vec4(INV_VIEW_MATRIX[3].xyz, 1.0)).xyz;
}

void fragment() {
	vec3 p = o_pos;
	vec3 V = normalize(o_cam - p);
	float rr = length(p.xy);
	float z_iris = sqrt(max(eye_r * eye_r - iris_r * iris_r, 0.0));
	float cornea = smoothstep(iris_r * 1.12, iris_r * 0.98, rr) * step(0.0, p.z);
	// cornea bulge: a smaller sphere for the normal (sharper highlight)
	vec3 n_obj = normalize(p);
	vec3 n_cor = normalize(p - vec3(0.0, 0.0, z_iris + cornea_h - 0.0078));
	n_obj = normalize(mix(n_obj, n_cor, cornea));
	// refraction through the cornea onto the iris plane
	float height = max(p.z - z_iris, 0.0) + cornea_h * clamp(1.0 - rr * rr / (iris_r * iris_r), 0.0, 1.0);
	vec3 r = refract(-V, n_cor, 1.0 / 1.336);
	vec2 ip = p.xy + refraction * r.xy / max(-r.z, 0.25) * height;
	float ir = length(ip) / iris_r;
	float ang = atan(ip.y, ip.x);
	// iris: radial fibres, a collarette, crypts, a dark limbal ring
	float fib = fbm(vec2(ang * 9.0, ir * 3.0)) * 0.6 + fbm(vec2(ang * 30.0, ir * 1.5)) * 0.4;
	vec3 ic = mix(iris_col * 0.55, iris_col, fib);
	ic = mix(ic, iris_col2, smoothstep(0.55, 0.38, ir) * 0.7 * fib);
	ic *= mix(1.0, 0.35, smoothstep(0.78, 1.0, ir));
	float pup = smoothstep(pupil + 0.03, pupil - 0.01, ir);
	ic = mix(ic, vec3(0.006), pup);
	// sclera: yellowed toward the corners, veins, redness
	vec2 sp = p.xy / eye_r;
	float veins = smoothstep(0.62, 0.70, fbm(sp * 9.0 + vec2(3.1, 7.7))) * smoothstep(0.25, 0.85, length(sp));
	vec3 sc = sclera_col * mix(1.0, 0.86, smoothstep(0.3, 0.95, length(sp)));
	sc = mix(sc, vec3(0.62, 0.18, 0.14), veins * (0.35 + bloodshot));
	sc = mix(sc, sc * vec3(1.0, 0.82, 0.78), bloodshot * smoothstep(0.4, 1.0, length(sp)));
	float in_iris = smoothstep(1.03, 0.99, length(p.xy + refraction * r.xy / max(-r.z, 0.25) * height) / iris_r) * step(0.0, p.z);
	vec3 col = mix(sc, ic, in_iris);
	// a milky cloud over the cornea
	col = mix(col, vec3(0.70, 0.72, 0.70), milky * cornea * 0.75);
	// lids shade the top of the ball
	float lid = mix(1.0, 1.0 - lid_shadow, smoothstep(-0.2, 0.75, p.y / eye_r));
	ALBEDO = col * lid;
	NORMAL = normalize((VIEW_MATRIX * MODEL_MATRIX * vec4(n_obj, 0.0)).xyz);
	ROUGHNESS = mix(0.16, 0.035, cornea);
	SPECULAR = 0.5;
	// fake environment reflection: the bright open end of the locker
	vec3 refl = reflect(-V, n_obj);
	float win = smoothstep(0.55, 0.85, refl.z) * smoothstep(-0.3, 0.2, refl.y) * smoothstep(0.6, 0.2, abs(refl.x));
	float fres = 0.04 + 0.96 * pow(1.0 - clamp(dot(n_obj, V), 0.0, 1.0), 5.0);
	EMISSION = vec3(0.55, 0.6, 0.68) * win * env_strength * (0.25 + fres) * lid * (1.0 - milky * 0.6);
}
