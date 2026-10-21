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
uniform vec4 muscle : source_color = vec4(0.5, 0.06, 0.06, 1.0);
uniform vec4 fat : source_color = vec4(0.86, 0.72, 0.42, 1.0);
uniform vec4 blood : source_color = vec4(0.3, 0.005, 0.01, 1.0);
uniform float wet = 1.0;
float h(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
float n(vec2 p) {
	vec2 i = floor(p); vec2 f = fract(p); f = f * f * (3.0 - 2.0 * f);
	return mix(mix(h(i), h(i + vec2(1, 0)), f.x), mix(h(i + vec2(0, 1)), h(i + vec2(1, 1)), f.x), f.y);
}
void fragment() {
	float fibre = n(vec2(UV.x * 90.0, UV.y * 6.0)) * 0.6 + n(vec2(UV.x * 220.0, UV.y * 14.0)) * 0.4;
	vec3 m = muscle.rgb * (0.55 + 0.75 * fibre);
	m = mix(m, fat.rgb * (0.8 + 0.3 * n(UV * 40.0)), COLOR.g);
	m = mix(m, blood.rgb, clamp(COLOR.r * (0.7 + 0.5 * n(UV * 23.0)), 0.0, 1.0));
	ALBEDO = m;
	ROUGHNESS = mix(0.6, 0.22, wet * (0.5 + 0.5 * COLOR.r) * fibre);
	SPECULAR = 0.5;
	SSS_STRENGTH = 0.0;
}
"""

const BONE_SHADER := """
shader_type spatial;
uniform vec4 bone : source_color = vec4(0.88, 0.84, 0.74, 1.0);
float h(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
void fragment() {
	float pit = h(floor(UV * vec2(60.0, 30.0)));
	vec3 c = bone.rgb * (0.82 + 0.18 * pit);
	// Marrow at the broken end (COLOR.g), blood smeared up the shaft (COLOR.r).
	c = mix(c, vec3(0.32, 0.03, 0.03), COLOR.g);
	c = mix(c, vec3(0.42, 0.02, 0.02), COLOR.r * (0.6 + 0.4 * pit));
	ALBEDO = c;
	ROUGHNESS = mix(0.55, 0.12, max(COLOR.r, COLOR.g));
}
"""

## A blood splat on a quad: a noisy core, tendrils and satellite droplets,
## wet and dark at the middle. age darkens and dries it.
const SPLAT_SHADER := """
shader_type spatial;
render_mode cull_disabled, depth_draw_never, blend_mix;
uniform float seed = 0.0;
uniform float age = 0.0;
uniform float smear = 0.0;   // 0 round splat, 1 a drag smear along +x
uniform float alpha_scale = 1.0;
float h(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7)) + seed * 17.13) * 43758.5453); }
float n(vec2 p) {
	vec2 i = floor(p); vec2 f = fract(p); f = f * f * (3.0 - 2.0 * f);
	return mix(mix(h(i), h(i + vec2(1, 0)), f.x), mix(h(i + vec2(0, 1)), h(i + vec2(1, 1)), f.x), f.y);
}
void fragment() {
	vec2 p = (UV - 0.5) * 2.0;
	p.x *= mix(1.0, 0.45, smear);
	float r = length(p);
	float a = atan(p.y, p.x);
	float edge = 0.45 + 0.2 * n(vec2(a * 2.5, seed)) + 0.12 * n(vec2(a * 9.0, seed + 3.0));
	// Tendrils thrown out from the core.
	float spikes = pow(n(vec2(a * 14.0, seed + 9.0)), 6.0) * 0.55 * (1.0 - smear);
	float body = 1.0 - smoothstep(edge + spikes - 0.04, edge + spikes, r);
	// Satellite droplets.
	vec2 cell = floor(p * 7.0);
	vec2 cp = fract(p * 7.0) - 0.5;
	float drop = step(0.82, h(cell)) * (1.0 - smoothstep(0.12, 0.2, length(cp - (vec2(h(cell + 1.0), h(cell + 2.0)) - 0.5) * 0.5)));
	drop *= step(0.5, r) * (1.0 - step(0.98, r));
	// Smear streaks.
	// A drag: streaks along x, ragged and tapering at both ends.
	float lanes = n(vec2(UV.y * 26.0, seed));
	float len = 0.55 + 0.4 * lanes;
	float taper = 1.0 - smoothstep(len * 0.6, len, abs((UV.x - 0.5) * 2.0));
	float streak = smear * (1.0 - smoothstep(0.35 * taper, 0.45 * taper + 0.01, abs(p.y) * (1.0 + 0.6 * (1.0 - lanes))));
	streak *= step(0.25, lanes + 0.3);
	float m = clamp(max(max(body, drop), streak), 0.0, 1.0);
	if (m < 0.02) { discard; }
	float core = 1.0 - smoothstep(0.0, edge, r);
	vec3 wet = mix(vec3(0.36, 0.012, 0.012), vec3(0.11, 0.0, 0.004), core * 0.85);
	vec3 dry = vec3(0.12, 0.025, 0.018);
	ALBEDO = mix(wet, dry, clamp(age / 90.0, 0.0, 0.7)) * (0.8 + 0.25 * n(UV * 30.0));
	ALPHA = m * alpha_scale * (0.85 + 0.15 * core);
	ROUGHNESS = mix(0.04, 0.6, clamp(age / 60.0, 0.0, 1.0));
	SPECULAR = 0.9;
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
		float edge = size * (0.75 + 0.35 * n1(a * 2.0 + fi * 10.0) + 0.25 * n1(a * 7.0 + fi * 3.0));
		float r = length(d);
		float splash = 1.0 - smoothstep(edge * 0.85, edge, r);
		// Thrown spatter around it.
		float sp = pow(n1(a * 9.0 + fi * 5.0), 10.0);
		splash = max(splash, (1.0 - smoothstep(edge * (1.0 + 0.5 * sp) - 0.004, edge * (1.0 + 0.5 * sp), r)) * step(0.5, sp));
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
## torn from). radius is the limb's; long_flaps for the severed piece (skin
## drawn off it), bone_out how far the broken bone stands proud.
## Returns a mesh with three surfaces: 0 skin/cloth flaps (uses `skin_mat`),
## 1 flesh, 2 bone.
static func torn_end(radius: float, seed: int, bone_out: float, flap_len: float) -> ArrayMesh:
	var key := "torn_%.3f_%d_%.3f_%.3f" % [radius, seed, bone_out, flap_len]
	if _cache.has(key):
		return _cache[key]
	var rng := RandomNumberGenerator.new()
	rng.seed = seed
	var mesh := ArrayMesh.new()
	_flaps(mesh, rng, radius, flap_len)
	_flesh(mesh, rng, radius)
	_bone(mesh, rng, radius * 0.36, bone_out)
	_cache[key] = mesh
	return mesh


# Skin flaps: a ring of tongues of skin/cloth torn to different lengths,
# curling out and down; vertex colour r is the blood on their inner faces.
static func _flaps(mesh: ArrayMesh, rng: RandomNumberGenerator, r: float, flap_len: float) -> void:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var count := 13
	for i in count:
		var a0 := TAU * (float(i) + rng.randf_range(-0.2, 0.1)) / count
		var a1 := TAU * (float(i + 1) + rng.randf_range(-0.1, 0.25)) / count
		var length := flap_len * pow(rng.randf(), 1.6) + r * 0.08
		var peel := rng.randf_range(0.05, 0.55)    # how far it has peeled back
		var segs := 5
		var rows := []
		for sgi in segs + 1:
			var t := float(sgi) / segs
			# Wide at the tear, ragging to an uneven, rounded tip; peeling
			# outward and sagging back toward the body as it goes.
			var w := lerpf(1.0, rng.randf_range(0.35, 0.75), t * t)
			var am := (a0 + a1) * 0.5 + rng.randf_range(-0.04, 0.04) * t
			var half := (a1 - a0) * 0.5 * w
			var out := r * (1.0 + 0.6 * peel * t)
			var y := length * t * (1.0 - peel * 0.8 * t) - peel * length * 0.5 * t * t
			var row := []
			for k in 3:
				var ak := am + half * (float(k) - 1.0)
				var jag := (rng.randf_range(-0.25, 0.25) * length * 0.25) if sgi == segs else 0.0
				row.append(Vector3(sin(ak) * out, y + jag, cos(ak) * out))
			rows.append(row)
		for sgi in segs:
			for k in 2:
				var q := [rows[sgi][k], rows[sgi][k + 1], rows[sgi + 1][k + 1], rows[sgi + 1][k]]
				var t0 := float(sgi) / segs
				var t1 := float(sgi + 1) / segs
				var ts := [t0, t0, t1, t1]
				for idx in [0, 1, 2, 0, 2, 3]:
					st.set_color(Color(clampf(0.3 + 0.7 * ts[idx], 0.0, 1.0), 0.0, 0.0))
					st.set_uv(Vector2((float(i) + (float(k) + (1.0 if idx in [1, 2] else 0.0)) * 0.5) / count, ts[idx]))
					st.add_vertex(q[idx])
	st.index()
	st.generate_normals()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, st.commit_to_arrays())


# The flesh: a lumpy, fibrous mound of torn muscle in a yellow fat ring,
# strands of it standing proud; vertex colour r blood, g fat.
static func _flesh(mesh: ArrayMesh, rng: RandomNumberGenerator, r: float) -> void:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var rings := 6
	var sides := 18
	var heights := []
	for ring in rings + 1:
		var row := []
		for side in sides:
			row.append(rng.randf_range(-0.25, 1.0))
		heights.append(row)
	var pts := []
	for ring in rings + 1:
		var row := []
		var t := float(ring) / rings           # 0 rim .. 1 centre
		for side in sides:
			var a := TAU * side / sides
			var rad := r * (1.08 - 1.08 * t)
			var lump: float = heights[ring][side]
			var y := r * (0.05 + 0.55 * sin(t * PI * 0.5)) + r * 0.28 * lump * t * (1.0 - t * 0.5)
			row.append(Vector3(sin(a) * rad, y, cos(a) * rad))
		pts.append(row)
	for ring in rings:
		for side in sides:
			var n := (side + 1) % sides
			var quad := [pts[ring][side], pts[ring][n], pts[ring + 1][n], pts[ring + 1][side]]
			var fat := 0.55 if ring == 0 else 0.0
			var blood := rng.randf_range(0.3, 0.9) if ring == 0 else rng.randf_range(0.1, 0.7)
			for idx in [0, 1, 2, 0, 2, 3]:
				st.set_color(Color(blood, fat, 0.0))
				st.set_uv(Vector2(float(side) / sides, float(ring) / rings))
				st.add_vertex(quad[idx])
	# Muscle fibre bundles torn to different lengths.
	for k in 14:
		var a := rng.randf() * TAU
		var d := rng.randf_range(0.2, 0.75) * r
		var base := Vector3(sin(a) * d, r * 0.2, cos(a) * d)
		var tip := base + Vector3(rng.randf_range(-0.4, 0.4) * r, rng.randf_range(0.3, 1.1) * r,
			rng.randf_range(-0.4, 0.4) * r)
		var w := r * rng.randf_range(0.07, 0.14)
		var axis := (tip - base).normalized()
		var side := axis.cross(Vector3.FORWARD if absf(axis.z) < 0.9 else Vector3.RIGHT).normalized()
		var side2 := axis.cross(side).normalized()
		# A torn bundle: a tapered four-sided tube, its end frayed.
		var ring0 := [base + side * w, base + side2 * w, base - side * w, base - side2 * w]
		var ring1 := [tip + side * w * 0.35, tip + side2 * w * 0.35, tip - side * w * 0.35, tip - side2 * w * 0.35]
		for j in 4:
			var jn := (j + 1) % 4
			for v in [ring0[j], ring0[jn], ring1[jn], ring0[j], ring1[jn], ring1[j]]:
				st.set_color(Color(0.3, 0.0, 0.0))
				st.set_uv(Vector2(float(j) / 4.0, (v - base).length() / r))
				st.add_vertex(v)
	st.index()
	st.generate_normals()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, st.commit_to_arrays())


# The bone: a shaft standing out of the flesh, its end snapped in a jagged
# crown with the marrow showing; blood smeared up from the flesh.
static func _bone(mesh: ArrayMesh, rng: RandomNumberGenerator, r: float, out: float) -> void:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var sides := 10
	var tops := []
	for s in sides:
		tops.append(out * rng.randf_range(0.55, 1.0) + (out * 0.35 if s % 3 == 0 else 0.0))
	var off := Vector3(rng.randf_range(-0.3, 0.3) * r, 0.0, rng.randf_range(-0.3, 0.3) * r)
	for s in sides:
		var n := (s + 1) % sides
		var a0 := TAU * s / sides
		var a1 := TAU * n / sides
		var b0 := off + Vector3(sin(a0) * r, -0.02, cos(a0) * r)
		var b1 := off + Vector3(sin(a1) * r, -0.02, cos(a1) * r)
		var t0 := off + Vector3(sin(a0) * r * 0.9, tops[s], cos(a0) * r * 0.9)
		var t1 := off + Vector3(sin(a1) * r * 0.9, tops[n], cos(a1) * r * 0.9)
		for v in [[b0, 0.55, 0.0], [b1, 0.55, 0.0], [t1, 0.0, 0.0], [b0, 0.55, 0.0], [t1, 0.0, 0.0], [t0, 0.0, 0.0]]:
			st.set_color(Color(v[1], v[2], 0.0))
			st.set_uv(Vector2(float(s) / sides, (v[0] as Vector3).y))
			st.add_vertex(v[0])
		# The marrow cavity: the broken end's inside.
		var c := off + Vector3(0.0, out * 0.4, 0.0)
		for v in [t0, t1, c]:
			st.set_color(Color(0.2, 1.0 if v == c else 0.6, 0.0))
			st.set_uv(Vector2(0.0, 0.0))
			st.add_vertex(v)
	st.index()
	st.generate_normals()
	mesh.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, st.commit_to_arrays())
