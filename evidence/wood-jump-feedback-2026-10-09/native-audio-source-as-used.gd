extends Node
# Turns what the native simulation reports into sound. It reads state; it
# never writes it. Every cue comes from a change the native already
# decided -- feet on a surface, a takeoff, a landing's real impact speed, a
# grab, a mantle, a canopy, a death, the plant's steam flow, its ballast
# striking steel -- so what is heard is what happened.
#
# Buses (created here, all into Master): Effects, Ambience, Interface; each
# has a player volume on the AUDIO page. Master ends in a hard limiter, so
# the mix can sit loud enough for a phone speaker without clipping.

const SoundBank := preload("res://presentation/audio/sound_bank.gd")
const FallReactions := preload("res://presentation/audio/fall_reactions.gd")
const SkyCycleScript := preload("res://presentation/sky_cycle.gd")
const RUBBLE_IMPACT := preload("res://assets/audio/rubble/concrete_break_denoised_843338_preview.wav")
const TIMBER_FRACTURE := preload("res://assets/audio/timber/timber_snap_400638_preview.wav")

const BUS_EFFECTS := &"Effects"
const BUS_AMBIENCE := &"Ambience"
const BUS_INTERFACE := &"Interface"
const EFFECT_VOICES := 6
const POSITIONAL_VOICES := 5

# One step per half head-bob cycle (main.gd HEAD_BOB_CYCLES_PER_METER 0.72),
# so each footfall lands on the camera's own dip.
const STRIDE_METERS := 1.0 / (2.0 * 0.72)
const STEP_MIN_SPEED := 0.3
const LAND_MIN_IMPACT := 2.0
const LAND_HARD_IMPACT := 7.0
const JUMP_MIN_RISE := 2.5
const FOOT_JUMP_RECEIPT_SECONDS := 0.45
# Traversal states as the native reports them.
const TRAVERSAL_HANGING := 1
const TRAVERSAL_MANTLING := 2
const TRAVERSAL_VAULTING := 3
const TRAVERSAL_CLIMBING := 4
# Where the plant's sounds come from: the motor and gearbox by the firebox,
# the steam from the top of the pressure vessel.
const PLANT_POSITION := Vector3(30.5, 2.0, -100.0)
const VESSEL_TOP := Vector3(30.5, 4.8, -101.5)
# The plant's native ranges (measured over 90 s of its own cycle): orifice
# flow to 2.27 kg/s, scoop 12 m/s, lift 5.7 m/s, ballast strikes 2-11 m/s
# of velocity lost in one frame, tipper up to 1.3 rad/s.
const FLOW_FULL_KG_S := 2.0
const SCOOP_FULL_MPS := 4.0
const LIFT_FULL_MPS := 3.0
const BALLAST_STRIKE_MPS := 1.5
const BALLAST_STRIKE_FULL_MPS := 9.0
const TIPPER_CREAK_RAD_S := 0.35
const CREAK_COOLDOWN := 0.6
const RUBBLE_IMPACT_COOLDOWN := 0.1
const RUBBLE_IMPACT_FULL_MPS := 8.0
# Birds: only near the ground, only by day, now and then.
const BIRD_CEILING_M := 30.0
const BIRD_GAP_SECONDS := Vector2(2.5, 8.0)

# Counted whether or not the bank has finished building, so the scripted
# UI runs can prove the cues fire.
var steps := 0
var jumps := 0
var landings := 0
var foot_plants := 0
var grabs := 0
var rustles := 0
var clangs := 0
var creaks := 0
var birds := 0

# The time of day, for the birds. Set by main.gd.
var sky: Node = null

var _bank := SoundBank.new()
var _bank_ready := false
# No output device (CI, headless): the Dummy driver never mixes, so a started
# playback is never torn down and leaks at exit (observed: ten AudioStreamWAV
# / AudioStreamPlaybackWAV). Cues are still counted; nothing is started.
# Movie Maker (--write-movie) also reports Dummy, but it drives that driver's
# mix itself and writes it out, so there the bank plays.
var _silent := false
var _bank_task := -1
var _voices: Array[AudioStreamPlayer] = []
var _next_voice := 0
var _voices_3d: Array[AudioStreamPlayer3D] = []
var _next_voice_3d := 0
var _wind: AudioStreamPlayer
var _rush: AudioStreamPlayer
var _drone: AudioStreamPlayer
var _hum: AudioStreamPlayer3D
var _hiss: AudioStreamPlayer3D
var _rattle: AudioStreamPlayer3D
var _motor: AudioStreamPlayer3D
var _water_screw_motor: AudioStreamPlayer3D
var _water_lift_drive: AudioStreamPlayer3D
var _water_lift_water: AudioStreamPlayer3D
var _stride := 0.0
var _was_grounded := true
var _last_landing_count := -1
var _last_foot_transfer_count := -1
var _last_foot_transfer_tick := -1
var _last_foot_push_start_count := -1
var _pending_foot_jump_count := -1
var _pending_foot_jump_age := 0.0
var _last_traversal := 0
var _last_chute := false
var _last_deaths := -1
var _last_crouched := false
var _last_ballast_velocity := Vector3.ZERO
var _last_scoop := Vector3.INF
var _last_tipper := INF
var _creak_cooldown := 0.0
var _bird_clock := 3.0
var _machines_seen := false
var fall_reactions: Node
var regression_machines := false
var pipe_motion_frames := 0
var pipe_crush_cues := 0
var _pipe_motion: AudioStreamPlayer3D
var _pipe_front := 0.0
var _pipe_deaths := -1
var sling_releases := 0
var sling_draw_cues := 0
var sling_comedy_cues := 0
var _sling_squeaked := false
var _sling_launches := 0
var _sling_draw := 0.0
var _sling_air_speed := 0.0
var _bullet_time: AudioStreamPlayer
var _ladder_strain: AudioStreamPlayer3D
var ladder_motion_frames := 0
var climb_regrips := 0
var _ladder_gain := 0.0
var _cinematic_active := false
var _cinematic_air_duck_db := 0.0
var sling_cinematic_cues := 0
var rubble_impact_cues := 0
var timber_fracture_cues := 0
var _last_rubble_impact_count := -1
var _rubble_impact_cooldown := 0.0
var _pending_rubble_speed := 0.0
var _pending_rubble_position := Vector3.ZERO


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	_silent = AudioServer.get_driver_name() == "Dummy" and Engine.get_write_movie_path().is_empty()
	for bus in [BUS_EFFECTS, BUS_AMBIENCE, BUS_INTERFACE]:
		if AudioServer.get_bus_index(bus) < 0:
			AudioServer.add_bus()
			var index := AudioServer.bus_count - 1
			AudioServer.set_bus_name(index, bus)
			AudioServer.set_bus_send(index, &"Master")
	var master := AudioServer.get_bus_index(&"Master")
	fall_reactions = FallReactions.new()
	fall_reactions.name = "FallReactions"
	add_child(fall_reactions)
	var has_limiter := false
	for i in AudioServer.get_bus_effect_count(master):
		has_limiter = has_limiter or AudioServer.get_bus_effect(master, i) is AudioEffectHardLimiter
	if not has_limiter:
		# +6 dB into a -0.8 dB ceiling: the mix sits near a phone game's usual
		# loudness, and the limiter, not the speaker, takes the peaks.
		var limiter := AudioEffectHardLimiter.new()
		limiter.pre_gain_db = 6.0
		limiter.ceiling_db = -0.8
		AudioServer.add_bus_effect(master, limiter)
	for i in EFFECT_VOICES:
		var voice := AudioStreamPlayer.new()
		voice.bus = BUS_EFFECTS
		add_child(voice)
		_voices.append(voice)
	for i in POSITIONAL_VOICES:
		_voices_3d.append(_positional_player(BUS_EFFECTS, Vector3.ZERO, 8.0, 160.0))
	_bullet_time = AudioStreamPlayer.new()
	_bullet_time.bus = BUS_EFFECTS
	add_child(_bullet_time)
	_wind = _looping_player(BUS_AMBIENCE)
	_rush = _looping_player(BUS_AMBIENCE)
	_drone = _looping_player(BUS_AMBIENCE)
	_hum = _positional_player(BUS_AMBIENCE, PLANT_POSITION, 8.0, 140.0)
	_hiss = _positional_player(BUS_AMBIENCE, VESSEL_TOP, 6.0, 110.0)
	_rattle = _positional_player(BUS_AMBIENCE, Vector3.ZERO, 5.0, 90.0)
	_motor = _positional_player(BUS_AMBIENCE, Vector3.ZERO, 6.0, 100.0)
	_water_screw_motor = _positional_player(BUS_AMBIENCE, Vector3(-18.0, 1.0, -98.0), 6.0, 100.0)
	_water_lift_drive = _positional_player(BUS_AMBIENCE, Vector3(-12.5, 4.0, -108.2), 5.0, 90.0)
	_water_lift_water = _positional_player(BUS_AMBIENCE, Vector3(-18.0, 4.5, -105.8), 5.0, 80.0)
	_ladder_strain = _positional_player(BUS_EFFECTS, Vector3(6, 124, -181.2), 6.0, 55.0)
	_pipe_motion = _positional_player(BUS_EFFECTS, Vector3(6, 2.4, -85), 10.0, 90.0)
	# ~0.35 s of GDScript synthesis on a desktop: off the main thread, so the
	# first frames are not held up; cues before it finishes are counted but
	# silent.
	_bank_task = WorkerThreadPool.add_task(_build_bank)


func _exit_tree() -> void:
	if _bank_task >= 0:
		WorkerThreadPool.wait_for_task_completion(_bank_task)
		_bank_task = -1
	for child in get_children():
		if child is AudioStreamPlayer or child is AudioStreamPlayer3D:
			child.stop()
			child.stream = null
	_bank.clips.clear()


# Stops every player now, and starts nothing after. The mixer releases a
# stopped playback on its next mix, so a caller about to quit under Movie
# Maker -- whose mixing ends with the recording -- gives it a few frames
# first, or the playbacks leak at exit.
func quiesce() -> void:
	_silent = true
	fall_reactions.quiesce()
	cancel_launch_cinematic()
	for child in get_children():
		if child is AudioStreamPlayer or child is AudioStreamPlayer3D:
			child.stop()


func _build_bank() -> void:
	_bank.build()
	_on_bank_ready.call_deferred()


func _on_bank_ready() -> void:
	_bank_ready = true
	print("SCRAPERX_AUDIO_BANK clips=%d msec=%d driver=%s" % [_bank.clips.size(), _bank.build_msec,
		AudioServer.get_driver_name()])
	if _silent:
		return
	for pair in [[_wind, &"wind_loop"], [_rush, &"rush_loop"], [_drone, &"drone_loop"],
			[_hum, &"hum_loop"], [_hiss, &"hiss_loop"], [_rattle, &"rattle_loop"],
			[_motor, &"motor_loop"], [_water_screw_motor, &"motor_loop"],
			[_water_lift_drive, &"rattle_loop"], [_water_lift_water, &"hiss_loop"], [_pipe_motion, &"rattle_loop"], [_ladder_strain, &"rattle_loop"]]:
		pair[0].stream = _bank.pick(pair[1])
	_wind.volume_db = -60.0
	_rush.volume_db = -60.0
	_drone.volume_db = -60.0
	_hum.volume_db = -6.0 if regression_machines else -80.0
	for player in [_hiss, _rattle, _motor, _water_screw_motor, _water_lift_drive, _water_lift_water, _pipe_motion, _ladder_strain]:
		player.volume_db = -80.0
	for player in [_wind, _rush, _drone, _hum, _hiss, _rattle, _motor, _water_screw_motor,
			_water_lift_drive, _water_lift_water, _pipe_motion, _ladder_strain]:
		player.play()


func set_volumes(master: float, effects: float, ambience: float, interface: float) -> void:
	for pair in [[&"Master", master], [BUS_EFFECTS, effects], [BUS_AMBIENCE, ambience],
			[BUS_INTERFACE, interface]]:
		var index := AudioServer.get_bus_index(pair[0])
		if index < 0:
			continue
		var level: float = pair[1]
		AudioServer.set_bus_mute(index, level <= 0.001)
		AudioServer.set_bus_volume_db(index, linear_to_db(maxf(level, 0.001)))


func ui_tap(back: bool = false) -> void:
	if not _bank_ready or _silent:
		return
	var voice := AudioStreamPlayer.new()
	voice.bus = BUS_INTERFACE
	voice.stream = _bank.pick(&"ui_back" if back else &"ui_tap")
	voice.volume_db = -8.0
	add_child(voice)
	voice.finished.connect(voice.queue_free)
	voice.play()


# Explicit restart establishes a quiet receipt baseline; no old contact is
# replayed when gameplay resumes. Death restores are handled by update().
func reset_landing_feedback(landing_count: int, deaths: int, foot_state: Dictionary = {}) -> void:
	_last_landing_count = landing_count
	_last_foot_transfer_count = int(foot_state.get("foot_transfer_count", -1))
	_last_foot_transfer_tick = int(foot_state.get("foot_transfer_tick", -1))
	_last_foot_push_start_count = int(foot_state.get("foot_push_start_count", -1))
	_clear_foot_jump()
	_last_deaths = deaths
	reset_reclaim_feedback()
	_stop_timber_fracture()


func _stop_timber_fracture() -> void:
	# Restart/death must not leave the previous native fracture sounding.
	# Other positional voices keep their existing ownership and lifecycle.
	for voice in _voices_3d:
		if voice.stream == TIMBER_FRACTURE:
			voice.stop()


# A native rewind consumes old powder/contact receipts without changing the
# death receipt that update() still needs for its lethal-impact response.
func reset_reclaim_feedback() -> void:
	_last_rubble_impact_count = -1
	_rubble_impact_cooldown = 0.0
	_pending_rubble_speed = 0.0
	_pending_rubble_position = Vector3.ZERO


# Once per rendered frame, with the native state main.gd already read.
func update(delta: float, position: Vector3, velocity: Vector3, grounded: bool,
		support_entity: int, traversal: int, chute: bool, deaths: int, crouched: bool,
		support_point_velocity: Vector3 = Vector3.ZERO, landing_state: Dictionary = {}) -> void:
	fall_reactions.update(delta, position, velocity, grounded, traversal, chute, deaths)
	# A planted rider moves with the support without taking a step. Native
	# point velocity includes rotation as well as the support's translation.
	var foot_velocity := velocity - support_point_velocity if grounded else velocity
	var horizontal := Vector2(foot_velocity.x, foot_velocity.z).length()

	if grounded and traversal == 0 and horizontal > STEP_MIN_SPEED:
		_stride += horizontal * delta
		if _stride >= STRIDE_METERS:
			_stride -= STRIDE_METERS
			steps += 1
			# Crouched feet are placed, not planted: the same step, softer.
			_play(_surface(support_entity, position.y),
				linear_to_db(clampf(0.35 + horizontal / 8.0, 0.35, 1.0)) - (9.0 if crouched else 3.0),
				randf_range(0.94, 1.06))
	elif not grounded:
		_stride = STRIDE_METERS * 0.5  # the first step after landing comes quickly

	var landing_count := int(landing_state.get("landing_count", -1))
	var impact := maxf(float(landing_state.get("landing_normal_speed_mps", 0.0)), 0.0)
	if _last_landing_count >= 0 and landing_count > _last_landing_count \
			and impact >= LAND_MIN_IMPACT and deaths == _last_deaths:
		landings += 1
		var hard := impact >= LAND_HARD_IMPACT
		_play(&"land_hard" if hard else &"land_soft",
			linear_to_db(clampf(impact / 12.0, 0.3, 1.0)), randf_range(0.95, 1.05))
	_last_landing_count = landing_count
	_update_foot_transfer(landing_state, position, deaths)
	var foot_jump := _consume_foot_jump(delta, landing_state, velocity, grounded,
		traversal, deaths, crouched)
	var climbing_jump := traversal != _last_traversal and _last_traversal == TRAVERSAL_CLIMBING \
		and traversal == 0 and velocity.y > JUMP_MIN_RISE and deaths == _last_deaths
	var ordinary_jump := (not grounded and _was_grounded and velocity.y > JUMP_MIN_RISE \
		and traversal == 0) or climbing_jump
	if ordinary_jump or foot_jump:
		jumps += 1
		if ordinary_jump:
			_play(&"jump", -4.0, 1.06 if climbing_jump else randf_range(0.95, 1.08))
		else:
			# Consume the real wood departure under mute without starting a
			# voice whose tail could surface after unmuting. Ordinary cues retain
			# their established behavior and coalesce with this receipt above.
			var audible := true
			for bus in [&"Master", BUS_EFFECTS]:
				var index := AudioServer.get_bus_index(bus)
				if index < 0 or AudioServer.is_bus_mute(index):
					audible = false
			if audible:
				_play(&"jump", -4.0, 1.0)

	if traversal != _last_traversal:
		if traversal in [TRAVERSAL_HANGING, TRAVERSAL_CLIMBING]:
			grabs += 1
			_play(&"grab", -2.0, randf_range(0.95, 1.05))
		elif traversal == TRAVERSAL_MANTLING or traversal == TRAVERSAL_VAULTING:
			_play(&"scrape", -4.0, randf_range(0.92, 1.08))
	if crouched != _last_crouched:
		rustles += 1
		_play(&"rustle", -8.0, randf_range(0.9, 1.1) if crouched else randf_range(1.05, 1.2))
	if chute and not _last_chute:
		_play(&"chute", -1.0, 1.0)
	if _last_deaths >= 0 and deaths > _last_deaths:
		_stop_timber_fracture()
		_play(&"impact_lethal", 0.0, 1.0)

	_was_grounded = grounded
	_last_traversal = traversal
	_last_chute = chute
	_last_deaths = deaths
	_last_crouched = crouched
	_update_air(position, velocity, grounded, delta)
	_update_birds(position, delta)


# The coupled plant, as the native runs it: steam through the orifice,
# the hoist chain, the lift drive, the ballast striking steel, the tipper's
# hinge turning under load.
func update_machines(delta: float, flow_kg_s: float, scoop: Vector3, ballast: Vector3,
		ballast_velocity: Vector3, tipper: Vector3, tipper_angle: float, lift: Vector3,
		lift_velocity: Vector3) -> void:
	var scoop_speed := 0.0
	var tipper_rate := 0.0
	if _machines_seen and delta > 0.0:
		scoop_speed = (scoop - _last_scoop).length() / delta
		tipper_rate = absf(tipper_angle - _last_tipper) / delta
		# Velocity lost in one frame: the ballast has hit something.
		var lost := (_last_ballast_velocity - ballast_velocity).length()
		if lost >= BALLAST_STRIKE_MPS:
			clangs += 1
			_play_at(&"clang", ballast,
				linear_to_db(clampf(lost / BALLAST_STRIKE_FULL_MPS, 0.3, 1.0)) + 2.0,
				randf_range(0.93, 1.07))
	_creak_cooldown = maxf(0.0, _creak_cooldown - delta)
	if tipper_rate >= TIPPER_CREAK_RAD_S and _creak_cooldown <= 0.0:
		_creak_cooldown = CREAK_COOLDOWN
		creaks += 1
		_play_at(&"creak", tipper, -3.0, randf_range(0.9, 1.1))
	_machines_seen = true
	_last_scoop = scoop
	_last_tipper = tipper_angle
	_last_ballast_velocity = ballast_velocity
	if not _bank_ready or _silent:
		return
	var steam := clampf(flow_kg_s / FLOW_FULL_KG_S, 0.0, 1.0)
	_hiss.volume_db = linear_to_db(maxf(steam, 0.0001)) - 3.0
	_hiss.pitch_scale = lerpf(0.85, 1.15, steam)
	_rattle.position = scoop
	var chain := clampf(scoop_speed / SCOOP_FULL_MPS, 0.0, 1.0)
	_rattle.volume_db = linear_to_db(maxf(chain, 0.0001)) - 4.0
	_rattle.pitch_scale = lerpf(0.8, 1.25, chain)
	_motor.position = lift
	var drive := clampf(absf(lift_velocity.y) / LIFT_FULL_MPS, 0.0, 1.0)
	_motor.volume_db = linear_to_db(maxf(drive, 0.0001)) - 5.0
	_motor.pitch_scale = lerpf(0.75, 1.2, drive)



# The screw sounds from authoritative shaft/load state, including a torque
# growl when stalled and coast-down after the switch is released.
func update_water_screw(rpm: float, torque_nm: float) -> void:
	if not _bank_ready or _silent:
		return
	var speed := clampf(absf(rpm) / 22.0, 0.0, 1.0)
	var load := clampf(absf(torque_nm) / 1500.0, 0.0, 1.0)
	var audible := maxf(speed, load * 0.45)
	_water_screw_motor.volume_db = linear_to_db(maxf(audible, 0.0001)) - 5.0
	_water_screw_motor.pitch_scale = lerpf(0.62, 1.20, speed) * lerpf(0.92, 1.0, load)


# Water hiss follows measured transfer. Rope/cage machinery follows measured
# solver tension, so a disconnected or unloaded lift goes quiet.
func update_water_lift(flow_m3_s: float, rope_tension_n: float) -> void:
	if not _bank_ready or _silent:
		return
	var flow := clampf(absf(flow_m3_s) / 0.12, 0.0, 1.0)
	var load := clampf(absf(rope_tension_n) / 30000.0, 0.0, 1.0)
	_water_lift_water.volume_db = linear_to_db(maxf(flow, 0.0001)) - 4.0
	_water_lift_water.pitch_scale = lerpf(0.82, 1.18, flow)
	_water_lift_drive.volume_db = linear_to_db(maxf(load, 0.0001)) - 7.0
	_water_lift_drive.pitch_scale = lerpf(0.72, 1.08, load)


# Native pan motion, loose-pipe energy and irreversible receiver compression
# provide sound inputs without introducing presentation-owned progress.
func update_pipe_bridge(state: Dictionary, deaths: int) -> void:
	if state.is_empty():
		return
	var at: Vector3 = state["pan"]
	var velocity: Vector3 = state["velocity"]
	var front := float(state["crush_front"])
	var gain := clampf(maxf(velocity.length() / 1.2, sqrt(maxf(0, state["pipe_energy"]) / 12000.0)), 0, 1)
	if gain > 0.03:
		pipe_motion_frames += 1
	if deaths == _pipe_deaths and _pipe_front <= 0.001 and front > 0.001:
		pipe_crush_cues += 1
		_play_at(&"creak", at, -2.0, 0.72)
	_pipe_front = front
	_pipe_deaths = deaths
	if _bank_ready and not _silent:
		_pipe_motion.position = at
		_pipe_motion.volume_db = linear_to_db(maxf(gain, 0.0001)) - 6.0
		_pipe_motion.pitch_scale = lerpf(0.65, 1.0, gain)


# Native receipts count real closing contacts, coalesced to the strongest
# contact per physics tick. Further audio coalescing retains the strongest
# received contact within 0.1s; intensity uses closing speed, not an impulse.
func update_reclaim_impacts(state: Dictionary, delta: float) -> void:
	var elapsed := maxf(delta, 0.0) if is_finite(delta) else 0.0
	_rubble_impact_cooldown = maxf(0.0, _rubble_impact_cooldown - elapsed)
	var count := int(state.get("rubble_impact_count", -1))
	if count < 0 or _last_rubble_impact_count < 0 or count < _last_rubble_impact_count:
		# Establish a quiet baseline on startup, missing state or native reset.
		_last_rubble_impact_count = count
		_rubble_impact_cooldown = 0.0
		_pending_rubble_speed = 0.0
		return
	if count > _last_rubble_impact_count:
		var speed := float(state.get("rubble_impact_speed_mps", 0.0))
		var at: Vector3 = state.get("rubble_impact_position", Vector3.INF)
		if is_finite(speed) and at.is_finite() and speed > _pending_rubble_speed:
			_pending_rubble_speed = speed
			_pending_rubble_position = at
	_last_rubble_impact_count = count
	if _pending_rubble_speed <= 0.0 or _rubble_impact_cooldown > 0.0:
		return
	var gain := clampf(_pending_rubble_speed / RUBBLE_IMPACT_FULL_MPS, 0.04, 1.0)
	rubble_impact_cues += 1
	# This recorded stream is already loaded; it does not wait for synthesis.
	_play_stream_at(RUBBLE_IMPACT, _pending_rubble_position, linear_to_db(gain) - 5.0,
		randf_range(0.96, 1.04))
	_pending_rubble_speed = 0.0
	_rubble_impact_cooldown = RUBBLE_IMPACT_COOLDOWN


func timber_fracture(at: Vector3, strength: float) -> void:
	if not at.is_finite() or not is_finite(strength) or strength <= 0.0:
		return
	# Main consumes/coalesces real native fracture receipts. Count valid cues
	# under Dummy too; the existing pool owns silent/voice behavior.
	timber_fracture_cues += 1
	# Consume muted receipts without starting a tail that can emerge when
	# Effects/Master is unmuted. This matches native foot-transfer feedback.
	for bus in [&"Master", BUS_EFFECTS]:
		var index := AudioServer.get_bus_index(bus)
		if index < 0 or AudioServer.is_bus_mute(index):
			return
	_play_stream_at(TIMBER_FRACTURE, at,
		-8.0 + linear_to_db(clampf(strength, 0.25, 1.0)), 1.0, BUS_EFFECTS)


func update_slingshot(state: Dictionary, velocity: Vector3) -> void:
	var launches := int(state.get("launch_count", 0))
	if launches > _sling_launches:
		sling_releases += 1
		_play(&"sling_release", -2.0, 1.0)
		_play(&"sling_wobble", -10.0, 1.0)
		sling_comedy_cues += 1
		_sling_squeaked = false
	_sling_launches = launches
	var draw := float(state.get("draw_m", 0.0))
	var max_draw := maxf(0.01, float(state.get("max_draw_m", 12.0)))
	if bool(state.get("drawing", false)) and draw > max_draw * 0.70 and not _sling_squeaked:
		_sling_squeaked = true
		sling_comedy_cues += 1
		_play(&"rubber_squeak", -11.0, 1.0)
	elif not bool(state.get("seated", false)) and not bool(state.get("released", false)):
		_sling_squeaked = false
	if bool(state.get("drawing", false)) and draw >= _sling_draw + 0.7:
		_sling_draw = draw
		sling_draw_cues += 1
		var tension := clampf(draw / maxf(float(state.get("max_draw_m", 12.0)), 0.1), 0.0, 1.0)
		_play(&"band_stretch", lerpf(-13.0, -5.0, tension), lerpf(0.75, 1.35, tension))
	elif draw < _sling_draw:
		_sling_draw = draw
	_sling_air_speed = velocity.length() if bool(state.get("released", false)) else 0.0


func _update_air(position: Vector3, velocity: Vector3, grounded: bool, delta: float) -> void:
	if not _bank_ready or _silent:
		return
	var altitude := clampf(position.y / 150.0, 0.0, 1.0)
	# Duck the entire air bed while the human voice speaks, with a quick
	# attack and smooth release. Raising only the voice left wind masking it.
	var voice_duck := 12.0 if fall_reactions.speaking() else 0.0
	var wind_db := lerpf(-12.0, -4.0, altitude) - voice_duck - _cinematic_air_duck_db
	_wind.volume_db = lerpf(_wind.volume_db, wind_db, 1.0 - exp(-(18.0 if voice_duck > 0.0 else 2.0) * delta))
	var drone_db := lerpf(-13.0, -24.0, altitude) - voice_duck - _cinematic_air_duck_db
	_drone.volume_db = lerpf(_drone.volume_db, drone_db, 1.0 - exp(-(18.0 if voice_duck > 0.0 else 1.5) * delta))
	var fall := 0.0 if grounded else maxf(clampf((-velocity.y - 6.0) / 24.0, 0.0, 1.0),
		clampf((_sling_air_speed - 12.0) / 85.0, 0.0, 1.0))
	var rush_db := lerpf(-60.0, -2.0, sqrt(fall)) if fall > 0.0 else -60.0
	rush_db -= voice_duck + _cinematic_air_duck_db
	_rush.volume_db = lerpf(_rush.volume_db, rush_db, 1.0 - exp(-(18.0 if voice_duck > 0.0 else 6.0) * delta))
	_rush.pitch_scale = lerpf(0.9, 1.35, fall)


# Now and then a bird in the conifers, somewhere around the listener, by day
# and near the ground only.
func _update_birds(position: Vector3, delta: float) -> void:
	_bird_clock -= delta
	if _bird_clock > 0.0:
		return
	_bird_clock = randf_range(BIRD_GAP_SECONDS.x, BIRD_GAP_SECONDS.y)
	if position.y > BIRD_CEILING_M or sky == null:
		return
	if SkyCycleScript.sun_direction(float(sky.hour)).y < 0.08:
		return
	birds += 1
	var angle := randf() * TAU
	var reach := randf_range(18.0, 50.0)
	_play_at(&"bird", position + Vector3(cos(angle) * reach, randf_range(4.0, 12.0),
		sin(angle) * reach), -6.0, randf_range(0.9, 1.15), BUS_AMBIENCE)


func _surface(entity: int, height: float) -> StringName:
	if entity == 51 and height < 0.5:
		return &"step_earth"      # the valley floor, off the grade
	if entity in [1, 35, 43, 45, 51]:
		return &"step_concrete"   # grade, apron, bay, handoff deck, dressing
	return &"step_metal"


func _play(name: StringName, volume_db: float, pitch: float) -> void:
	if not _bank_ready or _silent:
		return
	var voice := _voices[_next_voice]
	_next_voice = (_next_voice + 1) % _voices.size()
	voice.stream = _bank.pick(name)
	voice.volume_db = volume_db
	voice.pitch_scale = pitch
	voice.play()


func _play_at(name: StringName, at: Vector3, volume_db: float, pitch: float,
		bus: StringName = BUS_EFFECTS) -> void:
	if not _bank_ready or _silent:
		return
	_play_stream_at(_bank.pick(name), at, volume_db, pitch, bus)


func _play_stream_at(stream: AudioStream, at: Vector3, volume_db: float, pitch: float,
		bus: StringName = BUS_EFFECTS) -> void:
	if _silent or stream == null or _voices_3d.is_empty():
		return
	var voice := _voices_3d[_next_voice_3d]
	_next_voice_3d = (_next_voice_3d + 1) % _voices_3d.size()
	voice.bus = bus
	voice.position = at
	voice.stream = stream
	voice.volume_db = volume_db
	voice.pitch_scale = pitch
	voice.play()


func _looping_player(bus: StringName) -> AudioStreamPlayer:
	var player := AudioStreamPlayer.new()
	player.bus = bus
	add_child(player)
	return player


func _positional_player(bus: StringName, at: Vector3, unit_size: float,
		max_distance: float) -> AudioStreamPlayer3D:
	var player := AudioStreamPlayer3D.new()
	player.bus = bus
	player.position = at
	player.unit_size = unit_size
	player.max_distance = max_distance
	add_child(player)
	return player


# Read-only presentation clock shared with the orbital lens. A reserved
# voice prevents footsteps or the spring snap from stealing the time cue.
func update_launch_cinematic(shot: Dictionary) -> void:
	var active := bool(shot.get("active", false))
	var clock := float(shot.get("clock", 0.0))
	var phase := float(shot.get("orbit_phase", 0.0))
	if active and not _cinematic_active:
		sling_cinematic_cues += 1
		if _bank_ready and not _silent:
			_bullet_time.stream = _bank.pick(&"bullet_time")
			_bullet_time.play(maxf(0.0, clock))
	_cinematic_active = active
	if not active:
		cancel_launch_cinematic()
		return
	_cinematic_air_duck_db = lerpf(14.0, 0.0, phase)
	_bullet_time.volume_db = lerpf(-6.0, -2.0, phase)


func cancel_launch_cinematic() -> void:
	_cinematic_active = false
	_cinematic_air_duck_db = 0.0
	if _bullet_time != null:
		_bullet_time.stop()


func _clear_foot_jump() -> void:
	_pending_foot_jump_count = -1
	_pending_foot_jump_age = 0.0


func _consume_foot_jump(delta: float, state: Dictionary, velocity: Vector3,
		grounded: bool, traversal: int, deaths: int, crouched: bool) -> bool:
	for key in ["foot_push_start_count", "foot_push_support_entity_id",
			"foot_push_stop_reason", "foot_push_active", "foot_push_command_work_bound_j",
			"foot_push_peak_load_n"]:
		if not state.has(key):
			_last_foot_push_start_count = -1
			_clear_foot_jump()
			return false
	var count := int(state["foot_push_start_count"])
	var previous := _last_foot_push_start_count
	_last_foot_push_start_count = count
	var support := int(state["foot_push_support_entity_id"])
	var reason := int(state["foot_push_stop_reason"])
	var active := bool(state["foot_push_active"])
	var work := float(state["foot_push_command_work_bound_j"])
	var load := float(state["foot_push_peak_load_n"])
	# Native StopReason: None0 while active; successful finite completion
	# Expired9/StrokeExhausted10/CommandBudgetExhausted11. Cleared1 (Drop)
	# and invalid body/geometry/material/traction/reach2..8 never cue a Jump.
	var valid_stop := (active and reason == 0) or (not active and reason in [9, 10, 11])
	if previous < 0 or count < previous or count < 0 or deaths != _last_deaths \
			or crouched or traversal != 0 or not valid_stop or support < 2880 or support > 2891 \
			or not velocity.is_finite() or not is_finite(work) or work < 0.0 \
			or not is_finite(load) or load < 0.0 or not is_finite(delta) or delta < 0.0:
		_clear_foot_jump()
		return false
	if count > previous:
		# A count may skip rendered frames. Keep only the newest actual native
		# command, without replaying startup state or extending the physics push.
		_pending_foot_jump_count = count
		_pending_foot_jump_age = 0.0
	elif _pending_foot_jump_count >= 0:
		_pending_foot_jump_age += minf(delta, FOOT_JUMP_RECEIPT_SECONDS + 0.01)
	if _pending_foot_jump_count != count or _pending_foot_jump_age > FOOT_JUMP_RECEIPT_SECONDS \
			or (not active and grounded):
		_clear_foot_jump()
		return false
	if grounded or velocity.y <= 0.0 or work <= 0.0 or load <= 0.0:
		return false
	_clear_foot_jump()
	return true


func _update_foot_transfer(state: Dictionary, position: Vector3, deaths: int) -> void:
	var count := int(state.get("foot_transfer_count", -1))
	var tick := int(state.get("foot_transfer_tick", -1))
	var support := int(state.get("foot_transfer_support_entity_id", 0))
	var load := float(state.get("foot_transfer_peak_hand_load_n", 0.0))
	var new_transfer := _last_foot_transfer_count >= 0 and count > _last_foot_transfer_count \
		and tick > _last_foot_transfer_tick and tick > 0 and support > 0 \
		and deaths == _last_deaths and is_finite(load) and load >= 0.0
	_last_foot_transfer_count = count
	_last_foot_transfer_tick = tick
	if not new_transfer:
		return
	foot_plants += 1
	# The receipt's force scales a quiet foot plant, not a fabricated impact.
	# Muted receipts do not start a voice that could ring after unmuting.
	for bus in [&"Master", BUS_EFFECTS]:
		var index := AudioServer.get_bus_index(bus)
		if index < 0 or AudioServer.is_bus_mute(index):
			return
	var weight := clampf(load / 2400.0, 0.25, 1.0)
	_play(_surface(support, position.y), lerpf(-15.0, -9.0, weight), 1.0)


func climb_regrip(at: Vector3) -> void:
	climb_regrips += 1
	_play_at(&"grab", at, -8.0, 1.10)


# Passive bearing noise follows native COM speed. Loading makes the same
# real movement louder; a settled, unloaded ladder goes quiet.
func update_suspended_ladder(delta: float, velocity: Vector3, held: bool) -> void:
	var target := clampf(velocity.length() / 1.0, 0.0, 1.0)
	if target > 0.025:
		ladder_motion_frames += 1
	_ladder_gain = lerpf(_ladder_gain, target, 1.0 - exp(-10.0 * delta))
	if _bank_ready and not _silent:
		_ladder_strain.volume_db = linear_to_db(maxf(_ladder_gain, 0.0001)) - (7.0 if held else 14.0)
		_ladder_strain.pitch_scale = lerpf(0.65, 1.12, _ladder_gain)
