shader_type canvas_item;
render_mode unshaded;
uniform sampler2D screen_image : hint_screen_texture, repeat_disable, filter_linear;
uniform float strength = 0.0;

void fragment() {
	vec2 offset = SCREEN_UV - vec2(0.5);
	float edge = smoothstep(0.12, 0.68, length(offset));
	vec2 warp = offset * dot(offset, offset) * strength * 0.32;
	vec2 uv = SCREEN_UV + warp;
	vec2 streak = offset * strength * edge;
	vec3 c = texture(screen_image, uv).rgb * 0.28;
	c += texture(screen_image, uv - streak * 0.3).rgb * 0.24;
	c += texture(screen_image, uv - streak * 0.6).rgb * 0.20;
	c += texture(screen_image, uv - streak * 0.85).rgb * 0.16;
	c += texture(screen_image, uv - streak).rgb * 0.12;
	COLOR = vec4(c * (1.0 - edge * strength * 1.5), 1.0);
}
