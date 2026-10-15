extends Node3D
## The yard crew, drawn: what the native's workers (sim/crew.hpp) are doing,
## nothing they are not. Each worker is a figure of simple parts -- boots,
## trousers, a hi-vis vest, sleeves, hands, a head and a hard hat -- posed
## every frame from its native state: legs and arms swing with the metres it
## has walked; at a station it works (a hammering arm, head down); stepping
## aside it watches the player; startled, its arms come up and it leans back;
## shoved, its arms fly out.
##
## Every part type is one MultiMesh across the whole crew: fifteen draw calls
## (four of them cast shadows) however many workers there are.
##
## The Plate Shop's workers (feet in PLATE_SHOP_LOW..PLATE_SHOP_HIGH) are
## welders: at a station they crouch at the band's edge, visor down, an
## electrode in the right fist; in bursts the arc burns blue-white, lights
## what is round it, crackles, and throws sparks that spill over the edge
## and fall down the well.

const WORKER_FLOATS := 8
const MAX_WORKERS := 14
const PLATE_SHOP_LOW := 340.0
const PLATE_SHOP_HIGH := 484.0
const ROD := 0.35          # the electrode and its holder, from the fist
const WELD_CYCLE_S := 5.0  # a burst of arc, then slag chipped and a look
const WELD_BURST_S := 3.4
const WELD_HEARD_M := 70.0
const WALKING := 0
const WORKING := 1
const YIELDING := 2
const STARTLED := 3
const STUMBLING := 4

# Heights above the soles, metres.
const HIP_Y := 0.92
const SHOULDER_Y := 1.44
const SHOULDER_HALF := 0.2
const HIP_HALF := 0.1
const THIGH := 0.44
const SHIN := 0.44
const UPPER_ARM := 0.3
const FOREARM := 0.27
const STEP_M := 0.75   # a stride: a full leg cycle is two of them
const DRAW_RANGE_M := 160.0

var _native: Object
var _audio: Object
# Per worker: the step and hammer beat last sounded, so each sounds once.
var _last_step := PackedInt32Array()
var _last_beat := PackedInt32Array()
# Heard within this range only.
const HEARD_M := 30.0
var _parts := {}         # name -> MultiMesh
var _clock := 0.0
# Per welder (by worker index), made when first needed: its sparks, the
# arc's light and the arc itself.
var _welds := {}
var _torch := {}         # worker index -> the electrode's tip, world
# For a test: frames a welder's arc burned within drawing range.
var welding_frames := 0
# The native's capsule centre is its stand height above the soles.
const CENTRE_Y := 0.9

func setup(native: Object, audio: Object = null) -> void:
	_native = native
	_audio = audio
	_last_step.resize(MAX_WORKERS)
	_last_beat.resize(MAX_WORKERS)
	var skin := _material(Color("b07a5a"), 0.75)
	var trousers := _material(Color("2a3140"), 0.9)
	var vest := _material(Color("e8611a"), 0.7, Color("e8611a"), 0.08)
	var stripe := _material(Color("d8dadc"), 0.3, Color("d8dadc"), 0.25)
	var sleeve := _material(Color("3a3d40"), 0.9)
	var boot := _material(Color("2b1f16"), 0.8)
	var hat := _material(Color("f2c418"), 0.35)
	_add_part(&"boot", _box(Vector3(0.12, 0.1, 0.28)), boot, 2, false)
	_add_part(&"shin", _cylinder(0.062, 0.07, SHIN), trousers, 2, true)
	_add_part(&"thigh", _cylinder(0.075, 0.085, THIGH), trousers, 2, true)
	_add_part(&"hips", _box(Vector3(0.34, 0.2, 0.2)), trousers, 1, true)
	_add_part(&"torso", _box(Vector3(0.4, 0.52, 0.23)), vest, 1, true)
	_add_part(&"stripe", _box(Vector3(0.405, 0.05, 0.235)), stripe, 1, false)
	_add_part(&"upper_arm", _cylinder(0.055, 0.06, UPPER_ARM), sleeve, 2, false)
	_add_part(&"forearm", _cylinder(0.048, 0.055, FOREARM), sleeve, 2, false)
	_add_part(&"hand", _box(Vector3(0.08, 0.1, 0.05)), skin, 2, false)
	_add_part(&"head", _sphere(0.105), skin, 1, false)
	_add_part(&"hat", _hat_mesh(), hat, 1, false)
	_add_part(&"eye", _sphere(0.016), _material(Color("17120e"), 0.4), 2, false)
	_add_part(&"brow", _box(Vector3(0.16, 0.022, 0.03)), _material(Color("3a2a1e"), 0.9), 1, false)
	_add_part(&"visor", _box(Vector3(0.21, 0.17, 0.035)), _material(Color("1c1e1f"), 0.5), 1, false)
	_add_part(&"rod", _cylinder(0.011, 0.014, ROD), _material(Color("2a2623"), 0.6), 1, false)


func update(delta: float, camera_position: Vector3, player_position: Vector3) -> void:
	if _native == null:
		return
	_clock += delta
	var data: PackedFloat32Array = _native.get_workers()
	var count := mini(data.size() / WORKER_FLOATS, MAX_WORKERS)
	for w in MAX_WORKERS:
		if w >= count:
			_hide(w)
			continue
		var b := w * WORKER_FLOATS
		var centre := Vector3(data[b], data[b + 1], data[b + 2])
		if centre.distance_to(camera_position) > DRAW_RANGE_M:
			_hide(w)
			continue
		_pose(w, centre - Vector3(0.0, CENTRE_Y, 0.0), data[b + 3], int(data[b + 4]), data[b + 5],
			data[b + 6], player_position)
		_sound(w, centre - Vector3(0.0, CENTRE_Y, 0.0), int(data[b + 4]), data[b + 6], camera_position)
		if _is_welder(centre.y - CENTRE_Y):
			_weld(w, data[b + 3], int(data[b + 4]) == WORKING, camera_position)
	# A welder out of range, or no longer at work, is dark.
	for w in _welds:
		if w >= count or (data[w * WORKER_FLOATS + 1] - CENTRE_Y) < PLATE_SHOP_LOW or \
				Vector3(data[w * WORKER_FLOATS], data[w * WORKER_FLOATS + 1],
					data[w * WORKER_FLOATS + 2]).distance_to(camera_position) > DRAW_RANGE_M:
			_weld_off(w)


# Every figure in the crew, for a test: [feet, yaw, state] each.
func figures() -> Array:
	var out := []
	if _native == null:
		return out
	var data: PackedFloat32Array = _native.get_workers()
	for w in mini(data.size() / WORKER_FLOATS, MAX_WORKERS):
		var b := w * WORKER_FLOATS
		out.append([Vector3(data[b], data[b + 1] - CENTRE_Y, data[b + 2]), data[b + 3], int(data[b + 4])])
	return out


func _pose(w: int, feet: Vector3, yaw: float, state: int, speed: float, stride: float,
		player: Vector3) -> void:
	var body := Transform3D(Basis(Vector3.UP, yaw), feet)
	var gait := clampf(speed / 1.3, 0.0, 1.0) if state != WORKING else 0.0
	var phase := stride / STEP_M * PI
	var lean := 0.0          # forward (+) / back (-), radians
	var side_lean := 0.0
	var head_pitch := 0.0
	var arm := [Vector3(0.0, 0.0, 0.0), Vector3(0.0, 0.0, 0.0)]   # per side: pitch, roll, elbow
	var personal := float(w) * 1.7
	var welder := state == WORKING and _is_welder(feet.y)
	match state:
		WORKING when welder:
			# Crouched at the edge, leaning over the seam: the right arm
			# holds the electrode down to it, the left steadies on the knee.
			lean = 0.75
			head_pitch = 0.6
			arm[0] = Vector3(-0.775, 0.1, 0.3)
			arm[1] = Vector3(-0.4, -0.2, 0.9)
		WORKING:
			lean = 0.18
			head_pitch = 0.35
			# The right arm works -- a hammer's rise and strike -- the left holds.
			var beat := sin(_clock * 5.5 + personal)
			arm[0] = Vector3(-0.9 - 0.55 * maxf(beat, 0.0), 0.15, 1.2 - 0.5 * beat)
			arm[1] = Vector3(-0.7, -0.1, 1.0)
		YIELDING:
			arm[0] = Vector3(0.1, 0.12, 0.25)
			arm[1] = Vector3(0.1, -0.12, 0.25)
		STARTLED:
			lean = -0.22
			head_pitch = -0.1
			arm[0] = Vector3(-1.9, 0.35, 1.5)
			arm[1] = Vector3(-1.9, -0.35, 1.5)
		STUMBLING:
			lean = 0.3 * sin(personal)
			side_lean = 0.25 * cos(personal)
			arm[0] = Vector3(-0.4, 1.3, 0.3)
			arm[1] = Vector3(-0.6, -1.2, 0.4)
		_:
			# Walking: arms swing against the legs.
			arm[0] = Vector3(0.38 * gait * sin(phase), 0.08, 0.35)
			arm[1] = Vector3(-0.38 * gait * sin(phase), -0.08, 0.35)
			lean = 0.06 * gait
	# A slight bob with each step.
	var bob := absf(sin(phase)) * 0.025 * gait
	# A crouch: the hips 0.38 m lower, thighs forward, shins back to the soles.
	var hip_pos := Vector3(0.0, HIP_Y + bob - (0.38 if welder else 0.0), 0.0)
	var pelvis := Basis(Vector3.RIGHT, -lean * 0.3) * Basis(Vector3.BACK, side_lean * 0.3)
	_place(&"hips", w, 0, body * Transform3D(pelvis, hip_pos + Vector3(0.0, 0.03, 0.0)))
	# Legs: thigh from the hip, shin from the knee, boot at the ankle.
	for side in 2:
		var s := 1.0 if side == 0 else -1.0
		var swing := 0.42 * gait * sin(phase + (0.0 if side == 0 else PI))
		var knee_bend := 0.7 * gait * maxf(0.0, sin(phase + (0.0 if side == 0 else PI) + 1.2))
		if state == STARTLED:
			swing = -0.15 if side == 0 else 0.1
		elif welder:
			# Thigh 1.2 rad forward of upright (the pelvis already tips it
			# back by lean * 0.3), shin 0.82 rad back: the ankle under the hip.
			swing = 1.2 - lean * 0.3 + (0.08 if side == 0 else -0.08)
			knee_bend = 2.02 + (0.08 if side == 0 else -0.08)
		var hip_joint := hip_pos + Vector3(-HIP_HALF * s, -0.04, 0.0)
		var thigh_basis := pelvis * Basis(Vector3.RIGHT, -swing)
		var knee := hip_joint + thigh_basis * Vector3(0.0, -THIGH, 0.0)
		var shin_basis := thigh_basis * Basis(Vector3.RIGHT, knee_bend)
		var ankle := knee + shin_basis * Vector3(0.0, -SHIN, 0.0)
		# Keep the standing foot on the ground (the walk's lowest ankle).
		_place(&"thigh", w, side, body * _segment(hip_joint, thigh_basis, THIGH))
		_place(&"shin", w, side, body * _segment(knee, shin_basis, SHIN))
		var foot_basis := Basis(Vector3.RIGHT, -swing * 0.3)
		_place(&"boot", w, side, body * Transform3D(foot_basis, ankle + Vector3(0.0, -0.03, 0.06)))
	# Torso on the pelvis, leaning.
	var spine := pelvis * Basis(Vector3.RIGHT, lean) * Basis(Vector3.BACK, side_lean)
	var chest := hip_pos + spine * Vector3(0.0, 0.32, 0.0)
	_place(&"torso", w, 0, body * Transform3D(spine, chest))
	_place(&"stripe", w, 0, body * Transform3D(spine, chest + spine * Vector3(0.0, -0.06, 0.0)))
	var neck := hip_pos + spine * Vector3(0.0, SHOULDER_Y - HIP_Y + 0.08, 0.0)
	# The head looks at the player when it has reason to.
	var head_basis := spine * Basis(Vector3.RIGHT, head_pitch)
	if state == YIELDING or state == STARTLED:
		var to := body.affine_inverse() * player - neck
		var look_yaw := clampf(atan2(to.x, to.z), -0.9, 0.9)
		head_basis = spine * Basis(Vector3.UP, look_yaw) * Basis(Vector3.RIGHT, head_pitch)
	var head := neck + head_basis * Vector3(0.0, 0.14, 0.0)
	_place(&"head", w, 0, body * Transform3D(head_basis, head))
	_place(&"hat", w, 0, body * Transform3D(head_basis, head + head_basis * Vector3(0.0, 0.06, 0.0)))
	# A face: two eyes and a brow, on the front of the head (+z is forward).
	for side in 2:
		var s := 0.038 if side == 0 else -0.038
		_place(&"eye", w, side, body * Transform3D(head_basis, head + head_basis * Vector3(s, 0.012, 0.094)))
	_place(&"brow", w, 0, body * Transform3D(head_basis, head + head_basis * Vector3(0.0, 0.045, 0.088)))
	if welder:
		_place(&"visor", w, 0, body * Transform3D(head_basis, head + head_basis * Vector3(0.0, -0.01, 0.115)))
	else:
		_place(&"visor", w, 0, Transform3D(Basis().scaled(Vector3.ZERO), Vector3.ZERO))
	# Arms: from each shoulder, pitch forward (-) / back (+), roll out, elbow.
	for side in 2:
		var s := 1.0 if side == 0 else -1.0
		var a: Vector3 = arm[side]
		var shoulder := hip_pos + spine * Vector3(-SHOULDER_HALF * s, SHOULDER_Y - HIP_Y, 0.0)
		var upper_basis := spine * Basis(Vector3.RIGHT, a.x) * Basis(Vector3.BACK, -a.y * s)
		var elbow := shoulder + upper_basis * Vector3(0.0, -UPPER_ARM, 0.0)
		var fore_basis := upper_basis * Basis(Vector3.RIGHT, -a.z)
		var wrist := elbow + fore_basis * Vector3(0.0, -FOREARM, 0.0)
		_place(&"upper_arm", w, side, body * _segment(shoulder, upper_basis, UPPER_ARM))
		_place(&"forearm", w, side, body * _segment(elbow, fore_basis, FOREARM))
		_place(&"hand", w, side, body * Transform3D(fore_basis, wrist + fore_basis * Vector3(0.0, -0.05, 0.0)))
		if side == 0:
			if welder:
				# The electrode runs on from the fist along the forearm.
				_place(&"rod", w, 0, body * _segment(wrist, fore_basis, ROD))
				_torch[w] = body * (wrist + fore_basis * Vector3(0.0, -ROD, 0.0))
			else:
				_place(&"rod", w, 0, Transform3D(Basis().scaled(Vector3.ZERO), Vector3.ZERO))
				_torch.erase(w)


# A footfall each time a stride passes a step; a strike each time the
# working arm comes down (the same beat _pose draws).
func _sound(w: int, feet: Vector3, state: int, stride: float, listener: Vector3) -> void:
	if _audio == null:
		return
	var step := int(floor(stride / STEP_M))
	var near := feet.distance_to(listener) < HEARD_M
	if step != _last_step[w]:
		if near and _last_step[w] != 0:
			_audio.worker_step(feet)
		_last_step[w] = step
	var beat := int(floor((_clock * 5.5 + float(w) * 1.7) / TAU))
	if state == WORKING and beat != _last_beat[w] and near and not _is_welder(feet.y):
		_audio.worker_strike(feet + Vector3(0.0, 0.9, 0.0))
	_last_beat[w] = beat


func _is_welder(feet_y: float) -> bool:
	return feet_y > PLATE_SHOP_LOW and feet_y < PLATE_SHOP_HIGH


# A welder's arc, in bursts while it works: sparks thrown out over the edge
# and falling down the well, a flickering blue-white light, the arc itself,
# and its crackle.
func _weld(w: int, yaw: float, working: bool, camera_position: Vector3) -> void:
	var burning := working and _torch.has(w) and \
		fmod(_clock + float(w) * 3.1, WELD_CYCLE_S) < WELD_BURST_S
	if not burning:
		_weld_off(w)
		return
	if not _welds.has(w):
		_welds[w] = _make_weld()
	var parts: Dictionary = _welds[w]
	var tip: Vector3 = _torch[w]
	var sparks: CPUParticles3D = parts["sparks"]
	# Thrown forward (the welder's facing) and down.
	sparks.global_transform = Transform3D(Basis(Vector3.UP, yaw), tip)
	sparks.emitting = true
	var light: OmniLight3D = parts["light"]
	light.global_position = tip + Vector3(0.0, 0.25, 0.0)
	light.visible = true
	# The arc stutters: a fresh brightness every frame.
	var flicker := 0.55 + 0.45 * absf(sin(_clock * 97.0 + float(w))) * absf(sin(_clock * 41.0))
	light.light_energy = 3.2 * flicker
	var arc: MeshInstance3D = parts["arc"]
	arc.global_position = tip
	arc.scale = Vector3.ONE * (0.6 + 0.8 * flicker)
	arc.visible = true
	welding_frames += 1
	if _audio != null:
		_audio.worker_weld(w, tip, tip.distance_to(camera_position) < WELD_HEARD_M)


func _weld_off(w: int) -> void:
	if _welds.has(w):
		var parts: Dictionary = _welds[w]
		(parts["sparks"] as CPUParticles3D).emitting = false
		(parts["light"] as OmniLight3D).visible = false
		(parts["arc"] as MeshInstance3D).visible = false
	if _audio != null:
		_audio.worker_weld(w, Vector3.ZERO, false)


func _make_weld() -> Dictionary:
	var sparks := CPUParticles3D.new()
	sparks.name = "WeldSparks"
	sparks.amount = 120
	sparks.lifetime = 1.7
	sparks.local_coords = false
	sparks.emitting = false
	# Forward (+z, the welder's facing) and down a little, in a wide fan.
	sparks.direction = Vector3(0.0, -0.25, 1.0)
	sparks.spread = 48.0
	sparks.initial_velocity_min = 1.0
	sparks.initial_velocity_max = 3.8
	sparks.gravity = Vector3(0.0, -9.8, 0.0)
	sparks.damping_min = 0.2
	sparks.damping_max = 0.7
	sparks.particle_flag_align_y = true
	sparks.scale_amount_min = 0.5
	sparks.scale_amount_max = 1.1
	var cool := Gradient.new()
	cool.set_color(0, Color(1.0, 0.92, 0.65, 1.0))
	cool.add_point(0.35, Color(1.0, 0.55, 0.15, 1.0))
	cool.set_color(cool.get_point_count() - 1, Color(0.55, 0.1, 0.02, 0.0))
	sparks.color_ramp = cool
	var streak := BoxMesh.new()
	streak.size = Vector3(0.016, 0.13, 0.016)
	streak.material = _glow_material(4.0)
	sparks.mesh = streak
	add_child(sparks)
	var light := OmniLight3D.new()
	light.name = "WeldLight"
	light.light_color = Color(0.72, 0.82, 1.0)
	light.omni_range = 9.0
	light.omni_attenuation = 1.6
	light.shadow_enabled = false
	light.visible = false
	add_child(light)
	var arc := MeshInstance3D.new()
	arc.name = "WeldArc"
	var bead := SphereMesh.new()
	bead.radius = 0.03
	bead.height = 0.06
	bead.radial_segments = 8
	bead.rings = 4
	bead.material = _arc_material()
	arc.mesh = bead
	arc.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	arc.visible = false
	add_child(arc)
	return {"sparks": sparks, "light": light, "arc": arc}


# Hot metal: drawn additively from the particle's colour, bright enough to
# bloom.
func _glow_material(gain: float) -> ShaderMaterial:
	var shader := Shader.new()
	shader.code = """
shader_type spatial;
render_mode unshaded, blend_add, cull_disabled, depth_draw_never, shadows_disabled;
uniform float gain = 4.0;
void fragment() {
	ALBEDO = COLOR.rgb * gain * COLOR.a;
}
"""
	var material := ShaderMaterial.new()
	material.shader = shader
	material.set_shader_parameter(&"gain", gain)
	return material


func _arc_material() -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	material.albedo_color = Color(0.85, 0.92, 1.0)
	material.emission_enabled = true
	material.emission = Color(0.7, 0.85, 1.0)
	material.emission_energy_multiplier = 6.0
	return material


# How many welders' arcs are burning now: for a test.
func burning_count() -> int:
	var count := 0
	for w in _welds:
		if (_welds[w]["arc"] as MeshInstance3D).visible:
			count += 1
	return count


# The bounds every part is culled by (one for all of them), in world space:
# for a test.
func draw_bounds() -> AABB:
	var instance := get_node("Crew_boot") as MultiMeshInstance3D
	return instance.global_transform * instance.custom_aabb


# A part hanging from `joint` along the basis's -y, `length` long.
func _segment(joint: Vector3, basis: Basis, length: float) -> Transform3D:
	return Transform3D(basis, joint + basis * Vector3(0.0, -length * 0.5, 0.0))


func _place(part: StringName, w: int, side: int, transform: Transform3D) -> void:
	var mm: MultiMesh = _parts[part]
	var per := mm.instance_count / MAX_WORKERS
	mm.set_instance_transform(w * per + side, transform)


func _hide(w: int) -> void:
	for part in _parts:
		var mm: MultiMesh = _parts[part]
		var per := mm.instance_count / MAX_WORKERS
		for side in per:
			mm.set_instance_transform(w * per + side, Transform3D(Basis().scaled(Vector3.ZERO), Vector3.ZERO))


func _add_part(part: StringName, mesh: Mesh, material: Material, per_worker: int, shadows: bool) -> void:
	var mm := MultiMesh.new()
	mm.transform_format = MultiMesh.TRANSFORM_3D
	mm.mesh = mesh
	mm.instance_count = MAX_WORKERS * per_worker
	for i in mm.instance_count:
		mm.set_instance_transform(i, Transform3D(Basis().scaled(Vector3.ZERO), Vector3.ZERO))
	var instance := MultiMeshInstance3D.new()
	instance.name = "Crew_%s" % part
	instance.multimesh = mm
	instance.material_override = material
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON if shadows else \
		GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	# The crew works from the yard to the high rings: its parts' bounds are
	# the whole structure. (They were the yard's alone, so the workers on
	# the decks and rings were culled whenever the yard was out of view.)
	instance.custom_aabb = AABB(Vector3(-80, -2, -260), Vector3(160, 1700, 320))
	add_child(instance)
	_parts[part] = mm


func _material(color: Color, roughness: float, emission: Color = Color.BLACK,
		emission_energy: float = 0.0) -> StandardMaterial3D:
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.roughness = roughness
	if emission_energy > 0.0:
		material.emission_enabled = true
		material.emission = emission
		material.emission_energy_multiplier = emission_energy
	return material


func _box(size: Vector3) -> BoxMesh:
	var mesh := BoxMesh.new()
	mesh.size = size
	return mesh


func _cylinder(top: float, bottom: float, height: float) -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = top
	mesh.bottom_radius = bottom
	mesh.height = height
	mesh.radial_segments = 8
	mesh.rings = 1
	return mesh


func _sphere(radius: float) -> SphereMesh:
	var mesh := SphereMesh.new()
	mesh.radius = radius
	mesh.height = radius * 2.15
	mesh.radial_segments = 12
	mesh.rings = 6
	return mesh


# A hard hat: a dome with a peak to the front.
func _hat_mesh() -> ArrayMesh:
	var dome := SphereMesh.new()
	dome.radius = 0.125
	dome.height = 0.16
	dome.is_hemisphere = true
	dome.radial_segments = 12
	dome.rings = 4
	var st := SurfaceTool.new()
	st.append_from(dome, 0, Transform3D(Basis(), Vector3(0.0, -0.02, 0.0)))
	var brim := CylinderMesh.new()
	brim.top_radius = 0.15
	brim.bottom_radius = 0.15
	brim.height = 0.012
	brim.radial_segments = 12
	st.append_from(brim, 0, Transform3D(Basis().scaled(Vector3(1.0, 1.0, 1.15)), Vector3(0.0, -0.02, 0.02)))
	return st.commit()
