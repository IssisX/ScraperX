extends Node3D
## The Matron, drawn (src/sim/matron.hpp): her locker on the 154 m deck, the
## gauge that climbs while she strains, the bolts that pop and the door that
## tears off; her figure, a hooded, stooped old woman with arms to her knees,
## and the way she moves; and the rotten catwalk's plates. Every position and
## every state is the native's, and nothing here moves anything: she is a
## capsule that walks, and the pose is only what her state and the metres she
## has walked make of it.
##
## Her gaits: stooped and dragging when unaware, snapping her head about when
## she has lost the player, and on the hunt down on all fours in a stuttering
## scuttle (the pose is held and snaps from one to the next, as film that has
## dropped frames does), head cocked on a neck that keeps looking at you.

const DECK := 154.0
const LOCKER_CENTRE := Vector3(11.0, 155.1, -126.2)
const LOCKER_SIZE := Vector3(1.2, 2.2, 0.9)
const PLANKS := 11
const PLANK_FLOATS := 9
const PLANK_SIZE := Vector3(3.06, 0.08, 1.6)
const STAND_CENTRE := 1.05      # the native capsule's centre above her soles
const DRAW_RANGE_M := 140.0
const STATE_STRAINING := 1
const STATE_EMERGING := 2
const STATE_HUNTING := 3
const STATE_SEARCHING := 4
const STATE_SHUFFLING := 5
const STATE_DEAD := 6
const BOLT_COUNT := 4
const STEP_M := 0.7
const HEARD_MUSIC_M := 55.0

# Her proportions: long in the limb, narrow in the chest.
const THIGH := 0.46
const SHIN := 0.5
const UPPER_ARM := 0.44
const FOREARM := 0.5
const TORSO := 0.62
const NECK := 0.16

var _native: Object
var _audio: Object
var _clock := 0.0
var _last_state := -1
var _state_age := 0.0
var _bolts_seen := 0
var _knock_in := 0.6
var _breath_in := 1.0
var _shriek_in := 0.0
var _last_step := 0
var _last_pos := Vector3.ZERO
var _fall_speed := 0.0
var _was_seen := false
var _cry_fall := false
var _planks: Array[MeshInstance3D] = []
var _plank_bolted: Array[bool] = []
# The locker.
var _locker: Node3D
var _door: Node3D
var _door_gone := false
var _door_vel := Vector3.ZERO
var _door_spin := Vector3.ZERO
var _bolts: Array[MeshInstance3D] = []
var _dents: Array[MeshInstance3D] = []
var _needle: Node3D
var _lamp_light: OmniLight3D
var _lamp_glass: MeshInstance3D
var _lamp_glass_material: StandardMaterial3D
var _work_glass_material: StandardMaterial3D
var _work_light: OmniLight3D
var _vent_material: StandardMaterial3D
# Her.
var _figure: Node3D
var _eye_light: OmniLight3D
var _p := {}    # named pivots and parts
var _head_yaw := 0.0
# For a test: what the view has shown.
var figure_frames := 0
var door_tore_off := 0
var bolts_shown_popped := 0


func setup(native: Object, audio: Object = null) -> void:
	name = "MatronView"
	_native = native
	_audio = audio
	_build_locker()
	_build_figure()
	_build_planks()
	_build_stencils()


# --- the locker -------------------------------------------------------------


func _material(color: Color, rough: float, metallic := 0.0, emit := Color(0, 0, 0), energy := 0.0) -> StandardMaterial3D:
	var m := StandardMaterial3D.new()
	m.albedo_color = color
	m.roughness = rough
	m.metallic = metallic
	if energy > 0.0:
		m.emission_enabled = true
		m.emission = emit
		m.emission_energy_multiplier = energy
	return m


func _box(size: Vector3, material: Material, parent: Node3D, at := Vector3.ZERO) -> MeshInstance3D:
	var mesh := BoxMesh.new()
	mesh.size = size
	var node := MeshInstance3D.new()
	node.mesh = mesh
	node.material_override = material
	node.position = at
	parent.add_child(node)
	return node


func _cyl(top: float, bottom: float, height: float, material: Material, parent: Node3D, at := Vector3.ZERO, sides := 10) -> MeshInstance3D:
	var mesh := CylinderMesh.new()
	mesh.top_radius = top
	mesh.bottom_radius = bottom
	mesh.height = height
	mesh.radial_segments = sides
	mesh.rings = 1
	var node := MeshInstance3D.new()
	node.mesh = mesh
	node.material_override = material
	node.position = at
	parent.add_child(node)
	return node


func _ball(radius: float, material: Material, parent: Node3D, at := Vector3.ZERO, scale_to := Vector3.ONE) -> MeshInstance3D:
	var mesh := SphereMesh.new()
	mesh.radius = radius
	mesh.height = radius * 2.0
	mesh.radial_segments = 14
	mesh.rings = 8
	var node := MeshInstance3D.new()
	node.mesh = mesh
	node.material_override = material
	node.position = at
	node.scale = scale_to
	parent.add_child(node)
	return node


func _pivot(parent: Node3D, at := Vector3.ZERO) -> Node3D:
	var node := Node3D.new()
	node.position = at
	parent.add_child(node)
	return node


func _build_locker() -> void:
	var steel := _material(Color("5d6c66"), 0.8, 0.12)
	var worn := _material(Color("3c4440"), 0.9, 0.1)
	var rust := _material(Color("5a3a24"), 0.95)
	var bolt_metal := _material(Color("6f6a60"), 0.5, 0.8)
	_vent_material = _material(Color("0b0807"), 0.9, 0.0, Color("ff7a1c"), 0.0)
	_locker = _pivot(self, LOCKER_CENTRE)
	# The shell: back and sides and top and base, the door left to _door.
	var s := LOCKER_SIZE
	_box(Vector3(s.x, s.y, 0.05), steel, _locker, Vector3(0.0, 0.0, s.z * 0.5 - 0.025))
	_box(Vector3(0.05, s.y, s.z), steel, _locker, Vector3(-s.x * 0.5 + 0.025, 0.0, 0.0))
	_box(Vector3(0.05, s.y, s.z), steel, _locker, Vector3(s.x * 0.5 - 0.025, 0.0, 0.0))
	_box(Vector3(s.x, 0.06, s.z), worn, _locker, Vector3(0.0, s.y * 0.5 - 0.03, 0.0))
	_box(Vector3(s.x, 0.1, s.z), worn, _locker, Vector3(0.0, -s.y * 0.5 + 0.05, 0.0))
	# Inside: dark, so the burst locker is a black mouth.
	_box(Vector3(s.x - 0.1, s.y - 0.15, 0.02), _material(Color("050403"), 1.0), _locker, Vector3(0.0, 0.0, s.z * 0.5 - 0.06))
	# The door: on a pivot at its far edge so it can bow, with vents, bolts,
	# dents that push out from inside, the chain and the stencil.
	_door = _pivot(_locker, Vector3(0.0, 0.0, -s.z * 0.5))
	_box(Vector3(s.x, s.y, 0.045), steel, _door)
	for k in 5:
		_box(Vector3(0.5, 0.025, 0.05), _vent_material, _door, Vector3(0.0, 0.62 - 0.1 * float(k), -0.005))
	for k in BOLT_COUNT:
		var bolt := _cyl(0.04, 0.04, 0.04, bolt_metal, _door, Vector3(0.52, 0.8 - 0.55 * float(k), -0.04), 8)
		bolt.rotation.x = PI * 0.5
		_bolts.append(bolt)
	for at in [Vector3(-0.2, 0.2, -0.03), Vector3(0.18, -0.2, -0.03), Vector3(-0.15, -0.6, -0.03), Vector3(0.2, 0.55, -0.03)]:
		var dent := _ball(0.17, steel, _door, at, Vector3(1.0, 1.4, 0.18))
		_dents.append(dent)
	var chain := _material(Color("2d2a26"), 0.5, 0.9)
	for k in 9:
		var link := _box(Vector3(0.1, 0.045, 0.045), chain, _door, Vector3(-0.5 + 0.125 * float(k), -0.05, -0.05))
		link.rotation.x = 0.5 * float(k % 2)
	_box(Vector3(0.1, 0.13, 0.07), _material(Color("7a6a3a"), 0.4, 0.8), _door, Vector3(0.0, -0.1, -0.06))
	_box(Vector3(0.26, 0.4, 0.004), rust, _door, Vector3(-0.25, -0.88, -0.026))
	var stencil := Label3D.new()
	stencil.text = "9"
	stencil.font_size = 220
	stencil.pixel_size = 0.0016
	stencil.modulate = Color("b9b09a")
	stencil.position = Vector3(-0.25, -0.88, -0.03)
	stencil.rotation.y = PI
	stencil.double_sided = false
	_door.add_child(stencil)
	# The strain gauge on the wall beside it: a dial, a needle and a red zone.
	var gauge := _pivot(self, Vector3(LOCKER_CENTRE.x - LOCKER_SIZE.x * 0.5 - 0.03, DECK + 1.5, LOCKER_CENTRE.z - 0.05))
	gauge.rotation.y = PI * 0.5
	_box(Vector3(0.5, 0.5, 0.05), worn, gauge)
	var face := _cyl(0.2, 0.2, 0.03, _material(Color("cfc9b6"), 0.7), gauge, Vector3(0.0, 0.0, -0.035), 20)
	face.rotation.x = PI * 0.5
	for k in 7:
		var a := lerpf(-2.2, 2.2, float(k) / 6.0)
		var tick := _box(Vector3(0.012, 0.045, 0.01), _material(Color("2a2623"), 0.8), gauge, Vector3(sin(a) * 0.16, cos(a) * 0.16, -0.055))
		tick.rotation.z = -a
		if k >= 5:
			tick.scale = Vector3(1.8, 1.0, 1.0)
			tick.material_override = _material(Color("b01a12"), 0.6, 0.0, Color("ff2418"), 0.4)
	_needle = _pivot(gauge, Vector3(0.0, 0.0, -0.06))
	_box(Vector3(0.012, 0.17, 0.008), _material(Color("a31610"), 0.5, 0.0, Color("ff2418"), 0.5), _needle, Vector3(0.0, 0.07, 0.0))
	_needle.rotation.z = 2.2
	var label := Label3D.new()
	label.text = "LOAD"
	label.font_size = 60
	label.pixel_size = 0.0014
	label.modulate = Color("2a2623")
	label.position = Vector3(0.0, -0.1, -0.058)
	label.rotation.y = PI
	label.double_sided = false
	gauge.add_child(label)
	# The alarm lamp on the locker's top.
	_lamp_glass_material = _material(Color("7a1710"), 0.3, 0.0, Color("ff3a1c"), 0.0)
	_lamp_glass = _ball(0.1, _lamp_glass_material, _locker, Vector3(0.4, s.y * 0.5 + 0.1, -0.1), Vector3(1.0, 1.2, 1.0))
	_cyl(0.11, 0.13, 0.08, worn, _locker, Vector3(0.4, s.y * 0.5 + 0.04, -0.1), 10)
	# A caged work bulb clipped on the locker's top-left: the one steady light.
	_cyl(0.015, 0.015, 0.2, worn, _locker, Vector3(-0.35, s.y * 0.5 + 0.1, -0.2), 6)
	_work_glass_material = _material(Color("e9d9a0"), 0.3, 0.0, Color("ffd890"), 1.4)
	_ball(0.06, _work_glass_material, _locker, Vector3(-0.35, s.y * 0.5 + 0.22, -0.2))
	_work_light = OmniLight3D.new()
	_work_light.light_color = Color("ffd49a")
	_work_light.light_energy = 1.1
	_work_light.omni_range = 10.0
	_work_light.position = Vector3(-0.35, s.y * 0.5 + 0.3, -0.7)
	_locker.add_child(_work_light)
	_lamp_light = OmniLight3D.new()
	_lamp_light.light_color = Color("ff3418")
	_lamp_light.light_energy = 0.0
	_lamp_light.omni_range = 12.0
	_lamp_light.position = Vector3(0.4, s.y * 0.5 + 0.2, -0.5)
	_locker.add_child(_lamp_light)


func _update_locker(data: Dictionary, delta: float) -> void:
	var state := int(data["state"])
	var strain := float(data["strain"])
	var popped := int(data["bolts"])
	# Restored to before her burst (a checkpoint taken earlier): sealed again.
	if _door_gone and (state == 0 or state == STATE_STRAINING):
		_seal_locker()
	if state == STATE_STRAINING and not _door_gone:
		var shake := 0.003 + 0.022 * strain
		_locker.position = LOCKER_CENTRE + Vector3(sin(_clock * 73.0), 0.0, cos(_clock * 61.0)) * shake * (0.5 + 0.5 * sin(_clock * 9.0))
		_door.rotation.y = sin(_clock * 47.0) * (0.004 + 0.02 * strain) + 0.012 * float(popped)
		_vent_material.emission_enabled = true
		_vent_material.emission = Color("ff7a1c")
		_vent_material.emission_energy_multiplier = 0.2 + 2.6 * strain
		for k in _dents.size():
			var push := clampf(strain * 1.4 - 0.15 * float(k), 0.0, 1.0)
			_dents[k].scale = Vector3(1.0, 1.4, 0.18 + 0.9 * push)
			_dents[k].position.z = -0.03 - 0.05 * push
		# The lamp: faster as it climbs.
		var pulse := 0.5 + 0.5 * sin(_clock * TAU * (1.2 + 7.0 * strain))
		_lamp_light.light_energy = 0.4 + 4.0 * pulse
		_lamp_glass_material.emission_enabled = true
		_lamp_glass_material.emission_energy_multiplier = 0.3 + 3.0 * pulse
		# The work bulb stutters as it climbs.
		var flick := 0.55 + 0.45 * sin(_clock * 53.0) * sin(_clock * 17.0)
		_work_light.light_energy = lerpf(1.1, 1.1 * flick, strain)
		_work_glass_material.emission_energy_multiplier = lerpf(1.4, 1.4 * flick, strain)
		# Knocks, hastening.
		_knock_in -= delta
		if _knock_in <= 0.0:
			_knock_in = lerpf(1.5, 0.16, strain)
			if _audio != null:
				_audio.matron_cue(&"locker_knock", LOCKER_CENTRE, lerpf(-9.0, 0.0, strain), randf_range(0.85, 1.1))
	if state != STATE_STRAINING and not _door_gone:
		_locker.position = LOCKER_CENTRE
		_lamp_light.light_energy = 0.0
		_lamp_glass_material.emission_energy_multiplier = 0.0
	_needle.rotation.z = lerpf(2.2, -2.2, strain) if state == STATE_STRAINING or _door_gone else 2.2
	# Bolts pop one by one.
	while _bolts_seen < popped and _bolts_seen < BOLT_COUNT:
		_bolts[_bolts_seen].visible = false
		_bolts_seen += 1
		bolts_shown_popped = _bolts_seen
		if _audio != null:
			_audio.matron_cue(&"bolt_pop", LOCKER_CENTRE + Vector3(0.5, 0.0, -0.5), -2.0, randf_range(0.9, 1.15))
	# The burst: the door goes, into the shaft.
	if state == STATE_EMERGING and _last_state == STATE_STRAINING and not _door_gone:
		_tear_door()
	if _door_gone and _door.visible:
		_door_vel.y -= 9.81 * delta
		_door.global_position += _door_vel * delta
		_door.rotation += _door_spin * delta
		if _door.global_position.y < DECK - 40.0:
			_door.visible = false
		_lamp_light.light_energy = maxf(0.0, _lamp_light.light_energy - 8.0 * delta)


func _tear_door() -> void:
	_door_gone = true
	door_tore_off += 1
	var at := _door.global_position
	_door.reparent(self, true)
	_door.global_position = at
	_work_light.light_energy = 0.0
	_work_glass_material.emission_energy_multiplier = 0.0
	_door_vel = Vector3(0.0, 4.0, -16.0)
	_door_spin = Vector3(3.5, 1.2, 4.0)
	_door.visible = true
	_locker.position = LOCKER_CENTRE
	_lamp_glass_material.emission_energy_multiplier = 0.0
	_vent_material.emission_energy_multiplier = 0.0
	if _audio != null:
		_audio.matron_cue(&"locker_burst", LOCKER_CENTRE, 3.0, 1.0)
		_audio.matron_cue(&"matron_shriek", LOCKER_CENTRE, 0.0, 0.9)
		_shriek_in = 6.0


func _seal_locker() -> void:
	_door_gone = false
	_door.reparent(_locker, false)
	_door.position = Vector3(0.0, 0.0, -LOCKER_SIZE.z * 0.5)
	_door.rotation = Vector3.ZERO
	_door.visible = true
	_bolts_seen = 0
	_work_light.light_energy = 1.1
	_work_glass_material.emission_energy_multiplier = 1.4
	for bolt in _bolts:
		bolt.visible = true
	bolts_shown_popped = 0


# --- the catwalk -------------------------------------------------------------


func _build_planks() -> void:
	for i in PLANKS:
		var tone := Color("5e4b3a") if i % 2 == 0 else Color("4f4235")
		var plate := _box(PLANK_SIZE, _material(tone, 0.92, 0.35), self)
		# Rivets along both edges and a rusted seam: the plate reads as old plate.
		var rivet_material := _material(Color("3a3028"), 0.8, 0.6)
		for k in 6:
			for side in [-1.0, 1.0]:
				var rivet := _cyl(0.035, 0.035, 0.02, rivet_material, plate, Vector3(-1.3 + 0.52 * float(k), 0.05, side * 0.72), 8)
				rivet.scale = Vector3(1.0, 1.0, 1.0)
		_box(Vector3(PLANK_SIZE.x, 0.01, 0.06), _material(Color("2b2018"), 1.0), plate, Vector3(0.0, 0.045, 0.0))
		plate.visible = false
		_planks.append(plate)
		_plank_bolted.append(true)


func _build_stencils() -> void:
	# Hazard lettering painted on the deck at both ends of the catwalk: a plain
	# warning, so the climber who reads it is warned, and the one who does not
	# learns what it meant.
	for end in [-1.0, 1.0]:
		var text := Label3D.new()
		text.text = "WALKWAY\nCLOSED"
		text.font_size = 150
		text.pixel_size = 0.0075
		text.modulate = Color("c9a21e")
		text.outline_modulate = Color("2a2108")
		text.position = Vector3(end * 20.5, DECK + 0.03, -150.0)
		text.rotation = Vector3(-PI * 0.5, 0.0, 0.0)
		text.rotation.z = (PI * 0.5) if end > 0.0 else -PI * 0.5
		text.double_sided = false
		add_child(text)


func _update_planks(data: Dictionary) -> void:
	var planks: PackedFloat32Array = data["planks"]
	for i in PLANKS:
		var b := i * PLANK_FLOATS
		if b + PLANK_FLOATS > planks.size():
			break
		var gone := planks[b + 8] > 0.5
		var bolted := planks[b + 7] > 0.5
		var plate := _planks[i]
		plate.visible = not gone
		if gone:
			continue
		plate.global_position = Vector3(planks[b], planks[b + 1], planks[b + 2])
		plate.global_basis = Basis(Quaternion(planks[b + 3], planks[b + 4], planks[b + 5], planks[b + 6]))
		if _plank_bolted[i] and not bolted and _audio != null:
			_audio.matron_cue(&"plank_crack", plate.global_position, 3.0, randf_range(0.85, 1.1))
		_plank_bolted[i] = bolted


# --- her ---------------------------------------------------------------------


func _build_figure() -> void:
	var skin := _material(Color("b9b2a2"), 0.85)
	var dress := _material(Color("4a4034"), 0.95)
	var shawl := _material(Color("1f1c19"), 0.98)
	var hair := _material(Color("cfccc2"), 0.9)
	var sock := _material(Color("8b857a"), 0.9)
	var slipper := _material(Color("2a1f24"), 0.9)
	var eye_glass := _material(Color("120d0a"), 0.3, 0.0, Color("ffc04a"), 1.6)
	var gum := _material(Color("2c0a0c"), 0.7)
	var tooth := _material(Color("b8aa7e"), 0.7)
	_figure = _pivot(self)
	_figure.visible = false
	var body := _pivot(_figure, Vector3(0.0, 1.0, 0.0))
	_p["body"] = body
	_box(Vector3(0.3, 0.2, 0.2), dress, body, Vector3(0.0, -0.02, 0.0))
	var torso := _pivot(body, Vector3(0.0, 0.08, 0.0))
	_p["torso"] = torso
	var trunk := _cyl(0.12, 0.17, TORSO, dress, torso, Vector3(0.0, TORSO * 0.5, 0.0), 10)
	trunk.scale = Vector3(1.0, 1.0, 0.8)
	# The shawl over the shoulders and the hump under it.
	var cape := _cyl(0.1, 0.27, 0.3, shawl, torso, Vector3(0.0, TORSO - 0.1, -0.02), 10)
	cape.scale = Vector3(1.0, 1.0, 0.85)
	_ball(0.15, shawl, torso, Vector3(0.0, TORSO - 0.08, -0.14), Vector3(1.0, 0.85, 0.9))
	var skirt := _cyl(0.17, 0.3, 0.55, dress, body, Vector3(0.0, -0.3, 0.0), 12)
	skirt.scale = Vector3(1.0, 1.0, 0.85)
	_p["skirt"] = skirt
	# Neck and head.
	var neck := _pivot(torso, Vector3(0.0, TORSO + 0.02, 0.03))
	_p["neck"] = neck
	_cyl(0.035, 0.045, NECK, skin, neck, Vector3(0.0, NECK * 0.5, 0.0), 8)
	var head := _pivot(neck, Vector3(0.0, NECK, 0.0))
	_p["head"] = head
	_ball(0.095, skin, head, Vector3(0.0, 0.09, 0.0), Vector3(0.9, 1.25, 1.0))
	# A headscarf pulled low, and the stringy hair under it.
	var scarf := _ball(0.105, shawl, head, Vector3(0.0, 0.12, -0.02), Vector3(0.95, 1.05, 1.05))
	scarf.rotation.x = -0.2
	for k in 9:
		var a := lerpf(-0.9, 0.9, float(k) / 8.0)
		var strand := _cyl(0.004, 0.002, 0.55, hair, head, Vector3(sin(a) * 0.1, -0.1, -0.08 - 0.02 * cos(a)), 4)
		strand.rotation.x = 0.1
		strand.rotation.z = a * 0.15
	for side in [-1.0, 1.0]:
		var socket := _ball(0.032, _material(Color("090605"), 1.0), head, Vector3(side * 0.04, 0.1, 0.08), Vector3(1.0, 1.15, 0.6))
		var eye := _ball(0.011, eye_glass, head, Vector3(side * 0.04, 0.1, 0.1), Vector3.ONE)
		_p["eye%d" % int(side)] = eye
		socket.rotation.z = side * 0.25
	var nose := _cyl(0.001, 0.017, 0.06, skin, head, Vector3(0.0, 0.065, 0.1), 6)
	nose.rotation.x = -0.9
	# The jaw: hangs from a hinge under the ear line, with a grin of teeth.
	var jaw := _pivot(head, Vector3(0.0, 0.03, 0.01))
	_p["jaw"] = jaw
	_box(Vector3(0.1, 0.035, 0.11), skin, jaw, Vector3(0.0, -0.02, 0.05))
	_box(Vector3(0.07, 0.02, 0.05), gum, jaw, Vector3(0.0, 0.0, 0.07))
	for k in 7:
		_box(Vector3(0.008, 0.026, 0.006), tooth, jaw, Vector3(-0.036 + 0.012 * float(k), 0.006, 0.098))
	_box(Vector3(0.1, 0.012, 0.04), gum, head, Vector3(0.0, 0.034, 0.075))
	# Arms to her knees, and long, hooked hands.
	for side in [-1.0, 1.0]:
		var key := "l" if side < 0.0 else "r"
		var shoulder := _pivot(torso, Vector3(side * 0.19, TORSO - 0.05, 0.0))
		_p["sh_" + key] = shoulder
		var upper := _cyl(0.034, 0.038, UPPER_ARM, shawl, shoulder, Vector3(0.0, -UPPER_ARM * 0.5, 0.0), 8)
		var elbow := _pivot(shoulder, Vector3(0.0, -UPPER_ARM, 0.0))
		_p["el_" + key] = elbow
		_cyl(0.026, 0.032, FOREARM, skin, elbow, Vector3(0.0, -FOREARM * 0.5, 0.0), 8)
		var wrist := _pivot(elbow, Vector3(0.0, -FOREARM, 0.0))
		_p["wr_" + key] = wrist
		_box(Vector3(0.07, 0.1, 0.025), skin, wrist, Vector3(0.0, -0.05, 0.0))
		for f in 4:
			var finger := _pivot(wrist, Vector3(-0.027 + 0.018 * float(f), -0.1, 0.0))
			_cyl(0.005, 0.007, 0.12 - 0.012 * absf(float(f) - 1.5), skin, finger, Vector3(0.0, -0.06, 0.0), 5)
			finger.rotation.x = -0.35
		upper.rotation.z = 0.0
		_box(Vector3(0.012, 0.012, 0.03), skin, wrist, Vector3(side * 0.03, -0.04, 0.02))
	# Legs: thin, grey-stockinged, in old slippers.
	for side in [-1.0, 1.0]:
		var key := "l" if side < 0.0 else "r"
		var hip := _pivot(body, Vector3(side * 0.08, -0.08, 0.0))
		_p["hp_" + key] = hip
		_cyl(0.045, 0.04, THIGH, sock, hip, Vector3(0.0, -THIGH * 0.5, 0.0), 8)
		var knee := _pivot(hip, Vector3(0.0, -THIGH, 0.0))
		_p["kn_" + key] = knee
		_cyl(0.034, 0.028, SHIN, sock, knee, Vector3(0.0, -SHIN * 0.5, 0.0), 8)
		var ankle := _pivot(knee, Vector3(0.0, -SHIN, 0.0))
		_p["an_" + key] = ankle
		_box(Vector3(0.09, 0.05, 0.24), slipper, ankle, Vector3(0.0, -0.025, 0.07))
	# The music box on its cord at her hip.
	var cord := _pivot(body, Vector3(-0.2, -0.05, 0.0))
	_p["box_cord"] = cord
	_cyl(0.004, 0.004, 0.3, _material(Color("3a2f26"), 0.9), cord, Vector3(0.0, -0.15, 0.0), 4)
	_box(Vector3(0.13, 0.1, 0.1), _material(Color("6a4a2a"), 0.7), cord, Vector3(0.0, -0.33, 0.0))
	_box(Vector3(0.1, 0.008, 0.075), _material(Color("a79a62"), 0.5, 0.8), cord, Vector3(0.0, -0.28, 0.0))
	# A cold glimmer to be seen by in the dark.
	_eye_light = OmniLight3D.new()
	_eye_light.light_color = Color("c8d8b0")
	_eye_light.light_energy = 0.0
	_eye_light.omni_range = 5.0
	_eye_light.position = Vector3(0.0, 1.0, 0.4)
	_figure.add_child(_eye_light)


func _limb(up: Node3D, low: Node3D, fwd: float, bend: float, splay := 0.0) -> void:
	up.rotation = Vector3(-fwd, 0.0, splay)
	low.rotation = Vector3(bend, 0.0, 0.0)


# A phase held and snapped: film that has dropped frames.
func _stutter(x: float, rate: float) -> float:
	return floorf(x * rate) / rate


func _hash(n: int) -> float:
	var v := sin(float(n) * 12.9898) * 43758.5453
	return v - floorf(v)


func _pose(data: Dictionary, delta: float, player_position: Vector3, falling: bool) -> void:
	var state := int(data["state"])
	var speed := float(data["speed"])
	var stride := float(data["stride"])
	var sees := bool(data["sees_player"])
	var body: Node3D = _p["body"]
	var torso: Node3D = _p["torso"]
	var neck: Node3D = _p["neck"]
	var head: Node3D = _p["head"]
	var jaw: Node3D = _p["jaw"]
	var pitch := 0.7
	var hip_y := 0.98
	var head_pitch := 0.0
	var head_roll := 0.0
	var jaw_open := 0.08
	var yaw_to_player := 0.0
	var figure_yaw := _figure.rotation.y
	var to_player := player_position - _figure.global_position
	yaw_to_player = clampf(wrapf(atan2(to_player.x, to_player.z) - figure_yaw, -PI, PI), -1.4, 1.4)
	var legs_fwd := 0.0
	var legs_back := 0.0
	var arms_fwd := 0.0
	var arms_bend := 0.0
	var arms_splay := 0.0
	var knees := 0.0
	var wide := 0.0
	var tick := int(floorf(_clock * 14.0))
	if falling:
		pitch = 0.2
		hip_y = 1.0
		head_pitch = -0.5
		jaw_open = 0.9
		arms_fwd = 2.9 + sin(_clock * 31.0) * 0.3
		arms_splay = 0.5
		arms_bend = 0.3
		legs_fwd = sin(_clock * 27.0) * 0.8
		legs_back = -sin(_clock * 27.0) * 0.8
		knees = 0.5
	elif state == STATE_HUNTING and speed > 1.0:
		# All fours: hips high, chest low, arms reaching and elbows out, a
		# stuttering scuttle, the head up and cocked and always on you.
		var ph := _stutter(stride, 2.6) / 0.42 * PI
		pitch = 1.38 + 0.06 * sin(ph * 2.0)
		hip_y = 0.8 + 0.07 * (0.5 + 0.5 * sin(ph * 2.0 + _hash(tick) * 0.6))
		legs_fwd = 0.95 + sin(ph) * 0.65
		legs_back = 0.95 - sin(ph) * 0.65
		knees = 1.5 + 0.4 * cos(ph)
		arms_fwd = 1.05 + sin(ph + PI) * 0.55
		arms_bend = 1.35 - 0.5 * sin(ph + PI)
		arms_splay = 0.7
		head_pitch = -1.05
		head_roll = 0.65 + 0.3 * sin(_clock * 3.0)
		if sees:
			head_roll = 1.35
			jaw_open = 0.75
		else:
			jaw_open = 0.35
	elif state == STATE_HUNTING:
		# Stopped on the hunt (cornered by a wall, or listening): low, head snapping.
		pitch = 1.2
		hip_y = 0.65
		legs_fwd = 1.0
		legs_back = 1.0
		knees = 1.7
		arms_fwd = 0.9
		arms_bend = 1.2
		arms_splay = 0.7
		head_pitch = -0.9
		head_roll = 0.9 * (1.0 if _hash(int(_clock * 2.4)) > 0.5 else -1.0)
		jaw_open = 0.5
	elif state == STATE_EMERGING:
		# Out of the locker: in stages, unfolding, arms flung wide, head back.
		var stage := clampf(_state_age / 2.0, 0.0, 1.0)
		var snap := floorf(stage * 4.0) / 4.0
		pitch = lerpf(1.7, 0.4, snap)
		hip_y = lerpf(0.55, 1.0, snap)
		knees = lerpf(1.8, 0.2, snap)
		legs_fwd = lerpf(1.2, 0.0, snap)
		legs_back = legs_fwd
		arms_fwd = lerpf(0.4, 1.0, snap)
		arms_splay = lerpf(0.2, 1.5, snap)
		arms_bend = 0.4
		head_pitch = lerpf(-0.2, 0.6, snap)
		head_roll = (1.0 if int(_state_age * 6.0) % 2 == 0 else -1.0) * 0.4
		jaw_open = 0.95
	elif state == STATE_SEARCHING:
		# Head snapping to one side then another, a hand cupped to an ear.
		var look := _hash(int(_clock * 1.6) + 7)
		var target := (look - 0.5) * 3.0
		_head_yaw = lerpf(_head_yaw, target, 1.0 - exp(-22.0 * delta))
		pitch = 0.95
		hip_y = 0.93
		var step := stride / 0.5 * PI
		legs_fwd = sin(step) * 0.3
		legs_back = -sin(step) * 0.3
		knees = 0.25
		arms_fwd = 0.2
		arms_bend = 0.3
		head_pitch = 0.15
		head_roll = 0.3
		jaw_open = 0.2 + 0.15 * sin(_clock * 11.0)
	else:
		# Shuffling: stooped, dragging, a head that lolls and twitches.
		var step := stride / 0.42 * PI
		pitch = 0.68 + 0.04 * sin(_clock * 0.7)
		hip_y = 0.97
		legs_fwd = sin(step) * 0.28
		legs_back = -sin(step) * 0.28
		knees = 0.16 + 0.1 * maxf(0.0, sin(step + 1.0))
		arms_fwd = 0.3 + 0.1 * sin(step)
		arms_bend = 0.25
		head_pitch = 0.3
		head_roll = sin(_clock * 0.8) * 0.3 + (0.35 if _hash(int(floorf(_clock * 2.8))) > 0.93 else 0.0)
		jaw_open = 0.1 + 0.05 * sin(_clock * 3.0)
	body.position.y = hip_y
	torso.rotation.x = pitch
	neck.rotation = Vector3(head_pitch * 0.4, 0.0, head_roll * 0.3)
	head.rotation = Vector3(head_pitch * 0.6 - pitch * 0.25, _head_yaw if state == STATE_SEARCHING else yaw_to_player * (0.6 if state == STATE_HUNTING else 0.0), head_roll * 0.7)
	jaw.rotation.x = jaw_open
	_limb(_p["hp_l"], _p["kn_l"], legs_fwd, knees)
	_limb(_p["hp_r"], _p["kn_r"], legs_back, knees)
	_p["an_l"].rotation.x = -0.2
	_p["an_r"].rotation.x = -0.2
	_limb(_p["sh_l"], _p["el_l"], arms_fwd + (0.2 * sin(stride * 4.0) if state == STATE_SHUFFLING else 0.0), -arms_bend, -arms_splay)
	_limb(_p["sh_r"], _p["el_r"], arms_fwd, -arms_bend, arms_splay)
	if state == STATE_SEARCHING:
		# The right hand cupped to the ear.
		_limb(_p["sh_r"], _p["el_r"], 1.6, -2.0, 0.5)
	_p["box_cord"].rotation = Vector3(sin(stride * 3.0) * 0.4 + 0.2 * (pitch - 0.7), 0.0, sin(stride * 2.2) * 0.3)
	_p["skirt"].visible = hip_y > 0.9 or state == STATE_SEARCHING
	var eyes_on := 1.0 if state != STATE_DEAD else 0.0
	_eye_light.light_energy = 0.35 * eyes_on


func _hip_world() -> Vector3:
	return (_p["body"] as Node3D).global_position


func _update_figure(data: Dictionary, delta: float, camera_position: Vector3, player_position: Vector3) -> void:
	var state := int(data["state"])
	var alive := bool(data["alive"])
	var released := bool(data["released"])
	var out := released and alive and state >= STATE_EMERGING and state != STATE_DEAD
	var centre: Vector3 = data["position"]
	# Fall speed from her own height, for the plunge.
	_fall_speed = (centre.y - _last_pos.y) / maxf(delta, 0.0001) if delta > 0.0 else 0.0
	var falling := out and _fall_speed < -6.0 and centre.y < DECK + 0.4
	_last_pos = centre
	if not out or centre.distance_to(camera_position) > DRAW_RANGE_M:
		_figure.visible = false
		return
	_figure.visible = true
	figure_frames += 1
	_figure.global_position = Vector3(centre.x, centre.y - STAND_CENTRE, centre.z)
	_figure.rotation.y = float(data["yaw"])
	_pose(data, delta, player_position, falling)
	var feet := _figure.global_position
	var d := feet.distance_to(player_position)
	# Her music box: heard from far off, slowing as she nears.
	if _audio != null:
		var volume := lerpf(-4.0, -42.0, clampf(d / HEARD_MUSIC_M, 0.0, 1.0))
		_audio.matron_music(_hip_world(), volume, lerpf(0.84, 1.0, clampf(d / 30.0, 0.0, 1.0)))
		# Her breath, closer and quicker the nearer she is.
		_breath_in -= delta
		if _breath_in <= 0.0 and d < 24.0:
			_breath_in = lerpf(0.7, 2.0, clampf(d / 24.0, 0.0, 1.0))
			_audio.matron_cue(&"matron_breath", _hip_world(), lerpf(-1.0, -14.0, clampf(d / 24.0, 0.0, 1.0)), randf_range(0.9, 1.05))
		# Steps: a dragged slipper, or on the hunt hands and feet slapping.
		var steps := int(float(data["stride"]) / (STEP_M * (0.55 if state == STATE_HUNTING else 1.0)))
		if steps != _last_step:
			_last_step = steps
			if float(data["speed"]) > 0.3 and not falling:
				_audio.matron_cue(&"matron_step", feet, lerpf(-14.0, -3.0, clampf(float(data["speed"]) / 6.8, 0.0, 1.0)), randf_range(0.85, 1.1) * (1.0 if state != STATE_HUNTING else 1.25))
		# The shriek: the moment she sees you, and in the long fall.
		var sees := bool(data["sees_player"])
		_shriek_in = maxf(0.0, _shriek_in - delta)
		if sees and not _was_seen and _shriek_in <= 0.0 and d < 40.0:
			_audio.matron_cue(&"matron_shriek", _hip_world(), -2.0, randf_range(0.95, 1.1))
			_shriek_in = 7.0
		_was_seen = sees
		if falling and not _cry_fall:
			_cry_fall = true
			_audio.matron_cue(&"matron_shriek", _hip_world(), 2.0, 0.8)
		elif not falling:
			_cry_fall = false


# For a test and the look scenes: where her head is, and how far gone the
# locker is.
func head_world_position() -> Vector3:
	return (_p["head"] as Node3D).global_position + Vector3(0.0, 0.1, 0.0)


func locker_front_position() -> Vector3:
	return LOCKER_CENTRE + Vector3(0.0, -0.2, -0.5)


func figure_shown() -> bool:
	return _figure != null and _figure.visible


func update_view(data: Dictionary, delta: float, camera_position: Vector3, player_position: Vector3) -> void:
	if _native == null:
		return
	_clock += delta
	var state := int(data["state"])
	if state != _last_state:
		_state_age = 0.0
	_state_age += delta
	var near := camera_position.distance_to(LOCKER_CENTRE) < DRAW_RANGE_M or camera_position.distance_to(_last_pos) < DRAW_RANGE_M
	visible = near
	if not near:
		_last_state = state
		return
	_update_planks(data)
	_update_locker(data, delta)
	_update_figure(data, delta, camera_position, player_position)
	_last_state = state
