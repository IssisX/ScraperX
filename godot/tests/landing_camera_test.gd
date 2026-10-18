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


func _slide_checks() -> bool:
	# Synthetic bridge samples test lens behavior, not native slide physics.
	var baseline := {"landing_count": 0, "player_sliding": false,
		"slide_speed_mps": 0.0, "slide_count": 0}
	var active := {"landing_count": 0, "player_sliding": true,
		"slide_speed_mps": 8.0, "slide_count": 1}
	var unchanged := active.duplicate(true)
	var one := Response.new()
	var divided := Response.new()
	one.update(baseline, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	divided.update(baseline, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	one.update(active, 0.08, true, Vector3.RIGHT, Vector3.FORWARD)
	for i in 4:
		divided.update(active, 0.02, true, Vector3.RIGHT, Vector3.FORWARD)
	if not _check(one.rotation.distance_to(divided.rotation) < 0.000001
			and one.rotation.x < 0.0 and one.rotation.y == 0.0
			and one.translation == Vector3.ZERO and one.foot_settle == Vector2.ZERO
			and active == unchanged,
			"native slide gives subdivision-stable pitch without roll, eye-height or physics writes"):
		return false
	var landing := {"landing_count": 1, "landing_normal_speed_mps": 5.0,
		"landing_slip_velocity": Vector3(2.0, 0.0, -1.0), "landing_tangent_energy_j": 200.0,
		"foot_transfer_count": 1, "foot_transfer_tick": 90,
		"foot_transfer_support_entity_id": 11, "foot_transfer_peak_hand_load_n": 600.0}
	var plain_landing := Response.new()
	var combined_landing := Response.new()
	var combined_baseline := baseline.duplicate()
	combined_baseline.merge({"foot_transfer_count": 0, "foot_transfer_tick": 0})
	plain_landing.update(combined_baseline, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	combined_landing.update(combined_baseline, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	var combined_state := landing.duplicate()
	combined_state.merge(active)
	combined_state.landing_count = 1
	plain_landing.update(landing, 0.08, true, Vector3.RIGHT, Vector3.FORWARD)
	combined_landing.update(combined_state, 0.08, true, Vector3.RIGHT, Vector3.FORWARD)
	if not _check(combined_landing.translation == plain_landing.translation
			and combined_landing.foot_settle == plain_landing.foot_settle
			and combined_landing.rotation.y == plain_landing.rotation.y
			and absf(combined_landing.rotation.x - plain_landing.rotation.x - one.rotation.x) < 0.000001,
			"slide pitch adds to landing without changing impact, slip roll or foot-transfer settle"):
		return false
	var slow := Response.new()
	slow.update(baseline, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	var slow_state := active.duplicate()
	slow_state.slide_speed_mps = 2.0
	slow.update(slow_state, 0.08, true, Vector3.RIGHT, Vector3.FORWARD)
	if not _check(slow.rotation.x < 0.0 and slow.rotation.x > one.rotation.x,
			"measured support-relative slide speed scales pitch"):
		return false
	active.slide_speed_mps = 1000000.0
	for i in 120:
		one.update(active, 1.0 / 60.0, true, Vector3.RIGHT, Vector3.FORWARD)
	if not _check(is_finite(one.rotation.x) and absf(one.rotation.x) <= 0.035001,
			"extreme finite speed stays within two degrees of additive pitch"):
		return false
	var before := one.rotation.x
	active.player_sliding = false
	one.update(active, 0.016, true, Vector3.RIGHT, Vector3.FORWARD)
	if not _check(one.rotation.x < 0.0 and one.rotation.x > before,
			"loss of native slide contact releases the lens smoothly"):
		return false
	for i in 180:
		one.update(active, 1.0 / 60.0, true, Vector3.RIGHT, Vector3.FORWARD)
	if not _check(one.rotation == Vector2.ZERO,
			"inactive slide settles exactly to neutral even with stale speed"):
		return false
	active.player_sliding = true
	active.slide_speed_mps = 8.0
	one.update(active, 0.1, true, Vector3.RIGHT, Vector3.FORWARD)
	one.update(active, 0.1, false, Vector3.RIGHT, Vector3.FORWARD)
	one.update(active, 0.1, true, Vector3.RIGHT, Vector3.FORWARD)
	if not _check(one.rotation == Vector2.ZERO and one._slide_velocity == 0.0,
			"motion OFF clears slide and re-enable establishes a quiet baseline"):
		return false
	for reset_kind in ["restart", "rewind", "death"]:
		one.update(active, 0.1, true, Vector3.RIGHT, Vector3.FORWARD)
		var deaths := 0
		if reset_kind == "restart":
			one.reset()
		elif reset_kind == "rewind":
			active.slide_count = 0
		else:
			deaths = 1
		one.update(active, 0.1, true, Vector3.RIGHT, Vector3.FORWARD, deaths)
		if not _check(one.rotation == Vector2.ZERO and one._slide_velocity == 0.0,
				"%s clears slide without a historical entry impulse" % reset_kind):
			return false
	# Invalid bridge data clears only the slide channel. Landing still matches
	# a consumer that never received any slide fields.
	for invalid in [{}, {"player_sliding": true},
			{"player_sliding": true, "slide_speed_mps": NAN, "slide_count": 1},
			{"player_sliding": true, "slide_speed_mps": INF, "slide_count": 1},
			{"player_sliding": true, "slide_speed_mps": -1.0, "slide_count": 1},
			{"player_sliding": "true", "slide_speed_mps": 8.0, "slide_count": 1},
			{"player_sliding": true, "slide_speed_mps": "8", "slide_count": 1},
			{"player_sliding": true, "slide_speed_mps": 8.0, "slide_count": -1}]:
		var plain := Response.new()
		var combined := Response.new()
		plain.update({"landing_count": 0}, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
		combined.update(baseline, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
		combined.update(unchanged, 0.08, true, Vector3.RIGHT, Vector3.FORWARD)
		var invalid_landing := {"landing_count": 1, "landing_normal_speed_mps": 5.0}
		var sample := invalid_landing.duplicate()
		sample.merge(invalid)
		plain.update(invalid_landing, 0.08, true, Vector3.RIGHT, Vector3.FORWARD)
		combined.update(sample, 0.08, true, Vector3.RIGHT, Vector3.FORWARD)
		if not _check(combined.translation == plain.translation and combined.rotation == plain.rotation
				and combined.foot_settle == plain.foot_settle,
				"missing or invalid slide fields preserve the existing landing response"):
			return false
	return true


func _wall_stride_checks() -> bool:
	# Actual bridge fields drive only a lens roll; these samples do not prove
	# native wall support or device comfort.
	var baseline := {"landing_count": 0, "wall_stride_active": false,
		"wall_stride_pushing_off": false, "wall_stride_count": 0,
		"wall_stride_normal": Vector3.ZERO, "actual_foot_load_n": 0.0}
	var active := baseline.duplicate()
	active.merge({"wall_stride_active": true, "wall_stride_count": 1,
		"wall_stride_normal": Vector3.RIGHT, "actual_foot_load_n": 2400.0}, true)
	var unchanged := active.duplicate(true)
	var one := Response.new()
	var divided := Response.new()
	var opposite := Response.new()
	for lens in [one, divided, opposite]:
		lens.update(baseline, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	one.update(active, 0.08, true, Vector3.RIGHT, Vector3.FORWARD)
	for i in 4:
		divided.update(active, 0.02, true, Vector3.RIGHT, Vector3.FORWARD)
	var other_side := active.duplicate()
	other_side.wall_stride_normal = Vector3.LEFT
	opposite.update(other_side, 0.08, true, Vector3.RIGHT, Vector3.FORWARD)
	if not _check(one.rotation.y < 0.0 and opposite.rotation.y > 0.0
			and absf(one.rotation.y + opposite.rotation.y) < 0.000001
			and one.rotation.distance_to(divided.rotation) < 0.000001
			and absf(one.rotation.y) <= 0.045 and one.rotation.x == 0.0
			and one.translation == Vector3.ZERO and one.foot_settle == Vector2.ZERO
			and active == unchanged,
			"loaded opposite wall normals give opposite bounded, subdivision-stable lens bank"):
		return false
	var push_off := Response.new()
	push_off.update(baseline, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	var push_sample := active.duplicate()
	push_sample.wall_stride_pushing_off = true
	push_off.update(push_sample, 0.08, true, Vector3.RIGHT, Vector3.FORWARD)
	if not _check(push_off.rotation == one.rotation,
			"loaded push-off retains bank from the same actual planted foot"):
		return false
	var plain := Response.new()
	var combined := Response.new()
	var combined_baseline := baseline.duplicate()
	combined_baseline.merge({"player_sliding": false, "slide_speed_mps": 0.0, "slide_count": 0})
	plain.update(combined_baseline, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	combined.update(combined_baseline, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	var landing := {"landing_count": 1, "landing_normal_speed_mps": 5.0,
		"landing_slip_velocity": Vector3(2.0, 0.0, -1.0), "landing_tangent_energy_j": 200.0,
		"player_sliding": true, "slide_speed_mps": 8.0, "slide_count": 1}
	var combined_sample := active.duplicate()
	combined_sample.merge(landing, true)
	plain.update(landing, 0.08, true, Vector3.RIGHT, Vector3.FORWARD)
	combined.update(combined_sample, 0.08, true, Vector3.RIGHT, Vector3.FORWARD)
	if not _check(combined.translation == plain.translation and combined.foot_settle == plain.foot_settle
			and combined.rotation.x == plain.rotation.x
			and absf(combined.rotation.y - plain.rotation.y - one.rotation.y) < 0.000001,
			"wall bank adds to existing landing roll and slide pitch without changing either response"):
		return false
	var light := Response.new()
	light.update(baseline, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
	var light_state := active.duplicate()
	light_state.actual_foot_load_n = 600.0
	light.update(light_state, 0.08, true, Vector3.RIGHT, Vector3.FORWARD)
	if not _check(absf(light.rotation.y * 4.0 - one.rotation.y) < 0.000001,
			"actual measured load scales wall bank without a minimum synthetic load"):
		return false
	var facing := Response.new()
	facing.update(baseline, 0.0, true, Vector3.FORWARD, Vector3.LEFT)
	facing.update(active, 0.08, true, Vector3.FORWARD, Vector3.LEFT)
	if not _check(facing.rotation == Vector2.ZERO,
			"free-look right axis determines bank; a directly faced wall adds no roll"):
		return false
	active.actual_foot_load_n = 1000000.0
	for i in 120:
		one.update(active, 1.0 / 60.0, true, Vector3.RIGHT, Vector3.FORWARD)
	if not _check(is_finite(one.rotation.y) and absf(one.rotation.y) <= 0.045,
			"extreme finite foot load stays within the wall bank limit"):
		return false
	for release in [{"actual_foot_load_n": 0.0}, {"wall_stride_active": false}]:
		var released := active.duplicate()
		released.merge(release, true)
		for i in 120:
			one.update(active, 1.0 / 60.0, true, Vector3.RIGHT, Vector3.FORWARD)
		var before: float = one.rotation.y
		one.update(released, 0.016, true, Vector3.RIGHT, Vector3.FORWARD)
		if not _check(one.rotation.y > before and one.rotation.y < 0.0,
				"unloaded and inactive wall states release bank smoothly"):
			return false
		for i in 180:
			one.update(released, 1.0 / 60.0, true, Vector3.RIGHT, Vector3.FORWARD)
		if not _check(one.rotation == Vector2.ZERO,
				"released wall bank settles exactly to neutral"):
			return false
	for reset_kind in ["first", "restart", "disabled", "rewind", "death"]:
		var lens := Response.new()
		if reset_kind != "first":
			lens.update(baseline, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
			lens.update(active, 0.1, true, Vector3.RIGHT, Vector3.FORWARD)
		var sample := active.duplicate()
		var deaths := 0
		if reset_kind == "restart":
			lens.reset()
		elif reset_kind == "disabled":
			lens.update(sample, 0.1, false, Vector3.RIGHT, Vector3.FORWARD)
		elif reset_kind == "rewind":
			sample.wall_stride_count = 0
		elif reset_kind == "death":
			deaths = 1
		lens.update(sample, 0.1, true, Vector3.RIGHT, Vector3.FORWARD, deaths)
		if not _check(lens.rotation == Vector2.ZERO and lens._wall_velocity == 0.0,
				"%s seeds wall state without retaining or replaying bank" % reset_kind):
			return false
		lens.update(sample, 0.1, true, Vector3.RIGHT, Vector3.FORWARD, deaths)
		if not _check(lens.rotation.y < 0.0,
				"fresh loaded samples after %s still provide continuous bank" % reset_kind):
			return false
	for field in ["wall_stride_active", "wall_stride_pushing_off", "wall_stride_count",
			"wall_stride_normal", "actual_foot_load_n"]:
		var missing := active.duplicate()
		missing.erase(field)
		one.update(active, 0.1, true, Vector3.RIGHT, Vector3.FORWARD)
		one.update(missing, 0.1, true, Vector3.RIGHT, Vector3.FORWARD)
		one.update(active, 0.1, true, Vector3.RIGHT, Vector3.FORWARD)
		if not _check(one.rotation == Vector2.ZERO and one._wall_velocity == 0.0,
				"missing %s clears wall state and reappearance seeds quietly" % field):
			return false
	for invalid in [{"wall_stride_active": "true"}, {"wall_stride_pushing_off": 1},
			{"wall_stride_count": -1}, {"wall_stride_count": 1.0},
			{"wall_stride_normal": Vector3(NAN, 0.0, 0.0)},
			{"wall_stride_normal": Vector3(INF, 0.0, 0.0)}, {"wall_stride_normal": "right"},
			{"actual_foot_load_n": NAN}, {"actual_foot_load_n": INF},
			{"actual_foot_load_n": -1.0}, {"actual_foot_load_n": "2400"}]:
		var plain_invalid := Response.new()
		var combined_invalid := Response.new()
		plain_invalid.update(baseline, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
		combined_invalid.update(baseline, 0.0, true, Vector3.RIGHT, Vector3.FORWARD)
		combined_invalid.update(active, 0.1, true, Vector3.RIGHT, Vector3.FORWARD)
		var invalid_landing := {"landing_count": 1, "landing_normal_speed_mps": 5.0}
		var sample := active.duplicate()
		sample.merge(invalid_landing, true)
		sample.merge(invalid, true)
		plain_invalid.update(invalid_landing, 0.08, true, Vector3.RIGHT, Vector3.FORWARD)
		combined_invalid.update(sample, 0.08, true, Vector3.RIGHT, Vector3.FORWARD)
		if not _check(combined_invalid.translation == plain_invalid.translation and combined_invalid.rotation == plain_invalid.rotation
				and combined_invalid.foot_settle == plain_invalid.foot_settle,
				"malformed wall fields clear only wall bank and preserve actual landing response"):
			return false
	return true


func _wall_stride_audio_checks() -> bool:
	var audio := FootAudio.new()
	root.add_child(audio)
	var state := {"wall_stride_count": 0, "wall_stride_active": false,
		"foot_push_support_entity_id": 51, "foot_push_peak_load_n": 0.0}
	audio.reset_landing_feedback(0, 0, state)
	state.merge({"wall_stride_count": 1, "wall_stride_active": true,
		"foot_push_peak_load_n": 1500.0, "foot_push_command_work_bound_j": 0.0}, true)
	audio._update_wall_stride(state, Vector3(0, 2, 0), 0)
	audio._update_wall_stride(state, Vector3(0, 2, 0), 0)
	if not _check(audio.wall_steps == 1 and audio.played == [&"step_concrete"],
			"one actual loaded wall step cues its surface once, including fall absorption"):
		return false
	state.wall_stride_count = 2
	state.wall_stride_active = false
	audio._update_wall_stride(state, Vector3(0, 2, 0), 0)
	state.wall_stride_count = 3
	state.wall_stride_active = true
	state.foot_push_peak_load_n = 0.0
	audio._update_wall_stride(state, Vector3(0, 2, 0), 0)
	if not _check(audio.wall_steps == 1 and audio.played.size() == 1,
			"inactive or unloaded wall-step counts cannot fabricate a shoe impact"):
		return false
	state.wall_stride_count = 4
	state.foot_push_peak_load_n = 1500.0
	audio.reset_landing_feedback(0, 0, state)
	audio._update_wall_stride(state, Vector3(0, 2, 0), 0)
	state.wall_stride_count = 5
	audio._update_wall_stride(state, Vector3(0, 2, 0), 1)
	state.wall_stride_count = 0
	audio._update_wall_stride(state, Vector3(0, 2, 0), 0)
	if not _check(audio.wall_steps == 1 and audio.played.size() == 1,
			"restart, death and native rewind do not replay historical wall audio"):
		return false
	audio.queue_free()
	return true


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


func _landing_feedback_checks() -> bool:
	# Consumer receipts prove selection/coalescing, not native stumble physics
	# or physical device vibration. The native response remains the authority.
	var presentation := MainPresentation.new()
	var state := {"landing_count": 0, "landing_normal_speed_mps": 4.8,
		"landing_tangent_energy_j": 1250.0, "landing_response": 3,
		"landing_recovering": true, "landing_support_entity_id": 11}
	if not _check(presentation._consume_landing_feedback(state, 0, 0, 20.0).is_empty(),
			"landing feedback startup establishes a quiet baseline"):
		return false
	state.landing_count = 1
	var receipt := state.duplicate(true)
	var low: Dictionary = presentation._consume_landing_feedback(state, 0, 90, 20.0)
	if not _check(low.get("kind") == &"stumble" and float(low.get("strength", 0)) > 0.0
			and state == receipt, "active native sideways recovery earns a read-only low-normal cue"):
		return false
	if not _check(presentation._consume_landing_feedback(state, 0, 90, 20.0).is_empty(),
			"repeated landing samples never repeat the stumble pulse"):
		return false
	state.landing_count = 4
	state.landing_tangent_energy_j = 3000.0
	var high: Dictionary = presentation._consume_landing_feedback(state, 0, 360, 20.0)
	if not _check(high.get("kind") == &"stumble" and float(high.get("strength", 0)) > float(low["strength"])
			and float(high["strength"]) <= 0.85, "skipped receipts coalesce once with bounded measured-energy strength"):
		return false
	state.landing_count = 5
	if not _check(presentation._consume_landing_feedback(state, 0, 360, 20.0).is_empty(),
			"a single native tick cannot emit two landing pulses"):
		return false
	state.landing_count = 6
	state.landing_normal_speed_mps = 6.0
	var impact: Dictionary = presentation._consume_landing_feedback(state, 0, 450, 20.0)
	if not _check(impact.get("kind") == &"land" and is_equal_approx(float(impact.get("strength", 0)), 0.3),
			"existing normal-impact cue keeps its strength and priority without a second pulse"):
		return false
	state.landing_normal_speed_mps = 4.8
	state.landing_recovering = false
	state.landing_count = 7
	if not _check(presentation._consume_landing_feedback(state, 0, 540, 20.0).is_empty(),
			"a historical response without active native recovery stays quiet"):
		return false
	state.landing_recovering = true
	state.landing_response = 0
	state.landing_count = 8
	if not _check(presentation._consume_landing_feedback(state, 0, 630, 20.0).is_empty(),
			"ordinary native recovery never earns the sideways-stumble cue"):
		return false
	state.landing_response = 3
	state.landing_tangent_energy_j = NAN
	state.landing_count = 9
	if not _check(presentation._consume_landing_feedback(state, 0, 720, 20.0).is_empty(),
			"invalid energy cannot drive tactile strength"):
		return false
	state.landing_tangent_energy_j = 3000.0
	state.landing_count = 10
	presentation._settings = Settings.new()
	presentation._settings.vibration = false
	presentation._settings.head_bob = false
	presentation._settings.launch_cinematics = false
	var quiet: Dictionary = presentation._consume_landing_feedback(state, 0, 810, 20.0)
	presentation._haptic(quiet["kind"], float(quiet["strength"]))
	presentation._settings.vibration = true
	if not _check(presentation._consume_landing_feedback(state, 0, 810, 20.0).is_empty(),
			"vibration OFF consumes the receipt without replay while motion comfort is disabled"):
		return false
	presentation._reset_landing_feedback(state, 810)
	if not _check(presentation._consume_landing_feedback(state, 0, 810, 20.0).is_empty(),
			"restart seeds current landing count and tick quietly"):
		return false
	state.landing_count = 0
	if not _check(presentation._consume_landing_feedback(state, 0, 0, 20.0).is_empty(),
			"native rewind establishes a quiet landing epoch"):
		return false
	state.landing_count = 1
	if not _check(presentation._consume_landing_feedback(state, 1, 90, 20.0).is_empty(),
			"death restore suppresses the landing-success cue"):
		return false
	presentation._fb_deaths = 1
	state.landing_count = 2
	if not _check(presentation._consume_landing_feedback(state, 1, 180, 20.0).get("kind") == &"stumble",
			"fresh native recovery after death remains eligible"):
		return false
	presentation.free()
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
	if not _response_checks() or not _slide_checks() or not _wall_stride_checks() or not _wall_stride_audio_checks() or not _foot_transfer_checks() or not _landing_feedback_checks():
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
