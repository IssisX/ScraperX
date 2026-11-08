shader_type spatial;
// J-059 black eye ring (and lid): a flat quad hugging the eye socket.
// At grazing angles a flat quad reads as a thin rim standing off the skin
// (a monocle rim, #309 eye check). Fade its alpha by the facing angle
// (N.V) so the ring is only visible where it can sit on the skin: full on
// at the front and three-quarters, gone at the side. The fade is per-pixel
// at render time, so it is right for every camera with no per-frame script
// work and no dependence on when the injury layer last updated.
// ALBEDO/ALPHA replicate StandardMaterial3D's albedo texture x albedo
// colour; only the fade is added, so the front, three-quarter and close-up
// stills are unchanged (facing ~1 there). fade_end 0.65 keeps the
// three-quarter view (39 deg off the face normal, facing ~0.78) at full
// alpha; fade_start 0.3 drops the side view (75 deg, facing ~0.26) to ~0.
render_mode blend_mix, depth_draw_opaque, cull_back;

uniform sampler2D albedo_texture : source_color;
uniform vec4 albedo_color : source_color = vec4(1.0);
uniform float fade_start = 0.3;
uniform float fade_end = 0.65;

void fragment() {
	vec4 tex = texture(albedo_texture, UV);
	float facing = clamp(dot(NORMAL, VIEW), 0.0, 1.0);
	float fade = smoothstep(fade_start, fade_end, facing);
	ALBEDO = tex.rgb * albedo_color.rgb;
	ALPHA = tex.a * albedo_color.a * fade;
	ROUGHNESS = 0.6;
}
