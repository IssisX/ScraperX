shader_type spatial;
render_mode unshaded, blend_mix, depth_draw_never, cull_disabled;
// Lid shadow over the eyeball (the MetaHuman-style eye occlusion strip): dark at the lid margin,
// fading over ~3 mm. Without it the eyes read as balls stuck in holes.
uniform float strength = 1.0;
void fragment() {
	ALBEDO = vec3(0.025, 0.018, 0.016);
	ALPHA = clamp(COLOR.a * strength, 0.0, 1.0);
}
