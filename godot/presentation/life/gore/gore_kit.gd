extends RefCounted
## Gore materials and procedural wound geometry, shared by every gore node.
## Everything is built once and cached: on Android the shaders compile once
## and the meshes are reused (a handful of variants, picked by seed).

const BLOOD := Color(0.36, 0.01, 0.015)
const BLOOD_BRIGHT := Color(0.62, 0.03, 0.03)
const MUSCLE := Color(0.5, 0.06, 0.06)
const FAT := Color(0.86, 0.72, 0.42)
const BONE := Color(0.88, 0.84, 0.74)

static var _cache := {}

# --- shaders -----------------------------------------------------------------

## The joint under strain: cloth fraying open over stretched, bruising skin
## that finally splits to the red beneath. strain 0..1 (toward the tear),
## split 0..1 (the last fifth: a gash opening along a ragged seam).
const STRAIN_SHADER := """
shader_type spatial;
render_mode cull_disabled;
uniform vec4 cloth : source_color = vec4(0.12, 0.14, 0.11, 1.0);
uniform vec4 skin : source_color = vec4(0.78, 0.6, 0.52, 1.0);
uniform float strain : hint_range(0.0, 1.0) = 0.0;
uniform float split : hint_range(0.0, 1.0) = 0.0;
uniform float stretch_m = 0.0;
uniform float seed = 0.0;
varying vec3 lp;
float h(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + seed) * 43758.5453); }
float n(vec2 p) {
	vec2 i = floor(p); vec2 f = fract(p); f = f * f * (3.0 - 2.0 * f);
	return mix(mix(h(i), h(i + vec2(1, 0)), f.x), mix(h(i + vec2(0, 1)), h(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) { return 0.5 * n(p) + 0.25 * n(p * 2.1) + 0.125 * n(p * 4.3) + 0.0625 * n(p * 8.7); }
void vertex() {
	lp = VERTEX;
	// y is along the limb (0 at the joint, + toward the hand/foot); the band
	// necks in and draws out as it is pulled.
	float t = clamp(VERTEX.y / 0.12 + 0.5, 0.0, 1.0);
	float neck = 1.0 - strain * 0.32 * sin(t * 3.14159);
	VERTEX.xz *= neck;
	VERTEX.y += stretch_m * t;
}
void fragment() {
	float ang = atan(lp.x, lp.z);
	vec2 uv = vec2(ang * 1.6, lp.y * 18.0);
	float f = fbm(uv * 2.0);
	// Cloth rips first: open where the noise is under the strain.
	float rip = smoothstep(strain * 1.15 - 0.05, strain * 1.15 + 0.05, f + 0.25);
	// Skin: blanched where stretched, then mottled purple-red bruising.
	float bruise = smoothstep(0.25, 0.9, strain) * smoothstep(0.3, 0.75, fbm(uv * 3.3 + 7.0));
	vec3 sk = mix(skin.rgb, vec3(0.86, 0.8, 0.76), strain * 0.35);
	sk = mix(sk, vec3(0.36, 0.08, 0.16), bruise * 0.85);
	// Petechiae: pin-point bleeds.
	sk = mix(sk, vec3(0.45, 0.02, 0.03), step(0.985 - strain * 0.03, h(floor(uv * 26.0))) * strain);
	// The split: a jagged seam round the joint that gapes with split.
	float seam = abs(lp.y + (fbm(vec2(ang * 3.0, 1.0)) - 0.5) * 0.05);
	float gape = split * 0.028;
	float in_gash = 1.0 - smoothstep(gape * 0.6, gape, seam);
	float lip = (1.0 - smoothstep(gape, gape + 0.012, seam)) * step(0.001, split);
	vec3 gash = mix(vec3(0.55, 0.05, 0.05), vec3(0.9, 0.78, 0.5), step(0.75, fbm(uv * 9.0)) * 0.6); // muscle, fat
	gash = mix(gash, vec3(0.2, 0.0, 0.0), smoothstep(0.2, 0.9, 1.0 - seam / max(gape, 1e-4)));
	vec3 c = mix(sk, cloth.rgb, rip);
	c = mix(c, vec3(0.42, 0.02, 0.02), lip * (1.0 - rip * 0.6));
	c = mix(c, gash, in_gash);
	ALBEDO = c;
	ROUGHNESS = mix(mix(0.5, 0.9, rip), 0.15, max(in_gash, lip));
	SPECULAR = mix(0.4, 0.9, in_gash);
}
"""

## Wet tissue: muscle fibres, fat, blood. Vertex colour r = blood wetness,
## g = fibre/fat mix; UV.y runs along the fibres.
const FLESH_SHADER := """
shader_type spatial;
render_mode cull_disabled;
uniform vec4 muscle : source_color = vec4(0.42, 0.045, 0.04, 1.0);
uniform vec4 fat : source_color = vec4(0.82, 0.66, 0.36, 1.0);
uniform vec4 blood : source_color = vec4(0.16, 0.0, 0.005, 1.0);
uniform float wet = 1.0;
float h(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
float n(vec2 p) {
	vec2 i = floor(p); vec2 f = fract(p); f = f * f * (3.0 - 2.0 * f);
	return mix(mix(h(i), h(i + vec2(1, 0)), f.x), mix(h(i + vec2(0, 1)), h(i + vec2(1, 1)), f.x), f.y);
}
void fragment() {
	// Fibres run along UV.y; fine striation over coarse bundles.
	float bundle = n(vec2(UV.x * 40.0, UV.y * 3.0));
	float fibre = 0.6 * n(vec2(UV.x * 160.0, UV.y * 9.0)) + 0.4 * bundle;
	vec3 m = muscle.rgb * (0.65 + 0.6 * fibre);
	m = mix(m, vec3(0.62, 0.12, 0.1), smoothstep(0.7, 0.95, fibre) * 0.4);   // paler fascia
	// Fat: lumpy globules, not stripes.
	float glob = smoothstep(0.35, 0.6, n(UV * vec2(70.0, 25.0)));
	vec3 f = fat.rgb * (0.75 + 0.3 * glob);
	f = mix(f, vec3(0.55, 0.2, 0.1), (1.0 - glob) * 0.35);
	m = mix(m, f, COLOR.g);
	// Blood pooled in the folds, clotting dark.
	float clot = clamp(COLOR.r * (0.6 + 0.6 * n(UV * 31.0)), 0.0, 1.0);
	m = mix(m, blood.rgb, clot * 0.85);
	ALBEDO = m;
	float wetness = wet * (0.4 + 0.6 * n(UV * vec2(23.0, 11.0)));
	ROUGHNESS = mix(0.62, 0.2, wetness);
	SPECULAR = 0.45;
	BACKLIGHT = vec3(0.35, 0.03, 0.02);   // light through thin tissue
	RIM = 0.25;
	RIM_TINT = 0.8;
}
"""

const BONE_SHADER := """
shader_type spatial;
uniform vec4 bone : source_color = vec4(0.84, 0.79, 0.68, 1.0);
float h(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
float n(vec2 p) {
	vec2 i = floor(p); vec2 f = fract(p); f = f * f * (3.0 - 2.0 * f);
	return mix(mix(h(i), h(i + vec2(1, 0)), f.x), mix(h(i + vec2(0, 1)), h(i + vec2(1, 1)), f.x), f.y);
}
void fragment() {
	float grain = n(UV * vec2(8.0, 60.0));
	vec3 c = bone.rgb * (0.82 + 0.2 * grain);
	c = mix(c, vec3(0.72, 0.52, 0.42), 0.25 * n(UV * 13.0));    // periosteum, pinkish
	// Marrow: dark red, spongy.
	c = mix(c, vec3(0.28, 0.03, 0.03) * (0.7 + 0.5 * h(floor(UV * 90.0))), COLOR.g);
	// Blood smeared up from the meat.
	c = mix(c, vec3(0.3, 0.015, 0.015), clamp(COLOR.r * (0.6 + 0.6 * n(UV * 20.0)), 0.0, 1.0));
	ALBEDO = c;
	ROUGHNESS = mix(0.5, 0.22, max(COLOR.r, COLOR.g));
	SPECULAR = 0.4;
}
"""

## A blood splat on a quad: a noisy core, tendrils and satellite droplets,
## wet and dark at the middle. age darkens and dries it.
const SPLAT_SHADER := """
shader_type spatial;
render_mode cull_disabled, depth_draw_never, blend_mix;
uniform float seed = 0.0;
uniform float age = 0.0;
uniform float smear = 0.0;   // 0 a thrown splat (travelling +x), 1 a drag smear along +x
uniform float alpha_scale = 1.0;
float h(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + seed * 17.13) * 43758.5453); }
float n(vec2 p) {
	vec2 i = floor(p); vec2 f = fract(p); f = f * f * (3.0 - 2.0 * f);
	return mix(mix(h(i), h(i + vec2(1, 0)), f.x), mix(h(i + vec2(0, 1)), h(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) { return 0.5 * n(p) + 0.25 * n(p * 2.03) + 0.125 * n(p * 4.1) + 0.0625 * n(p * 8.3); }
// Thickness of the blood at p (0 none .. 1 deep): the main pool elongated
// the way it travelled, a lobed irregular edge, a tail of drops thrown on
// ahead, a few short directional streaks; for a smear, broken lanes.
float thick(vec2 p) {
	float t = 0.0;
	if (smear < 0.5) {
		vec2 q = p - vec2(-0.25, 0.0);
		q.x *= 0.72;
		float r = length(q);
		float edge = 0.42 + 0.16 * (fbm(q * 3.0 + seed) - 0.5) * 2.0;
		t = smoothstep(edge, edge - 0.12, r) * (0.6 + 0.4 * fbm(q * 6.0 + 3.0));
		// Drops thrown on ahead (+x), shrinking with distance, along a wobbling line.
		for (int i = 0; i < 9; i++) {
			float fi = float(i);
			float x = 0.2 + fi * 0.085 + 0.03 * h(vec2(fi, 1.0));
			float y = (h(vec2(fi, 2.0)) - 0.5) * 0.18 * (1.0 + fi * 0.15);
			float rr = 0.055 * (1.0 - fi / 10.0) * (0.6 + 0.6 * h(vec2(fi, 3.0)));
			vec2 d = p - vec2(x, y);
			d.x *= 0.8;
			t = max(t, smoothstep(rr, rr * 0.4, length(d)) * 0.8);
		}
		// Two or three short streaks where it hit hardest.
		float a = atan(q.y, q.x);
		float streak = pow(n(vec2(a * 3.0, seed)), 5.0) * step(abs(a), 1.1);
		t = max(t, smoothstep(edge + 0.25 * streak, edge + 0.25 * streak - 0.06, r) * step(0.2, streak) * 0.5);
	} else {
		// A body dragged through it: a broad wiped band, thinning and
		// breaking up at its sides and ends, finger-like streaks off its tail.
		float wob = (fbm(vec2(p.x * 1.5, seed)) - 0.5) * 0.3;
		float band = smoothstep(0.5, 0.25, abs(p.y + wob));
		float ends = smoothstep(1.0, 0.45 + 0.3 * fbm(vec2(p.y * 4.0, 2.0)), abs(p.x));
		float grain = fbm(p * vec2(1.5, 9.0) + seed);
		t = band * ends * smoothstep(0.25, 0.6, grain + 0.25 * band);
	}
	return t;
}
void fragment() {
	vec2 p = (UV - 0.5) * 2.0;
	float t = thick(p);
	if (t < 0.03) { discard; }
	// The surface of the pool from its thickness: wet highlights round the
	// meniscus of each lobe and drop.
	float e = 0.02;
	float tx = thick(p + vec2(e, 0.0)) - thick(p - vec2(e, 0.0));
	float ty = thick(p + vec2(0.0, e)) - thick(p - vec2(0.0, e));
	NORMAL_MAP = normalize(vec3(-tx * 2.5, -ty * 2.5, 1.0)) * 0.5 + 0.5;
	NORMAL_MAP_DEPTH = 1.0;
	float dry = clamp(age / 120.0, 0.0, 1.0);
	// Thin at the edges: coagulated dark brown-red; thick: deep, almost black red.
	vec3 thin = mix(vec3(0.3, 0.02, 0.015), vec3(0.16, 0.03, 0.02), dry);
	vec3 deep = mix(vec3(0.17, 0.0, 0.004), vec3(0.08, 0.01, 0.008), dry);
	float rim = smoothstep(0.03, 0.18, t) * (1.0 - smoothstep(0.18, 0.45, t));
	vec3 c = mix(thin, deep, smoothstep(0.15, 0.7, t));
	c = mix(c, vec3(0.1, 0.01, 0.005), rim * 0.6);      // the clotting edge
	ALBEDO = c;
	ALPHA = clamp(smoothstep(0.03, 0.12, t) * alpha_scale * (0.82 + 0.18 * t), 0.0, 1.0);
	ROUGHNESS = mix(0.04, 0.55, dry) + rim * 0.2;
	SPECULAR = 0.85;
}
"""

## The lens: blood drops that run down, a darkening vignette, a red-out.
const LENS_SHADER := """
shader_type canvas_item;
uniform float blood = 0.0;     // drops on the glass, 0..1
uniform float vignette = 0.0;  // darkening, 0..1
uniform float redout = 0.0;    // 0..1
uniform float seed = 0.0;
uniform float run = 0.0;       // seconds the drops have been running
float h(float x) { return fract(sin(x * 91.17 + seed * 7.3) * 43758.5453); }
float n1(float x) { float i = floor(x); float f = fract(x); f = f * f * (3.0 - 2.0 * f); return mix(h(i), h(i + 1.0), f); }
void fragment() {
	vec2 uv = UV;
	float aspect = SCREEN_PIXEL_SIZE.y / SCREEN_PIXEL_SIZE.x;
	float cover = 0.0;
	float thick = 0.0;
	// A dozen splashes thrown on the glass, the more the more blood, each
	// irregular, thick at the middle, running down in a streak.
	for (int i = 0; i < 14; i++) {
		float fi = float(i);
		if (h(fi * 3.1) > blood * 1.1) { continue; }
		vec2 c = vec2(h(fi * 1.7), h(fi * 2.9) * 0.8 + 0.05);
		// Edges of the frame catch more.
		c.x = mix(c.x, step(0.5, c.x), 0.35);
		float size = mix(0.025, 0.11, h(fi * 5.3) * h(fi * 5.3));
		vec2 d = (uv - c) * vec2(aspect, 1.0);
		float a = atan(d.y, d.x);
		float edge = size * (0.8 + 0.3 * n1(a * 1.5 + fi * 10.0) + 0.08 * n1(a * 6.0 + fi * 3.0));
		float r = length(d);
		float splash = 1.0 - smoothstep(edge * 0.85, edge, r);
		// Thrown spatter around it.
		// A drop or two flicked off it, not a starburst.
		vec2 fl = d - vec2(cos(fi * 2.3), sin(fi * 2.3)) * edge * 1.5;
		splash = max(splash, 1.0 - smoothstep(size * 0.12, size * 0.18, length(fl)));
		// The run: down from its bottom, narrowing, as long as it has run.
		float len = min(run * (0.05 + 0.18 * h(fi * 7.7)), 0.55) * step(0.35, size / 0.11);
		float w = size * 0.28 * (1.0 - clamp((d.y - 0.0) / max(len, 1e-4), 0.0, 1.0) * 0.7);
		float streak = step(0.0, d.y) * step(d.y, len) * (1.0 - smoothstep(w * 0.6, w, abs(d.x + 0.01 * sin(d.y * 40.0 + fi))));
		float m = max(splash, streak);
		cover = max(cover, m);
		thick = max(thick, m * (1.0 - smoothstep(0.0, edge, r) * 0.6));
	}
	vec2 v = uv - 0.5;
	float vig = smoothstep(0.2, 0.75, length(v * vec2(1.0, 0.85))) * vignette;
	vec3 col = mix(vec3(0.38, 0.01, 0.01), vec3(0.12, 0.0, 0.0), thick);
	float a = cover * (0.55 + 0.4 * thick);
	// The red-out: the edges first, a blood-dark wash in.
	float ro = redout * (0.3 + 0.6 * smoothstep(0.05, 0.7, length(v)));
	vec3 base = mix(vec3(0.0), vec3(0.28, 0.0, 0.0), clamp(redout * 1.4, 0.0, 1.0));
	float base_a = max(vig * 0.9, ro);
	col = mix(base, col, cover);
	COLOR = vec4(col, clamp(max(base_a, a), 0.0, 0.97));
}
"""

## Blood soaking into cloth/skin (a next pass over her apron and hands):
## soak climbs from the hem (or the fingertips), smear wipes across.
const SOAK_SHADER := """
shader_type spatial;
render_mode blend_mix, depth_draw_never, cull_disabled;
uniform float soak = 0.0;       // 0..1 of the height soaked
uniform float spatter = 0.0;    // flecks over all of it
uniform float smear = 0.0;      // the wipe's broad streaks
uniform float y_from = 0.0;     // soaked from this model-space height...
uniform float y_to = 1.0;       // ...up toward this one
uniform float seed = 0.0;
uniform float base_y = 0.0;      // world height the heights are above (her soles)
varying vec3 mp;
varying vec3 wp;
float h(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + seed) * 43758.5453); }
float n(vec2 p) {
	vec2 i = floor(p); vec2 f = fract(p); f = f * f * (3.0 - 2.0 * f);
	return mix(mix(h(i), h(i + vec2(1, 0)), f.x), mix(h(i + vec2(0, 1)), h(i + vec2(1, 1)), f.x), f.y);
}
void vertex() {
	mp = VERTEX;
	wp = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	// Toward the eye, not along the normal: some cloth is wound inside out.
	VERTEX += (inverse(MODEL_MATRIX) * vec4(normalize(CAMERA_POSITION_WORLD - wp), 0.0)).xyz * 0.004;
}
void fragment() {
	float t = clamp((wp.y - base_y - y_from) / max(y_to - y_from, 1e-3), 0.0, 1.0);
	float wobble = n(mp.xz * 9.0 + mp.y * 3.0) * 0.25;
	float soaked = 1.0 - smoothstep(soak - 0.08, soak + 0.04, t + wobble - 0.1);
	soaked *= step(0.001, soak);
	float fleck = step(1.0 - spatter * 0.22, n(mp.xy * 70.0 + mp.z * 31.0)) * step(0.001, spatter);
	float streak = smear * smoothstep(0.55, 0.8, n(vec2(mp.x * 4.0, mp.y * 26.0 + mp.z * 4.0)));
	float m = clamp(max(max(soaked, fleck), streak * 0.9), 0.0, 1.0);
	if (m < 0.02) { discard; }
	ALBEDO = mix(vec3(0.3, 0.012, 0.012), vec3(0.09, 0.004, 0.004), soaked * (0.4 + 0.6 * n(mp.xy * 18.0))) * (0.75 + 0.35 * n(mp.xy * 40.0));
	ALPHA = m * 0.94;
	ROUGHNESS = mix(0.12, 0.45, soaked);
	SPECULAR = 0.8;
}
"""


## On a whole skinned body (an overlay): blood round each wound (spreading
## with wound_r), running down the body from it under gravity, spatter flecks
## everywhere; and the strained joint: skin blanched then bruising purple
## round `joint`, petechiae, and the split, a ragged ring across the joint's
## axis gaping red to the fat and muscle beneath.
const BODY_GORE_SHADER := """
shader_type spatial;
render_mode blend_mix, depth_draw_never, cull_back;
uniform vec4 wound_a = vec4(0.0, -1000.0, 0.0, 0.0);   // xyz centre, w radius
uniform vec4 wound_b = vec4(0.0, -1000.0, 0.0, 0.0);
uniform vec4 wound_c = vec4(0.0, -1000.0, 0.0, 0.0);
uniform vec4 wound_d = vec4(0.0, -1000.0, 0.0, 0.0);
uniform float spatter = 0.0;
uniform vec3 joint = vec3(0.0, -1000.0, 0.0);
uniform vec3 joint_axis = vec3(0.0, 1.0, 0.0);
uniform float strain = 0.0;
uniform float split = 0.0;
uniform float seed = 0.0;
varying vec3 wp;
varying vec3 lp;
float h(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + seed) * 43758.5453); }
float n(vec2 p) {
	vec2 i = floor(p); vec2 f = fract(p); f = f * f * (3.0 - 2.0 * f);
	return mix(mix(h(i), h(i + vec2(1, 0)), f.x), mix(h(i + vec2(0, 1)), h(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 p) { return 0.5 * n(p) + 0.25 * n(p * 2.1) + 0.125 * n(p * 4.3); }
float wound(vec4 w, vec3 p) {
	if (w.w <= 0.0) { return 0.0; }
	vec3 d = p - w.xyz;
	float r = length(d);
	float edge = w.w * (0.75 + 0.5 * fbm(lp.xy * 14.0 + lp.z * 7.0));
	float pool = 1.0 - smoothstep(edge * 0.8, edge, r);
	// Runs: down from it, in narrow rivulets.
	float below = -d.y;
	float lane = fbm(vec2((p.x + p.z) * 38.0, 0.0));
	float run = step(0.0, below) * step(below, w.w * 3.5 * lane) * step(0.62, lane)
		* (1.0 - smoothstep(w.w * 0.6, w.w * 1.1, length(d.xz)));
	return max(pool, run);
}
void vertex() {
	wp = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	lp = VERTEX;
	VERTEX += (inverse(MODEL_MATRIX) * vec4(normalize(CAMERA_POSITION_WORLD - wp), 0.0)).xyz * 0.003;
}
void fragment() {
	float b = max(max(wound(wound_a, wp), wound(wound_b, wp)), max(wound(wound_c, wp), wound(wound_d, wp)));
	float fleck = step(1.0 - spatter * 0.05, n(lp.xy * 90.0 + lp.z * 41.0)) * step(0.001, spatter);
	b = max(b, fleck);
	vec3 blood = mix(vec3(0.3, 0.01, 0.01), vec3(0.1, 0.0, 0.0), n(lp.xy * 33.0) * 0.7);
	// The strained joint: a band round the limb at its root. The cloth rips
	// open over it (raggedly, more with strain), the skin under stretched
	// pale then bruising purple, pin-point bleeds, then the split.
	vec3 ax = normalize(joint_axis);
	vec3 dj = wp - joint;
	float along = dot(dj, ax);
	float radial = length(dj - ax * along);
	float dist = abs(along);
	float zone = (1.0 - smoothstep(0.05, 0.1, dist)) * (1.0 - smoothstep(0.13, 0.17, radial));
	float ang = atan(dot(dj, normalize(cross(ax, vec3(0.0, 0.0, 1.0)) + vec3(1e-4))), dot(dj, normalize(cross(ax, vec3(1.0, 0.0, 0.0)) + vec3(1e-4))));
	float rip = zone * step(fbm(vec2(ang * 2.0, along * 30.0)) + dist * 6.0, strain * 1.25);
	float bruise = rip * smoothstep(0.3, 0.9, strain) * smoothstep(0.35, 0.7, fbm(lp.xy * 30.0));
	float blanch = rip;
	float pete = rip * strain * step(0.97, h(floor(lp.xy * 260.0)));
	float ring = abs(along + (fbm(vec2(ang * 3.0, 2.0)) - 0.5) * 0.03);
	float gape = split * 0.024;
	float near = 1.0 - smoothstep(0.13, 0.17, radial);
	float gash = (1.0 - smoothstep(gape * 0.5, gape, ring)) * step(0.001, split) * near;
	float lip = (1.0 - smoothstep(gape, gape + 0.012, ring)) * step(0.001, split) * near;
	vec3 inside = mix(vec3(0.5, 0.04, 0.04), vec3(0.88, 0.74, 0.46), step(0.72, fbm(lp.xy * 120.0)) * 0.7);
	inside = mix(inside, vec3(0.1, 0.0, 0.0), smoothstep(0.3, 1.0, 1.0 - ring / max(gape, 1e-4)));
	vec3 c = blood;
	float a = b * 0.95;
	vec3 sk = mix(mix(vec3(0.7, 0.52, 0.45), vec3(0.8, 0.7, 0.66), strain * 0.5), vec3(0.3, 0.05, 0.15), clamp(bruise * 1.3, 0.0, 1.0));
	float ska = blanch;
	c = mix(c, sk, step(a, ska));
	a = max(a, ska);
	c = mix(c, vec3(0.4, 0.0, 0.02), max(pete, lip));
	a = max(a, max(pete, lip));
	c = mix(c, inside, gash);
	a = max(a, gash);
	if (a < 0.02) { discard; }
	ALBEDO = c;
	ALPHA = a;
	ROUGHNESS = mix(0.5, 0.08, max(b, gash));
	SPECULAR = 0.8;
}
"""


static func body_gore_material(seed: float) -> ShaderMaterial:
	var m := ShaderMaterial.new()
	m.shader = shader("body_gore", BODY_GORE_SHADER)
	m.set_shader_parameter("seed", seed)
	return m


static func shader(key: String, code: String) -> Shader:
	if _cache.has(key):
		return _cache[key]
	var s := Shader.new()
	s.code = code
	_cache[key] = s
	return s


static func material(key: String) -> Material:
	var k := "mat_" + key
	if _cache.has(k):
		return _cache[k]
	var m := ShaderMaterial.new()
	match key:
		"flesh":
			m.shader = shader("flesh", FLESH_SHADER)
		"bone":
			m.shader = shader("bone", BONE_SHADER)
		"blood_particle":
			var sm := StandardMaterial3D.new()
			sm.albedo_color = Color(0.42, 0.01, 0.01)
			sm.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
			sm.vertex_color_use_as_albedo = false
			sm.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
			sm.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA_SCISSOR
			sm.alpha_scissor_threshold = 0.4
			sm.albedo_texture = _drop_texture()
			_cache[k] = sm
			return sm
		"rib":
			var rb := StandardMaterial3D.new()
			rb.albedo_color = Color(0.86, 0.8, 0.7)
			rb.roughness = 0.35
			_cache[k] = rb
			return rb
		"sinew":
			var sn := StandardMaterial3D.new()
			sn.albedo_color = Color(0.62, 0.16, 0.14)
			sn.roughness = 0.12
			sn.vertex_color_use_as_albedo = true
			sn.cull_mode = BaseMaterial3D.CULL_DISABLED
			_cache[k] = sn
			return sn
	_cache[k] = m
	return m


static func strain_material(cloth: Color, skin: Color, seed: float) -> ShaderMaterial:
	var m := ShaderMaterial.new()
	m.shader = shader("strain", STRAIN_SHADER)
	m.set_shader_parameter("cloth", cloth)
	m.set_shader_parameter("skin", skin)
	m.set_shader_parameter("seed", seed)
	return m


static func splat_material(seed: float, smear: float) -> ShaderMaterial:
	var m := ShaderMaterial.new()
	m.shader = shader("splat", SPLAT_SHADER)
	m.set_shader_parameter("seed", seed)
	m.set_shader_parameter("smear", smear)
	return m


static func soak_material(y_from: float, y_to: float, seed: float) -> ShaderMaterial:
	var m := ShaderMaterial.new()
	m.shader = shader("soak", SOAK_SHADER)
	m.set_shader_parameter("y_from", y_from)
	m.set_shader_parameter("y_to", y_to)
	m.set_shader_parameter("seed", seed)
	return m


static func _drop_texture() -> Texture2D:
	var img := Image.create(32, 32, false, Image.FORMAT_RGBA8)
	for y in 32:
		for x in 32:
			var d := Vector2(x - 15.5, y - 15.5).length() / 15.5
			var a := clampf((1.0 - d) * 3.0, 0.0, 1.0)
			var shade := 0.55 + 0.25 * clampf(1.0 - Vector2(x - 11.0, y - 11.0).length() / 8.0, 0.0, 1.0)
			img.set_pixel(x, y, Color(shade, shade, shade, a))
	return ImageTexture.create_from_image(img)


# --- the torn end -------------------------------------------------------------

## A ragged torn end, local +y out of the wound (away from the body it was
## torn from). radius is the limb's; bone_out how far the broken bone stands
## proud; flap_len how far the skin rim curls. Three surfaces: 0 the skin rim
## (skin outside, raw inside), 1 the meat (muscle mass, fat layer, hanging
## fibre strands), 2 the bone (one shaft, a jagged break, the marrow).
static func torn_end(radius: float, seed: int, bone_out: float, flap_len: float) -> ArrayMesh:
	var key := "torn2_%.3f_%d_%.3f_%.3f" % [radius, seed, bone_out, flap_len]
	if _cache.has(key):
		return _cache[key]
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	var noise := FastNoiseLite.new()
	noise.seed = seed
	noise.noise_type = FastNoiseLite.TYPE_SIMPLEX_SMOOTH
	noise.frequency = 1.0
	noise.fractal_octaves = 3
	var mesh := ArrayMesh.new()
	_skin_rim(mesh, rng, noise, radius, flap_len)
	_meat(mesh, rng, noise, radius)
	_bone(mesh, rng, noise, radius * 0.36, bone_out)
	_cache[key] = mesh
	return mesh


# The skin's torn edge: a ring round the wound, its top torn unevenly, the
# torn margin curling outward and back like a rolled lip.
static func _skin_rim(mesh: ArrayMesh, rng: RandomNumberGenerator, noise: FastNoiseLite, r: float, curl_len: float) -> void:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var sides := 40
	var rows := 5
	var grid := []
	for row in rows + 1:
		var t := float(row) / rows
		var line := []
		for side in sides + 1:
			var a := TAU * float(side % sides) / sides
			# How far this part of the margin tore free (0 a clean edge, 1 a long tongue).
			var tongue := clampf(0.5 + 0.9 * noise.get_noise_1d(float(side % sides) * 0.35), 0.0, 1.0)
			var reach := curl_len * (0.25 + 0.75 * tongue)
			var arc := t * PI * 0.75 * (0.6 + 0.6 * tongue)     # rolling out and back
			var out := r * 1.0 + sin(arc) * reach * 0.6
			var y := -0.012 + sin(arc * 0.5) * reach * 0.55 - (1.0 - cos(arc)) * reach * 0.25
			var lump := 1.0 + 0.06 * noise.get_noise_2d(a * 3.0, t * 4.0)
			line.append(Vector3(sin(a) * out * lump, y, cos(a) * out * lump))
		grid.append(line)
	for row in rows:
		for side in sides:
			var q := [grid[row][side], grid[row][side + 1], grid[row + 1][side + 1], grid[row + 1][side]]
			var uvs := [Vector2(float(side) / sides, float(row) / rows), Vector2(float(side + 1) / sides, float(row) / rows),
				Vector2(float(side + 1) / sides, float(row + 1) / rows), Vector2(float(side) / sides, float(row + 1) / rows)]
			for idx in [0, 1, 2, 0, 2, 3]:
				st.set_color(Color(clampf(0.25 + 0.75 * uvs[idx].y, 0.0, 1.0), 0.0, 0.0))
				st.set_uv(uvs[idx])
				st.add_vertex(q[idx])
	st.index()
	st.generate_normals()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, st.commit_to_arrays())


# The meat: a lumpy mass of torn muscle bellies swelling out of the wound,
# ringed by the yellow fat under the skin, fibre bundles hanging off it.
# Vertex colour r = blood (pooled in the folds), g = fat.
static func _meat(mesh: ArrayMesh, rng: RandomNumberGenerator, noise: FastNoiseLite, r: float) -> void:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var lat := 10
	var lon := 32
	var bellies := []
	for i in 5:
		var a := rng.randf() * TAU
		bellies.append([Vector2(sin(a), cos(a)) * rng.randf_range(0.15, 0.65), rng.randf_range(0.25, 0.5), rng.randf_range(0.2, 0.55)])
	var pts := []
	var cols := []
	for i in lat + 1:
		var t := float(i) / lat                 # 0 rim .. 1 crown
		var row := []
		var crow := []
		for j in lon + 1:
			var a := TAU * float(j % lon) / lon
			var rad := r * 0.96 * cos(t * PI * 0.5)
			var xz := Vector2(sin(a), cos(a)) * rad
			var h := r * 0.45 * sin(t * PI * 0.5)
			var swell := 0.0
			for b in bellies:
				var d: float = (xz / r - (b[0] as Vector2)).length()
				swell += float(b[2]) * exp(-d * d / (float(b[1]) * float(b[1])))
			var n := noise.get_noise_3d(xz.x / r * 2.5, h / r * 2.5, float(rng.seed % 97))
			h += r * (0.35 * swell + 0.12 * n) * smoothstep(0.0, 0.3, t)
			var p := Vector3(xz.x, h - 0.005, xz.y) * (1.0 + 0.05 * n)
			row.append(p)
			# Fat in the outer band, blood pooled where the surface dips.
			var fat := 1.0 - smoothstep(0.08, 0.2, t)
			var blood := clampf(0.35 - 0.9 * n + 0.4 * (1.0 - swell), 0.0, 1.0)
			crow.append(Color(blood, fat, 0.0))
		pts.append(row)
		cols.append(crow)
	for i in lat:
		for j in lon:
			var ids := [[i, j], [i, j + 1], [i + 1, j + 1], [i + 1, j]]
			for k in [0, 1, 2, 0, 2, 3]:
				var ij: Array = ids[k]
				st.set_color(cols[ij[0]][ij[1]])
				st.set_uv(Vector2(float(ij[1]) / lon, float(ij[0]) / lat))
				st.add_vertex(pts[ij[0]][ij[1]])
	# Fibre bundles torn long: flat tapering ribbons sagging out and down
	# over the rim, a few layered on each other.
	for f in 7:
		var a := rng.randf() * TAU
		var d := rng.randf_range(0.3, 0.85) * r
		var base := Vector3(sin(a) * d, r * 0.25, cos(a) * d)
		var outward := Vector3(sin(a), 0.0, cos(a))
		var side := Vector3(cos(a), 0.0, -sin(a))
		var length := r * rng.randf_range(0.6, 1.5)
		var w := r * rng.randf_range(0.1, 0.22)
		var segs := 6
		var prev_l := Vector3.ZERO
		var prev_r := Vector3.ZERO
		for sg in segs + 1:
			var t := float(sg) / segs
			var p := base + outward * length * 0.7 * t + Vector3(0, length * (0.35 * t - 0.9 * t * t), 0)
			var ww := w * (1.0 - 0.75 * t)
			var l := p - side * ww
			var rr := p + side * ww
			if sg > 0:
				for v in [[prev_l, t - 1.0 / segs], [prev_r, t - 1.0 / segs], [rr, t], [prev_l, t - 1.0 / segs], [rr, t], [l, t]]:
					st.set_color(Color(0.25 + 0.5 * float(v[1]), 0.0, 0.0))
					st.set_uv(Vector2(0.5, float(v[1]) * 3.0))
					st.add_vertex(v[0])
			prev_l = l
			prev_r = rr
	st.index()
	st.generate_normals()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, st.commit_to_arrays())


# The bone: one shaft out of the meat, snapped across at a slant with a
# jagged lip, the marrow cavity dark inside; blood up it from the meat.
# Vertex colour r = blood, g = marrow.
static func _bone(mesh: ArrayMesh, rng: RandomNumberGenerator, noise: FastNoiseLite, r: float, out: float) -> void:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var sides := 16
	var slant := rng.randf_range(0.4, 0.9)
	var slant_a := rng.randf() * TAU
	var off := Vector3(rng.randf_range(-0.25, 0.25) * r, 0.0, rng.randf_range(-0.25, 0.25) * r)
	var tops := []
	for sd in sides:
		var a := TAU * sd / sides
		var top := out * (1.0 + slant * 0.5 * cos(a - slant_a))
		top += out * 0.35 * noise.get_noise_1d(float(sd) * 1.7 + 40.0)
		if sd % 5 == 0:
			top += out * 0.3            # a splinter
		tops.append(maxf(top, out * 0.25))
	var rings := 4
	for sd in sides:
		var nx := (sd + 1) % sides
		var a0 := TAU * sd / sides
		var a1 := TAU * nx / sides
		for ring in rings:
			var t0 := float(ring) / rings
			var t1 := float(ring + 1) / rings
			var verts := []
			for pr in [[a0, t0, tops[sd]], [a1, t0, tops[nx]], [a1, t1, tops[nx]], [a0, t1, tops[sd]]]:
				var swell := 1.0 + 0.12 * (1.0 - float(pr[1]))     # flaring toward the joint end
				var y := -0.01 + float(pr[2]) * float(pr[1])
				verts.append([off + Vector3(sin(pr[0]) * r * swell, y, cos(pr[0]) * r * swell), clampf(0.8 - 1.4 * float(pr[1]), 0.0, 1.0)])
			for idx in [0, 1, 2, 0, 2, 3]:
				st.set_color(Color(verts[idx][1], 0.0, 0.0))
				st.set_uv(Vector2(float(sd) / sides, verts[idx][0].y * 10.0))
				st.add_vertex(verts[idx][0])
		# The broken end: the cortex's thickness inward, then the marrow.
		var o0 := off + Vector3(sin(a0) * r, tops[sd] - 0.01, cos(a0) * r)
		var o1 := off + Vector3(sin(a1) * r, tops[nx] - 0.01, cos(a1) * r)
		var i0 := off + Vector3(sin(a0) * r * 0.62, tops[sd] * 0.92 - 0.012, cos(a0) * r * 0.62)
		var i1 := off + Vector3(sin(a1) * r * 0.62, tops[nx] * 0.92 - 0.012, cos(a1) * r * 0.62)
		var c := off + Vector3(0.0, out * 0.55, 0.0)
		for v in [[o0, 0.0, 0.0], [o1, 0.0, 0.0], [i1, 0.15, 0.3], [o0, 0.0, 0.0], [i1, 0.15, 0.3], [i0, 0.15, 0.3],
				[i0, 0.3, 1.0], [i1, 0.3, 1.0], [c, 0.5, 1.0]]:
			st.set_color(Color(v[1], v[2], 0.0))
			st.set_uv(Vector2(0.0, 0.0))
			st.add_vertex(v[0])
	st.index()
	st.generate_normals()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, st.commit_to_arrays())


# --- the ribcage -------------------------------------------------------------

## Broken ribs: curved arcs of bone (each a thin tube along a circle's arc),
## snapped and splayed; blood on their roots. `count` arcs, `splay` 0..1
## how far they are levered out of the chest. Local: +y out of the chest,
## x across it, z along the body (+ to the head).
static func ribcage(seed: int, count: int, splay: float) -> ArrayMesh:
	var key := "ribs_%d_%d_%.2f" % [seed, count, splay]
	if _cache.has(key):
		return _cache[key]
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	for i in count:
		var side := -1.0 if i % 2 == 0 else 1.0
		var z := (float(i / 2) - float(count / 2) * 0.5) * 0.035
		var radius := rng.randf_range(0.1, 0.13)
		var from := rng.randf_range(0.1, 0.35)
		var to := from + rng.randf_range(0.35, 0.7)
		var lift := splay * rng.randf_range(0.5, 1.2)
		var segs := 8
		var prev := []
		for sg in segs + 1:
			var t := lerpf(from, to, float(sg) / segs)
			# Round the chest from the spine side toward the sternum, the
			# broken end levered up and out.
			var u := float(sg) / segs
			var c := Vector3(side * radius * sin(t * PI * 0.5), radius * (cos(t * PI * 0.5) - 1.0) * 0.6 + lift * 0.09 * u * u, z + 0.01 * u)
			var tan := Vector3(side * cos(t * PI * 0.5), -sin(t * PI * 0.5) * 0.6 + lift * 0.5 * u, 0.0).normalized()
			var nrm := tan.cross(Vector3(0, 0, 1)).normalized()
			var bin := tan.cross(nrm).normalized()
			var rr := 0.0055 * (1.0 - 0.45 * u)
			var ring := []
			for k in 6:
				var a := TAU * k / 6.0
				ring.append(c + (nrm * cos(a) * 1.3 + bin * sin(a)) * rr)
			if not prev.is_empty():
				for k in 6:
					var kn := (k + 1) % 6
					for v in [prev[k], prev[kn], ring[kn], prev[k], ring[kn], ring[k]]:
						st.set_color(Color(clampf(0.7 - u, 0.0, 1.0) if sg < 3 else 0.05, 0.0, 0.0))
						st.set_uv(Vector2(float(k) / 6.0, u))
						st.add_vertex(v)
			prev = ring
		# The jagged broken tip.
		var tip: Vector3 = (prev[0] + prev[3]) * 0.5
		var tip_tan := Vector3(side * cos(to * PI * 0.5), -sin(to * PI * 0.5) * 0.6 + lift * 0.5, 0.0).normalized()
		var point := tip + tip_tan * rng.randf_range(0.006, 0.016)
		for k in 6:
			for v in [prev[k], prev[(k + 1) % 6], point]:
				st.set_color(Color(0.1, 0.4, 0.0))
				st.set_uv(Vector2.ZERO)
				st.add_vertex(v)
	st.index()
	st.generate_normals()
	var m := ArrayMesh.new()
	m.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, st.commit_to_arrays())
	_cache[key] = m
	return m


## An open wound mass (chest cavity, the ruin): the meat surface alone.
static func wound_mass(radius: float, seed: int) -> ArrayMesh:
	var key := "mass_%.3f_%d" % [radius, seed]
	if _cache.has(key):
		return _cache[key]
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	var noise := FastNoiseLite.new()
	noise.seed = seed
	noise.frequency = 1.0
	var m := ArrayMesh.new()
	_meat(m, rng, noise, radius)
	_cache[key] = m
	return m
