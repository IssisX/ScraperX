extends Node3D
## One joint's wound, local +y out of it. Before the tear: a strain band
## (cloth fraying open, skin stretched and bruising, a split gaping in the
## last fifth). Torn: ragged flaps, flesh, the snapped bone, and tendon and
## sinew strands hanging off it as short verlet ropes.

const Kit := preload("res://presentation/life/gore/gore_kit.gd")

var radius := 0.05
var seed := 1
var flap_len := 0.06
var bone_out := 0.05
var cloth := Color(0.12, 0.14, 0.11)
var skin := Color(0.78, 0.6, 0.52)
var strands := 6
var strand_len := Vector2(0.06, 0.2)

var _band: MeshInstance3D
var _band_mat: ShaderMaterial
var _end: MeshInstance3D
var _sinew: MeshInstance3D
var _im: ImmediateMesh
var _points: Array = []    # per strand: Array[Vector3] world
var _prev: Array = []
var _anchors: Array = []   # per strand: local anchor
var _rest: Array = []      # per strand: segment length
var torn := false
const SEGMENTS := 7


func setup(r: float, s: int, cloth_color: Color, skin_color: Color, flaps: float, bone: float) -> void:
	radius = r
	seed = s
	cloth = cloth_color
	skin = skin_color
	flap_len = flaps
	bone_out = bone
	_band = MeshInstance3D.new()
	var cyl := CylinderMesh.new()
	cyl.top_radius = r * 1.04
	cyl.bottom_radius = r * 1.04
	cyl.height = 0.12
	cyl.radial_segments = 24
	cyl.rings = 12
	_band.mesh = cyl
	_band_mat = Kit.strain_material(cloth, skin, float(s))
	_band.material_override = _band_mat
	_band.visible = false
	add_child(_band)
	_end = MeshInstance3D.new()
	_end.mesh = Kit.torn_end(r, s, bone, flaps)
	var flap_mat := StandardMaterial3D.new()
	flap_mat.albedo_color = skin.lerp(Color(0.5, 0.05, 0.05), 0.25)
	flap_mat.vertex_color_use_as_albedo = false
	flap_mat.cull_mode = BaseMaterial3D.CULL_DISABLED
	flap_mat.roughness = 0.35
	# Skin outside, blood inside: the flaps' inner faces read red.
	var inner := ShaderMaterial.new()
	inner.shader = Kit.shader("flap", """
shader_type spatial;
render_mode cull_disabled;
uniform vec4 skin : source_color;
uniform vec4 cloth : source_color;
float h(vec2 p) { return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453); }
void fragment() {
	float b = COLOR.r * (0.6 + 0.4 * h(floor(UV * vec2(40.0, 12.0))));
	vec3 outside = mix(cloth.rgb, skin.rgb, step(0.45, UV.y));
	outside = mix(outside, vec3(0.4, 0.02, 0.02), b * 0.55);
	vec3 inside = mix(vec3(0.6, 0.06, 0.05), vec3(0.85, 0.7, 0.45), step(0.8, h(floor(UV * 30.0))) * 0.5);
	ALBEDO = FRONT_FACING ? outside : inside;
	ROUGHNESS = FRONT_FACING ? mix(0.75, 0.2, b) : 0.1;
	SPECULAR = 0.7;
}
""")
	inner.set_shader_parameter("skin", skin)
	inner.set_shader_parameter("cloth", cloth)
	_end.set_surface_override_material(0, inner)
	_end.set_surface_override_material(1, Kit.material("flesh"))
	_end.set_surface_override_material(2, Kit.material("bone"))
	_end.visible = false
	add_child(_end)
	_im = ImmediateMesh.new()
	_sinew = MeshInstance3D.new()
	_sinew.mesh = _im
	_sinew.material_override = Kit.material("sinew")
	_sinew.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_sinew.visible = false
	_sinew.top_level = true
	add_child(_sinew)
	var rng := RandomNumberGenerator.new()
	rng.seed = s * 7919
	for i in strands:
		var a := rng.randf() * TAU
		var d := rng.randf_range(0.1, 0.8) * r
		_anchors.append(Vector3(sin(a) * d, r * 0.25, cos(a) * d))
		_rest.append(rng.randf_range(strand_len.x, strand_len.y) / SEGMENTS)


## Before the tear: strain 0..1 toward the tear.
var band_on := true

func set_strain(strain: float) -> void:
	if torn or _band == null or not band_on:
		return
	var s := clampf(strain, 0.0, 1.0)
	_band.visible = s > 0.02
	_band_mat.set_shader_parameter("strain", s)
	_band_mat.set_shader_parameter("split", clampf((s - 0.78) / 0.22, 0.0, 1.0))
	_band_mat.set_shader_parameter("stretch_m", 0.05 * s * s)


func tear() -> void:
	if torn or _end == null:
		return
	torn = true
	_band.visible = false
	_end.visible = true
	_sinew.visible = true
	_points.clear()
	_prev.clear()
	var xf := global_transform
	for i in strands:
		var pts: Array[Vector3] = []
		for k in SEGMENTS + 1:
			pts.append(xf * (_anchors[i] + Vector3(0, float(k) * float(_rest[i]), 0)))
		_points.append(pts)
		_prev.append(pts.duplicate())


func heal() -> void:
	torn = false
	if _end != null:
		_end.visible = false
		_sinew.visible = false
		_band.visible = false


func _process(delta: float) -> void:
	if not torn or not is_visible_in_tree() or delta <= 0.0:
		return
	var cam := get_viewport().get_camera_3d()
	if cam != null and cam.global_position.distance_to(global_position) > 20.0:
		return
	var xf := global_transform
	var dt := minf(delta, 1.0 / 30.0)
	var g := Vector3(0, -9.8, 0) * dt * dt
	for i in _points.size():
		var pts: Array = _points[i]
		var prev: Array = _prev[i]
		pts[0] = xf * _anchors[i]
		for k in range(1, pts.size()):
			var p: Vector3 = pts[k]
			var v: Vector3 = (p - prev[k]) * 0.96
			prev[k] = p
			pts[k] = p + v + g
		for it in 3:
			pts[0] = xf * _anchors[i]
			for k in range(1, pts.size()):
				var a: Vector3 = pts[k - 1]
				var b: Vector3 = pts[k]
				var d := b - a
				var l := d.length()
				if l < 1.0e-6:
					continue
				var corr := d * (1.0 - float(_rest[i]) / l)
				if k == 1:
					pts[k] = b - corr
				else:
					pts[k - 1] = a + corr * 0.5
					pts[k] = b - corr * 0.5
	_draw_strands(cam)


func _draw_strands(cam: Camera3D) -> void:
	_im.clear_surfaces()
	_im.surface_begin(Mesh.PRIMITIVE_TRIANGLES)
	var eye := cam.global_position if cam != null else global_position + Vector3(0, 0, 1)
	for i in _points.size():
		var pts: Array = _points[i]
		var w0 := radius * 0.11
		for k in range(pts.size() - 1):
			var a: Vector3 = pts[k]
			var b: Vector3 = pts[k + 1]
			var side := (b - a).cross(eye - a).normalized()
			var wa := w0 * (1.0 - float(k) / pts.size()) + 0.0012
			var wb := w0 * (1.0 - float(k + 1) / pts.size()) + 0.0012
			var c := Color(0.55, 0.08, 0.07) if i % 2 == 0 else Color(0.86, 0.8, 0.68)  # tendon pale
			for v in [a - side * wa, a + side * wa, b + side * wb, a - side * wa, b + side * wb, b - side * wb]:
				_im.surface_set_color(c)
				_im.surface_add_vertex(v)
	_im.surface_end()


## Where to spray from and which way (world).
func mouth() -> Transform3D:
	return global_transform
