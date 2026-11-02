extends Node3D
## The summit as the player sees it beyond its kit bodies (src/sim/summit.hpp):
## the searchlight's beam and lens, its dead ember while it waits, and the
## windsock on the runway arm's pole. Every position and every state is the
## native's: the lamp is a kit body that turns on its own motor, the beam is a
## child of it, and nothing here moves anything.

signal ignited

const LAMP_ENTITY := 2981
# The lamp's drum, in the lamp body's own frame (summit.cpp lamp_parts).
const TILT := 0.6
const DRUM_CENTRE := Vector3(1.0, 7.0, 0.0)
const DRUM_HALF_LENGTH := 4.6
const LENS_RADIUS := 3.05
# The beam: a cone from the lens, far enough to cross the sky and reach the
# mountains and the clouds.
const BEAM_LENGTH := 2400.0
const BEAM_FAR_RADIUS := 190.0
# The glare at the lens: what reads as a star at the top from far below.
const GLARE_SIZE := 36.0
const IGNITE_SECONDS := 4.0
# The windsock, on the pole at the runway arm's end (summit.cpp): the pole's
# top, and the wind's own swells (physics_world_movement.cpp apply_wind).
const POLE_TOP := Vector3(-49.1, 809.64, -188.88)
const WIND_BASE := 4.0
const WIND_GUST := 16.0
const SOCK_LENGTH := 3.6
const SOCK_RINGS := 6
const SOCK_SEGMENTS := 10

var _last_beam: Node3D
var _chains: Array[MeshInstance3D] = []
var _lamp: Node3D
var _beam: MeshInstance3D
var _beam_material: ShaderMaterial
var _lens: MeshInstance3D
var _lens_material: StandardMaterial3D
var _spot: SpotLight3D
var _ember: OmniLight3D
var _glare: MeshInstance3D
var _glare_material: StandardMaterial3D
var _sock: MeshInstance3D
var _lit := false
var _lit_age := 0.0
var _shown_lit := false
var _first_update := true


func setup(kit_root: Node3D) -> void:
	name = "SummitView"
	_lamp = kit_root.get_node_or_null("KitBody%d" % LAMP_ENTITY) as Node3D
	if _lamp != null:
		_far_visible(_lamp)
		_build_lamp_lights()
	_build_sock()
	_last_beam = kit_root.get_node_or_null("KitBody2968") as Node3D
	for i in 2:
		var link := MeshInstance3D.new()
		var mesh := CylinderMesh.new()
		mesh.top_radius = 0.035
		mesh.bottom_radius = 0.035
		mesh.height = 1.0
		mesh.radial_segments = 6
		link.mesh = mesh
		var material := StandardMaterial3D.new()
		material.albedo_color = Color(0.16, 0.18, 0.20)
		material.metallic = 0.8
		link.material_override = material
		add_child(link)
		_chains.append(link)


# What a kit body's parts are drawn to is its size's worth of metres; the dead
# lamp is the thing to see from the ground, so it is drawn at any distance.
func _far_visible(root: Node) -> void:
	for node in root.get_children():
		if node is GeometryInstance3D:
			(node as GeometryInstance3D).visibility_range_end = 0.0
		_far_visible(node)


func _beam_direction() -> Vector3:
	return Vector3(cos(TILT), sin(TILT), 0.0)


func _build_lamp_lights() -> void:
	var direction := _beam_direction()
	var lens_at := DRUM_CENTRE + direction * (DRUM_HALF_LENGTH + 0.16)
	var aim := Basis(Quaternion(Vector3.UP, direction))
	# The glowing face of the lens.
	var disc := CylinderMesh.new()
	disc.top_radius = LENS_RADIUS
	disc.bottom_radius = LENS_RADIUS
	disc.height = 0.06
	disc.radial_segments = 28
	disc.rings = 1
	_lens_material = StandardMaterial3D.new()
	_lens_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_lens_material.albedo_color = Color(0.05, 0.04, 0.03)
	_lens_material.emission_enabled = true
	_lens_material.emission = Color(1.0, 0.55, 0.2)
	_lens_material.emission_energy_multiplier = 0.0
	_lens = MeshInstance3D.new()
	_lens.name = "LensGlow"
	_lens.mesh = disc
	_lens.material_override = _lens_material
	_lens.transform = Transform3D(aim, lens_at)
	_lens.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_lens.visibility_range_end = 0.0
	_lamp.add_child(_lens)
	# The beam: a cone from the lens, brightest at the lamp and fading out.
	var cone := CylinderMesh.new()
	cone.top_radius = BEAM_FAR_RADIUS
	cone.bottom_radius = LENS_RADIUS * 0.9
	cone.height = BEAM_LENGTH
	cone.radial_segments = 36
	cone.rings = 1
	cone.cap_top = false
	cone.cap_bottom = false
	var shader := Shader.new()
	shader.code = """
shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_draw_never, fog_disabled;
uniform vec3 tint : source_color = vec3(1.0, 0.92, 0.7);
uniform float strength = 0.0;
uniform float beam_length = 1800.0;
varying float along;
void vertex() {
	along = clamp((VERTEX.y + 0.5 * beam_length) / beam_length, 0.0, 1.0);
}
void fragment() {
	float facing = abs(dot(normalize(NORMAL), normalize(VIEW)));
	float core = pow(facing, 2.2);
	float fade = pow(1.0 - along, 1.4);
	ALBEDO = tint;
	ALPHA = clamp(core * fade * strength * 0.8, 0.0, 1.0);
}
"""
	_beam_material = ShaderMaterial.new()
	_beam_material.shader = shader
	_beam = MeshInstance3D.new()
	_beam.name = "Beam"
	_beam.mesh = cone
	_beam.material_override = _beam_material
	_beam.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_beam.transform = Transform3D(aim, lens_at + direction * (BEAM_LENGTH * 0.5))
	_beam.visibility_range_end = 0.0
	_beam.extra_cull_margin = BEAM_LENGTH
	_beam.visible = false
	_lamp.add_child(_beam)
	# What it lights: a narrow spot along the beam, and the lamp's own ember.
	_spot = SpotLight3D.new()
	_spot.name = "BeamLight"
	_spot.transform = Transform3D(Basis(Quaternion(Vector3.FORWARD, direction)), lens_at + direction * 0.5)
	_spot.spot_range = 900.0
	_spot.spot_angle = 3.5
	_spot.spot_attenuation = 0.8
	_spot.light_energy = 0.0
	_spot.light_color = Color(1.0, 0.93, 0.78)
	_spot.shadow_enabled = false
	_spot.visible = false
	_lamp.add_child(_spot)
	var gradient := Gradient.new()
	gradient.set_color(0, Color(1.0, 0.96, 0.85, 1.0))
	gradient.set_color(1, Color(1.0, 0.75, 0.4, 0.0))
	var falloff := GradientTexture2D.new()
	falloff.gradient = gradient
	falloff.fill = GradientTexture2D.FILL_RADIAL
	falloff.fill_from = Vector2(0.5, 0.5)
	falloff.fill_to = Vector2(0.5, 0.0)
	falloff.width = 128
	falloff.height = 128
	_glare_material = StandardMaterial3D.new()
	_glare_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_glare_material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	_glare_material.blend_mode = BaseMaterial3D.BLEND_MODE_ADD
	_glare_material.billboard_mode = BaseMaterial3D.BILLBOARD_ENABLED
	_glare_material.albedo_texture = falloff
	_glare_material.disable_fog = true
	var quad := QuadMesh.new()
	quad.size = Vector2(GLARE_SIZE, GLARE_SIZE)
	_glare = MeshInstance3D.new()
	_glare.name = "Glare"
	_glare.mesh = quad
	_glare.material_override = _glare_material
	_glare.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_glare.visibility_range_end = 0.0
	_glare.position = lens_at + direction * 1.5
	_lamp.add_child(_glare)
	_ember = OmniLight3D.new()
	_ember.name = "Ember"
	_ember.transform = Transform3D(Basis.IDENTITY, lens_at + direction * 1.0)
	_ember.omni_range = 70.0
	_ember.light_color = Color(1.0, 0.5, 0.18)
	_ember.light_energy = 0.0
	_ember.shadow_enabled = false
	_lamp.add_child(_ember)


func _build_sock() -> void:
	var tool := SurfaceTool.new()
	tool.begin(Mesh.PRIMITIVE_TRIANGLES)
	var orange := Color(0.92, 0.38, 0.08)
	var white := Color(0.9, 0.88, 0.82)
	for ring in SOCK_RINGS:
		var r0 := lerpf(0.55, 0.22, float(ring) / SOCK_RINGS)
		var r1 := lerpf(0.55, 0.22, float(ring + 1) / SOCK_RINGS)
		var y0 := SOCK_LENGTH * float(ring) / SOCK_RINGS
		var y1 := SOCK_LENGTH * float(ring + 1) / SOCK_RINGS
		tool.set_color(orange if ring % 2 == 0 else white)
		for s in SOCK_SEGMENTS:
			var a0 := TAU * float(s) / SOCK_SEGMENTS
			var a1 := TAU * float(s + 1) / SOCK_SEGMENTS
			var p00 := Vector3(cos(a0) * r0, -y0, sin(a0) * r0)
			var p01 := Vector3(cos(a1) * r0, -y0, sin(a1) * r0)
			var p10 := Vector3(cos(a0) * r1, -y1, sin(a0) * r1)
			var p11 := Vector3(cos(a1) * r1, -y1, sin(a1) * r1)
			for point in [p00, p10, p01, p01, p10, p11]:
				tool.add_vertex(point)
	tool.generate_normals()
	var material := StandardMaterial3D.new()
	material.vertex_color_use_as_albedo = true
	material.roughness = 0.9
	material.cull_mode = BaseMaterial3D.CULL_DISABLED
	var sock_mesh := tool.commit()
	sock_mesh.surface_set_material(0, material)
	_sock = MeshInstance3D.new()
	_sock.name = "Windsock"
	_sock.mesh = sock_mesh
	_sock.position = POLE_TOP
	_sock.visibility_range_end = 900.0
	add_child(_sock)


# The wind the native blows (apply_wind), from the simulation's clock.
static func wind_at(t: float) -> float:
	var gust := clampf(0.35 + 0.35 * sin(TAU * t / 7.3) + 0.3 * sin(TAU * t / 2.9 + 1.0), 0.0, 1.0)
	return WIND_BASE + (WIND_GUST - WIND_BASE) * gust


func update_view(state: Dictionary, delta: float, clock: float) -> void:
	if _last_beam != null:
		for i in _chains.size():
			var x := -2.0 if i == 0 else 2.0
			var top := Vector3(-7.0 + x, 807.64, -184.0)
			var end := _last_beam.global_transform * Vector3(x, 0.3, 0.0)
			var run := end - top
			_chains[i].visible = not bool(state["sling_free"])
			_chains[i].transform = Transform3D(Basis(Quaternion(Vector3.UP, run.normalized())).scaled(
				Vector3(1.0, run.length(), 1.0)), (top + end) * 0.5)
	_lit = bool(state["lit"])
	if _first_update:
		# A world that opens already lit (the profile remembers the climb that
		# lit it) shows the beam at once, with no ignition.
		_first_update = false
		if _lit:
			_shown_lit = true
			_lit_age = IGNITE_SECONDS
	if _lit and not _shown_lit:
		_shown_lit = true
		_lit_age = 0.0
		ignited.emit()
	if _lit:
		_lit_age += delta
	var ignition := clampf(_lit_age / IGNITE_SECONDS, 0.0, 1.0) if _lit else 0.0
	if _lamp != null:
		_beam.visible = _lit
		_spot.visible = _lit
		_beam_material.set_shader_parameter("strength", ignition)
		_spot.light_energy = 14.0 * ignition
		# Dead, the lamp keeps a faint ember: a slow swell now and then, the
		# tell that something up there is waiting.
		var flicker := pow(maxf(0.0, sin(clock * 0.9) * sin(clock * 0.31 + 1.0)), 3.0)
		var glow := 0.25 + 1.2 * flicker
		if _lit:
			glow = lerpf(glow, 9.0, ignition)
		_lens_material.emission_energy_multiplier = glow
		_ember.light_energy = glow * 0.5
		# The glare: a faint amber ember while dead, a white star once lit.
		var glare := 0.06 + 0.25 * flicker
		var tint := Color(1.0, 0.6, 0.25)
		var size := 0.3
		if _lit:
			glare = lerpf(glare, 1.0, ignition)
			tint = tint.lerp(Color(1.0, 0.97, 0.9), ignition)
			size = lerpf(size, 1.0, ignition)
		_glare_material.albedo_color = Color(tint.r, tint.g, tint.b, glare)
		_glare.scale = Vector3.ONE * size
	# The sock streams west with the wind, hanging straighter in a lull.
	var wind := wind_at(clock)
	var lean := lerpf(0.2, 1.5, clampf(wind / 12.0, 0.0, 1.0))
	var flutter := 0.1 * sin(clock * 6.0) + 0.05 * sin(clock * 9.7)
	var direction := Vector3(-sin(lean), -cos(lean), flutter).normalized()
	_sock.transform = Transform3D(Basis(Quaternion(Vector3.DOWN, direction)), POLE_TOP)


# Where the beam points now, in the world (for a look along it).
func beam_world_direction() -> Vector3:
	if _lamp == null:
		return Vector3.UP
	return (_lamp.global_transform.basis * _beam_direction()).normalized()


func lamp_world_position() -> Vector3:
	if _lamp == null:
		return Vector3.ZERO
	return _lamp.global_transform * (DRUM_CENTRE + _beam_direction() * DRUM_HALF_LENGTH)
