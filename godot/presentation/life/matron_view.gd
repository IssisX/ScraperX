extends Node3D
## The Matron, drawn (src/sim/matron.hpp): her locker on the 154 m deck, the
## gauge that climbs while she strains, the bolts that pop and the door that
## tears off; her figure, a real old woman built from MakeHuman's scanned body
## (life/matron/, tools/body/build_matron_body.py) in the first-aid
## attendant's uniform gone foul, and the way she moves; and the rotten
## catwalk's plates. Every position and every state is the native's, and
## nothing here moves anything: she is a capsule that walks, and the pose is
## only what her state and the metres she has walked make of it.
##
## Her gaits: stooped and dragging when unaware, snapping her head about when
## she has lost the player, and on the hunt down on all fours in a fast,
## jerking scuttle, head cocked on a neck that keeps looking at you. Every
## move of hers is drawn through: her body between the simulation's ticks
## (render_position, render_yaw), her joints never faster than a joint turns
## (POSE_RAD_S), her hands reaching to each new hold. Nothing jumps a frame.

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
const STATE_GUARDING := 7   # at her post in the mouth of the way out
const STATE_PEERING := 8    # stopped short of a noise, looking
const STATE_UNRIGGING := 9  # at the way out's eye, taking the player's rope off it
const STATE_STAGGERING := 10  # a blow took her balance
const STATE_DOWN := 11        # a blow took her off her feet
# Holding the bay's leaf up (src/sim/bay.hpp): about the most her palms give.
const BRACE_FULL_N := 930.0
const TOPPLE_S := 0.35   # her fall to the steel, and her rise off it
const BOLT_COUNT := 4
# How fast her drawn joints may go to the pose her state asks for: a body's
# limbs whip round this fast, and a change of gait or state is that whip,
# not a jump. Her hips' height and her limbs' swell likewise.
const POSE_RAD_S := 20.0
const POSE_HIP_M_S := 4.0
const POSE_SCALE_S := 3.0
# Her whole figure turned or laid over (toppled, head down her pipe).
const FIGURE_RAD_S := 20.0
# Her hands onto each new hold: most of the way in a tenth of a second.
const HAND_EASE := 22.0
const POSE_KEYS := ["torso", "neck", "head", "jaw", "sh_l", "sh_r", "el_l", "el_r",
	"hp_l", "hp_r", "kn_l", "kn_r", "an_l", "an_r"]

# Her proportions: long in the limb, narrow in the chest.
const THIGH := 0.46
const SHIN := 0.5
const UPPER_ARM := 0.44
const FOREARM := 0.5
const TORSO := 0.62
const NECK := 0.16

const MatronSound := preload("res://presentation/audio/matron_sound.gd")

var _native: Object
var _audio: Object
# What she does heard (presentation/audio/matron_sound.gd): made with the
# director, none without one.
var _sound: RefCounted
var _clock := 0.0
var _last_state := -1
var _state_age := 0.0
var _bolts_seen := 0
var _knock_in := 0.6
var _player_d := 20.0   # her last distance to the player, for her dread
var _last_pos := Vector3.ZERO
var _fall_speed := 0.0
# Her facing as drawn this frame (between ticks), and her joints, hips, limb
# swell and figure as last drawn. Fresh: drawn where the pose is (she has
# just come into sight, or been put somewhere).
var _drawn_yaw := 0.0
# How far over she is drawn, toppled (0 standing, 1 laid flat), and the way
# she went: eased at TOPPLE_S's pace (_topple).
var _topple_k := 0.0
var _topple_fall := Vector3(0.0, 0.0, 1.0)
var _pose_pivots: Array[Node3D] = []
var _pose_shown := PackedVector3Array()
var _scale_shown := PackedVector3Array()
var _hip_shown := Vector3.ZERO
var _figure_shown := Quaternion.IDENTITY
var _pose_fresh := true
# Her two holds as drawn on what she climbs (matron_rig.gd contact reaches
# for these).
var _held_shown := PackedVector3Array([Vector3.ZERO, Vector3.ZERO])
var _hands_fresh := true
# Going down a line's bare cable: the sparks off her forearms.
var _sparks: CPUParticles3D
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
	if audio != null:
		_sound = MatronSound.new(audio)
	_build_locker()
	_build_figure()
	_build_planks()
	_build_stencils()
	_build_sparks()


# Sparks off a steel cable where her forearms ride it: hot, short, falling.
func _build_sparks() -> void:
	var hot := StandardMaterial3D.new()
	hot.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	hot.albedo_color = Color(1.0, 0.62, 0.22)
	hot.emission_enabled = true
	hot.emission = Color(1.0, 0.55, 0.15)
	hot.emission_energy_multiplier = 3.0
	var spark := BoxMesh.new()
	spark.size = Vector3(0.012, 0.012, 0.07)
	spark.material = hot
	_sparks = CPUParticles3D.new()
	_sparks.mesh = spark
	_sparks.amount = 48
	_sparks.lifetime = 0.45
	_sparks.local_coords = false
	_sparks.emitting = false
	_sparks.emission_shape = CPUParticles3D.EMISSION_SHAPE_SPHERE
	_sparks.emission_sphere_radius = 0.12
	_sparks.direction = Vector3.DOWN
	_sparks.spread = 65.0
	_sparks.initial_velocity_min = 1.5
	_sparks.initial_velocity_max = 4.5
	_sparks.gravity = Vector3(0.0, -9.81, 0.0)
	_sparks.particle_flag_align_y = true
	add_child(_sparks)


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
	# Its line, 301 pixels tall, inside the 0.4 m plate.
	stencil.pixel_size = 0.0013
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
		_audio.matron_dread(LOCKER_CENTRE, _player_d)
		_sound.dread_in = 6.0


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


const MatronRig := preload("res://presentation/life/matron_rig.gd")
const MatronLook := preload("res://presentation/life/matron_look.gd")
const BODY_FILE := "res://presentation/life/matron/matron.glb"
const RIG_FILE := "res://presentation/life/matron/matron_rig.json"
var _rig: RefCounted     # matron_rig.gd: pivots to bones, contact, eyes, hair
var _look: RefCounted    # matron_look.gd: her materials and the exposure on them


func _build_figure() -> void:
	# The site's first-aid attendant (the story, 2026-10-19), redesigned from
	# scratch (the owner, 2026-10-20): a real old woman, heavy in the old way,
	# pale and wrinkled, big knuckled hands, her uniform torn and foul, long
	# thin grey hair that swings, eyes that find you. _pose() turns the pivots
	# below; matron_rig.gd copies them to her skeleton's bones.
	_figure = _pivot(self)
	_figure.visible = false
	_build_pivots()
	_load_body()
	# A cold glimmer to be seen by in the dark.
	_eye_light = OmniLight3D.new()
	_eye_light.light_color = Color("d4e4c4")
	_eye_light.light_energy = 0.0
	_eye_light.omni_range = 5.0
	_eye_light.position = Vector3(0.0, 1.5, 1.4)
	_figure.add_child(_eye_light)


# The joints _pose() works on, named as it names them. matron_rig.gd moves
# each to her body's own joint.
func _build_pivots() -> void:
	var body := _pivot(_figure, Vector3(0.0, 1.0, 0.0))
	_p["body"] = body
	var skirt := _pivot(body)
	_p["skirt"] = skirt
	_p["rags"] = _pivot(skirt, Vector3(0.0, -0.58, 0.0))
	var torso := _pivot(body, Vector3(0.0, 0.08, 0.0))
	_p["torso"] = torso
	_p["cape"] = _pivot(torso, Vector3(0.0, 0.6, -0.05))
	_p["hump"] = _pivot(torso, Vector3(0.0, 0.52, -0.15))
	_p["vertebrae"] = []
	var neck := _pivot(torso, Vector3(0.0, 0.62, -0.075))
	_p["neck"] = neck
	var head := _pivot(neck, Vector3(0.0, NECK, 0.03))
	_p["head"] = head
	var centre := _pivot(head, Vector3(0.0, 0.1, 0.0))
	_p["jaw"] = _pivot(centre, Vector3(0.0, -0.03, -0.02))
	_p["hair"] = _pivot(centre)
	for side in [-1.0, 1.0]:
		var key := "l" if side < 0.0 else "r"
		var shoulder := _pivot(torso, Vector3(side * 0.19, 0.55, -0.06))
		_p["sh_" + key] = shoulder
		var elbow := _pivot(shoulder, Vector3(0.0, -UPPER_ARM, 0.0))
		_p["el_" + key] = elbow
		var wrist := _pivot(elbow, Vector3(0.0, -FOREARM, 0.0))
		_p["wr_" + key] = wrist
		# Four fingers of three joints and a thumb of two, each joint a pivot.
		var curls := []
		for f in 4:
			var joint: Node3D = _pivot(wrist, Vector3(-0.0225 + 0.015 * float(f), -0.092, 0.0))
			var chain := []
			for seg in 3:
				chain.append(joint)
				joint = _pivot(joint, Vector3(0.0, -0.04, 0.0))
			curls.append(chain)
		var thumb_joint: Node3D = _pivot(wrist, Vector3(side * 0.03, -0.045, 0.0))
		thumb_joint.rotation.z = side * 0.6
		var thumb := []
		for seg in 2:
			thumb.append(thumb_joint)
			thumb_joint = _pivot(thumb_joint, Vector3(0.0, -0.035, 0.0))
		curls.append(thumb)
		_p["fingers_" + key] = curls
	for side in [-1.0, 1.0]:
		var key := "l" if side < 0.0 else "r"
		var hip := _pivot(body, Vector3(side * 0.085, -0.08, 0.0))
		_p["hp_" + key] = hip
		var knee := _pivot(hip, Vector3(0.0, -THIGH, 0.0))
		_p["kn_" + key] = knee
		_p["an_" + key] = _pivot(knee, Vector3(0.0, -SHIN, 0.0))
	_p["box_cord"] = _pivot(body, Vector3(-0.2, -0.05, 0.0))


# Her body: the skinned glTF read at runtime (kept as it is by its .import),
# dressed by matron_look.gd and driven by matron_rig.gd.
func _load_body() -> void:
	var doc := GLTFDocument.new()
	var state := GLTFState.new()
	var err := doc.append_from_file(BODY_FILE, state)
	if err != OK:
		push_error("matron: cannot read her body %s (%s)" % [BODY_FILE, error_string(err)])
		return
	var root := doc.generate_scene(state) as Node3D
	if root == null:
		push_error("matron: her body %s made no scene" % BODY_FILE)
		return
	var skeletons := root.find_children("*", "Skeleton3D", true, false)
	var rig_data: Variant = JSON.parse_string(FileAccess.get_file_as_string(RIG_FILE))
	if skeletons.is_empty() or not (rig_data is Dictionary):
		push_error("matron: her body has no skeleton, or %s is not her rig" % RIG_FILE)
		root.free()
		return
	_figure.add_child(root)
	_look = MatronLook.new()
	_look.build()
	_look.dress(root)
	_rig = MatronRig.new()
	_rig.setup(skeletons[0] as Skeleton3D, rig_data, self)


# Blood on her (gore_view.gd decides how much; matron_look.gd lays it on).
func apply_gore(soak: float, spatter: float, smear: float, hands: float, hand_l: Vector3, hand_r: Vector3,
		floor_y: float = 0.0) -> void:
	if _look != null:
		_look.apply_gore(soak, spatter, smear, hands, hand_l, hand_r, floor_y)


# The exposure's work on her look (matron_look.gd), one way.
func _apply_exposure(exposure: float) -> void:
	if _look != null:
		_look.apply_exposure(exposure)


# Her real arms are shorter than her old figure's: on all fours her chest
# comes down to arm's length (a head-down bear crawl, hips high), the arms and
# the head keeping their angle in the world, so her palms reach the deck.
func _crawl_retune(data: Dictionary, falling: bool) -> void:
	var turn := int(data.get("turn", 0))
	if int(data["state"]) != STATE_HUNTING or falling or _fall_speed > 1.0:
		return
	if bool(data.get("clinging", false)) or bool(data.get("climbing", false)) or bool(data.get("on_pipe", false)) \
			or bool(data.get("on_line", false)) or (turn == 0 and bool(data.get("wary", false))):
		return
	if not ((float(data["speed"]) > 1.0 and int(data.get("stage", 0)) > 0) or turn == 2):
		return
	(_p["torso"] as Node3D).rotation.x += 0.75
	for s in ["l", "r"]:
		(_p["sh_" + s] as Node3D).rotation.x -= 0.75
	(_p["head"] as Node3D).rotation.x -= 0.55
	(_p["neck"] as Node3D).rotation.x -= 0.2


# After the pose and wherever the figure was laid (toppled, head down): the
# pivots onto the bones, her feet and hands onto what is there, her eyes on
# the player's, her hair swung.
func _drive_body(data: Dictionary, delta: float, camera_position: Vector3, falling: bool) -> void:
	if _rig == null:
		return
	_crawl_retune(data, falling)
	_ease_pose(delta)
	_rig.apply()
	_rig.contact(data, falling, _held_shown)
	_rig.aim_eyes(camera_position, bool(data["sees_player"]), _clock)
	_rig.step_hair(delta)


# Her joints, hips and limb swell go to this frame's pose no faster than
# POSE_RAD_S (and its kin): her gait passes through, a change of state is a
# whip of the limbs instead of a jump.
func _ease_pose(delta: float) -> void:
	if _pose_pivots.is_empty():
		for key in POSE_KEYS:
			_pose_pivots.append(_p[key] as Node3D)
		_pose_shown.resize(_pose_pivots.size())
		_scale_shown.resize(_pose_pivots.size())
		_pose_fresh = true
	var body: Node3D = _p["body"]
	if _pose_fresh:
		for i in _pose_pivots.size():
			_pose_shown[i] = _pose_pivots[i].rotation
			_scale_shown[i] = _pose_pivots[i].scale
		_hip_shown = body.position
		_pose_fresh = false
		return
	for i in _pose_pivots.size():
		var pivot := _pose_pivots[i]
		_pose_shown[i] = _pose_shown[i].move_toward(pivot.rotation, POSE_RAD_S * delta)
		pivot.rotation = _pose_shown[i]
		_scale_shown[i] = _scale_shown[i].move_toward(pivot.scale, POSE_SCALE_S * delta)
		pivot.scale = _scale_shown[i]
	_hip_shown = _hip_shown.move_toward(body.position, POSE_HIP_M_S * delta)
	body.position = _hip_shown


# Her figure turned and laid as her state says (upright, toppled, head down
# her pipe), turned over about her middle no faster than FIGURE_RAD_S.
func _ease_figure(centre: Vector3, delta: float) -> void:
	var target := _figure.global_basis.get_rotation_quaternion()
	var local := _figure.global_basis.inverse() * (_figure.global_position - centre)
	if _pose_fresh:
		_figure_shown = target
	else:
		var angle := _figure_shown.angle_to(target)
		if angle > 1.0e-4:
			_figure_shown = _figure_shown.slerp(target, minf(1.0, FIGURE_RAD_S * delta / angle))
	var shown := Basis(_figure_shown)
	_figure.global_basis = shown
	_figure.global_position = centre + shown * local


# Her hands on what she climbs: each reaches its new hold over a moment
# (HAND_EASE); a new climb, or one far off, starts where its hold is.
func _ease_hands(data: Dictionary, delta: float) -> void:
	if not (bool(data.get("climbing", false)) and data.has("climb_hand")):
		_hands_fresh = true
		return
	var want: Vector3 = data["climb_hand"]
	var want2: Vector3 = data["climb_hand2"] if data.has("climb_hand2") else want
	if _hands_fresh or _pose_fresh or _held_shown[0].distance_to(want) > 1.5 or _held_shown[1].distance_to(want2) > 1.5:
		_held_shown[0] = want
		_held_shown[1] = want2
		_hands_fresh = false
		return
	var k := 1.0 - exp(-HAND_EASE * delta)
	_held_shown[0] = _held_shown[0].lerp(want, k)
	_held_shown[1] = _held_shown[1].lerp(want2, k)


func _limb(up: Node3D, low: Node3D, fwd: float, bend: float, splay := 0.0) -> void:
	up.rotation = Vector3(-fwd, 0.0, splay)
	low.rotation = Vector3(bend, 0.0, 0.0)


# A value in 0..1 that wanders, as quick as its rate, with no step anywhere:
# _hash at each whole t, eased between.
func _noise(t: float) -> float:
	var i := floorf(t)
	var f := t - i
	f = f * f * (3.0 - 2.0 * f)
	return lerpf(_hash(int(i)), _hash(int(i) + 1), f)


func _hash(n: int) -> float:
	var v := sin(float(n) * 12.9898) * 43758.5453
	return v - floorf(v)


# An unseen player cannot steer her head through steel. Acoustic attention
# follows the native estimate until it expires, then returns to body heading.
func perceived_target(data: Dictionary, player_position: Vector3) -> Vector3:
	if bool(data.get("sees_player", false)):
		return player_position
	if bool(data.get("lead_valid", false)):
		return Vector3(float(data["lead_x"]), float(data.get("lead_floor_y", 0.0)) + 1.6, float(data["lead_z"]))
	return _figure.global_position + _figure.global_basis.z * 5.0


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
	var to_player := perceived_target(data, player_position) - _figure.global_position
	yaw_to_player = clampf(wrapf(atan2(to_player.x, to_player.z) - figure_yaw, -PI, PI), -1.4, 1.4)
	var legs_fwd := 0.0
	var legs_back := 0.0
	var arms_fwd := 0.0
	var arms_bend := 0.0
	var arms_splay := 0.0
	# One arm higher than the other (+ the left), and the higher one
	# straighter: hand over hand.
	var arms_alt := 0.0
	var bend_alt := 0.0
	var knees := 0.0
	var wide := 0.0
	var turn := int(data.get("turn", 0))
	# Wary of a noise, she comes on as she searches: upright, slow, head snapping.
	if state == STATE_HUNTING and turn == 0 and bool(data.get("wary", false)):
		state = STATE_SEARCHING
	var still_on_post := state == STATE_GUARDING and speed < 0.3
	# Going up a ladder after the player.
	var climbing := bool(data.get("climbing", false)) or bool(data.get("on_pipe", false))
	var climb_move_active := bool(data.get("climb_move_active", false))
	var climb_move := str(data.get("climb_move", "walk"))
	var climb_phase := int(data.get("climb_phase", 0))
	var climb_airborne := bool(data.get("climb_airborne", false))
	var climb_sliding := bool(data.get("climb_sliding", false))
	var climb_leg_driving := bool(data.get("climb_leg_driving", false))
	var climb_bar_traverse := bool(data.get("climb_bar_traverse", false))
	var pipe_sliding := bool(data.get("pipe_sliding", false))
	var pipe_stunned := bool(data.get("pipe_stunned", false))
	var pipe_listening := bool(data.get("pipe_listening", false))
	# Rising off the floor in the Turn: a vault, or a leap over a gap.
	var vaulting := turn == 2 and _fall_speed > 1.0 and not climbing
	# Holding on to a floor that moves (a cage she stepped into as it left).
	var clinging := bool(data.get("clinging", false))
	# Going down a line's bare cable on her forearms.
	var on_line := bool(data.get("on_line", false))
	# Her hands up under the bay's leaf, holding it (her trap).
	var bracing := bool(data.get("bracing", false))
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
	elif state == STATE_DOWN and bool(data.get("kneeling", false)):
		# No room to lie: crumpled on her knees, slumped over them, the head
		# hung, the arms dropped.
		pitch = 0.95
		hip_y = 0.42
		knees = 2.3
		legs_fwd = 1.55
		legs_back = 1.55
		arms_fwd = 0.35
		arms_bend = 0.4
		arms_splay = 0.2
		head_pitch = 0.65
		head_roll = 0.35
		jaw_open = 0.35
	elif state == STATE_DOWN:
		# Flat on the steel (the figure toppled in _topple): limbs flung loose
		# as she landed, one knee drawn up, fingers crawling.
		pitch = 0.0
		hip_y = 0.98
		arms_fwd = 0.6
		arms_splay = 1.15
		arms_bend = 0.35
		arms_alt = 0.5
		legs_fwd = 0.25
		legs_back = -0.15
		knees = 0.45
		head_roll = 0.4
		jaw_open = 0.6
	elif state == STATE_STAGGERING:
		# Thrown off her balance: her trunk taken the way the blow went, arms
		# flung out to catch it, feet stamping to get under her again.
		var fall_v: Vector2 = data.get("fall", Vector2(0.0, 1.0))
		var face := Vector2(sin(figure_yaw), cos(figure_yaw))
		var ahead := fall_v.dot(face)
		pitch = 0.7 + 0.55 * ahead
		hip_y = 0.95
		arms_fwd = 1.9 - 0.6 * ahead
		arms_splay = 1.1
		arms_bend = 0.25
		legs_fwd = 0.55 * sin(_clock * 15.0)
		legs_back = -0.55 * sin(_clock * 15.0)
		knees = 0.6
		head_pitch = -0.55 * ahead
		head_roll = 0.3 * sin(_clock * 9.0)
		jaw_open = 0.75
	elif bracing:
		# Under the leaf's edge: one foot set before the other, the trunk leant
		# up into it, both arms up before her with the palms under it,
		# trembling with the load; the head tipped up, the jaw shut on it.
		var load := clampf(float(data.get("support_n", 0.0)) / BRACE_FULL_N, 0.0, 1.0)
		var shake := sin(_clock * 37.0) * 0.04 * load
		pitch = 0.25
		hip_y = 0.92
		knees = 0.45
		legs_fwd = 0.25
		legs_back = 0.35
		arms_fwd = 2.1 + shake
		arms_bend = 0.75 - 0.15 * load
		arms_splay = 0.3
		head_pitch = -0.45
		jaw_open = 0.05
	elif on_line:
		# Hung from the cable by her forearms hooked over it, the body
		# swinging under, legs trailing, the head craned down the line.
		pitch = 0.15
		hip_y = 1.0
		arms_fwd = 2.95
		arms_bend = 1.25
		arms_splay = 0.25
		legs_fwd = 0.35 + 0.25 * sin(_clock * 2.2)
		legs_back = 0.1 - 0.2 * sin(_clock * 2.2 + 0.8)
		knees = 0.5
		head_pitch = -0.9
		head_roll = 0.3 * sin(_clock * 1.3)
		jaw_open = 0.95
	elif pipe_stunned:
		# Slammed onto the flange: folded over it, her arms round the pipe,
		# her head hanging off her neck, then lifting.
		pitch = 1.1
		hip_y = 0.55
		knees = 2.0
		legs_fwd = 1.4
		legs_back = 1.3
		arms_fwd = 1.5
		arms_bend = 1.4
		arms_splay = 0.6
		head_pitch = 0.9
		head_roll = 0.6
		jaw_open = 0.8
	elif pipe_sliding:
		# Torn off, her legs round the pipe and her arms up it, sliding, the
		# head thrown back, screaming.
		pitch = -0.1
		hip_y = 1.0
		arms_fwd = 2.9
		arms_bend = 0.35
		arms_splay = 0.35
		legs_fwd = 0.9
		legs_back = 0.9
		knees = 1.6
		head_pitch = -1.0
		head_roll = 0.2 * sin(_clock * 23.0)
		jaw_open = 1.0
	elif pipe_listening:
		# Just off the pipe: stock still, her head tipped right over onto
		# her shoulder, listening for where the climber went.
		pitch = 0.2
		hip_y = 1.0
		arms_fwd = 0.3
		arms_bend = 0.7
		arms_splay = 0.2
		head_pitch = 0.15
		head_roll = 0.75
		jaw_open = 0.2
	elif climb_move_active and climb_move == "gap_jump":
		# A gap is run, a short planted-leg drive, then a real ballistic arc.
		if climb_leg_driving:
			pitch = 0.65
			hip_y = 0.82
			arms_fwd = 0.9
			arms_bend = 0.35
			arms_splay = 0.2
			legs_fwd = 1.35
			legs_back = 0.55
			knees = 1.45
			head_pitch = -0.4
			jaw_open = 0.35
		elif climb_airborne:
			pitch = 0.0
			hip_y = 1.0
			arms_fwd = 2.85
			arms_alt = 0.25 * sin(_clock * 9.0)
			arms_bend = 0.25
			arms_splay = 0.45
			legs_fwd = -0.65
			legs_back = 0.9
			knees = 1.15
			head_pitch = -0.45
			jaw_open = 0.65
		elif climb_phase <= 1:
			pitch = 0.45
			hip_y = 0.92
			arms_fwd = 0.75
			arms_bend = 0.25
			arms_splay = 0.15
			legs_fwd = 0.45
			legs_back = -0.2
			knees = 0.45
			head_pitch = -0.2
		else:
			pitch = 0.25
			hip_y = 0.96
			arms_fwd = 1.2
			arms_bend = 0.35
			arms_splay = 0.25
			legs_fwd = 0.45
			legs_back = 0.15
			knees = 0.8
			head_pitch = -0.35
			jaw_open = 0.25
	elif climb_move_active and climb_move == "jump_catch":
		# Before the catch she winds up, drives off the planted leg, then
		# reaches for the bar. Once caught, the native move becomes rung_up.
		if climb_phase < 2:
			pitch = 0.35
			hip_y = 0.92
			arms_fwd = 1.4
			arms_bend = 0.4
			arms_splay = 0.2
			arms_alt = 0.3
			legs_fwd = 0.5
			legs_back = 0.1
			knees = 0.65
			head_pitch = -0.8
		elif climb_leg_driving:
			pitch = 0.3
			hip_y = 0.82
			arms_fwd = 2.35
			arms_alt = 0.35
			arms_bend = 0.35
			legs_fwd = 1.2
			legs_back = 0.55
			knees = 1.35
			head_pitch = -1.0
		else:
			pitch = 0.0
			hip_y = 1.0
			arms_fwd = 2.95
			arms_alt = 0.35 * sin(_clock * 7.0)
			arms_bend = 0.2
			arms_splay = 0.3
			legs_fwd = 0.8
			legs_back = 0.35
			knees = 0.9
			head_pitch = -1.1
			jaw_open = 0.45
	elif climb_move_active and climb_move in ["pole_up", "pole_down"]:
		if climb_phase == 0 and not climbing:
			pitch = 0.35
			hip_y = 0.95
			arms_fwd = 0.8
			arms_bend = 0.35
			legs_fwd = 0.4
			legs_back = -0.2
			knees = 0.35
			head_pitch = -0.4
		elif climb_move == "pole_up" and climb_phase >= 3 and not climb_sliding:
			# At the pole head, she transfers her hands onto the lip and mantles.
			pitch = 0.9
			hip_y = 0.78
			arms_fwd = 2.5
			arms_bend = 1.0
			arms_splay = 0.25
			legs_fwd = 1.0
			legs_back = 0.5
			knees = 1.25
			head_pitch = -0.65
			jaw_open = 0.3
		elif climb_sliding:
			# A slipping squeeze: limbs clamp, but her weight is dropping.
			pitch = -0.1
			hip_y = 1.0
			arms_fwd = 2.9
			arms_bend = 0.35
			arms_splay = 0.4
			legs_fwd = 1.0
			legs_back = 0.8
			knees = 1.5
			head_pitch = -0.8
			jaw_open = 0.8
		else:
			# Arms wrap close around the post and her knees pinch it.
			var ph := stride / 0.6 * PI
			pitch = 0.2
			hip_y = 1.0
			arms_fwd = 2.15
			arms_alt = 0.12 * sin(ph)
			arms_bend = 1.3
			arms_splay = 0.55
			legs_fwd = 0.95
			legs_back = 0.95
			knees = 1.4
			head_pitch = -0.7
			jaw_open = 0.3
	elif climb_move_active and climb_move == "hang_drop" and climb_phase >= 1:
		if climb_phase == 1 and not climb_airborne:
			# Still on the lip: shoulders stretched, feet hanging clear.
			pitch = 0.05
			hip_y = 1.0
			arms_fwd = 2.9
			arms_bend = 1.2
			arms_splay = 0.15
			legs_fwd = 0.2
			legs_back = 0.2
			knees = 0.35
			head_pitch = -0.65
			jaw_open = 0.4
		else:
			# The hands have let go; the body drops away from the lip.
			pitch = 0.15
			hip_y = 1.0
			arms_fwd = 2.75
			arms_bend = 0.35
			arms_splay = 0.45
			legs_fwd = 0.85
			legs_back = -0.55
			knees = 1.0
			head_pitch = -0.35
			jaw_open = 0.6
	elif climb_move_active and climb_move == "step_down" and climb_phase >= 2 and climb_airborne:
		pitch = 0.15
		hip_y = 1.0
		arms_fwd = 2.7
		arms_bend = 0.35
		arms_splay = 0.45
		legs_fwd = 0.8
		legs_back = -0.45
		knees = 0.9
		head_pitch = -0.35
		jaw_open = 0.55
	elif climb_move_active and (climb_move == "mantle" or (climb_move == "rung_up" and climb_phase >= 3)):
		# The last pull over the lip is a whole-body effort, not another rung.
		pitch = 0.9
		hip_y = 0.78
		arms_fwd = 2.5
		arms_bend = 1.0
		arms_splay = 0.25
		legs_fwd = 1.0
		legs_back = 0.5
		knees = 1.25
		head_pitch = -0.65
		jaw_open = 0.3
	elif climb_move_active and climb_move == "rung_down" and climb_phase >= 1:
		# Descending rungs: the hips stay close while she feels for the next step.
		var ph := stride / 0.6 * PI
		pitch = 0.3
		hip_y = 1.0
		arms_fwd = 2.25
		arms_alt = 0.2 * sin(ph)
		arms_bend = 0.9
		arms_splay = 0.25
		legs_fwd = 0.55
		legs_back = 0.55
		knees = 1.1
		head_pitch = 0.35
		jaw_open = 0.2
	elif climb_bar_traverse:
		# Across a web of horizontal members: chest low, arms alternating
		# overhead, feet trailing instead of stepping as on a ladder.
		var ph := stride / 0.45 * PI
		pitch = 0.45
		hip_y = 0.92
		arms_fwd = 2.8
		arms_alt = 0.35 * sin(ph)
		arms_bend = 1.05
		bend_alt = -0.2 * sin(ph)
		arms_splay = 0.35
		legs_fwd = 0.45 + 0.25 * sin(_clock * 3.0)
		legs_back = 0.35 - 0.25 * sin(_clock * 3.0 + 0.8)
		knees = 0.9
		head_pitch = -0.75
		head_roll = 0.15 * sin(_clock * 2.0)
		jaw_open = 0.35
	elif climbing:
		# Up the ladder after the player, hand over hand: the higher hand
		# reaching for the next rung while the lower one hauls, the other
		# side's knee coming up to step, her face turned up the ladder at
		# whoever is above her.
		var ph := stride / 0.6 * PI
		pitch = 0.1
		hip_y = 1.0
		arms_fwd = 2.55
		arms_alt = 0.45 * sin(ph)
		arms_bend = 0.75
		bend_alt = -0.55 * sin(ph)
		arms_splay = 0.25
		legs_fwd = 0.75 + 0.55 * sin(ph + PI)
		legs_back = 0.75 + 0.55 * sin(ph)
		knees = 1.25
		head_pitch = -1.1
		head_roll = 0.15 * sin(_clock * 1.4)
		jaw_open = 0.45
		# Gap-fillers for the climbing moves, one pose each. The main
		# climb branches above own an active move; this block only runs
		# inside the ladder branch, for the phases those branches do not
		# cover (for example rung_down at phase 0, step_down before it
		# is airborne). As written in the prep commit this block was
		# dead: it read climb_move_phase / climb_move_airborne, which
		# the bridge never sent, and CamelCase move names, while the
		# bridge sends snake-case move_name(). Fixed here to the real
		# bridge keys climb_phase / climb_airborne (the bridge also
		# exports the climb_move_* aliases). With climb_move absent
		# or "walk", no case matches and the ladder pose above is
		# untouched. Hands: both arms pose together here because these
		# moves hold with both hands; the rig reaches the right wrist
		# to climb_hand and the left wrist to the bridge's own second
		# hold climb_hand2 when the native validated it, falling back
		# to the one hold otherwise (see matron_rig.gd contact()).
		# Feet: legs/knees above are the feet pose; the support under
		# them is the bridge's climb_feet_support point when known,
		# else the floor_y / support_n already exported.
		var move_kind := String(data.get("climb_move", ""))
		if move_kind != "" and move_kind != "walk":
			var move_phase := int(data.get("climb_phase", data.get("climb_move_phase", 0)))
			var move_air := bool(data.get("climb_airborne", data.get("climb_move_airborne", false)))
			match move_kind:
				"rung_down":
					# Down the ladder, face to the rungs, hands working
					# downward instead of up, eyes on where her feet go.
					arms_alt = -0.45 * sin(ph)
					bend_alt = 0.55 * sin(ph)
					head_pitch = 0.6
					pitch = 0.3
				"pole_up", "pole_down":
					# Hands close together round the pole, legs wrapped
					# round it, not the wide ladder spread.
					arms_splay = 0.05
					arms_fwd = 2.9
					arms_alt = 0.1 * sin(ph)
					knees = 1.6
					legs_fwd = 0.9
					legs_back = 0.9
					head_pitch = -1.1 if move_kind == "pole_up" else 0.5
				"mantle":
					# Hauling up over a lip: hips rise with the phase,
					# arms pressing down on the lip, trunk leaning in.
					var mant := clampf(float(move_phase) / 2.0, 0.0, 1.0)
					hip_y = 0.7 + 0.3 * mant
					pitch = 0.5 - 0.4 * mant
					arms_fwd = 2.2
					arms_bend = 1.0
					knees = 1.6
				"step_down":
					# Off an edge onto what is under it: arms still up
					# on the hold, legs reaching down, trunk upright.
					arms_fwd = 2.7
					legs_fwd = 1.1
					legs_back = 0.2
					knees = 0.6
					pitch = 0.4
				"jump_catch":
					# Reaching for a bar or a ladder foot: both arms out
					# high together, legs trailing, then the catch.
					arms_fwd = 2.9
					arms_splay = 0.15
					arms_alt = 0.0
					legs_fwd = 1.2
					legs_back = -0.2
					knees = 0.9
					head_pitch = -0.8
					jaw_open = 0.6
				"gap_jump":
					if move_air:
						# Ballistic, over the gap: arms forward, one leg
						# reaching for the landing, the other trailing.
						arms_fwd = 2.7
						legs_fwd = 0.9
						legs_back = -0.4
						knees = 0.7
						pitch = -0.1
					else:
						# The leg drive before it: crouched, knees deep,
						# arms back for the push.
						hip_y = 0.75
						knees = 1.8
						arms_fwd = 1.8
						arms_bend = 0.9
				"hang_drop":
					# Hanging, then dropping: arms straight up on the
					# hold, legs dangling loose under her.
					arms_fwd = 2.9
					arms_bend = 0.4
					arms_splay = 0.2
					legs_fwd = 0.3 + 0.2 * sin(_clock * 2.0)
					legs_back = 0.1
					knees = 0.5
					head_pitch = -0.3
	elif clinging:
		# Down on her haunches with a hand locked on the cage's frame either
		# side, rocking with it, the head never off the player.
		pitch = 1.05
		hip_y = 0.6
		knees = 1.9
		legs_fwd = 1.1
		legs_back = 1.1
		arms_fwd = 0.35
		arms_bend = 1.1
		arms_splay = 0.95
		head_pitch = -0.85
		head_roll = 0.25 * sin(_clock * 1.7)
		jaw_open = 0.55
	elif state == STATE_HUNTING and speed > 1.0 and int(data.get("stage", 0)) == 0:
		# A pest hurries after you upright: an old woman's hobble at a speed
		# no old woman has, trunk tipped forward, both arms out stiff for
		# you, one leg dragging a beat behind the other, the head craned
		# forward and cocked, the mouth hanging.
		var ph := stride / 0.62 * PI
		pitch = 0.55 + 0.05 * sin(ph * 2.0)
		hip_y = 0.95 + 0.04 * sin(ph * 2.0)
		legs_fwd = 0.55 * sin(ph)
		legs_back = -0.35 * sin(ph - 0.6)
		knees = 0.35 + 0.35 * maxf(0.0, cos(ph))
		arms_fwd = 1.35 + 0.08 * sin(ph * 2.0 + 1.0)
		arms_bend = 0.2
		arms_splay = 0.12
		head_pitch = -0.55
		head_roll = 0.45 + 0.1 * sin(_clock * 2.6)
		jaw_open = 0.6 if sees else 0.35
	elif state == STATE_HUNTING and speed > 1.0:
		# All fours (dangerous and fully changed): hips high, chest low, arms reaching and elbows out, a
		# jerking scuttle, the head up and cocked and always on you.
		var ph := stride / 0.42 * PI
		pitch = 1.38 + 0.06 * sin(ph * 2.0)
		hip_y = 0.8 + 0.07 * (0.5 + 0.5 * sin(ph * 2.0 + _noise(_clock * 14.0) * 0.6))
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
		head_roll = 0.9 * clampf((2.0 * _noise(_clock * 2.4 + 53.0) - 1.0) * 3.0, -1.0, 1.0)
		jaw_open = 0.5
	elif state == STATE_EMERGING:
		# Out of the locker: in stages, unfolding, arms flung wide, head back.
		var stage := clampf(_state_age / 2.0, 0.0, 1.0)
		var snap := stage * stage * (3.0 - 2.0 * stage)
		pitch = lerpf(1.7, 0.4, snap)
		hip_y = lerpf(0.55, 1.0, snap)
		knees = lerpf(1.8, 0.2, snap)
		legs_fwd = lerpf(1.2, 0.0, snap)
		legs_back = legs_fwd
		arms_fwd = lerpf(0.4, 1.0, snap)
		arms_splay = lerpf(0.2, 1.5, snap)
		arms_bend = 0.4
		head_pitch = lerpf(-0.2, 0.6, snap)
		head_roll = 0.4 * clampf(sin(_state_age * 6.0 * PI) * 3.0, -1.0, 1.0)
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
	elif still_on_post:
		# At her post in the mouth of the way out: upright and still as a
		# post, the head holding one way, then snapping round to another.
		var look := _hash(int(floorf(_clock / 2.6)) + 31)
		_head_yaw = lerpf(_head_yaw, (look - 0.5) * 2.4, 1.0 - exp(-30.0 * delta))
		pitch = 0.18
		hip_y = 1.0
		knees = 0.05
		arms_fwd = 0.08
		arms_bend = 0.1
		arms_splay = 0.05
		head_pitch = 0.05
		jaw_open = 0.04
	elif state == STATE_UNRIGGING and speed < 0.3:
		# Bent to the cage's eye at her knee, both hands at the shackle,
		# working it, her head down over them.
		pitch = 1.25
		hip_y = 0.85
		knees = 0.6
		legs_fwd = 0.25
		legs_back = 0.1
		arms_fwd = 1.15 + 0.12 * sin(_clock * 13.0)
		arms_bend = 0.9 + 0.15 * sin(_clock * 9.0 + 1.0)
		arms_splay = 0.12
		head_pitch = 0.5
		head_roll = 0.2 * sin(_clock * 2.3)
		jaw_open = 0.2
	elif state == STATE_PEERING:
		# Stopped short of a noise, craning toward it: leant out from the
		# hips, the neck stretched, the head tilting, her breath held.
		pitch = 1.0
		hip_y = 0.9
		knees = 0.4
		legs_fwd = 0.3
		legs_back = -0.1
		arms_fwd = 0.55
		arms_bend = 0.6
		arms_splay = 0.25
		head_pitch = -0.75
		head_roll = 0.55 * sin(_clock * 0.9)
		jaw_open = 0.28
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
		head_roll = sin(_clock * 0.8) * 0.3 + (0.35 * sin(PI * fposmod(_clock * 2.8, 1.0)) if _hash(int(floorf(_clock * 2.8))) > 0.93 else 0.0)
		jaw_open = 0.1 + 0.05 * sin(_clock * 3.0)
	# The Turn: something in her breaks. The spine bends where it should not
	# (a hunch that throws the shoulders to one side and folds the chest the
	# other way), the neck cracks round till the head looks back over her
	# shoulder, the elbows fold backwards, one knee goes the wrong way, the
	# jaw drops off its hinge. The joints wrench round ten times a second.
	var twist := 0.0
	var side_bend := 0.0
	var elbow_flip := 1.0
	var knee_flip := 1.0
	var head_back := 0.0
	var limb_scale := 1.0
	var jolt := Vector3.ZERO
	if turn > 0 and not falling:
		var beat := _clock * 10.0
		var r1 := _noise(beat + 101.0) - 0.5
		var r2 := _noise(beat + 211.0) - 0.5
		var r3 := _noise(beat + 307.0) - 0.5
		twist = 0.9 * r1
		side_bend = 0.55 + 0.4 * r2
		elbow_flip = -1.0
		# One beat in three the knee goes the right way.
		var kp := fposmod(beat, 3.0)
		knee_flip = 1.0 - 2.0 * smoothstep(0.7, 1.0, kp) + 2.0 * smoothstep(2.7, 3.0, kp)
		head_back = 2.6 if turn == 1 else 1.5 + 1.2 * r3
		limb_scale = 1.3
		jaw_open = 1.35
		jolt = Vector3(r1, r2 * 0.5, r3) * 0.05
		if climbing:
			# On the rungs the Turn breaks her where she hangs: the climb's
			# pose holds, the joints go wrong.
			pass
		elif turn == 1:
			# The wind-up: still, folded forward at the waist, and shaking.
			pitch = 0.55 + 0.9 * (0.5 + 0.5 * sin(_clock * 3.0))
			hip_y = 0.95
			arms_fwd = 0.3
			arms_bend = 0.8
			knees = 0.35
			legs_fwd = 0.0
			legs_back = 0.0
			jolt *= 2.0
		else:
			# Lunging: on all fours as on the hunt, but wilder, and a joint
			# always wrenching somewhere else.
			pitch = 1.38 + 0.25 * r2
			arms_fwd = 1.1 + r1
			arms_splay = 1.0
			knees = 1.6 + r3
			if vaulting:
				# Over what was in her way: knees tucked up under her, both
				# hands planted down ahead on it.
				pitch = 1.15
				knees = 2.3
				knee_flip = 1.0
				legs_fwd = 1.7
				legs_back = 1.7
				arms_fwd = 1.9
				arms_bend = 0.2
				arms_splay = 0.35
	# The Turn's jolt shakes her hips about where the pose has them: an offset,
	# never a drift.
	body.position = Vector3(jolt.x, hip_y + jolt.y, jolt.z)
	(_p["torso"] as Node3D).rotation.y = twist
	(_p["torso"] as Node3D).rotation.z = side_bend * 0.5 if turn > 0 else 0.0
	(_p["sh_l"] as Node3D).scale = Vector3.ONE * limb_scale
	(_p["sh_r"] as Node3D).scale = Vector3.ONE * limb_scale
	(_p["hp_l"] as Node3D).scale = Vector3.ONE * (1.0 + 0.2 * (limb_scale - 1.0))
	(_p["hp_r"] as Node3D).scale = Vector3.ONE * (1.0 + 0.2 * (limb_scale - 1.0))
	torso.rotation.x = pitch
	neck.rotation = Vector3(head_pitch * 0.4, head_back, head_roll * 0.3)
	head.rotation = Vector3(head_pitch * 0.6 - pitch * 0.25, _head_yaw if state == STATE_SEARCHING or still_on_post else yaw_to_player * (1.0 if clinging else 0.6 if state == STATE_HUNTING else 0.0), head_roll * 0.7)
	jaw.rotation.x = jaw_open
	_limb(_p["hp_l"], _p["kn_l"], legs_fwd, knees * knee_flip)
	_limb(_p["hp_r"], _p["kn_r"], legs_back, knees)
	_p["an_l"].rotation.x = -0.2
	_p["an_r"].rotation.x = -0.2
	# A piece of the player's stock in her hands (her mischief): both arms out
	# before her chest under it, elbows bent, whatever her legs are doing.
	if int(data.get("carrying", 0)) != 0:
		arms_fwd = 1.35
		arms_bend = 0.55
		arms_splay = 0.08
		arms_alt = 0.0
		bend_alt = 0.0
	_limb(_p["sh_l"], _p["el_l"], arms_fwd + arms_alt + (0.2 * sin(stride * 4.0) if state == STATE_SHUFFLING else 0.0), -(arms_bend + bend_alt) * elbow_flip, -arms_splay)
	_limb(_p["sh_r"], _p["el_r"], arms_fwd - arms_alt, -(arms_bend - bend_alt) * elbow_flip, arms_splay)
	if state == STATE_SEARCHING and turn == 0:
		# The right hand cupped to the ear.
		_limb(_p["sh_r"], _p["el_r"], 1.6, -2.0, 0.5)
	_p["box_cord"].rotation = Vector3(sin(stride * 3.0) * 0.4 + 0.2 * (pitch - 0.7), 0.0, sin(stride * 2.2) * 0.3)
	# Fingers: relaxed hooks that curl into claws on the hunt, twitching in the Turn.
	var curl := 0.35
	if state == STATE_HUNTING:
		curl = 1.0
	elif state == STATE_SEARCHING:
		curl = 0.7
	elif state == STATE_EMERGING:
		curl = 1.3
	if falling:
		curl = 0.05
	var beat_f := int(floorf(_clock * 9.0))
	for hand in ["l", "r"]:
		var fingers: Array = _p["fingers_" + hand]
		for f in fingers.size():
			var c := curl
			if turn > 0:
				c = 0.1 + 1.5 * _noise(_clock * 9.0 + float(f * 3 + (0 if hand == "l" else 11)) * 13.0)
			elif state == STATE_HUNTING and speed > 1.0:
				# The planted hand spreads, the lifted one claws.
				var phase := sin(stride / 0.42 * PI + (PI if hand == "l" else 0.0))
				c = lerpf(0.5, 1.4, 0.5 + 0.5 * phase)
			var chain: Array = fingers[f]
			for seg in chain.size():
				(chain[seg] as Node3D).rotation.x = -c * (1.0 + 0.15 * float(seg)) * (0.8 if f == fingers.size() - 1 else 1.0)
	# What swells under the cloth in the Turn: the hump and a ridge of spine.
	var swell := 1.0 if turn > 0 else 0.0
	(_p["hump"] as Node3D).scale = (_p["hump"] as Node3D).scale.lerp(Vector3.ONE * (1.0 + 0.5 * swell), 1.0 - exp(-18.0 * delta))
	var knobs: Array = _p["vertebrae"]
	for k in knobs.size():
		var target := Vector3.ONE * swell * (0.8 + 0.4 * _hash(k + beat_f))
		(knobs[k] as Node3D).scale = (knobs[k] as Node3D).scale.lerp(target, 1.0 - exp(-20.0 * delta))
	# Rags and hair stream out behind her at a run and hang when she is still.
	var drag := clampf(speed / 7.0, 0.0, 1.0)
	(_p["rags"] as Node3D).rotation = Vector3(-0.5 * drag + 0.06 * sin(_clock * 4.0), 0.0, 0.08 * sin(_clock * 3.1 + 1.0))
	# Hair hangs from the head under gravity, not along her bending neck.
	(_p["hair"] as Node3D).global_basis = Basis(Vector3.UP, _figure.rotation.y) * Basis(Vector3.RIGHT, -0.5 * drag + 0.05 * sin(_clock * 2.3)) * Basis(Vector3.FORWARD, 0.06 * sin(_clock * 1.9))
	(_p["cape"] as Node3D).rotation = Vector3(-0.15 * drag, 0.0, 0.0)
	var eyes_on := 1.0 if state != STATE_DEAD else 0.0
	_eye_light.light_energy = (0.5 if turn > 0 else 0.3) * eyes_on


# Down on the steel (not kneeling): her whole figure toppled from her feet the
# way the blow sent her (on her back, her face, her side, as she was facing),
# laid over the lying body the simulation has under her middle; over
# TOPPLE_S going down, and back up over the last of her time there.
func _topple(data: Dictionary, centre: Vector3, delta: float) -> void:
	var want := 0.0
	if int(data["state"]) == STATE_DOWN and not bool(data.get("kneeling", false)):
		var fall_v: Vector2 = data.get("fall", Vector2(0.0, 1.0))
		var fall_now := Vector3(fall_v.x, 0.0, fall_v.y)
		if fall_now.length_squared() >= 1.0e-6:
			_topple_fall = fall_now.normalized()
			var down_s := float(data.get("down_s", 0.0))
			# She rises on the simulation's clock: over the last TOPPLE_S of the
			# time she lies there. None left and still down, she is pinned,
			# crawling out flat to where she can stand.
			var left_s := float(data.get("down_left_s", 0.0))
			var rise := clampf(left_s / TOPPLE_S, 0.0, 1.0) if left_s > 0.0 else 1.0
			want = clampf(down_s / TOPPLE_S, 0.0, 1.0) * rise
	# Down or up at the topple's own pace, never at once: pinned, or let up
	# off the steel, she goes over (or comes up) in TOPPLE_S, not in a frame.
	_topple_k = want if _pose_fresh else move_toward(_topple_k, want, delta / TOPPLE_S)
	if _topple_k <= 0.0:
		return
	var fall := _topple_fall
	var k := _topple_k * _topple_k * (3.0 - 2.0 * _topple_k)
	var axis := Vector3.UP.cross(fall).normalized()
	_figure.global_basis = Basis(axis, 0.5 * PI * k) * Basis(Vector3.UP, _drawn_yaw)
	var floor_y := centre.y - STAND_CENTRE
	_figure.global_position = Vector3(centre.x, floor_y, centre.z) - fall * (1.0 * k) + Vector3(0.0, 0.18 * k, 0.0)


# Coming down the corner pipe head first (fully changed): her figure turned
# over about her hips, her feet up the pipe and her face down it.
func _pipe_head_down(data: Dictionary, centre: Vector3) -> void:
	if not bool(data.get("pipe_head_down", false)):
		return
	var yaw := _drawn_yaw
	var across := Basis(Vector3.UP, yaw) * Vector3.RIGHT
	_figure.global_basis = Basis(across, PI) * Basis(Vector3.UP, yaw)
	_figure.global_position = Vector3(centre.x, centre.y + STAND_CENTRE, centre.z)


func _update_figure(data: Dictionary, delta: float, camera_position: Vector3, player_position: Vector3) -> void:
	var state := int(data["state"])
	var alive := bool(data["alive"])
	var released := bool(data["released"])
	var out := released and alive and state >= STATE_EMERGING and state != STATE_DEAD
	# Where she is drawn: between the simulation's last two ticks, as the
	# player is (an older native without it: the tick's own).
	var centre: Vector3 = data["position"] if previewing else data.get("render_position", data["position"])
	_drawn_yaw = float(data["yaw"] if previewing else data.get("render_yaw", data["yaw"]))
	if centre.distance_to(_last_pos) > 1.5:
		_pose_fresh = true
	# Fall speed from her own height, for the plunge.
	_fall_speed = (centre.y - _last_pos.y) / maxf(delta, 0.0001) if delta > 0.0 else 0.0
	var falling := out and _fall_speed < -6.0 and centre.y < float(data.get("floor_y", DECK)) + 0.4
	_last_pos = centre
	var on_line := bool(data.get("on_line", false))
	_sparks.emitting = out and on_line
	if on_line:
		_sparks.global_position = centre + Vector3(0.0, 1.55, 0.0)
	if not out or centre.distance_to(camera_position) > DRAW_RANGE_M:
		_figure.visible = false
		_pose_fresh = true
		_hear(data, delta, false, centre - Vector3(0.0, STAND_CENTRE, 0.0), player_position, falling)
		return
	_figure.visible = true
	figure_frames += 1
	_apply_exposure(float(data.get("exposure", 0.0)))
	_figure.global_position = Vector3(centre.x, centre.y - STAND_CENTRE, centre.z)
	_figure.rotation = Vector3(0.0, _drawn_yaw, 0.0)
	_pose(data, delta, player_position, falling)
	_topple(data, centre, delta)
	_pipe_head_down(data, centre)
	_ease_figure(centre, delta)
	_ease_hands(data, delta)
	_drive_body(data, delta, camera_position, falling)
	_hear(data, delta, true, _figure.global_position, player_position, falling)


# Her sounds (presentation/audio/matron_sound.gd), every frame whether she
# is drawn or not, so what she did out of sight is never heard late and her
# loops fall silent the moment she is gone. `near`: out, alive and drawn;
# `feet` her soles.
func _hear(data: Dictionary, delta: float, near: bool, feet: Vector3, player_position: Vector3,
		falling: bool) -> void:
	if _sound == null or previewing:
		return
	_player_d = feet.distance_to(player_position)
	_sound.update(data, delta, near, feet, head_world_position(), player_position, falling)


# Her hands on the player heard, once a frame after _hear, from the hold
# main_feel.gd _feel_gore read for the gore.
func hear_hold(hold: Dictionary) -> void:
	if _sound != null and not previewing:
		_sound.hear_hold(hold)


# For a test and the look scenes: where her head is, and how far gone the
# locker is.
func head_world_position() -> Vector3:
	if _rig != null:
		return _rig.bone_world("head") * Vector3(0.0, 0.09, 0.03)
	return (_p["head"] as Node3D).global_position + Vector3(0.0, 0.1, 0.0)


func locker_front_position() -> Vector3:
	return LOCKER_CENTRE + Vector3(0.0, -0.2, -0.5)


func figure_shown() -> bool:
	return _figure != null and _figure.visible


# For the look scenes: the figure posed from made-up data, the native's own
# frames ignored while this is on. Her look follows the exposure (the
# native's, 0..1 over her time out, matron_look.gd); what she does follows her
# rage instead (matron.hpp, the stages).
var previewing := false


func preview(data: Dictionary, delta: float, camera_position: Vector3, player_position: Vector3) -> void:
	_apply(data, delta, camera_position, player_position)


func update_view(data: Dictionary, delta: float, camera_position: Vector3, player_position: Vector3) -> void:
	if previewing:
		return
	# The punch's hit-stop (strike_feel.gd): her drawn body holds its frame;
	# the native under it does not stop.
	if hold_frame:
		return
	_apply(data, delta, camera_position, player_position)
	_apply_flinch(delta)


# The player's punch landing on her (physics_world_strike.cpp): the sim
# moved her body by the contact's force-time; the drawn body takes it from
# the waist up, tipped away along the landing's normal, sized by its
# impulse, and comes back on a stiff spring. Additive over her pose.
const FLINCH_DEG_PER_NS := 0.16    # ~8 deg for a 50 N*s landing
const FLINCH_MAX_DEG := 14.0
const FLINCH_DECAY := 9.0
const FLINCH_FREQ := 15.0
var hold_frame := false
var flinches := 0
var _flinch_axis := Vector3.RIGHT
var _flinch_deg := 0.0
var _flinch_t := 10.0

func flinch(normal: Vector3, impulse_ns: float) -> void:
	var away := Vector3(-normal.x, 0.0, -normal.z)
	if away.length_squared() < 1.0e-6:
		return
	_flinch_axis = Vector3.UP.cross(away.normalized()).normalized()
	_flinch_deg = minf(FLINCH_MAX_DEG, maxf(impulse_ns, 45.0) * FLINCH_DEG_PER_NS)
	_flinch_t = 0.0
	flinches += 1

func _apply_flinch(delta: float) -> void:
	if _flinch_deg <= 0.0 or not _figure.visible:
		return
	_flinch_t += delta
	var a := deg_to_rad(_flinch_deg) * exp(-FLINCH_DECAY * _flinch_t) * cos(FLINCH_FREQ * _flinch_t)
	if _flinch_t > 1.0:
		_flinch_deg = 0.0
		return
	# About her hips, not her feet: the legs stay planted.
	var hips := _figure.global_position + Vector3(0.0, 1.0, 0.0)
	var r := Basis(_flinch_axis, a)
	_figure.global_basis = r * _figure.global_basis
	_figure.global_position = hips + r * (_figure.global_position - hips)


func _apply(data: Dictionary, delta: float, camera_position: Vector3, player_position: Vector3) -> void:
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
		var centre: Vector3 = data.get("render_position", data["position"])
		_hear(data, delta, false, centre - Vector3(0.0, STAND_CENTRE, 0.0), player_position, false)
		return
	_update_planks(data)
	_update_locker(data, delta)
	_update_figure(data, delta, camera_position, player_position)
	_last_state = state
