shader_type spatial;
render_mode diffuse_burley, specular_schlick_ggx, cull_disabled, depth_prepass_alpha;

uniform sampler2D portrait_texture : source_color, filter_linear_mipmap, repeat_disable;
uniform sampler2D skin_albedo : source_color, filter_linear_mipmap, repeat_enable;
uniform sampler2D skin_normal : hint_normal, filter_linear_mipmap, repeat_enable;
uniform sampler2D skin_roughness : hint_default_white, filter_linear_mipmap, repeat_enable;
uniform bool has_skin_albedo = false;
uniform bool has_skin_normal = false;
uniform bool has_skin_roughness = false;
uniform vec4 skin_color : source_color = vec4(1.0);
uniform vec3 skin_tint = vec3(1.0);
uniform vec2 skin_uv_scale = vec2(1.0);
uniform vec2 skin_uv_offset = vec2(0.0);
uniform vec4 roughness_channel = vec4(1.0, 0.0, 0.0, 0.0);
uniform float roughness_value : hint_range(0.0, 1.0) = 0.5;
uniform float normal_scale = 1.0;
uniform float metallic_value = 0.0;
uniform float specular_value = 0.5;
uniform float portrait_strength : hint_range(0.0, 1.0) = 1.0;

void fragment() {
	vec2 skin_uv = UV * skin_uv_scale + skin_uv_offset;
	vec4 skin_sample = skin_color;
	if (has_skin_albedo) { skin_sample *= texture(skin_albedo, skin_uv); }
	vec3 skin = skin_sample.rgb * skin_tint;
	ALPHA = skin_sample.a;
	float inside_photo = step(0.0, UV2.x) * step(UV2.x, 1.0) * step(0.0, UV2.y) * step(UV2.y, 1.0);
	float blend = COLOR.a * portrait_strength * inside_photo;
	ALBEDO = mix(skin, texture(portrait_texture, UV2).rgb, blend);
	ROUGHNESS = roughness_value;
	if (has_skin_roughness) { ROUGHNESS *= dot(texture(skin_roughness, skin_uv), roughness_channel); }
	METALLIC = metallic_value;
	SPECULAR = specular_value;
	if (has_skin_normal) {
		NORMAL_MAP = texture(skin_normal, skin_uv).rgb;
		NORMAL_MAP_DEPTH = normal_scale;
	}
}
