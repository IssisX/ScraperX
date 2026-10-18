extends Node3D

# Read-only body presentation. A sole is planted only at a supplied native
# material point. Leg parity is artistic; native foot motors have no side.
const THIGH := 0.44
const SHIN := 0.44
const MAX_HIP_LEAN := 0.18
const ANKLE_HEIGHT := 0.14
const RETURN_TIME := 0.12

class Leg:
	var side := 1.0
	var upper: MeshInstance3D
	var lower: MeshInstance3D
	var knee: MeshInstance3D
	var boot: MeshInstance3D
	var sole: MeshInstance3D
	var point := Vector3.ZERO
	var frame := Basis.IDENTITY
	var initialized := false
	var planted := false
	var returning := false
	var return_elapsed := 0.0
	var return_point := Vector3.ZERO
	var return_rotation := Quaternion.IDENTITY

var _legs: Array[Leg] = []
var _last_tick := -1
var _planted_index := -1

func build(sleeve: Material, boot: Material, sole: Material) -> void:
	if not _legs.is_empty():
		return
	var upper_mesh := _cylinder(0.075, 0.095)
	var lower_mesh := _cylinder(0.052, 0.074)
	var knee_mesh := SphereMesh.new()
	knee_mesh.radius = 0.073
	knee_mesh.height = 0.146
	knee_mesh.radial_segments = 10
	knee_mesh.rings = 5
	var boot_mesh := _work_boot()
	var sole_mesh := BoxMesh.new()
	sole_mesh.size = Vector3(0.16, 0.025, 0.31)
	for side in [-1.0, 1.0]:
		var leg := Leg.new()
		leg.side = side
		leg.upper = _mesh(upper_mesh, sleeve)
		leg.lower = _mesh(lower_mesh, sleeve)
		leg.knee = _mesh(knee_mesh, sleeve)
		leg.boot = _mesh(boot_mesh, boot)
		leg.sole = _mesh(sole_mesh, sole)
		_legs.append(leg)
	reset()

func reset() -> void:
	_last_tick = -1
	_planted_index = -1
	visible = false
	for leg in _legs:
		leg.initialized = false
		leg.planted = false
		leg.returning = false
		leg.return_elapsed = 0.0
		leg.point = Vector3.ZERO
		leg.frame = Basis.IDENTITY
		leg.return_point = Vector3.ZERO
		leg.return_rotation = Quaternion.IDENTITY
		_set_visible(leg, false)

func update_legs(state: Dictionary, _camera: Transform3D, delta: float) -> void:
	var tick := int(state.get("native_tick", -1))
	var rewound := _last_tick >= 0 and tick < _last_tick
	var operating := StringName(state.get("operating", &""))
	if rewound or bool(state.get("dead", false)) or (
			operating != &"" and operating != &"grappling_gun"):
		reset()
		_last_tick = tick
		return
	var com: Vector3 = state.get("render_position", Vector3.INF)
	var facing: Vector3 = state.get("facing", Vector3.FORWARD)
	if not com.is_finite() or not facing.is_finite() or tick < 0:
		reset()
		return
	facing.y = 0.0
	if not is_finite(facing.length_squared()) or facing.length_squared() < 0.0001:
		reset()
		return
	facing = facing.normalized()
	var body := _sole_frame(Vector3.UP, facing)
	var crouched := bool(state.get("player_crouched", false))
	var half_height := 0.6 if crouched else 0.9
	var anchor: Vector3 = state.get("foot_push_world_anchor", Vector3.INF)
	var normal: Vector3 = state.get("foot_push_world_normal", Vector3.ZERO)
	var contact_tick := int(state.get("foot_push_source_contact_tick", -1))
	var generation := int(state.get("foot_push_start_count", 0))
	var valid := bool(state.get("foot_push_active", false)) \
		and int(state.get("foot_push_support_entity_id", -1)) > 0 \
		and contact_tick > 0 and contact_tick <= tick \
		and anchor.is_finite() and normal.is_finite() \
		and is_finite(normal.length_squared()) and normal.length_squared() > 0.0001
	var selected := generation % 2 if valid else -1
	_planted_index = -1
	visible = true
	_last_tick = tick
	var dt := clampf(delta, 0.0, RETURN_TIME) if is_finite(delta) else 0.0
	var velocity: Vector3 = state.get("velocity", Vector3.ZERO)
	velocity.y = 0.0
	var speed := velocity.length() if velocity.is_finite() else 0.0
	var travel := velocity / speed if speed > 0.001 else facing
	var stride := clampf(speed / 3.5, 0.0, 1.0)
	var phase := float(state.get("bob_phase", 0.0)) * 0.5
	if not is_finite(phase):
		phase = 0.0
	var grounded := bool(state.get("player_grounded", false))
	for i in _legs.size():
		var leg := _legs[i]
		var neutral := com + body.x * leg.side * 0.115 + facing * 0.035 - Vector3.UP * half_height
		if grounded:
			# Supplied body phase drives both feet; there is no animation clock.
			var cycle := phase + float(i) * PI
			var crouch_scale := 0.6 if crouched else 1.0
			neutral += travel * sin(cycle) * 0.13 * stride * crouch_scale
			neutral += Vector3.UP * maxf(0.0, cos(cycle)) * 0.055 * stride * crouch_scale
		else:
			# Unloaded feet tuck under the body, without asserting a contact.
			neutral += Vector3.UP * 0.17 + facing * 0.09
		var target := neutral
		var frame := body
		var planting := i == selected
		if planting:
			target = anchor
			frame = _sole_frame(normal.normalized(), facing)
		elif leg.planted:
			leg.returning = true
			leg.return_elapsed = 0.0
			leg.return_point = leg.point
			leg.return_rotation = leg.frame.get_rotation_quaternion()
		leg.planted = false
		if planting:
			leg.returning = false
		elif leg.returning:
			leg.return_elapsed = minf(RETURN_TIME, leg.return_elapsed + dt)
			var weight := smoothstep(0.0, RETURN_TIME, leg.return_elapsed)
			target = leg.return_point.lerp(neutral, weight)
			frame = Basis(leg.return_rotation.slerp(body.get_rotation_quaternion(), weight))
			leg.returning = leg.return_elapsed < RETURN_TIME
		var hip := com + body.x * leg.side * 0.115 + Vector3.UP * (0.015 if not crouched else -0.03)
		var ankle := target + frame.y * ANKLE_HEIGHT + frame.z * 0.045
		var distance := hip.distance_to(ankle)
		# Lean only the presented hip, never move the sole to fit the leg.
		if planting and distance > THIGH + SHIN - 0.002:
			hip += hip.direction_to(ankle) * minf(MAX_HIP_LEAN, distance - (THIGH + SHIN - 0.002))
			distance = hip.distance_to(ankle)
		if distance > THIGH + SHIN - 0.001 or distance < 0.04:
			_set_visible(leg, false)
			leg.initialized = false
			leg.returning = false
			continue
		leg.point = target
		leg.frame = frame
		leg.initialized = true
		leg.planted = planting
		if planting:
			_planted_index = i
		_set_visible(leg, true)
		var down := (ankle - hip) / distance
		# A wall plant folds the knee away from the wall, keeping the boot
		# visible below the body rather than burying it behind the shin.
		var bend_direction := normal.normalized() if planting and absf(normal.y) < 0.2 else facing
		var pole := bend_direction - down * bend_direction.dot(down)
		if pole.length_squared() < 0.0001:
			pole = body.x * leg.side
		pole = pole.normalized()
		var along := (THIGH * THIGH - SHIN * SHIN + distance * distance) / (2.0 * distance)
		var bend := sqrt(maxf(0.0, THIGH * THIGH - along * along))
		var knee := hip + down * along + pole * bend
		_segment(leg.upper, hip, knee)
		_segment(leg.lower, knee, ankle)
		_place(leg.knee, Basis.IDENTITY, knee)
		_place(leg.boot, frame, target + frame.y * 0.095)
		_place(leg.sole, frame, target + frame.y * 0.0125)

# Diagnostics expose the actual drawn sole bottom, not a second target cache.
func planted_foot_index() -> int:
	return _planted_index

func planted_foot_position() -> Vector3:
	if _planted_index < 0:
		return Vector3.INF
	var sole := _legs[_planted_index].sole
	return sole.global_transform * Vector3(0.0, -0.0125, 0.0)

func planted_foot_normal() -> Vector3:
	if _planted_index < 0:
		return Vector3.ZERO
	return _legs[_planted_index].sole.global_basis.y.normalized()

func _sole_frame(normal: Vector3, facing: Vector3) -> Basis:
	var toe := facing - normal * facing.dot(normal)
	if toe.length_squared() < 0.0001:
		# On a head-on wall, point the toe upward along that wall.
		toe = Vector3.UP - normal * normal.y
	if toe.length_squared() < 0.0001:
		toe = Vector3.FORWARD - normal * normal.dot(Vector3.FORWARD)
	toe = toe.normalized()
	return Basis(toe.cross(normal).normalized(), normal, -toe)

func _segment(mesh: MeshInstance3D, start: Vector3, end: Vector3) -> void:
	var axis := end - start
	var y := axis.normalized()
	var reference := Vector3.FORWARD if absf(y.dot(Vector3.FORWARD)) < 0.95 else Vector3.RIGHT
	var x := y.cross(reference).normalized()
	_place(mesh, Basis(x, y * axis.length(), x.cross(y)), (start + end) * 0.5)

func _place(mesh: MeshInstance3D, frame: Basis, point: Vector3) -> void:
	mesh.transform = global_transform.affine_inverse() * Transform3D(frame, point)

func _set_visible(leg: Leg, shown: bool) -> void:
	leg.upper.visible = shown
	leg.lower.visible = shown
	leg.knee.visible = shown
	leg.boot.visible = shown
	leg.sole.visible = shown

func _mesh(shape: Mesh, material: Material) -> MeshInstance3D:
	var mesh := MeshInstance3D.new()
	mesh.mesh = shape
	mesh.material_override = material
	mesh.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(mesh)
	return mesh

func _cylinder(top: float, bottom: float) -> CylinderMesh:
	var shape := CylinderMesh.new()
	shape.top_radius = top
	shape.bottom_radius = bottom
	shape.height = 1.0
	shape.radial_segments = 10
	shape.rings = 1
	return shape

# Three chamfered outline rings make a rounded toe and sloping instep.
# Built once, shared by both boots, with one surface and the supplied leather.
func _work_boot() -> ArrayMesh:
	var outline := PackedVector2Array([
		Vector2(-0.052, 0.13), Vector2(-0.072, 0.11),
		Vector2(-0.075, -0.065), Vector2(-0.065, -0.115),
		Vector2(-0.035, -0.145), Vector2(0.035, -0.145),
		Vector2(0.065, -0.115), Vector2(0.075, -0.065),
		Vector2(0.072, 0.11), Vector2(0.052, 0.13)])
	var rings: Array[PackedVector3Array] = []
	for ring in 3:
		var vertices := PackedVector3Array()
		for point in outline:
			var shrink := 0.88 if ring == 2 else (0.96 if ring == 0 else 1.0)
			var height := -0.07 if ring == 0 else (-0.04 if ring == 1 else 0.07)
			# Toe cap stays low while the heel rises to cover the ankle.
			if ring == 2:
				height -= clampf((-point.y - 0.015) / 0.13, 0.0, 1.0) * 0.07
			vertices.append(Vector3(point.x * shrink, height, point.y * shrink))
		rings.append(vertices)
	var surface := SurfaceTool.new()
	surface.begin(Mesh.PRIMITIVE_TRIANGLES)
	for ring in 2:
		for i in outline.size():
			var j := (i + 1) % outline.size()
			for vertex in [rings[ring][i], rings[ring][j], rings[ring + 1][j],
				rings[ring][i], rings[ring + 1][j], rings[ring + 1][i]]:
				surface.add_vertex(vertex)
	for i in outline.size():
		var j := (i + 1) % outline.size()
		for vertex in [Vector3(0, 0.055, 0), rings[2][i], rings[2][j],
			Vector3(0, -0.07, 0), rings[0][j], rings[0][i]]:
			surface.add_vertex(vertex)
	surface.generate_normals()
	return surface.commit()
