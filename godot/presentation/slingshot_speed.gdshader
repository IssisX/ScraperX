shader_type canvas_item;
uniform sampler2D screen_image : hint_screen_texture, repeat_disable, filter_linear;
uniform float strength : hint_range(0.0, 0.05) = 0.0;
uniform float soft_blur : hint_range(0.0, 1.0) = 0.0;

void fragment() {
	vec2 offset = SCREEN_UV - vec2(0.5);
	vec2 uv = SCREEN_UV + offset * dot(offset, offset) * strength * 0.32;
	// Nine bounded samples, independent of speed. The shared
	// orbit clock narrows the soft opening to a sharp full revolution.
	vec2 step_uv = SCREEN_PIXEL_SIZE * (soft_blur * 3.0);
	vec4 c = texture(screen_image, uv) * 0.25;
	c += texture(screen_image, uv + vec2(step_uv.x, 0.0)) * 0.125;
	c += texture(screen_image, uv - vec2(step_uv.x, 0.0)) * 0.125;
	c += texture(screen_image, uv + vec2(0.0, step_uv.y)) * 0.125;
	c += texture(screen_image, uv - vec2(0.0, step_uv.y)) * 0.125;
	c += texture(screen_image, uv + step_uv) * 0.0625;
	c += texture(screen_image, uv - step_uv) * 0.0625;
	c += texture(screen_image, uv + vec2(step_uv.x, -step_uv.y)) * 0.0625;
	c += texture(screen_image, uv + vec2(-step_uv.x, step_uv.y)) * 0.0625;
	COLOR = c;
}
