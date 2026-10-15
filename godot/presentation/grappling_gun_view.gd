extends Node3D
class_name GrapplingGunView

# Presentation only. Main supplies already-interpolated native world poses.
# No aim, inventory, capture, line payout, or physical state is owned here.
const HOOK_CAPACITY := 4
const ROPE_RADIUS := 0.014
# The end face of the authored barrel, in tool mesh coordinates.
const MUZZLE_LOCAL := Vector3(0, 0.012, -0.297)
const RECEIVER_OFFSET := Vector3(0.105, 0, 0)
const MODEL_SCALE := 0.5

var _camera: Camera3D
var _tool: MeshInstance3D
var _rack_tool: MeshInstance3D
var _hooks: Array[MeshInstance3D] = []
var _line: MeshInstance3D
var _built := false
var _hook_geometry: Array = []
var _geometry_error_reported := false
var _spool: MeshInstance3D
var _lamp_material: StandardMaterial3D
var recoil_amount := 0.0
var spool_phase := 0.0
var launch_receipts := 0


func reset_feedback() -> void:
	recoil_amount = 0.0
	if _lamp_material != null:
		_lamp_material.emission_energy_multiplier = 0.15


func present_feedback(feedback: Dictionary, delta: float, reduced_motion := false) -> void:
	if not bool(feedback.get("equipped", false)):
		reset_feedback()
		return
	if bool(feedback.get("fresh", false)):
		if bool(feedback.get("launch", false)):
			launch_receipts += 1
			recoil_amount = minf(recoil_amount + 1.0, 1.0)
		# Decorative drum face follows actual native take-up distance. Its
		# phase never changes the straight native line or its paid-out length.
		spool_phase = fposmod(spool_phase + float(feedback.get("wound_m", 0.0)) / 0.035, TAU)
	if reduced_motion:
		recoil_amount = 0.0
	elif delta > 0.0:
		recoil_amount *= exp(-minf(delta, 0.1) / 0.065)
		if recoil_amount < 0.0001:
			recoil_amount = 0.0
	if is_instance_valid(_spool):
		_spool.rotation.z = spool_phase
	if _lamp_material != null:
		var danger := clampf(float(feedback.get("danger", 0.0)), 0.0, 1.0)
		_lamp_material.emission = Color("dc9a42").lerp(Color("ed4938"), danger)
		_lamp_material.emission_energy_multiplier = 0.15 + 1.2 * clampf(float(feedback.get("power", 0.0)), 0.0, 1.0) + 2.0 * danger


func configure_hook_geometry(parts: Array) -> void:
	if _built:
		push_error("Native hook geometry must be configured before the view is built")
		return
	_hook_geometry.clear()
	for part in parts:
		if not (part is Dictionary) or not (part.get("polygon") is PackedVector3Array):
			push_error("Native hook geometry requires convex XY prism dictionaries")
			return
		var polygon: PackedVector3Array = part["polygon"]
		var z_low := float(part.get("z_low", NAN))
		var z_high := float(part.get("z_high", NAN))
		if polygon.size() < 3 or not is_finite(z_low) or not is_finite(z_high) or z_high <= z_low:
			push_error("Native hook geometry has invalid prism dimensions")
			return
		var area := 0.0
		for i in polygon.size():
			var a := polygon[i]
			var b := polygon[(i + 1) % polygon.size()]
			var c := polygon[(i + 2) % polygon.size()]
			if not a.is_finite() or a.z != 0.0 or (b - a).cross(c - b).z < 0.0:
				push_error("Native hook geometry requires finite convex CCW XY polygons")
				return
			area += a.x * b.y - b.x * a.y
		if area <= 0.0:
			push_error("Native hook geometry requires positive polygon area")
			return
	# Copy only after every prism validates: never keep a partial hook.
	_hook_geometry = parts.duplicate(true)
	_geometry_error_reported = false


func configure(camera: Camera3D) -> void:
	_camera = camera
	_build_once()


func _launch_frame(state: Dictionary) -> Variant:
	var origin: Variant = state.get("launch_origin")
	var supplied: Variant = state.get("launch_direction")
	if not (origin is Vector3) or not (supplied is Vector3):
		return null
	var direction: Vector3 = supplied
	if not origin.is_finite() or not direction.is_finite() or direction.length_squared() < 0.000001:
		return null
	direction = direction.normalized()
	# Project the camera's right onto the native aim plane. This retains
	# natural visual roll without look_at's world-up singularity at vertical.
	var right := _camera.global_basis.x
	right -= direction * right.dot(direction)
	if right.length_squared() < 0.000001:
		var reference := Vector3.UP if absf(direction.y) < 0.9 else Vector3.FORWARD
		right = direction.cross(reference)
	right = right.normalized()
	var basis := Basis(right, right.cross(direction).normalized(), -direction).scaled(Vector3.ONE * MODEL_SCALE)
	return Transform3D(basis, origin - basis * MUZZLE_LOCAL)


func update_state(state: Dictionary) -> void:
	_build_once()
	if not _built:
		return
	var frame: Variant = _launch_frame(state) if is_instance_valid(_camera) else null
	_tool.visible = bool(state.get("equipped", false)) and frame is Transform3D
	if _tool.visible:
		# Recoil belongs to the tool presentation only. Camera/aim/player and
		# native recoil remain owned by their existing authoritative paths.
		var kick := Transform3D(Basis.from_euler(Vector3(-0.075, 0.0, 0.012) * recoil_amount),
			Vector3(0.0, -0.008, 0.035) * recoil_amount)
		_tool.global_transform = frame * kick
	_rack_tool.visible = bool(state.get("available", false)) and not bool(state.get("acquired", false))
	if _rack_tool.visible:
		var rack: Vector3 = state.get("rack_position", Vector3.ZERO)
		_rack_tool.global_transform = Transform3D(Basis.IDENTITY.scaled(Vector3.ONE * MODEL_SCALE), rack)

	# active means the native body is present, including spent/released hooks.
	# Equip and line state never suppress bodies that still exist in the world.
	var supplied: Variant = state.get("hooks")
	var count := 0
	if supplied is Array:
		count = mini(supplied.size(), HOOK_CAPACITY)
	for i in HOOK_CAPACITY:
		var instance := _hooks[i]
		instance.visible = false
		if i >= count or not (supplied[i] is Dictionary):
			continue
		var hook: Dictionary = supplied[i]
		if not bool(hook.get("active", false)):
			continue
		var point: Variant = hook.get("position")
		var orientation: Variant = hook.get("rotation")
		if not (point is Vector3) or not (orientation is Quaternion):
			continue
		var native_position: Vector3 = point
		var native_rotation: Quaternion = orientation
		if not native_position.is_finite() or not native_rotation.is_finite():
			continue
		if not native_rotation.is_normalized():
			continue
		instance.global_transform = Transform3D(Basis(native_rotation), native_position)
		instance.visible = true

	_line.visible = false
	if not bool(state.get("line_attached", false)):
		return
	var start_value: Variant = state.get("rope_start")
	var end_value: Variant = state.get("rope_end")
	if not (start_value is Vector3) or not (end_value is Vector3):
		return
	var start: Vector3 = start_value
	var end: Vector3 = end_value
	if not start.is_finite() or not end.is_finite():
		return
	var span := end - start
	var length := span.length()
	if not is_finite(length) or length <= 0.000001:
		return
	var axis := span / length
	# Pick a stable perpendicular without look_at's vertical singularity.
	var reference := Vector3.UP
	if absf(axis.y) > 0.95:
		reference = Vector3.RIGHT
	var cross_axis := reference.cross(axis).normalized()
	var basis := Basis(cross_axis * ROPE_RADIUS, axis * length,
		cross_axis.cross(axis) * ROPE_RADIUS)
	_line.global_transform = Transform3D(basis, start + span * 0.5)
	_line.visible = true
	# A straight native massless line has no rendered sag/wrap/contact claims.
	# paid_out_length and tension never invent a different endpoint or motion.


func _build_once() -> void:
	if _built:
		return
	if _hook_geometry.is_empty():
		if not _geometry_error_reported:
			push_error("Native hook geometry is absent; configure_hook_geometry must precede view build")
			_geometry_error_reported = true
		return
	_built = true
	var paint := _finish(Color("34444b"), 0.35, 0.72)
	var steel := _finish(Color("b4bdc0"), 0.95, 0.3)
	var ochre := _finish(Color("cc822d"), 0.3, 0.62)
	var dark := _finish(Color("20272b"), 0.65, 0.55)
	var tool_mesh := ArrayMesh.new()
	var surface := SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	# Compact receiver, angled grip, shoulder stub, and protective canister.
	_append(surface, _box(Vector3(0.095, 0.10, 0.23)), Vector3(0, 0, 0.015) + RECEIVER_OFFSET)
	_append(surface, _box(Vector3(0.065, 0.135, 0.075)),
		Vector3(0, -0.103, 0.025) + RECEIVER_OFFSET, Basis(Vector3.RIGHT, -0.22))
	_append(surface, _box(Vector3(0.08, 0.065, 0.09)), Vector3(0, 0.01, 0.155) + RECEIVER_OFFSET)
	_append(surface, _cylinder(0.041, 0.16), Vector3(0.078, -0.014, 0.016) + RECEIVER_OFFSET,
		Basis(Vector3.RIGHT, PI * 0.5))
	_commit(surface, tool_mesh, paint)

	surface = SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	_append_guide(surface, 0.034, 0.025, -0.295, -0.065)
	_append(surface, _box(Vector3(0.11, 0.02, 0.05)), Vector3(0.0525, -0.028, -0.075))
	_append(surface, _box(Vector3(0.048, 0.008, 0.17)), Vector3(0, 0.061, -0.013) + RECEIVER_OFFSET)
	# Four closed bars form a readable trigger guard with a real visual opening.
	_append(surface, _box(Vector3(0.024, 0.009, 0.074)), Vector3(0, -0.099, -0.017) + RECEIVER_OFFSET)
	_append(surface, _box(Vector3(0.024, 0.045, 0.009)), Vector3(0, -0.078, -0.05) + RECEIVER_OFFSET)
	_append(surface, _box(Vector3(0.024, 0.009, 0.074)), Vector3(0, -0.055, -0.017) + RECEIVER_OFFSET)
	_append(surface, _box(Vector3(0.024, 0.045, 0.009)), Vector3(0, -0.078, 0.016) + RECEIVER_OFFSET)
	# Small exposed edges communicate wear without textures or overlay passes.
	_append(surface, _box(Vector3(0.003, 0.004, 0.055)), Vector3(0.048, 0.043, 0.04) + RECEIVER_OFFSET)
	_commit(surface, tool_mesh, steel)

	surface = SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	_append_guide(surface, 0.037, 0.025, -0.2565, -0.2315)
	_append(surface, _box(Vector3(0.004, 0.055, 0.10)), Vector3(0.049, 0, 0.01) + RECEIVER_OFFSET)
	_commit(surface, tool_mesh, ochre)

	surface = SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	_append_guide(surface, 0.027, 0.025, -0.297, -0.295)
	_append(surface, _box(Vector3(0.009, 0.029, 0.013)), Vector3(0, -0.072, -0.005) + RECEIVER_OFFSET,
		Basis(Vector3.RIGHT, -0.2))
	_append(surface, _cylinder(0.043, 0.013), Vector3(0.078, -0.014, 0.102) + RECEIVER_OFFSET,
		Basis(Vector3.RIGHT, PI * 0.5))
	_commit(surface, tool_mesh, dark)
	_tool = _instance("CameraTool", tool_mesh)
	_rack_tool = _instance("RackTool", tool_mesh)
	_tool.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var spool_mesh := ArrayMesh.new()
	surface = SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	for i in 3:
		var angle := float(i) * TAU / 3.0
		_append(surface, _box(Vector3(0.007, 0.027, 0.005)),
			Vector3(-sin(angle), cos(angle), 0.0) * 0.019,
			Basis(Vector3.FORWARD, -angle))
	_commit(surface, spool_mesh, ochre)
	_spool = MeshInstance3D.new()
	_spool.name = "ReelDrumFace"
	_spool.mesh = spool_mesh
	_spool.position = Vector3(0.078, -0.014, 0.112) + RECEIVER_OFFSET
	_spool.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_tool.add_child(_spool)
	var lamp := MeshInstance3D.new()
	lamp.name = "LoadLamp"
	lamp.mesh = _box(Vector3(0.009, 0.005, 0.018))
	lamp.position = Vector3(0.029, 0.066, 0.017) + RECEIVER_OFFSET
	lamp.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_lamp_material = _finish(Color("d29a50"), 0.15, 0.35)
	_lamp_material.emission_enabled = true
	_lamp_material.emission = Color("dc9a42")
	_lamp_material.emission_energy_multiplier = 0.15
	lamp.material_override = _lamp_material
	_tool.add_child(lamp)

	# Match the native compound shape about BODY POSITION, not its COM.
	var hook_mesh := ArrayMesh.new()
	surface = SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	# Every visible hook prism comes from the same native authored geometry.
	for part in _hook_geometry:
		_append_prism(surface, part["polygon"], part["z_low"], part["z_high"])
	_commit(surface, hook_mesh, steel)
	for i in HOOK_CAPACITY:
		_hooks.append(_instance("NativeHook%d" % i, hook_mesh))

	# Unit-height +Y cylinder: only its transform changes on a render update.
	var cable := _cylinder(1.0, 1.0)
	cable.radial_segments = 8
	cable.material = _finish(Color("514d43"), 0.25, 0.82)
	_line = _instance("NativeLine", cable)
	_line.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF


func _instance(label: String, mesh: Mesh) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.name = label
	instance.mesh = mesh
	add_child(instance)
	# World readback must stay exact even if this presentation owner is moved.
	instance.top_level = true
	instance.physics_interpolation_mode = Node.PHYSICS_INTERPOLATION_MODE_OFF
	instance.visible = false
	return instance


func _finish(color: Color, metallic: float, roughness: float) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.metallic = metallic
	material.roughness = roughness
	return material


func _box(size: Vector3) -> BoxMesh:
	var mesh := BoxMesh.new()
	mesh.size = size
	return mesh


func _cylinder(radius: float, height: float) -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = radius
	mesh.bottom_radius = radius
	mesh.height = height
	mesh.radial_segments = 12
	return mesh


func _append(surface: SurfaceTool, mesh: Mesh, offset: Vector3,
		basis: Basis = Basis.IDENTITY) -> void:
	surface.append_from(mesh, 0, Transform3D(basis, offset))


func _commit(surface: SurfaceTool, mesh: ArrayMesh, material: Material) -> void:
	surface.set_material(material)
	surface.commit(mesh)


# Open-top guide: real mesh walls and end faces leave the upper sight
# channel clear, including the camera directly behind a vertical launch.
func _append_guide(surface: SurfaceTool, outer: float, inner: float, z_low: float, z_high: float) -> void:
	for i in 12:
		var a := deg_to_rad(150.0 + float(i) * 20.0)
		var b := a + deg_to_rad(20.0)
		var polygon := PackedVector3Array()
		for pair in [Vector2(a, outer), Vector2(b, outer), Vector2(b, inner), Vector2(a, inner)]:
			polygon.append(Vector3(cos(pair.x) * pair.y, 0.012 + sin(pair.x) * pair.y, 0))
		_append_prism(surface, polygon, z_low, z_high)


func _append_prism(surface: SurfaceTool, polygon: PackedVector3Array,
		z_low: float, z_high: float) -> void:
	var first := polygon[0]
	for i in range(1, polygon.size() - 1):
		var b := polygon[i]
		var c := polygon[i + 1]
		_append_triangle(surface, Vector3(first.x, first.y, z_high),
			Vector3(b.x, b.y, z_high), Vector3(c.x, c.y, z_high))
		_append_triangle(surface, Vector3(first.x, first.y, z_low),
			Vector3(c.x, c.y, z_low), Vector3(b.x, b.y, z_low))
	for i in polygon.size():
		var a := polygon[i]
		var b := polygon[(i + 1) % polygon.size()]
		var bottom_a := Vector3(a.x, a.y, z_low)
		var bottom_b := Vector3(b.x, b.y, z_low)
		var top_a := Vector3(a.x, a.y, z_high)
		var top_b := Vector3(b.x, b.y, z_high)
		_append_triangle(surface, bottom_a, bottom_b, top_b)
		_append_triangle(surface, bottom_a, top_b, top_a)


func _append_triangle(surface: SurfaceTool, a: Vector3, b: Vector3, c: Vector3) -> void:
	var normal := (b - a).cross(c - a).normalized()
	# Supplied face order is outward CCW; Godot front faces are clockwise.
	for point in [a, c, b]:
		surface.set_normal(normal)
		surface.add_vertex(point)
