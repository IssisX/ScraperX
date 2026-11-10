// The horizon past the far plane (environment job 4, lane C): ranges beyond
// the basin's back range, out to about 30 km, drawn in the sky because the
// camera's far plane (2,600 m, main.tscn) cannot hold them. Each layer is a
// crest line round the whole horizon at its own distance; its crest's angle
// above the eye's level comes from its height less the eye's (eye_height, fed
// by far_world.gd) and the earth's curve, so as he climbs the ranges sink and
// more of them show. Farther layers lie deeper in the air: paler, bluer, and
// lit a little on the side toward the sun. Needs hash/value_noise/fbm, so it
// is included after them.

uniform float eye_height = 2.0;
uniform float horizon_detail = 1.0;

// The crest's height (m) of layer k at direction az (the unit circle, xz).
float far_crest(vec2 around, float k) {
	float broad = fbm(around * (2.2 + k * 0.9) + vec2(k * 13.7, 5.1 - k * 3.3));
	float sharp = 1.0 - abs(2.0 * value_noise(around * (11.0 + k * 4.0) + vec2(3.3 * k, 1.7)) - 1.0);
	return mix(500.0, 2300.0 + 500.0 * k, broad * broad) + 180.0 * sharp - 260.0;
}

vec3 far_horizon(vec3 col, vec3 dir, vec3 haze, vec3 to_light, float day) {
	if (horizon_detail < 0.5 || dir.y > 0.2) {
		return col;
	}
	float h = length(dir.xz);
	if (h < 1e-4) {
		return col;
	}
	vec2 around = dir.xz / h;
	float elev = atan(dir.y, h);
	float facing = max(dot(normalize(vec3(-around.x, 0.4, -around.y)), to_light), 0.0);
	vec3 rock = mix(vec3(0.03, 0.035, 0.05), vec3(0.30, 0.34, 0.42), day);
	// From the farthest in.
	for (int i = 3; i >= 0; i--) {
		float k = float(i);
		float dist = 4500.0 * pow(1.85, k);
		float crest = far_crest(around, k);
		float angle = (crest - eye_height) / dist - dist / (2.0 * 6.371e6);
		float edge = 0.6 / dist;   // about a metre of soft crest at its range
		float cover = smoothstep(angle + edge, angle - edge, elev);
		if (cover <= 0.0) {
			continue;
		}
		// Snow on the high crests, more on the near side of the light.
		float below = (angle - elev) * dist;   // metres down from the crest
		float snow = smoothstep(1500.0, 2100.0, crest) * smoothstep(380.0, 60.0, below);
		vec3 face = rock * (0.75 + 0.45 * facing);
		face = mix(face, mix(vec3(0.1, 0.11, 0.14), vec3(0.78, 0.81, 0.86), day), snow * 0.8);
		float air = clamp(0.42 + 0.14 * k, 0.0, 0.9);
		col = mix(col, mix(face, haze, air), cover);
	}
	return col;
}
