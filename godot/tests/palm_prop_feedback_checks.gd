extends RefCounted
## Synthetic native receipts exercise presentation consumers, not vault physics.
const Arms := preload("res://presentation/first_person_arms.gd")
const Director := preload("res://presentation/audio/audio_director.gd")

class RecordedAudio extends Director:
	var observed: Array[StringName] = []
	func _play_at(sound: StringName, _at: Vector3, _level: float, _pitch: float,
			_bus: StringName = BUS_EFFECTS) -> void:
		observed.append(sound)


static func run(check: Callable, root: Node) -> bool:
	var ok := true
	var arms := Arms.new()
	root.add_child(arms)
	var material := StandardMaterial3D.new()
	arms.build(material, material, material, material, material, material, material, material, material, material)
	var state := {"traversal": 0, "progress": 0.0, "position": Vector3(0, 2.1, 0),
		"render_position": Vector3(0, 2.1, 0), "velocity": Vector3(0, -1, -5.5),
		"grounded": false, "affordance": false, "ledge_point": Vector3.ZERO,
		"structure_normal": Vector3.ZERO, "operating": &"", "chute": false,
		"hand_left": Vector3(0.22, 1.65, -0.4), "hand_right": Vector3(-0.22, 1.65, -0.4),
		"landing": {"palm_left_active": true, "palm_right_active": true,
			"palm_left_normal": Vector3.UP, "palm_right_normal": Vector3.UP,
			"traversal_hand_constraint_count": 0}}
	var unchanged := state.duplicate(true)
	var camera := Transform3D(Basis.IDENTITY, Vector3(0, 2.65, 0))
	arms.update_arms(state, camera, 1.0 / 90.0)
	for hand in arms._hands:
		var point: Vector3 = state.hand_left if hand.side < 0 else state.hand_right
		ok = bool(check.call(hand.pose == Arms.POSE_PROP and hand.anchored
			and hand.world_wrist.distance_to(point + Vector3.UP * Arms.PALM_HALF_THICKNESS) < 1.0e-6,
			"native non-gripping palm stays on actual presented material point")) and ok
	camera.basis = Basis(Vector3.UP, 1.1)
	arms.update_arms(state, camera, 1.0 / 60.0)
	for hand in arms._hands:
		var point: Vector3 = state.hand_left if hand.side < 0 else state.hand_right
		ok = bool(check.call(hand.world_wrist.distance_to(point + Vector3.UP * Arms.PALM_HALF_THICKNESS) < 1.0e-6,
			"head turn cannot drag a planted palm away from its support")) and ok
	ok = bool(check.call(state == unchanged, "palm presentation leaves native receipt unchanged")) and ok
	state.landing.palm_left_active = false
	arms.update_arms(state, camera, 1.0 / 90.0)
	for hand in arms._hands:
		ok = bool(check.call((hand.pose == Arms.POSE_PROP) == (hand.side > 0),
			"actual one-palm release preserves only the remaining contact")) and ok
	arms.free()
	return _audio(check) and ok


static func _audio(check: Callable) -> bool:
	var ok := true
	var audio := RecordedAudio.new()
	var effects := AudioServer.get_bus_index(Director.BUS_EFFECTS)
	var added := effects < 0
	if added:
		AudioServer.add_bus()
		effects = AudioServer.bus_count - 1
		AudioServer.set_bus_name(effects, Director.BUS_EFFECTS)
	var master := AudioServer.get_bus_index(&"Master")
	var was_master_muted := AudioServer.is_bus_mute(master)
	var was_effects_muted := AudioServer.is_bus_mute(effects)
	AudioServer.set_bus_mute(master, false)
	AudioServer.set_bus_mute(effects, false)
	var receipt := {"palm_vault_count": 0, "palm_left_active": false, "palm_right_active": false,
		"palm_left_force_n": Vector3.ZERO, "palm_right_force_n": Vector3.ZERO}
	audio.reset_landing_feedback(0, 0, receipt)
	audio._update_palm_prop(receipt, Vector3.ZERO, 0)
	receipt.palm_vault_count = 1
	receipt.palm_left_active = true
	audio._update_palm_prop(receipt, Vector3.ZERO, 0)
	ok = bool(check.call(audio.observed.is_empty(), "palm acquisition without solved load makes no impact sound")) and ok
	receipt.palm_left_force_n = Vector3.UP * 600.0
	var unchanged := receipt.duplicate(true)
	audio._update_palm_prop(receipt, Vector3.ZERO, 0)
	audio._update_palm_prop(receipt, Vector3.ZERO, 0)
	ok = bool(check.call(audio.observed.size() == 1 and audio.observed[0] == &"grab" and receipt == unchanged,
		"one loaded palm episode selects one read-only palm slap")) and ok
	AudioServer.set_bus_mute(effects, true)
	receipt.palm_vault_count = 2
	audio._update_palm_prop(receipt, Vector3.ZERO, 0)
	AudioServer.set_bus_mute(effects, false)
	audio._update_palm_prop(receipt, Vector3.ZERO, 0)
	ok = bool(check.call(audio.observed.size() == 1, "muted palm receipt never replays after unmute")) and ok
	receipt.palm_vault_count = 3
	receipt.palm_left_force_n = Vector3(NAN, 0, 0)
	audio._update_palm_prop(receipt, Vector3.ZERO, 0)
	receipt.palm_left_active = false
	audio._update_palm_prop(receipt, Vector3.ZERO, 0)
	ok = bool(check.call(audio.observed.size() == 1, "invalid force and released palm cannot create a contact sound")) and ok
	receipt.palm_vault_count = 4
	receipt.palm_left_active = true
	receipt.palm_left_force_n = Vector3.UP * 600.0
	audio._update_palm_prop(receipt, Vector3.ZERO, 1)
	ok = bool(check.call(audio.observed.size() == 1, "death restore consumes palm history quietly")) and ok
	audio.reset_landing_feedback(0, 1, receipt)
	audio._update_palm_prop(receipt, Vector3.ZERO, 1)
	ok = bool(check.call(audio.observed.size() == 1, "restart consumes the existing palm episode quietly")) and ok
	AudioServer.set_bus_mute(master, was_master_muted)
	AudioServer.set_bus_mute(effects, was_effects_muted)
	if added:
		AudioServer.remove_bus(effects)
	audio.free()
	return ok
