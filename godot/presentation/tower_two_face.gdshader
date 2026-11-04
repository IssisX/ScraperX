shader_type spatial;

// Tower two's face (environment plan, job 14): the 1.6 km neighbouring shaft
// as poured concrete, on the same box (main_world.gd _build_tower_skin). Look
// only: no relief, nothing proud of the face, and nothing that runs on from
// one lift to the next, so nothing reads as a way up (rule 64).
// - Lift joints every LIFT metres (a jump-form pour), each lift a shade off
//   its neighbours; the form's panel joints, faint, every PANEL metres.
// - Form-tie plugs on a TIE_X by TIE_Y grid; a few bleed rust down the face.
// - Cold joints: some lifts carry a wavy seam where a pour stopped and went
//   on later.
// - Rain staining: streaks run down from each joint's lip, and longer ones
//   down the face.
// - Service runs below service_top: a conduit riser within one lift, and a
//   short cable tray above a joint, at scattered places.
// Every line is box-filtered by its pixel footprint (fwidth) and every fine
// pattern gives way to its mean as it nears a pixel, so it does not shimmer
// from the yard. `fine` 0 (GRAPHICS LOW) drops the ties, the panel joints and
// the fine streaks.

uniform vec3 base_colour : source_color = vec3(0.306, 0.282, 0.255);
uniform float base_roughness = 0.94;
uniform float fine = 1.0;
uniform float service_top = 500.0;

const float LIFT = 4.2;
const float PANEL = 2.4;
const float TIE_X = 0.6;
const float TIE_Y = 1.05;
const float TIE_R = 0.02;
const float RISER_SLOT = 9.0;
const float TRAY_SLOT = 12.0;
const vec3 CONDUIT = vec3(0.1, 0.1, 0.105);   // sRGB, as base_colour: weathered galvanising
const vec3 RUST_TINT = vec3(1.18, 0.82, 0.58);

varying vec3 world;
varying vec3 world_normal;

// Hashes without sine (stable on phone GPUs at large arguments).
float hash1(float p) {
	p = fract(p * 0.1031);
	p *= p + 33.33;
	p *= p + p;
	return fract(p);
}

float hash2(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

float noise1(float x) {
	float i = floor(x);
	float f = fract(x);
	f = f * f * (3.0 - 2.0 * f);
	return mix(hash1(i), hash1(i + 1.0), f);
}

float noise2(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash2(i), hash2(i + vec2(1.0, 0.0)), f.x),
		mix(hash2(i + vec2(0.0, 1.0)), hash2(i + vec2(1.0, 1.0)), f.x), f.y);
}

// How much of a pixel `fw` wide, its middle `d` from a line's middle, the
// line `w` wide covers (a box filter).
float line_cover(float d, float w, float fw) {
	float low = max(d - 0.5 * fw, -0.5 * w);
	float high = min(d + 0.5 * fw, 0.5 * w);
	return max(high - low, 0.0) / max(fw, 1e-5);
}

// An sRGB colour as StandardMaterial3D's albedo takes it under this renderer.
vec3 output_colour(vec3 srgb) {
	if (OUTPUT_IS_SRGB) {
		return srgb;
	}
	return mix(pow((srgb + vec3(0.055)) * (1.0 / (1.0 + 0.055)), vec3(2.4)), srgb * (1.0 / 12.92),
		lessThan(srgb, vec3(0.04045)));
}

void vertex() {
	world = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = (MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz;
}

void fragment() {
	vec3 n = normalize(world_normal);
	float y = world.y;
	// Along the face: x on the north and south faces, z on the east and west.
	float s = abs(n.z) > abs(n.x) ? world.x : world.z;
	float fs = max(fwidth(s), 1e-5);
	float fy = max(fwidth(y), 1e-5);
	float fw = max(fs, fy);
	// The weathering of decades, tens of metres across.
	float value = 0.8 + 0.24 * noise2(vec2(s / 41.0, y / 67.0));
	vec3 tint = vec3(1.0);
	float dark = 0.0;
	if (abs(n.y) < 0.7) {
		float lift = floor(y / LIFT);
		float in_lift = fract(y / LIFT) * LIFT;
		float panel = floor(s / PANEL);
		// Each lift a pour of its own; each panel's face a little apart.
		value *= 0.86 + 0.2 * hash2(vec2(lift, 7.0));
		value *= 1.0 + fine * (0.08 * hash2(vec2(lift, panel)) - 0.04);

		// Lift joints, and the panels' joints.
		float d_joint = abs(fract(y / LIFT + 0.5) - 0.5) * LIFT;
		value *= 1.0 - 0.6 * line_cover(d_joint, 0.03, fy);
		float d_panel = abs(fract(s / PANEL + 0.5) - 0.5) * PANEL;
		value *= 1.0 - 0.16 * fine * line_cover(d_panel, 0.012, fs);

		// A cold joint on about one lift in five.
		float cold = step(hash2(vec2(lift, 3.0)), 0.2);
		float y_cold = lift * LIFT + LIFT * (0.25 + 0.5 * hash2(vec2(lift, 5.0)))
			+ 0.35 * (noise1(s / 2.7 + lift) - 0.5);
		value *= 1.0 - cold * (0.35 * line_cover(abs(y - y_cold), 0.05, fy)
			+ 0.03 * step(y_cold, y));

		// Rain from each joint's lip: streaks, strongest just under the joint
		// above, each a different length.
		float below = LIFT - in_lift;
		float drip = exp(-below / mix(0.6, 2.2, noise1(s / 0.9 + lift * 1.7)));
		float streaks = noise1(s / 0.37 + hash1(lift) * 50.0) * noise1(s / 1.3 + lift * 3.1);
		streaks = 2.0 * streaks * streaks;
		float keep = clamp(1.0 - fs / 0.2, 0.0, 1.0) * fine;
		float stain = drip * mix(0.25, streaks, keep) * 0.6;
		// Longer stains down the face.
		float run = smoothstep(0.55, 0.95, noise2(vec2(s / 3.3, y / 60.0)));
		stain += run * mix(0.5, noise1(s / 2.9 + 11.0), clamp(1.0 - fs / 1.0, 0.0, 1.0)) * 0.4;
		value *= 1.0 - stain;

		// Form-tie plugs; about one in eight bleeds rust.
		vec2 g = (fract(vec2(s / TIE_X, y / TIE_Y)) - 0.5) * vec2(TIE_X, TIE_Y);
		float sharp = 1.0 - smoothstep(TIE_R - 0.5 * fw, TIE_R + 0.5 * fw, length(g));
		float mean = 3.14159 * TIE_R * TIE_R / (TIE_X * TIE_Y);
		value *= 1.0 - 0.55 * fine * mix(sharp, mean, smoothstep(0.5 * TIE_R, 2.0 * TIE_R, fw));
		float rusty = step(hash2(floor(vec2(s / TIE_X, y / TIE_Y)) + 17.0), 0.12);
		float rust = rusty * step(g.y, 0.0) * exp(g.y / 0.25)
			* line_cover(abs(g.x), 2.4 * TIE_R, fs) * clamp(1.0 - fw / 0.05, 0.0, 1.0) * fine;
		tint = mix(tint, RUST_TINT, 0.8 * rust);

		// Service runs, low on the shaft: a riser within one lift, never
		// running on to the next; a short tray above a joint.
		float served = step(y, service_top);
		float slot = floor(s / RISER_SLOT);
		float riser = served * step(hash2(vec2(lift, slot) + 41.0), 0.12);
		float x_riser = (slot + 0.2 + 0.6 * hash2(vec2(lift, slot) + 43.0)) * RISER_SLOT;
		float in_span = step(0.4, in_lift) * step(in_lift, LIFT - 0.4);
		dark = riser * in_span * line_cover(abs(s - x_riser), 0.08, fs);
		float bay = floor(s / TRAY_SLOT);
		float tray = served * step(hash2(vec2(lift, bay) + 59.0), 0.08);
		float from = bay * TRAY_SLOT + 0.5;
		float length_m = TRAY_SLOT * (0.45 + 0.4 * hash2(vec2(lift, bay) + 61.0));
		float in_bay = step(from, s) * step(s, from + length_m);
		dark = max(dark, tray * in_bay * line_cover(abs(in_lift - 0.45), 0.22, fy));
		// Their shadow on the face: under a tray, beside a riser.
		value *= 1.0 - 0.35 * max(tray * in_bay * line_cover(abs(in_lift - 0.3), 0.08, fy),
			riser * in_span * line_cover(abs(s - x_riser - 0.07), 0.06, fs));
	}
	vec3 albedo = base_colour * value * tint;
	ALBEDO = mix(albedo, output_colour(CONDUIT), dark);
	ROUGHNESS = base_roughness;
}
