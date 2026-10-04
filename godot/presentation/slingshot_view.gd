extends Node3D

# Presentation only. Band endpoints, draw, work, aim, and release events are
# native reads. The camera's short slow-motion request changes elapsed time,
# never tick length, spring force, energy, or the rider's launch velocity.

const WoodShader := preload("res://presentation/slingshot_wood.gdshader")
const LeatherShader := preload("res://presentation/slingshot_leather.gdshader")
const SpeedShader := preload("res://presentation/slingshot_speed.gdshader")
const ClimberCharacter := preload("res://presentation/climber_character.gd")
const Style := preload("res://presentation/ui/ui_style.gd")
const Cinema := preload("res://presentation/launch_cinematic.gd")
const LandingGuide := preload("res://presentation/slingshot_landing_guide.gd")
const CINEMATIC_SECONDS := Cinema.TOTAL_SECONDS
const ORBIT_SECONDS := Cinema.ORBIT_SECONDS
const BAND_SEGMENTS := 8
const FRAME_ENTITY := 1950
const POUCH_ENTITY := 2900

var _main: Node3D
var _wood: ShaderMaterial
var _leather: ShaderMaterial
var _rubber: StandardMaterial3D
var _brass: StandardMaterial3D
var _bands: Array = []
var _retrieval_tether: MeshInstance3D
var _retrieval_control: Node3D
var _retrieval_wheel: Node3D
var _retrieval_label: Label3D
var _anchor_details: Array[Node3D] = []
var _pouch: Node3D
var _leather_mesh: MeshInstance3D
var _leather_stitches: MultiMeshInstance3D
var _leather_vertices := PackedVector3Array()
var _guide_shoe: Node3D
var _avatar: Node3D
var _hud: AimHud
var _speed_rect: ColorRect
var _speed_material: ShaderMaterial
var _canvas: CanvasLayer
var _speed_canvas: CanvasLayer
var _styled_kit := false
var _fork_body: Node3D
var _last_launch := -1
var _cinematic_clock := -1.0
var _cinematic_forward := Vector3.FORWARD
var _cinematic_start := Transform3D.IDENTITY
var _cinematic_start_player := Vector3.ZERO
var _shot_velocity := Vector3.ZERO
var _flight_frame := Basis.IDENTITY
var _recoil_seen := false
var _bullet_clock := 0.0
var _orbit_phase := 0.0
var _focus_blur := 0.0
var _seat_was_released := false
var _seat_ready_seconds := 0.0
var _landing_flight_active := false
var _last_guided_launch := -1
var _comfortable := true
var _simulation_scale := 1.0
var _preview_points := PackedVector3Array()
var _preview_mesh: MeshInstance3D
var _target_marker: MeshInstance3D


func setup(main: Node3D) -> void:
	_main = main
	name = "SlingshotPresentation"
	if get_parent() == null:
		main.add_child(self)
	_wood = ShaderMaterial.new()
	_wood.shader = WoodShader
	_leather = ShaderMaterial.new()
	_leather.shader = LeatherShader
	_rubber = _material(Color("141613"), 0.72, 0.0)
	_rubber.metallic_specular = 0.23
	_brass = _material(Color("b69048"), 0.42, 0.65)
	_build_bands()
	_build_pouch()
	_build_retrieval_control()
	_build_avatar()
	_preview_mesh = MeshInstance3D.new()
	_preview_mesh.name = "NativeLaunchPrediction"
	_preview_mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(_preview_mesh)
	_build_target_marker()
	_build_overlay()
	_style_native_kit()


func get_simulation_scale() -> float:
	if _main != null:
		var settings: Variant = _main.get("_settings")
		if settings != null and (not bool(settings.get("head_bob")) \
				or not bool(settings.get("launch_cinematics"))) and (_cinematic_clock >= 0.0 or _simulation_scale != 1.0):
			cancel_cinematic()
	return _simulation_scale


func cancel_cinematic(clear_launch_guidance: bool = false) -> void:
	if clear_launch_guidance:
		_landing_flight_active = false
		_last_guided_launch = -1
	_seat_ready_seconds = 0.0
	_seat_was_released = false
	_cinematic_clock = -1.0
	_simulation_scale = 1.0
	_orbit_phase = 0.0
	_focus_blur = 0.0
	_recoil_seen = false
	_bullet_clock = 0.0
	_last_launch = -1
	if _avatar != null:
		_avatar.visible = false
	if _speed_rect != null:
		_speed_rect.visible = false
	if _hud != null:
		_hud.state = {}
		_hud.landing_hint = ""
		_hud.cinematic = false
		_hud.queue_redraw()
	if _preview_mesh != null:
		_preview_mesh.visible = false
		_preview_points = PackedVector3Array()
	if _target_marker != null:
		_target_marker.visible = false
	if _main != null:
		var arms := _main.get_node_or_null("FirstPersonArms")
		if arms != null:
			arms.visible = true


func is_cinematic_active() -> bool:
	return _cinematic_clock >= 0.0 and _comfortable


func get_cinematic_state() -> Dictionary:
	return {"active": is_cinematic_active(), "clock": maxf(_cinematic_clock, 0.0),
		"orbit_phase": _orbit_phase, "angle_rad": TAU * _orbit_phase, "soft_blur": _focus_blur}


func update_view(delta: float, state: Dictionary, player_position: Vector3,
		player_velocity: Vector3, camera: Camera3D) -> void:
	if state.is_empty() or _main == null:
		cancel_cinematic(true)
		return
	if not _styled_kit:
		_style_native_kit()
	var settings: Variant = _main.get("_settings")
	# Head motion OFF is the existing motion-comfort setting; it also quiets
	# the orbital camera and distortion. A dedicated state flag is accepted.
	_comfortable = not bool(state.get("reduced_motion", false)) \
		and (settings == null or (bool(settings.get("head_bob")) and bool(settings.get("launch_cinematics"))))
	if bool(state.get("seated", false)) and not bool(state.get("released", false)):
		# Charged and slack aim share the measured, interpolated rail axis.
		# The finite native gimbal must move before the lens shows that aim.
		var yaw := -float(state.get("yaw_rad", 0.0))
		var elevation := float(state.get("elevation_rad", 0.0))
		_main.set("_yaw", yaw)
		_main.set("_pitch", elevation)
		camera.rotation = Vector3(elevation, yaw, camera.rotation.z)
	var launch := int(state.get("launch_count", 0))
	if _last_guided_launch >= 0 and launch > _last_guided_launch:
		_landing_flight_active = true
	_last_guided_launch = launch
	if not bool(state.get("released", false)) or (bool(state.get("player_grounded", false)) and not bool(state.get("seated", false))):
		_landing_flight_active = false
	if _last_launch >= 0 and launch > _last_launch and _comfortable:
		_cinematic_clock = 0.0
		_cinematic_start = camera.global_transform
		_cinematic_start_player = player_position
		_shot_velocity = player_velocity
		_recoil_seen = false
		_bullet_clock = 0.0
		_orbit_phase = 0.0
		var yaw := float(state.get("yaw_rad", 0.0))
		_cinematic_forward = Vector3(sin(yaw), 0.0, -cos(yaw))
	_last_launch = launch
	if not _comfortable:
		_cinematic_clock = -1.0
	_update_bands(state)
	_update_pouch(state)
	_update_retrieval_control(state, player_position)
	_update_prediction(state)
	if bool(state.get("seated", false)) and not is_cinematic_active():
		_pose_avatar(player_position, player_velocity, delta, state)
	_update_cinematic(maxf(delta, 0.0), player_position, player_velocity, camera, state)
	var router: Variant = _main.get("_router")
	_hud.family = int(router.glyph_family()) if router != null else Style.Family.KEYBOARD
	_seat_ready_seconds = maxf(0.0, _seat_ready_seconds - delta)
	if _seat_was_released and not bool(state.get("released", false)) and player_position.distance_to(state.get("neutral_position", player_position)) < 24.0:
		_seat_ready_seconds = 4.0
	_seat_was_released = bool(state.get("released", false))
	_hud.state = state.duplicate()
	_hud.state["seat_ready"] = _seat_ready_seconds > 0.0
	_hud.state["launch_flight"] = _landing_flight_active
	_hud.landing_hint = LandingGuide.message(_hud.state, player_position, player_velocity)
	_hud.cinematic = is_cinematic_active()
	_hud.player_speed = player_velocity.length()
	_hud.vertical_speed = player_velocity.y
	_hud.launch_phase = _cinematic_clock
	_hud.queue_redraw()


func _style_native_kit() -> void:
	var kit := _main.get_node_or_null("KitPresentation")
	if kit == null:
		return
	var frame := kit.get_node_or_null("KitBody%d" % FRAME_ENTITY)
	if frame == null:
		return
	_fork_body = frame
	for entity in [FRAME_ENTITY, 1951]:
		var timber_body := kit.get_node_or_null("KitBody%d" % entity)
		if timber_body == null:
			continue
		var timber_mesh := _native_timber_mesh(entity)
		_wood.set_shader_parameter("part_grain_uv", timber_mesh != null)
		for child in timber_body.get_children():
			if child is MeshInstance3D:
				for surface in child.mesh.get_surface_count():
					var material: Material = child.mesh.surface_get_material(surface)
					# Native timber becomes oak; rails and coach bolts stay metal.
					if material is StandardMaterial3D and material.albedo_color.is_equal_approx(Color("4a3420")):
						if timber_mesh != null:
							child.mesh = timber_mesh
						child.material_override = _wood
	var pouch := kit.get_node_or_null("KitBody%d" % POUCH_ENTITY)
	if pouch != null:
		for child in pouch.get_children():
			if child is MeshInstance3D:
				child.visible = false
	_styled_kit = true


func _native_timber_mesh(entity: int = FRAME_ENTITY) -> ArrayMesh:
	var native: Variant = _main.get("_native")
	if native == null:
		return null
	var body := int(native.get_kit_body_index(entity))
	if body < 0:
		return null
	var parts: PackedFloat32Array = native.get_kit_body_parts(body)
	var surface := SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	var count := 0
	for p in range(0, parts.size() - 12, 13):
		if int(parts[p + 10]) != 2:
			continue
		var pose := Transform3D(Basis(Quaternion(parts[p + 6], parts[p + 7], parts[p + 8], parts[p + 9])),
			Vector3(parts[p + 3], parts[p + 4], parts[p + 5]))
		if int(parts[p + 11]) == 2:
			var capsule := CapsuleMesh.new()
			capsule.radius = parts[p]
			capsule.height = 2.0 * (parts[p + 1] + parts[p])
			capsule.radial_segments = 24
			capsule.rings = 10
			surface.append_from(capsule, 0, pose)
		elif int(parts[p + 11]) == 0:
			_beveled_box(surface, Vector3(parts[p], parts[p + 1], parts[p + 2]), pose)
		else:
			continue
		count += 1
	return surface.commit() if count > 0 else null


func _beveled_box(surface: SurfaceTool, half: Vector3, pose: Transform3D) -> void:
	# A timber's bounds remain the native box's exact bounds. Small rounded
	# edges add catchlights without enlarging anything the player collides.
	var bevel := minf(half.x, minf(half.y, half.z)) * 0.14
	var inside := half - Vector3.ONE * bevel
	for axis in 3:
		var a := (axis + 1) % 3
		var b := (axis + 2) % 3
		for sign in [-1.0, 1.0]:
			var normal := Vector3.ZERO
			normal[axis] = sign
			var corners: Array[Vector3] = []
			for pair in [Vector2(-1, -1), Vector2(1, -1), Vector2(1, 1), Vector2(-1, 1)]:
				var v := Vector3.ZERO
				v[axis] = half[axis] * sign
				v[a] = inside[a] * pair.x
				v[b] = inside[b] * pair.y
				corners.append(v)
			_face(surface, corners, normal, pose)
	for edge_axis in 3:
		var a := (edge_axis + 1) % 3
		var b := (edge_axis + 2) % 3
		for sa in [-1.0, 1.0]:
			for sb in [-1.0, 1.0]:
				var corners: Array[Vector3] = []
				for pair in [Vector2(-1, 0), Vector2(1, 0), Vector2(1, 1), Vector2(-1, 1)]:
					var v := Vector3.ZERO
					v[edge_axis] = inside[edge_axis] * pair.x
					v[a] = (half[a] if pair.y == 0 else inside[a]) * sa
					v[b] = (inside[b] if pair.y == 0 else half[b]) * sb
					corners.append(v)
				var normal := Vector3.ZERO
				normal[a] = sa
				normal[b] = sb
				_face(surface, corners, normal.normalized(), pose)
	for sx in [-1.0, 1.0]:
		for sy in [-1.0, 1.0]:
			for sz in [-1.0, 1.0]:
				_face(surface, [Vector3(half.x * sx, inside.y * sy, inside.z * sz),
					Vector3(inside.x * sx, half.y * sy, inside.z * sz),
					Vector3(inside.x * sx, inside.y * sy, half.z * sz)], Vector3(sx, sy, sz).normalized(), pose)


func _face(surface: SurfaceTool, corners: Array, normal: Vector3, pose: Transform3D) -> void:
	if ((corners[1] - corners[0]) as Vector3).cross(corners[2] - corners[0]).dot(normal) < 0.0:
		corners.reverse()
	for index in range(1, corners.size() - 1):
		for vertex in [corners[0], corners[index], corners[index + 1]]:
			surface.set_normal(pose.basis * normal)
			surface.set_uv(Vector2(vertex.x, vertex.z))
			surface.set_uv2(Vector2(vertex.y, 0.0))
			surface.add_vertex(pose * vertex)


func _material(color: Color, roughness: float, metal: float) -> StandardMaterial3D:
	var result := StandardMaterial3D.new()
	result.albedo_color = color
	result.roughness = roughness
	result.metallic = metal
	return result


func _mesh(parent: Node3D, geometry: Mesh, material: Material,
		at: Vector3 = Vector3.ZERO) -> MeshInstance3D:
	var instance := MeshInstance3D.new()
	instance.mesh = geometry
	instance.material_override = material
	instance.position = at
	parent.add_child(instance)
	return instance


func _box(size: Vector3) -> BoxMesh:
	var mesh := BoxMesh.new()
	mesh.size = size
	return mesh


func _capsule(radius: float, height: float) -> CapsuleMesh:
	var mesh := CapsuleMesh.new()
	mesh.radius = radius
	mesh.height = maxf(height, radius * 2.0)
	mesh.radial_segments = 14
	mesh.rings = 4
	return mesh


func _sphere(radius: float) -> SphereMesh:
	var mesh := SphereMesh.new()
	mesh.radius = radius
	mesh.height = radius * 2.0
	mesh.radial_segments = 16
	mesh.rings = 8
	return mesh


func _band_cylinder() -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = 0.26
	mesh.bottom_radius = 0.26
	mesh.height = 1.0
	mesh.radial_segments = 20
	mesh.rings = 1
	return mesh


func _segment(instance: MeshInstance3D, a: Vector3, b: Vector3,
		width: float = 1.0, depth: float = 1.0) -> void:
	var span := b - a
	if span.length_squared() < 0.000001:
		instance.visible = false
		return
	instance.visible = true
	var y := span.normalized()
	var x := y.cross(Vector3.FORWARD)
	if x.length_squared() < 0.001:
		x = y.cross(Vector3.RIGHT)
	x = x.normalized()
	var z := x.cross(y).normalized()
	instance.transform = Transform3D(Basis(x * width, y * span.length(), z * depth), (a + b) * 0.5)


func _build_bands() -> void:
	var tether := CylinderMesh.new()
	tether.top_radius = 0.022
	tether.bottom_radius = 0.022
	tether.height = 1.0
	tether.radial_segments = 10
	_retrieval_tether = _mesh(self, tether, _material(Color("b5986c"), 0.88, 0.0))
	_retrieval_tether.name = "NativeRetrievalTether"
	_retrieval_tether.visible = false
	for side in 2:
		var pieces: Array[MeshInstance3D] = []
		for segment in BAND_SEGMENTS:
			var band := _mesh(self, _band_cylinder(), _rubber)
			band.name = "RubberBand%d_%d" % [side, segment]
			pieces.append(band)
		_bands.append(pieces)
		var detail := Node3D.new()
		detail.name = "BandAnchor%d" % side
		add_child(detail)
		# Brass pressure plates, three coach bolts, and the folded rubber
		# end make the load path readable where the band meets the fork.
		_mesh(detail, _box(Vector3(0.96, 0.36, 0.52)), _brass)
		for bx in [-0.33, 0.0, 0.33]:
			var bolt := _mesh(detail, _sphere(0.055), _brass, Vector3(bx, 0.195, 0.0))
			bolt.scale.y = 0.35
		_mesh(detail, _box(Vector3(0.62, 0.11, 0.80)), _rubber, Vector3(0.0, -0.09, 0.25))
		_anchor_details.append(detail)


func _update_bands(state: Dictionary) -> void:
	var pouch: Vector3 = state.get("pouch_position", Vector3(6.0, 1.2, -55.0))
	_retrieval_tether.visible = bool(state.get("recovering", false))
	if _retrieval_tether.visible:
		var station: Vector3 = state.get("neutral_position", Vector3.ZERO)
		if not state.has("neutral_position"):
			var native: Variant = _main.get("_native")
			var body := int(native.get_kit_body_index(1951))
			if body >= 0:
				station = native.get_kit_body_transform(body).origin
		_segment(_retrieval_tether, station, pouch)
	var anchors := [state.get("anchor_left", pouch), state.get("anchor_right", pouch)]
	var rest := maxf(float(state.get("band_rest_m", 10.4403065089)), 0.001)
	for side in 2:
		var anchor: Vector3 = anchors[side]
		_anchor_details[side].position = anchor
		# The new fork tips are upright in world space, irrespective of the
		# separate rail's aim. Dressing stays on the actual fixed anchors.
		_anchor_details[side].basis = Basis.IDENTITY
		var span := pouch - anchor
		var length := span.length()
		var sag := minf(maxf(rest - length, 0.0) * 0.10, 0.70)
		var thickness := sqrt(minf(rest / maxf(length, 0.1), 1.0))
		var attachment := pouch + Vector3(-0.84 if side == 0 else 0.84, 0.28, 0.0)
		for segment in BAND_SEGMENTS:
			var a := float(segment) / float(BAND_SEGMENTS)
			var b := float(segment + 1) / float(BAND_SEGMENTS)
			var start := anchor.lerp(attachment, a) - Vector3.UP * (sin(a * PI) * sag)
			var end := anchor.lerp(attachment, b) - Vector3.UP * (sin(b * PI) * sag)
			_segment(_bands[side][segment], start, end, thickness, thickness)


func _build_pouch() -> void:
	_pouch = Node3D.new()
	_pouch.name = "LeatherCradle"
	add_child(_pouch)
	# One continuous native-authored leather sheet replaces the box shelf.
	_leather_mesh = MeshInstance3D.new()
	_leather_mesh.name = "NativeLeatherSurface"
	_leather_mesh.material_override = _leather
	_pouch.add_child(_leather_mesh)
	_leather_stitches = MultiMeshInstance3D.new()
	_leather_stitches.material_override = _material(Color("b9996b"), 0.96, 0.0)
	var stitches := MultiMesh.new()
	stitches.transform_format = MultiMesh.TRANSFORM_3D
	stitches.mesh = _box(Vector3(0.026, 0.009, 0.005))
	stitches.instance_count = 76
	_leather_stitches.multimesh = stitches
	_pouch.add_child(_leather_stitches)
	for side in [-1.0, 1.0]:
		_mesh(_pouch, _box(Vector3(0.055, 0.14, 0.50)), _leather, Vector3(side * 0.84, 0.45, 0.0))
		_mesh(_pouch, _box(Vector3(0.075, 0.10, 0.20)), _brass, Vector3(side * 0.84, 0.48, 0.0))
		_mesh(_pouch, _capsule(0.045, 0.24), _rubber, Vector3(side * 0.65, 0.43, 0.37))
	# Readable rolling shoes join the leather seat to the actual native
	# 2901 launch rails. They remain attached to the pouch after it leaves
	# the guide; small wheel/axle detail adds no collision proxy.
	_guide_shoe = Node3D.new()
	_guide_shoe.name = "LaunchGuideRollingShoe"
	_pouch.add_child(_guide_shoe)
	var steel := _material(Color("343b3b"), 0.52, 0.6)
	for side in [-1.0, 1.0]:
		_mesh(_guide_shoe, _box(Vector3(0.16, 0.17, 0.38)), steel, Vector3(side * 0.83, -0.13, 0.0))
		for z in [-0.14, 0.14]:
			var wheel := CylinderMesh.new()
			wheel.top_radius = 0.12
			wheel.bottom_radius = 0.12
			wheel.height = 0.10
			wheel.radial_segments = 16
			var roller := _mesh(_guide_shoe, wheel, steel, Vector3(side * 0.93, -0.06, z))
			roller.rotation.z = PI * 0.5


func _update_pouch(state: Dictionary) -> void:
	_pouch.position = state.get("pouch_position", Vector3.ZERO)
	var vertices: PackedVector3Array = state.get("leather_vertices", PackedVector3Array())
	if _leather_vertices.is_empty() and vertices.size() == 171:
		_leather_vertices = vertices
		var surface := SurfaceTool.new()
		surface.begin(Mesh.PRIMITIVE_TRIANGLES)
		for row in 8:
			for column in 18:
				for corner in [Vector2i(0, 0), Vector2i(0, 1), Vector2i(1, 1),
						Vector2i(0, 0), Vector2i(1, 1), Vector2i(1, 0)]:
					var x: int = column + corner.x
					var z: int = row + corner.y
					surface.set_uv(Vector2(float(x)/18.0, float(z)/8.0))
					surface.add_vertex(vertices[z*19+x])
		surface.generate_normals()
		_leather_mesh.mesh = surface.commit()
	var flex := float(state.get("leather_deflection_m", 0.0))
	_leather.set_shader_parameter("native_fold_m", flex)
	# Stitches follow the same native fold; anchored rim stays taut.
	if not _leather_vertices.is_empty():
		var index := 0
		for row in [0, 8]:
			for column in 19:
				var point := _leather_vertices[row*19+column] + Vector3.UP*0.008
				_leather_stitches.multimesh.set_instance_transform(index, Transform3D(Basis.IDENTITY, point))
				index += 1
		for row in 8:
			for column in [0, 18]:
				var point := _leather_vertices[row*19+column] + Vector3.UP*0.008
				_leather_stitches.multimesh.set_instance_transform(index, Transform3D(Basis.IDENTITY, point))
				index += 1
		# Remaining stitch slots are hidden, never left at the sheet centre.
		for unused in range(index, 76):
			_leather_stitches.multimesh.set_instance_transform(unused, Transform3D(Basis.IDENTITY.scaled(Vector3.ZERO), Vector3.ZERO))
	# The native pouch has restricted angular DOFs and stays upright. Aim
	# rotates the empty fork, not the leather body or its collision surface.
	_pouch.rotation = Vector3.ZERO
	_pouch.visible = state.has("pouch_position")


func _build_avatar() -> void:
	_avatar = ClimberCharacter.new()
	add_child(_avatar)
	_avatar.build()
	_avatar.visible = false


func _build_retrieval_control() -> void:
	# The native yellow post carries this handwheel. It is a manual control
	# for the real retrieval tether, with no presentation motor or force.
	_retrieval_control = Node3D.new()
	_retrieval_control.name = "ManualPouchReturnHandwheel"
	add_child(_retrieval_control)
	_retrieval_wheel = Node3D.new()
	_retrieval_control.add_child(_retrieval_wheel)
	var steel := _material(Color("363c39"), 0.48, 0.6)
	var ring := TorusMesh.new()
	ring.inner_radius = 0.26
	ring.outer_radius = 0.31
	ring.rings = 24
	ring.ring_segments = 8
	var rim := _mesh(_retrieval_wheel, ring, steel)
	rim.rotation.x = PI * 0.5
	for angle in [0.0, TAU / 3.0, TAU * 2.0 / 3.0]:
		var spoke := _mesh(_retrieval_wheel, _box(Vector3(0.035, 0.29, 0.035)), steel,
			Vector3(-sin(angle), cos(angle), 0.0) * 0.13)
		spoke.rotation.z = angle
	var grip := _mesh(_retrieval_wheel, _capsule(0.032, 0.11), _rubber, Vector3(0.26, 0.0, -0.06))
	grip.rotation.x = PI * 0.5
	_mesh(_retrieval_control, _sphere(0.052), _brass)
	_retrieval_label = Label3D.new()
	_retrieval_label.text = "RETURN SEAT"
	_retrieval_label.font = Style.font_label()
	_retrieval_label.font_size = 44
	_retrieval_label.pixel_size = 0.008
	_retrieval_label.outline_size = 5
	_retrieval_label.outline_modulate = Style.INK
	_retrieval_label.modulate = Style.AMBER
	_retrieval_label.billboard = BaseMaterial3D.BILLBOARD_ENABLED
	_retrieval_label.position = Vector3.UP * 0.85
	_retrieval_control.add_child(_retrieval_label)
	_retrieval_control.visible = false


func _update_retrieval_control(state: Dictionary, player_position: Vector3) -> void:
	_retrieval_control.visible = state.has("retrieval_control_position")
	if not _retrieval_control.visible:
		return
	_retrieval_control.position = state["retrieval_control_position"]
	_retrieval_wheel.rotation.z = fmod(float(state.get("retrieval_work_j", 0.0)) / 3500.0, TAU)
	_retrieval_label.text = "HOLD REEL TO RETURN SEAT" if bool(state.get("recovering", false)) else ("SEAT READY · WALK INTO POUCH" if _seat_ready_seconds > 0.0 else "RETURN SEAT · TAP ACTION")
	_retrieval_label.visible = (bool(state.get("released", false)) or _seat_ready_seconds > 0.0) \
		and not bool(state.get("seated", false)) \
		and player_position.distance_to(_retrieval_control.position) < 24.0


func _pose_avatar(player_position: Vector3, velocity: Vector3, delta: float, state: Dictionary) -> void:
	if _avatar.has_method("reaction"):
		_avatar.reaction(state, velocity, _cinematic_clock)
	var yaw := float(state.get("yaw_rad", 0.0))
	_avatar.update_pose(player_position, velocity, Vector3(sin(yaw), 0.0, -cos(yaw)), delta)


func _build_overlay() -> void:
	_speed_canvas = CanvasLayer.new()
	_speed_canvas.name = "LaunchSpeedCanvas"
	_speed_canvas.layer = 0
	_main.add_child(_speed_canvas)
	_canvas = CanvasLayer.new()
	_canvas.name = "LaunchCinematicCanvas"
	_canvas.layer = 7
	_main.add_child(_canvas)
	_speed_rect = ColorRect.new()
	_speed_rect.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_speed_rect.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_speed_material = ShaderMaterial.new()
	_speed_material.shader = SpeedShader
	_speed_rect.material = _speed_material
	_speed_rect.visible = false
	_speed_canvas.add_child(_speed_rect)
	_hud = AimHud.new()
	_hud.name = "SlingshotAimInterface"
	_hud.mouse_filter = Control.MOUSE_FILTER_IGNORE
	_hud.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
	_canvas.add_child(_hud)


func _update_cinematic(delta: float, position: Vector3, velocity: Vector3,
		camera: Camera3D, state: Dictionary) -> void:
	_simulation_scale = 1.0
	var active := is_cinematic_active()
	_avatar.visible = active
	_speed_rect.visible = active
	var arms := _main.get_node_or_null("FirstPersonArms")
	if arms != null:
		arms.visible = not active
	if not active:
		_focus_blur = 0.0
		return
	var normal := camera.global_transform
	_cinematic_clock += delta
	var t := _cinematic_clock
	if t >= CINEMATIC_SECONDS:
		_cinematic_clock = -1.0
		_avatar.visible = false
		_speed_rect.visible = false
		_focus_blur = 0.0
		if arms != null:
			arms.visible = true
		return
	# Orbit on the cinematic clock. Slow the world only after the shot is
	# actually moving, so the first impulse is not hidden inside a freeze.
	if not _recoil_seen and t >= 0.06 and velocity.length() >= 40.0:
		_recoil_seen = true
		_bullet_clock = 0.0
	if _recoil_seen:
		_bullet_clock += delta
		_simulation_scale = Cinema.simulation_scale(_bullet_clock)
	_pose_avatar(position, velocity, delta, state)
	_shot_velocity = _shot_velocity.lerp(velocity, 1.0 - exp(-5.0 * delta))
	_orbit_phase = Cinema.orbit_phase(t)
	var progress := clampf(t / ORBIT_SECONDS, 0.0, 1.0)
	var angle := -TAU * _orbit_phase
	var flight_axis := _shot_velocity.normalized() if _shot_velocity.length_squared() > 1.0 else Vector3.UP
	var right := _cinematic_forward.cross(Vector3.UP)
	right -= flight_axis * right.dot(flight_axis)
	if right.length_squared() < 0.0001:
		right = flight_axis.cross(Vector3.FORWARD if absf(flight_axis.z) < 0.8 else Vector3.UP)
	right = right.normalized()
	_flight_frame = Basis(right, flight_axis, right.cross(flight_axis)).orthonormalized()
	var speed := clampf(_shot_velocity.length() / 95.0, 0.0, 1.0)
	var rise := clampf(_shot_velocity.y / 95.0, -1.0, 1.0)
	var radius := 3.05 + speed * 0.7 + 0.45 * sin(progress * PI)
	var offset := (_flight_frame.z * cos(angle) + _flight_frame.x * sin(angle)) * radius
	# A low opening cranes up as the rider rises, then dives back to eye
	# level. Bounded banking follows the accelerating sweep, without shake.
	var height := 0.36 + sin(progress * PI) * (0.55 + 0.5 * rise)
	var eye := position + offset + flight_axis * height
	var lead := _shot_velocity * 0.012
	lead = lead.limit_length(1.0)
	var torso := _avatar.get_node_or_null("WorkJacket") as Node3D
	var target := position + Vector3.UP * 0.25 + lead
	if torso != null:
		target = torso.global_position + torso.global_basis * Vector3.UP * 0.25 + lead
	var shot_basis := Basis.looking_at(target - eye)
	shot_basis = shot_basis.rotated((target - eye).normalized(), sin(angle) * speed * 0.045)
	var shot := Transform3D(shot_basis, eye)
	var reveal := smoothstep(0.0, 0.16, t)
	var follow_start := _cinematic_start
	follow_start.origin += position - _cinematic_start_player
	shot = follow_start.interpolate_with(shot, reveal)
	var return_weight := smoothstep(ORBIT_SECONDS, CINEMATIC_SECONDS, t)
	camera.global_transform = shot.interpolate_with(normal, return_weight)
	_avatar.visible = return_weight < 0.72
	_focus_blur = Cinema.soft_blur(t)
	_speed_material.set_shader_parameter("soft_blur", _focus_blur)
	# Focus clears as the sweep accelerates. No late radial smearing hides
	# the final full revolution or the first-person handoff.
	_speed_material.set_shader_parameter("strength", 0.008 * _focus_blur)
	camera.fov += (2.0 + speed * 4.0) * sin(progress * PI) * (1.0 - return_weight)


func _build_target_marker() -> void:
	var surface := SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_LINES)
	var vertices := [Vector3.UP, Vector3.RIGHT, Vector3.FORWARD,
		Vector3.LEFT, Vector3.BACK, Vector3.DOWN]
	for edge in [Vector2i(0, 1), Vector2i(0, 2), Vector2i(0, 3), Vector2i(0, 4),
			Vector2i(5, 1), Vector2i(5, 2), Vector2i(5, 3), Vector2i(5, 4),
			Vector2i(1, 2), Vector2i(2, 3), Vector2i(3, 4), Vector2i(4, 1)]:
		surface.add_vertex(vertices[edge.x])
		surface.add_vertex(vertices[edge.y])
	var material := _material(Color("f7cb73"), 1.0, 0.0)
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.set_flag(BaseMaterial3D.FLAG_DISABLE_DEPTH_TEST, true)
	surface.set_material(material)
	_target_marker = _mesh(self, surface.commit(), material)
	_target_marker.name = "PredictedContactDiamond"
	_target_marker.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_target_marker.visible = false


func _update_prediction(state: Dictionary) -> void:
	# Native projection shares the native springs and clips its capsule to
	# the first contact in the present world. Moving scenery can still make
	# a later shot differ, so the UI labels this a predicted contact.
	var points: PackedVector3Array = state.get("trajectory_points", PackedVector3Array())
	_preview_mesh.visible = bool(state.get("seated", false)) \
		and not bool(state.get("released", false)) and points.size() > 1
	_target_marker.visible = _preview_mesh.visible
	if _target_marker.visible:
		_target_marker.position = points[points.size() - 1]
		var distance := points[0].distance_to(_target_marker.position)
		_target_marker.scale = Vector3.ONE * clampf(distance * 0.018, 1.0, 5.0)
	if not _preview_mesh.visible or points == _preview_points:
		return
	_preview_points = points
	var surface := SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_LINES)
	for index in range(0, points.size() - 1, 2):
		surface.add_vertex(points[index])
		surface.add_vertex(points[index + 1])
	var material := _material(Color("e8bd68"), 1.0, 0.0)
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	surface.set_material(material)
	_preview_mesh.mesh = surface.commit()


class AimHud extends Control:
	const Ui := preload("res://presentation/ui/ui_style.gd")
	var state: Dictionary = {}
	var family := Ui.Family.KEYBOARD
	var cinematic := false
	var player_speed := 0.0
	var vertical_speed := 0.0
	var launch_phase := -1.0
	var landing_hint := ""

	func has_thought_bubble() -> bool:
		return cinematic and bool(state.get("released", false)) \
			and vertical_speed > 8.0 and launch_phase >= 0.32 and launch_phase < 1.50

	func _draw() -> void:
		if not cinematic and not landing_hint.is_empty():
			_draw_landing_hint(landing_hint)
		var seated := bool(state.get("seated", false))
		var retrieving := bool(state.get("recovering", false))
		var retrieve_available := bool(state.get("can_retrieve", false))
		if not seated and not cinematic and not retrieving and not retrieve_available:
			if bool(state.get("seat_ready", false)):
				_draw_landing_hint("SEAT READY · WALK INTO THE POUCH AND TAP ACTION")
			return
		var u := Ui.unit(self)
		var safe := Ui.safe_rect(self)
		var c := get_viewport_rect().size * 0.5
		if cinematic or (seated and bool(state.get("released", false))):
			var rect := Rect2(Vector2(c.x - 145.0 * u, safe.position.y + 58.0 * u), Vector2(290.0, 90.0) * u)
			Ui.plate(self, rect, Ui.with_alpha(Ui.INK, 0.75), Ui.AMBER, 3.0 * u, 12.0 * u)
			Ui.text(self, Ui.font_label(), "RELEASE", rect.position + Vector2(20.0, 29.0) * u,
				int(20.0 * u), Ui.PAPER_DIM)
			Ui.text(self, Ui.font_digits(), "%.1f M/S" % player_speed, rect.position + Vector2(20.0, 70.0) * u,
				int(32.0 * u), Ui.PAPER)
			if has_thought_bubble():
				var thought := Rect2(Vector2(c.x - 280.0 * u, safe.position.y + 175.0 * u),
					Vector2(560.0, 120.0) * u)
				Ui.plate(self, thought, Ui.PAPER, Ui.AMBER, 2.5 * u, 18.0 * u)
				Ui.text(self, Ui.font_label(), "MY STOMACH", thought.position + Vector2(280.0, 46.0) * u,
					int(32.0 * u), Ui.INK, HORIZONTAL_ALIGNMENT_CENTER)
				Ui.text(self, Ui.font_label(), "TOOK THE STAIRS.", thought.position + Vector2(280.0, 91.0) * u,
					int(32.0 * u), Ui.INK, HORIZONTAL_ALIGNMENT_CENTER)
				draw_circle(thought.position + Vector2(337.0, 130.0) * u, 6.0 * u, Ui.PAPER)
				draw_circle(thought.position + Vector2(346.0, 145.0) * u, 3.0 * u, Ui.PAPER)
			return
		if not seated:
			var width := minf(900.0 * u, safe.size.x * 0.78)
			var rect := Rect2(Vector2(c.x - width * 0.5, safe.position.y + 58.0 * u), Vector2(width, 146.0 * u))
			Ui.plate(self, rect, Ui.with_alpha(Ui.INK, 0.88), Ui.AMBER, 3.0 * u, 12.0 * u)
			Ui.text(self, Ui.font_label(), "RETURN SEAT", rect.position + Vector2(22.0, 35.0) * u,
				int(25.0 * u), Ui.PAPER)
			if retrieving:
				Ui.text(self, Ui.font_digits(), "%.1f M TO CATCH  /  %.1f KW PULL" % [
					(state.get("pouch_position", Vector3.ZERO) as Vector3).distance_to(state.get("neutral_position", Vector3.ZERO)),
					float(state.get("retrieval_source_power_w", 0.0)) / 1000.0],
					rect.position + Vector2(22.0, 80.0) * u, int(22.0 * u), Ui.AMBER)
				var pull_hint := "MOVE BACK TO REEL"
				if family == Ui.Family.KEYBOARD:
					pull_hint = "HOLD S TO REEL"
				elif family == Ui.Family.TOUCH:
					pull_hint = "HOLD REEL UNTIL READY"
				Ui.text(self, Ui.font_label(), pull_hint,
					rect.position + Vector2(22.0, 118.0) * u, int(18.0 * u), Ui.PAPER_DIM)
				var left := rect.end.x - 147.0 * u
				left += Ui.draw_binding(self, family, &"drop", Vector2(left, rect.position.y + 112.0 * u), 28.0 * u)
				Ui.text(self, Ui.font_label(), "STOP", Vector2(left + 10.0 * u, rect.position.y + 118.0 * u),
					int(18.0 * u), Ui.PAPER)
			else:
				var left := rect.position.x + 22.0 * u
				left += Ui.draw_binding(self, family, &"action", Vector2(left, rect.position.y + 90.0 * u), 34.0 * u)
				Ui.text(self, Ui.font_label(), "RETRIEVE THE LEATHER POUCH", Vector2(left + 14.0 * u, rect.position.y + 98.0 * u),
					int(24.0 * u), Ui.AMBER)
			return
		var draw_m := maxf(float(state.get("draw_m", 0.0)), 0.0)
		var max_draw := maxf(float(state.get("max_draw_m", 12.0)), 0.001)
		var fraction := clampf(draw_m / max_draw, 0.0, 1.0)
		var release_ready := bool(state.get("release_ready", true))
		var aim_ready := bool(state.get("aim_ready", true))
		var aim_locked := bool(state.get("aim_locked", false))
		var accent := Ui.AMBER.lerp(Ui.HAZARD, smoothstep(0.72, 1.0, fraction))
		# The aim aperture contracts with actual draw, rather than time held.
		var radius := lerpf(52.0, 31.0, fraction) * u
		Ui.ring(self, c, radius, Ui.with_alpha(accent, 0.8), 2.5 * u)
		for angle in [0.0, PI * 0.5, PI, PI * 1.5]:
			var d := Vector2.from_angle(angle)
			draw_line(c + d * (radius + 5.0 * u), c + d * (radius + 15.0 * u), accent, 3.0 * u, true)
		Ui.text(self, Ui.font_digits(), "%+.1f DEG" % rad_to_deg(float(state.get("elevation_rad", 0.0))),
			c + Vector2(0.0, -radius - 27.0 * u), int(28.0 * u), Ui.PAPER,
			HORIZONTAL_ALIGNMENT_CENTER, int(4.0 * u))
		if aim_locked:
			Ui.text(self, Ui.font_label(), "RELEASED", c + Vector2(0.0, radius + 32.0 * u),
				int(22.0 * u), Ui.PAPER_DIM, HORIZONTAL_ALIGNMENT_CENTER, int(4.0 * u))
		elif not aim_ready:
			Ui.text(self, Ui.font_label(), "AIMING...", c + Vector2(0.0, radius + 32.0 * u),
				int(22.0 * u), Ui.PAPER_DIM, HORIZONTAL_ALIGNMENT_CENTER, int(4.0 * u))
		else:
			var aim_hint := "DRAG RIGHT SIDE TO AIM" if family == Ui.Family.TOUCH else "LOOK TO AIM"
			Ui.text(self, Ui.font_label(), aim_hint, c + Vector2(0.0, radius + 32.0 * u),
				int(22.0 * u), Ui.PAPER_DIM, HORIZONTAL_ALIGNMENT_CENTER, int(4.0 * u))
		var predicted: PackedVector3Array = state.get("trajectory_points", PackedVector3Array())
		if predicted.size() > 1:
			Ui.text(self, Ui.font_digits(), "PREDICTED CONTACT ~%.0f M" % predicted[predicted.size() - 1].y,
				c + Vector2(0.0, radius + 66.0 * u), int(28.0 * u), Ui.AMBER,
				HORIZONTAL_ALIGNMENT_CENTER, int(4.0 * u))
		# Position above the existing action strip and clear of both thumbs.
		var width := minf(690.0 * u, safe.size.x * 0.70)
		var p := Vector2(c.x - width * 0.5, c.y + 230.0 * u)
		var rect := Rect2(p, Vector2(width, 258.0 * u))
		Ui.plate(self, rect, Ui.with_alpha(Ui.INK, 0.90), accent, 4.0 * u, 16.0 * u)
		Ui.text(self, Ui.font_label(), "MANUAL DRAW" if aim_ready or aim_locked else "AIMING...", p + Vector2(24.0, 36.0) * u,
			int(24.0 * u), Ui.PAPER)
		Ui.text(self, Ui.font_digits(), "%.2f / %.0f M" % [draw_m, max_draw],
			p + Vector2(width - 24.0 * u, 36.0 * u), int(24.0 * u), accent, HORIZONTAL_ALIGNMENT_RIGHT)
		var bar := Rect2(p + Vector2(24.0, 58.0) * u, Vector2(width - 48.0 * u, 24.0 * u))
		draw_rect(bar, Ui.with_alpha(Ui.PAPER, 0.14))
		if fraction > 0.0:
			draw_rect(Rect2(bar.position, Vector2(bar.size.x * fraction, bar.size.y)), accent)
		for index in 11:
			var x := bar.position.x + bar.size.x * float(index) / 10.0
			draw_line(Vector2(x, bar.position.y), Vector2(x, bar.end.y), Ui.INK, 2.0 * u)
		Ui.text(self, Ui.font_digits(), "%.1f KJ IN BANDS" % (float(state.get("energy_j", 0.0)) / 1000.0),
			p + Vector2(24.0, 117.0) * u, int(25.0 * u), Ui.PAPER)
		Ui.text(self, Ui.font_label(), "%.1f KW PULL" % (float(state.get("source_power_w", 0.0)) / 1000.0),
			p + Vector2(width - 24.0 * u, 117.0 * u), int(20.0 * u), Ui.PAPER_DIM, HORIZONTAL_ALIGNMENT_RIGHT)
		var caption := "DRAG MOVE BACK TO DRAW"
		if family == Ui.Family.KEYBOARD:
			caption = "HOLD S TO DRAW THE BANDS"
		elif family != Ui.Family.TOUCH:
			caption = "LEFT STICK BACK TO DRAW"
		if not aim_ready and not aim_locked:
			caption = "RAIL SETTLING TO YOUR AIM"
		Ui.text(self, Ui.font_label(), caption, p + Vector2(24.0, 157.0) * u,
			int(26.0 * u), Ui.PAPER_DIM)
		var cursor := p.x + 24.0 * u
		var prompt_y := p.y + 193.0 * u
		cursor += Ui.draw_binding(self, family, &"action", Vector2(cursor, prompt_y), 30.0 * u)
		Ui.text(self, Ui.font_label(), "RELEASE" if release_ready else "DRAW MORE",
			Vector2(cursor + 11.0 * u, prompt_y + 7.0 * u),
			int(22.0 * u), accent)
		cursor = p.x + width * 0.55
		cursor += Ui.draw_binding(self, family, &"drop", Vector2(cursor, prompt_y), 30.0 * u)
		Ui.text(self, Ui.font_label(), "LEAVE", Vector2(cursor + 11.0 * u, prompt_y + 7.0 * u),
			int(22.0 * u), Ui.PAPER_DIM)
		if not release_ready:
			Ui.text(self, Ui.font_label(), "DRAW MORE TO CLEAR THE LAUNCH GUIDE", p + Vector2(24.0, 232.0) * u,
				int(16.0 * u), Ui.PAPER_DIM)
		elif predicted.size() > 1:
			Ui.text(self, Ui.font_label(), "DOTTED ARC: PREDICTED CONTACT", p + Vector2(24.0, 232.0) * u,
				int(16.0 * u), Ui.with_alpha(Ui.PAPER_DIM, 0.75))


	func _draw_landing_hint(text: String) -> void:
		var u := Ui.unit(self)
		var safe := Ui.safe_rect(self)
		var width := minf(1440.0 * u, safe.size.x * 0.78)
		var font := Ui.font_label()
		var font_size := int(28.0 * u)
		var lines := _hint_lines(text, width - 48.0 * u, font, font_size)
		var rect := Rect2(Vector2(size.x * 0.5 - width * 0.5, safe.position.y + 58.0 * u),
			Vector2(width, (32.0 + 44.0 * lines.size()) * u))
		Ui.plate(self, rect, Ui.with_alpha(Ui.INK, 0.88), Ui.AMBER, 2.0 * u, 10.0 * u)
		for i in lines.size():
			Ui.text(self, font, lines[i], rect.position + Vector2(width * 0.5, (44.0 + 44.0 * i) * u),
				font_size, Ui.PAPER, HORIZONTAL_ALIGNMENT_CENTER)

	func _hint_lines(text: String, width: float, font: Font, font_size: int) -> Array[String]:
		var lines: Array[String] = []
		var line := ""
		for word in text.split(" ", false):
			var candidate: String = word if line.is_empty() else line + " " + word
			if not line.is_empty() and Ui.text_width(font, candidate, font_size) > width:
				lines.append(line)
				line = word
			else:
				line = candidate
		if not line.is_empty():
			lines.append(line)
		return lines
