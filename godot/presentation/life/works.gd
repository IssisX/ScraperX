extends Node3D
## The structure at work: presentation only, driven by what the world is
## actually doing.
##
## Beacons: every machine of BEACON_MIN_KG or more carries an amber beacon on
## its top. It turns and flashes while that machine moves (its speed sampled
## each frame, as the audio hears it) and for a moment after; a still machine's
## beacon is dark. All beacons are two MultiMesh draws.
##
## Steam: the tower's vent stacks breathe -- each on its own slow rhythm, puffs
## of steam rising and spreading. A pool of STEAM_POOL emitters is moved to the
## vents nearest the camera, so the cost is fixed however many vents there are.

const BEACON_MIN_KG := 250.0
const BEACON_MAX := 160
const BEACON_HOLD_S := 2.5
const BEACON_MOVING_MPS := 0.05
const STEAM_POOL := 4
const STEAM_RANGE_M := 140.0

var _beacon_nodes: Array[Node3D] = []
var _beacon_local := PackedVector3Array()
var _beacon_machine := PackedInt32Array()   # index into the machines list
var _beacon_lit := PackedFloat32Array()      # seconds of light left
var _beacon_angle := PackedFloat32Array()
var _domes: MultiMesh
var _flashes: MultiMesh
var _vents := PackedVector3Array()
var _steam: Array[CPUParticles3D] = []
var _steam_vent := PackedInt32Array()
var _clock := 0.0


func setup(vents: PackedVector3Array) -> void:
	_vents = vents
	_domes = _multimesh(_dome_mesh(), _beacon_material(false))
	_flashes = _multimesh(_flash_mesh(), _beacon_material(true))
	for i in STEAM_POOL:
		var steam := _steam_emitter()
		steam.name = "Steam%d" % i
		add_child(steam)
		_steam.append(steam)
		_steam_vent.append(-1)


# A beacon on `node` (a moving kit body) at `local` (its top), lit while the
# machine at `machine` in the machines list moves.
func add_beacon(node: Node3D, local: Vector3, machine: int) -> void:
	if _beacon_nodes.size() >= BEACON_MAX:
		return
	_beacon_nodes.append(node)
	_beacon_local.append(local)
	_beacon_machine.append(machine)
	_beacon_lit.append(0.0)
	_beacon_angle.append(float(_beacon_nodes.size()) * 1.3)


func lit_count() -> int:
	var count := 0
	for lit in _beacon_lit:
		if lit > 0.0:
			count += 1
	return count


func beacon_count() -> int:
	return _beacon_nodes.size()


# Where the steaming vents are, for a test to look at.
func steaming_at() -> Array:
	var out := []
	for i in STEAM_POOL:
		if _steam[i].emitting and _steam_vent[i] >= 0:
			out.append(_vents[_steam_vent[i]])
	return out


func steaming_count() -> int:
	var count := 0
	for steam in _steam:
		if steam.emitting:
			count += 1
	return count


func update(delta: float, camera: Vector3, machines: Array) -> void:
	_clock += delta
	_domes.visible_instance_count = _beacon_nodes.size()
	_flashes.visible_instance_count = _beacon_nodes.size()
	for i in _beacon_nodes.size():
		var node := _beacon_nodes[i]
		var machine: Dictionary = machines[_beacon_machine[i]]
		if float(machine["speed"]) > BEACON_MOVING_MPS:
			_beacon_lit[i] = BEACON_HOLD_S
		else:
			_beacon_lit[i] = maxf(0.0, _beacon_lit[i] - delta)
		var lit := _beacon_lit[i] > 0.0
		if lit:
			_beacon_angle[i] += delta * 7.0
		var at: Vector3 = node.global_transform * _beacon_local[i]
		if not node.visible:
			_domes.set_instance_transform(i, Transform3D(Basis().scaled(Vector3.ZERO), at))
			_flashes.set_instance_transform(i, Transform3D(Basis().scaled(Vector3.ZERO), at))
			continue
		_domes.set_instance_transform(i, Transform3D(Basis(), at))
		_domes.set_instance_custom_data(i, Color(1.0 if lit else 0.0, 0.0, 0.0, 0.0))
		_flashes.set_instance_transform(i, Transform3D(Basis(Vector3.UP, _beacon_angle[i]) if lit
			else Basis().scaled(Vector3.ZERO), at + Vector3(0.0, 0.1, 0.0)))
		_flashes.set_instance_custom_data(i, Color(1.0 if lit else 0.0, 0.0, 0.0, 0.0))
	_update_steam(camera)


func _update_steam(camera: Vector3) -> void:
	# The STEAM_POOL vents nearest the camera, in range.
	var nearest: Array = []
	for v in _vents.size():
		var d := _vents[v].distance_to(camera)
		if d < STEAM_RANGE_M:
			nearest.append([d, v])
	nearest.sort_custom(func(a: Array, b: Array) -> bool: return a[0] < b[0])
	for i in STEAM_POOL:
		var steam := _steam[i]
		if i >= nearest.size():
			steam.emitting = false
			_steam_vent[i] = -1
			continue
		var v: int = nearest[i][1]
		if _steam_vent[i] != v:
			_steam_vent[i] = v
			steam.global_position = _vents[v]
			steam.restart()
		# Each vent breathes on its own slow rhythm: about 4 s of steam, 3 s quiet.
		var rhythm := fmod(_clock + float(v) * 2.3, 7.0)
		steam.emitting = rhythm < 4.0


func _multimesh(mesh: Mesh, material: Material) -> MultiMesh:
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.use_custom_data = true
	mm.mesh = mesh
	mm.instance_count = BEACON_MAX
	mm.visible_instance_count = 0
	var instance := MultiMeshInstance3D.new()
	instance.multimesh = mm
	instance.material_override = material
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	instance.custom_aabb = AABB(Vector3(-400, -10, -500), Vector3(800, 1700, 800))
	add_child(instance)
	return mm


# The dome glows amber when lit; the flash is two crossed blades of light
# that the beacon turns, drawn additively.
func _beacon_material(flash: bool) -> ShaderMaterial:
	var shader := Shader.new()
	if flash:
		shader.code = """
shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_draw_never;
varying float lit;
void vertex() { lit = INSTANCE_CUSTOM.r; }
void fragment() {
	float edge = 1.0 - abs(UV.y - 0.5) * 2.0;
	float along = 1.0 - UV.x;
	// Seen from a few metres, the blades sweep across the whole view: they
	// fade in with distance, so a rider beside the beacon is not dazzled.
	float near = smoothstep(2.5, 7.0, length(VERTEX));
	ALBEDO = vec3(1.0, 0.55, 0.12) * lit * edge * along * 1.6 * near;
}
"""
	else:
		shader.code = """
shader_type spatial;
varying float lit;
void vertex() { lit = INSTANCE_CUSTOM.r; }
void fragment() {
	ALBEDO = mix(vec3(0.35, 0.18, 0.04), vec3(1.0, 0.6, 0.15), lit);
	// Bright enough to bloom from across the tower; close up, a lamp.
	float near = smoothstep(2.5, 7.0, length(VERTEX));
	EMISSION = vec3(1.0, 0.5, 0.1) * lit * mix(0.6, 4.0, near);
	ROUGHNESS = 0.3;
}
"""
	var material := ShaderMaterial.new()
	material.shader = shader
	return material


func _dome_mesh() -> Mesh:
	var st := SurfaceTool.new()
	var base := CylinderMesh.new()
	base.top_radius = 0.1
	base.bottom_radius = 0.11
	base.height = 0.06
	base.radial_segments = 10
	st.append_from(base, 0, Transform3D(Basis(), Vector3(0.0, 0.03, 0.0)))
	var dome := SphereMesh.new()
	dome.radius = 0.085
	dome.height = 0.17
	dome.radial_segments = 10
	dome.rings = 5
	st.append_from(dome, 0, Transform3D(Basis(), Vector3(0.0, 0.1, 0.0)))
	return st.commit()


func _flash_mesh() -> Mesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	for blade in 2:
		var dir := Vector3.RIGHT if blade == 0 else Vector3.LEFT
		var side := Vector3.FORWARD * 0.0
		var up := Vector3.UP * 0.07
		var length := 0.9
		var a := Vector3.ZERO - up
		var b := dir * length - up * 2.5
		var c := dir * length + up * 2.5
		var d := Vector3.ZERO + up
		for v in [[a, Vector2(0, 0)], [b, Vector2(1, 0)], [c, Vector2(1, 1)],
				[a, Vector2(0, 0)], [c, Vector2(1, 1)], [d, Vector2(0, 1)]]:
			st.set_uv(v[1])
			st.add_vertex(v[0] + side)
	return st.commit()


func _steam_emitter() -> CPUParticles3D:
	var steam := CPUParticles3D.new()
	steam.amount = 30
	steam.lifetime = 3.6
	steam.emitting = false
	steam.local_coords = false
	steam.direction = Vector3(0.0, 1.0, 0.0)
	steam.spread = 14.0
	steam.initial_velocity_min = 1.6
	steam.initial_velocity_max = 2.4
	steam.gravity = Vector3(0.25, 0.35, 0.0)
	steam.damping_min = 0.5
	steam.damping_max = 0.9
	steam.scale_amount_min = 0.6
	steam.scale_amount_max = 1.0
	var grow := Curve.new()
	grow.add_point(Vector2(0.0, 0.35))
	grow.add_point(Vector2(1.0, 2.6))
	steam.scale_amount_curve = grow
	var fade := Gradient.new()
	fade.set_color(0, Color(0.93, 0.94, 0.96, 0.62))
	fade.set_color(1, Color(0.92, 0.93, 0.95, 0.0))
	steam.color_ramp = fade
	var quad := QuadMesh.new()
	quad.size = Vector2(1.8, 1.8)
	var material := StandardMaterial3D.new()
	material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.billboard_mode = BaseMaterial3D.BILLBOARD_PARTICLES
	material.vertex_color_use_as_albedo = true
	material.albedo_texture = _puff_texture()
	quad.material = material
	steam.mesh = quad
	return steam


# A soft round puff, generated once.
func _puff_texture() -> ImageTexture:
	var size := 32
	var image := Image.create(size, size, false, Image.FORMAT_RGBA8)
	for y in size:
		for x in size:
			var d := Vector2(x - size * 0.5 + 0.5, y - size * 0.5 + 0.5).length() / (size * 0.5)
			var a := clampf(1.0 - d, 0.0, 1.0)
			image.set_pixel(x, y, Color(1.0, 1.0, 1.0, a * a))
	return ImageTexture.create_from_image(image)
