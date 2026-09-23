shader_type sky;
// Alpine sky over the yard: a height-graded dome coloured by the sun's
// elevation, the sun (or, at night, the moon) drawn from the one shadowed
// DirectionalLight3D (LIGHT0), a drifting cloud deck lit from that light, and
// stars that come out as the day fades. Cheap enough for a phone: value noise,
// four octaves, and no loops over lights.

uniform float day_factor : hint_range(0.0, 1.0) = 1.0;    // 0 night .. 1 day
uniform float dusk_factor : hint_range(0.0, 1.0) = 0.0;   // warm band at low sun
uniform bool light_is_moon = false;
uniform vec3 zenith_day : source_color = vec3(0.17, 0.34, 0.66);
uniform vec3 horizon_day : source_color = vec3(0.66, 0.73, 0.80);
uniform vec3 zenith_night : source_color = vec3(0.022, 0.032, 0.07);
uniform vec3 horizon_night : source_color = vec3(0.08, 0.10, 0.155);
uniform vec3 dusk_glow : source_color = vec3(0.98, 0.52, 0.24);
uniform vec3 ground_haze : source_color = vec3(0.34, 0.37, 0.40);
uniform float cloud_coverage : hint_range(0.0, 1.0) = 0.52;
uniform vec2 cloud_wind = vec2(0.0035, 0.0012);

float hash(vec2 p) {
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}

float value_noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash(i), hash(i + vec2(1.0, 0.0)), u.x),
		mix(hash(i + vec2(0.0, 1.0)), hash(i + vec2(1.0, 1.0)), u.x), u.y);
}

float fbm(vec2 p) {
	float sum = 0.0;
	float amplitude = 0.5;
	for (int octave = 0; octave < 4; octave++) {
		sum += amplitude * value_noise(p);
		p = p * 2.03 + vec2(17.1, 9.3);
		amplitude *= 0.5;
	}
	return sum;
}

void sky() {
	vec3 dir = normalize(EYEDIR);
	float up = dir.y;

	// Dome: zenith to horizon, day to night, with a warm band toward a low sun.
	float height = clamp(up, 0.0, 1.0);
	vec3 zenith = mix(zenith_night, zenith_day, day_factor);
	vec3 horizon = mix(horizon_night, horizon_day, day_factor);
	vec3 col = mix(horizon, zenith, pow(height, 0.55));
	vec3 to_light = LIGHT0_ENABLED ? normalize(LIGHT0_DIRECTION) : vec3(0.0, 1.0, 0.0);
	float facing = max(dot(dir, to_light), 0.0);
	float band = exp(-abs(up) * 7.0);
	if (!light_is_moon) {
		col += dusk_glow * dusk_factor * band * (0.35 + 0.65 * pow(facing, 3.0));
	}
	// Below the horizon: haze over the valley, never a bare void.
	col = mix(col, mix(horizon, ground_haze * mix(0.18, 1.0, day_factor), 0.7),
		smoothstep(0.0, -0.08, up));

	// Stars, above the horizon only, fading in with the night.
	if (up > 0.0 && day_factor < 0.6) {
		vec2 star_cell = floor(dir.xz / (up + 0.35) * 420.0);
		float star = step(0.9975, hash(star_cell));
		col += vec3(star) * (1.0 - day_factor / 0.6) * smoothstep(0.0, 0.25, up) * 0.9;
	}

	// The light itself: a hard disc with a soft halo. The moon is smaller,
	// cooler and has no bloom-worthy core.
	if (LIGHT0_ENABLED && !AT_CUBEMAP_PASS) {
		float angle = acos(clamp(dot(dir, to_light), -1.0, 1.0));
		if (light_is_moon) {
			col += vec3(0.78, 0.84, 0.95) * (1.0 - smoothstep(0.0085, 0.0105, angle)) * 1.4;
			col += vec3(0.30, 0.36, 0.50) * exp(-angle * 18.0) * 0.25;
		} else {
			vec3 sun_col = mix(vec3(1.0, 0.62, 0.34), vec3(1.0, 0.96, 0.88), clamp(to_light.y * 3.0, 0.0, 1.0));
			col += sun_col * (1.0 - smoothstep(0.0095, 0.0125, angle)) * 18.0;
			col += sun_col * exp(-angle * 14.0) * 0.45 * day_factor;
			col += sun_col * exp(-angle * 3.5) * 0.06 * day_factor;
		}
	}

	// Cloud deck: a plane far overhead, so clouds foreshorten toward the
	// horizon and thin into the haze there.
	if (up > 0.015) {
		vec2 uv = dir.xz / (up + 0.08) * 0.9 + TIME * cloud_wind;
		float shape = fbm(uv * 1.6);
		float detail = fbm(uv * 5.3 + 3.1);
		float density = smoothstep(1.0 - cloud_coverage, 1.0 - cloud_coverage + 0.28,
			shape * 0.78 + detail * 0.22);
		density *= smoothstep(0.015, 0.22, up);
		// Lit side toward the light, darker bellies, warm rims at dusk.
		float lit = 0.55 + 0.45 * facing;
		vec3 cloud_day = mix(vec3(0.50, 0.53, 0.58), vec3(1.0, 0.99, 0.97), lit);
		cloud_day = mix(cloud_day, dusk_glow * 1.1, dusk_factor * 0.55 * (0.3 + 0.7 * facing));
		vec3 cloud_night = vec3(0.045, 0.052, 0.075) + vec3(0.10, 0.11, 0.15) * facing * float(light_is_moon);
		vec3 cloud = mix(cloud_night, cloud_day, day_factor) * (0.82 + 0.18 * (1.0 - detail));
		col = mix(col, cloud, density * 0.92);
	}

	COLOR = col;
}
