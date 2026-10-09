extends SceneTree

const Response := preload("res://presentation/landing_camera_response.gd")
const MainPresentation := preload("res://presentation/main.gd")
const Director := preload("res://presentation/audio/audio_director.gd")
const Settings := preload("res://presentation/ui/settings_store.gd")

class FootAudio extends Director:
	var played: Array[StringName] = []
	var levels: Array[float] = []
	func _play(sound: StringName, level: float, _pitch: float) -> void:
		played.append(sound)
		levels.append(level)

var _checks := 0
var _main: Node3D


func _initialize() -> void:
	_run.call_deferred()


func _check(condition: bool, message: String) -> bool:
	_checks += 1
	if condition:
		return true
	push_error("SCRAPERX_LANDING_CAMERA FAIL " + message)
	quit(43)
	return false


func _response_checks() -> bool:
	var baseline := {"landing_count": 0, "landing_balance": 1.0}
	var impact := {"landing_count": 1, "landing_normal_speed_mps": 5.0,
		"landing_approach_energy_j": 1200.0, "landing_tangent_energy_j": 150.0,
		"landing_slip_velocity": Vector3(3.0, 0.0, -1.0),
		"landing_balance": 0.25, "landing_recovering": true}
	var unchanged := impact.duplicate(true)
	var one := Response.new()
	var divided := Response.new()
	one.update(baseline, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	divided.update(baseline, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	one.update(impact, 0.08, true, Vector3.RIGHT, Vector3.FORWARD)
	for i in 4:
		divided.update(impact, 0.02, true, Vector3.RIGHT, Vector3.FORWARD)
	if not _check(one.translation.distance_to(divided.translation) < 0.000001 \
			and one.rotation.distance_to(divided.rotation) < 0.000001,
			"critical damping is invariant to splitting a rendering frame"):
		return false
	if not _check(one.translation.y < -0.01 and absf(one.rotation.y) > 0.0001 \
			and impact == unchanged, "normal impact compresses, actual slip sways, native state stays read-only"):
		return false
	one.update(impact, 0.016, false, Vector3.RIGHT, Vector3.FORWARD)
	if not _check(one.translation == Vector3.ZERO and one.rotation == Vector2.ZERO,
			"motion comfort clears lens movement immediately"):
		return false
	impact.landing_recovering = false
	impact.landing_balance = 1.0
	impact.landing_slip_velocity = Vector3.ZERO
	for i in 120:
		divided.update(impact, 1.0 / 60.0, true, Vector3.RIGHT, Vector3.FORWARD)
	if not _check(divided.translation == Vector3.ZERO and divided.rotation == Vector2.ZERO,
			"completed footing recovery settles exactly to the true eye"):
		return false
	divided.reset()
	divided.update(impact, 0.1, true, Vector3.RIGHT, Vector3.FORWARD)
	return _check(divided.translation == Vector3.ZERO and divided.rotation == Vector2.ZERO,
		"restart seeds current event count without replaying a historical impact")


func _foot_transfer_checks() -> bool:
	# Scripted receipts exercise presentation only; no native completion or
	# device sound/vibration is claimed by these consumer checks.
	var state := {"landing_count": 0, "landing_balance": 1.0,
		"foot_transfer_count": 0, "foot_transfer_tick": 0,
		"foot_transfer_support_entity_id": 0, "foot_transfer_peak_hand_load_n": 0.0}
	var lens := Response.new()
	var presentation := MainPresentation.new()
	var audio := FootAudio.new()
	audio._last_deaths = 0
	var master := AudioServer.get_bus_index(&"Master")
	var effects := AudioServer.get_bus_index(&"Effects")
	var added_effects := effects < 0
	if added_effects:
		AudioServer.add_bus()
		effects = AudioServer.bus_count - 1
		AudioServer.set_bus_name(effects, &"Effects")
	var master_muted := AudioServer.is_bus_mute(master)
	var effects_muted := AudioServer.is_bus_mute(effects)
	AudioServer.set_bus_mute(master, false)
	AudioServer.set_bus_mute(effects, false)
	lens.update(state, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	audio._update_foot_transfer(state, Vector3(0, 67, 0), 0)
	if not _check(presentation._consume_foot_transfer(state, 0) == 0.0
			and lens.foot_settle == Vector2.ZERO and audio.played.is_empty(),
			"foot transfer startup establishes a quiet baseline"):
		return false
	state.foot_transfer_count = 1
	state.foot_transfer_tick = 90
	state.foot_transfer_support_entity_id = 2801
	state.foot_transfer_peak_hand_load_n = 600.0
	var native_receipt := state.duplicate(true)
	var low_strength: float = presentation._consume_foot_transfer(state, 0)
	lens.update(state, 0.06, true, Vector3.RIGHT, Vector3.FORWARD)
	audio._update_foot_transfer(state, Vector3(0, 67, 0), 0)
	var low_dip := lens.foot_settle.x
	var low_level := audio.levels[0] if not audio.levels.is_empty() else -INF
	if not _check(low_strength > 0.0 and low_dip < 0.0
			and lens.translation == Vector3.ZERO and lens.rotation == Vector2.ZERO
			and audio.played == [&"step_metal"] and state == native_receipt,
			"completion receipt selects the receiving surface cue without fabricating landing impact"):
		return false
	var lens_before := lens.foot_settle
	var velocity_before := lens._foot_velocity
	lens.update(state, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	audio._update_foot_transfer(state, Vector3(0, 67, 0), 0)
	if not _check(presentation._consume_foot_transfer(state, 0) == 0.0
			and audio.played.size() == 1 and lens.foot_settle == lens_before
			and lens._foot_velocity == velocity_before,
			"repeated render samples do not replay a native foot transfer"):
		return false
	state.foot_transfer_count = 4
	state.foot_transfer_tick = 360
	state.foot_transfer_peak_hand_load_n = 2400.0
	var high_lens := Response.new()
	high_lens.update({"landing_count": 0, "foot_transfer_count": 0, "foot_transfer_tick": 0},
		0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	high_lens.update(state, 0.06, true, Vector3.RIGHT, Vector3.FORWARD)
	var high_strength: float = presentation._consume_foot_transfer(state, 0)
	lens.update(state, 0.06, true, Vector3.RIGHT, Vector3.FORWARD)
	audio._update_foot_transfer(state, Vector3(0, 67, 0), 0)
	if not _check(high_strength > low_strength and high_lens.foot_settle.x < low_dip
			and audio.played.size() == 2 and audio.levels[1] > low_level
			and absf(lens.foot_settle.x) <= 0.01 and absf(lens.foot_settle.y) <= 0.0025,
			"skipped native receipts coalesce once and measured load scales restrained feedback"):
		return false
	# A count change alone, without a new authoritative completion tick, is quiet.
	state.foot_transfer_count = 5
	lens_before = lens.foot_settle
	velocity_before = lens._foot_velocity
	lens.update(state, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	audio._update_foot_transfer(state, Vector3(0, 67, 0), 0)
	if not _check(presentation._consume_foot_transfer(state, 0) == 0.0
			and audio.played.size() == 2 and lens.foot_settle == lens_before
			and lens._foot_velocity == velocity_before,
			"same native tick cannot emit a second foot-transfer cue"):
		return false
	# Disable all three channels, consume an event, then enable them again.
	presentation._settings = Settings.new()
	presentation._settings.vibration = false
	AudioServer.set_bus_mute(effects, true)
	state.foot_transfer_count = 6
	state.foot_transfer_tick = 540
	var quiet_strength: float = presentation._consume_foot_transfer(state, 0)
	presentation._haptic(&"footplant", quiet_strength) # OFF must return before device routing.
	lens.update(state, 0.06, false, Vector3.RIGHT, Vector3.FORWARD)
	audio._update_foot_transfer(state, Vector3(0, 67, 0), 0)
	presentation._settings.vibration = true
	AudioServer.set_bus_mute(effects, false)
	lens.update(state, 0.06, true, Vector3.RIGHT, Vector3.FORWARD)
	audio._update_foot_transfer(state, Vector3(0, 67, 0), 0)
	if not _check(presentation._consume_foot_transfer(state, 0) == 0.0
			and audio.played.size() == 2 and lens.foot_settle == Vector2.ZERO,
			"settings OFF consume events without later camera/audio/haptic replay"):
		return false
	# Restarts seed the current receipt; a native rewind seeds a new epoch.
	presentation._reset_foot_transfer_feedback(state)
	audio.reset_landing_feedback(0, 0, state)
	lens.reset()
	lens.update(state, 0.06, true, Vector3.RIGHT, Vector3.FORWARD)
	audio._update_foot_transfer(state, Vector3(0, 67, 0), 0)
	if not _check(presentation._consume_foot_transfer(state, 0) == 0.0
			and audio.played.size() == 2 and lens.foot_settle == Vector2.ZERO,
			"explicit restart consumes the current completed transfer quietly"):
		return false
	state.foot_transfer_count = 0
	state.foot_transfer_tick = 0
	lens.update(state, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	audio._update_foot_transfer(state, Vector3(0, 67, 0), 0)
	if not _check(presentation._consume_foot_transfer(state, 0) == 0.0,
			"native receipt rewind establishes a quiet new baseline"):
		return false
	state.foot_transfer_count = 1
	state.foot_transfer_tick = 90
	lens.update(state, 0.06, true, Vector3.RIGHT, Vector3.FORWARD, 1)
	audio._update_foot_transfer(state, Vector3(0, 67, 0), 1)
	if not _check(presentation._consume_foot_transfer(state, 1) == 0.0
			and audio.played.size() == 2 and lens.foot_settle == Vector2.ZERO,
			"death restore cannot turn an old transfer into a success cue"):
		return false
	# A fresh valid transfer after recovery still works and settles to zero.
	presentation._fb_deaths = 1
	audio._last_deaths = 1
	state.foot_transfer_count = 2
	state.foot_transfer_tick = 180
	lens.update(state, 0.06, true, Vector3.RIGHT, Vector3.FORWARD, 1)
	audio._update_foot_transfer(state, Vector3(0, 67, 0), 1)
	if not _check(presentation._consume_foot_transfer(state, 1) > 0.0
			and audio.played.size() == 3 and lens.foot_settle.x < 0.0,
			"a new native completion after rewind/death remains eligible"):
		return false
	for i in 120:
		lens.update(state, 1.0 / 60.0, true, Vector3.RIGHT, Vector3.FORWARD, 1)
	if not _check(lens.foot_settle == Vector2.ZERO,
			"receiving-foot lens settle returns exactly to the true eye"):
		return false
	AudioServer.set_bus_mute(master, master_muted)
	AudioServer.set_bus_mute(effects, effects_muted)
	if added_effects:
		AudioServer.remove_bus(effects)
	presentation.free()
	audio.free()
	return true


func _drop(height: float, move_x: float) -> Dictionary:
	var native: Object = _main._native
	if not _check(bool(native.debug_restart_at(Vector3(6.0, height, -25.0))), "clear grade-drop staging"):
		return {}
	_main._landing_camera.reset()
	native.set_move_input(move_x, 0.0)
	var dt := float(native.get_fixed_step_seconds())
	_main._render_snapshot(0.0)
	var before := int(native.get_landing_state()["landing_count"])
	var witness: Dictionary = {}
	var deepest := 0.0
	for tick in 240:
		native.advance_frame(dt)
		var physical_position: Vector3 = native.get_player_position()
		var physical_velocity: Vector3 = native.get_player_linear_velocity()
		_main._render_snapshot(dt)
		if not _check(native.get_player_position() == physical_position \
				and native.get_player_linear_velocity() == physical_velocity,
				"render feedback never changes the physical player"):
			return {}
		deepest = minf(deepest, _main._landing_camera.translation.y)
		var contact: Dictionary = native.get_landing_state()
		if int(contact["landing_count"]) > before and witness.is_empty():
			witness = contact.duplicate(true)
		if not witness.is_empty() and tick > 120:
			break
	native.set_move_input(0.0, 0.0)
	if not _check(not witness.is_empty() and float(witness["landing_approach_energy_j"]) > 0.0 \
			and float(witness["landing_normal_speed_mps"]) > 0.0 \
			and int(witness["landing_support_entity_id"]) > 0,
			"ordinary grade contact reports its own approach energy and real support"):
		return {}
	if not _check(deepest < -0.001 and deepest > -0.25,
			"ordinary native landing drives bounded knee compression"):
		return {}
	witness["observed_camera_dip_m"] = deepest
	return witness


func _run() -> void:
	if not _response_checks() or not _foot_transfer_checks():
		return
	if "--landing-camera-unit-only" in OS.get_cmdline_user_args():
		print("SCRAPERX_LANDING_CAMERA_UNIT PASS checks=%d" % _checks)
		quit(0)
		return
	_main = load("res://main.tscn").instantiate()
	root.add_child(_main)
	_main.set_process(false)
	if not _check(_main._native != null and _main._native.has_method("get_landing_state"),
			"shipping native landing bridge is loaded"):
		return
	_main._settings.head_bob = true
	_main._head_bob_on = true
	var low := _drop(1.4, 0.0)
	if low.is_empty():
		return
	var high := _drop(4.0, 1.0)
	if high.is_empty():
		return
	if not _check(float(high["landing_approach_energy_j"]) > float(low["landing_approach_energy_j"]) \
			and float(high["observed_camera_dip_m"]) < float(low["observed_camera_dip_m"]),
			"a larger real fall carries more contact energy and compresses the lens farther"):
		return
	_main._head_bob_on = false
	_main._render_snapshot(1.0 / 60.0)
	if not _check(_main._landing_camera.translation == Vector3.ZERO \
			and _main._landing_camera.rotation == Vector2.ZERO,
			"the shipping camera honors head motion OFF"):
		return
	print("SCRAPERX_LANDING_CAMERA PASS checks=%d low_j=%.2f high_j=%.2f low_dip_m=%.4f high_dip_m=%.4f" % [
		_checks, float(low["landing_approach_energy_j"]), float(high["landing_approach_energy_j"]),
		float(low["observed_camera_dip_m"]), float(high["observed_camera_dip_m"])])
	_main.queue_free()
	await process_frame
	quit(0)
