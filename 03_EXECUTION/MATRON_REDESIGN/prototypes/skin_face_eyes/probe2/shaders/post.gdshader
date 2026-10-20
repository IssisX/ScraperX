shader_type canvas_item;
// A phone camera, not a film: sensor noise that grows in the dark (luma + a little chroma),
// a slight lens softness, faint lateral chromatic aberration, gentle vignette.
uniform float night = 0.0;
uniform float seed = 1.0;
float h(vec2 p) { return fract(sin(dot(p + seed, vec2(12.9898, 78.233))) * 43758.5453); }
void fragment() {
	vec2 uv = UV; vec2 px = TEXTURE_PIXEL_SIZE;
	vec2 c = uv - 0.5;
	float ca = 0.0012 * length(c) * 2.0;
	vec3 col;
	col.r = texture(TEXTURE, uv + c * ca).r;
	col.g = texture(TEXTURE, uv).g;
	col.b = texture(TEXTURE, uv - c * ca).b;
	// softness: a light 5-tap blur mixed in (the phone lens + its denoiser)
	vec3 b = (texture(TEXTURE, uv + vec2(px.x, 0)).rgb + texture(TEXTURE, uv - vec2(px.x, 0)).rgb
		+ texture(TEXTURE, uv + vec2(0, px.y)).rgb + texture(TEXTURE, uv - vec2(0, px.y)).rgb) * 0.25;
	col = mix(col, b, 0.35 + 0.25 * night);
	float lum = dot(col, vec3(0.2126, 0.7152, 0.0722));
	float amp = mix(0.012, 0.035, night) * (0.6 + 0.8 * (1.0 - smoothstep(0.0, 0.5, lum)));
	float g = (h(uv * 1024.0) + h(uv * 1024.0 + 7.1) - 1.0);
	vec3 cn = vec3(h(uv * 1024.0 + 3.3), h(uv * 1024.0 + 5.7), h(uv * 1024.0 + 9.1)) - 0.5;
	col += g * amp + cn * amp * 0.5 * night;
	col *= 1.0 - 0.28 * dot(c, c) * 2.0;
	COLOR = vec4(max(col, 0.0), 1.0);
}
