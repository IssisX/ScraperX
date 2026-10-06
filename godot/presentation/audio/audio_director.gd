extends Node
# Turns what the native simulation reports into sound. It reads state; it
# never writes it. Every cue comes from a change the native already
# decided -- feet on a surface, a takeoff, a landing's real impact speed, a
# grab, a mantle, a canopy, a death, a machine moving and stopping -- so what
# is heard is what happened.
#
# Buses (created here): Effects, Ambience, Interface into Master, each with a
# player volume on the AUDIO page; Voice, the climber's own, into Effects.
# Master ends in a hard limiter, so the mix can sit loud enough for a phone
# speaker without clipping.

const SoundBank := preload("res://presentation/audio/sound_bank.gd")
const VoiceLines := preload("res://presentation/audio/voice_lines.gd")
const SkyCycleScript := preload("res://presentation/sky_cycle.gd")

const BUS_EFFECTS := &"Effects"
const BUS_AMBIENCE := &"Ambience"
const BUS_INTERFACE := &"Interface"
const BUS_VOICE := &"Voice"
const EFFECT_VOICES := 6
const POSITIONAL_VOICES := 5

# One step per half head-bob cycle (main.gd HEAD_BOB_CYCLES_PER_METER 0.72),
# so each footfall lands on the camera's own dip.
const STRIDE_METERS := 1.0 / (2.0 * 0.72)
const STEP_MIN_SPEED := 0.3
const LAND_MIN_IMPACT := 2.0
const LAND_HARD_IMPACT := 7.0
const JUMP_MIN_RISE := 2.5
# Traversal states as the native reports them.
const TRAVERSAL_HANGING := 1
const TRAVERSAL_MANTLING := 2
const TRAVERSAL_VAULTING := 3
# Machines: every moving kit body heavy enough to be one (main.gd) is heard
# from its own motion as the native runs it -- a groan while it moves, louder
# with the speed of its farthest point; a creak now and then at a hinge while
# a body turns on it; a clang where it stops harder than a governor stops it.
# S1's cage eases into its stops at 2 m/s^2, and the recent peak a stop is
# measured against falls at 3 m/s^2, so an eased stop leaves nothing to clang.
const MACHINE_MOVE_MPS := 0.1
const MACHINE_FULL_MPS := 3.0
const MACHINE_PEAK_FALL := 3.0
const MACHINE_CLANG_MPS := 0.8
const MACHINE_CREAK_GAP := 0.9
const MACHINE_LOOPS := 2
# The climber's fear voice (GDD 8.4), from the fall the native is running:
# a yelp once the drop is fast enough to mean it (10 m/s is 5.1 m of free
# fall -- past any vault or step-down), then panic while it goes on, terror
# once it has gone on for 45 m (a yelp alone lasts ~25 m of a fall). It
# stops the instant the fall does: cut dead by a lethal impact, a groan or a
# swear on a hard landing lived through, relief when the canopy opens or a
# hand catches hold (a ledge, a ladder, a mantle).
const VOICE_YELP_MPS := 10.0
const VOICE_TERROR_DROP_M := 45.0
const VOICE_GAP_SECONDS := Vector2(0.08, 0.3)
# While a line is spoken the whole ambience bed (altitude wind, the yard,
# the falling-air rush) ducks under it: at height the wind, not the rush, is
# what a voice has to carry over.
const VOICE_DUCK_DB := 8.0
# Birds: only near the ground, only by day, now and then.
const BIRD_CEILING_M := 30.0
const BIRD_GAP_SECONDS := Vector2(2.5, 8.0)

# Counted whether or not the bank has finished building, so the scripted
# UI runs can prove the cues fire.
var steps := 0
var jumps := 0
var landings := 0
var grabs := 0
var rustles := 0
var birds := 0
var creaks := 0
var clangs := 0
# Voice lines started, by tier (yelp, panic, terror, relief, pain), and how
# many lines a lethal impact cut short.
var voice_lines := {}
var voice_cut := 0
# Which line, and the breath before the next: random in play, seeded by the
# scripted runs so they hear the same lines every time.
var voice_rng := RandomNumberGenerator.new()

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
# The bed's levels before the voice's duck, smoothed separately from it.
var _wind_db := -60.0
var _drone_db := -60.0
var _rush_db := -60.0
var _duck_db := 0.0
var _machine_loops: Array[AudioStreamPlayer3D] = []
var _voice: AudioStreamPlayer
var _voice_clips := {}   # tier -> Array[AudioStreamOggVorbis]
var _voice_bags := {}    # tier -> clip indices not yet played this round
var _voice_last := {}    # tier -> index played last
var _voice_left := 0.0   # seconds of the current line still to run
var _voice_tier := &""   # tier of the line running, "" when quiet
var _fall_top := -INF    # highest point since the climber last had support
var _fall_lines := 0     # fear lines said in this fall
var _fall_worst := &""   # worst tier said in this fall
var _machine_state := {}
var _stride := 0.0
var _was_grounded := true
var _last_vy := 0.0
var _last_traversal := 0
var _last_chute := false
var _last_deaths := -1
var _last_crouched := false
var _bird_clock := 3.0


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	_silent = AudioServer.get_driver_name() == "Dummy" and Engine.get_write_movie_path().is_empty()
	for pair in [[BUS_EFFECTS, &"Master"], [BUS_AMBIENCE, &"Master"], [BUS_INTERFACE, &"Master"],
			[BUS_VOICE, BUS_EFFECTS]]:
		if AudioServer.get_bus_index(pair[0]) < 0:
			AudioServer.add_bus()
			var index := AudioServer.bus_count - 1
			AudioServer.set_bus_name(index, pair[0])
			AudioServer.set_bus_send(index, pair[1])
	var master := AudioServer.get_bus_index(&"Master")
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
	_voice = _looping_player(BUS_VOICE)
	voice_rng.randomize()
	_load_voice()
	_wind = _looping_player(BUS_AMBIENCE)
	_rush = _looping_player(BUS_AMBIENCE)
	_drone = _looping_player(BUS_AMBIENCE)
	for i in MACHINE_LOOPS:
		_machine_loops.append(_positional_player(BUS_EFFECTS, Vector3.ZERO, 6.0, 110.0))
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
	_voice_clips.clear()


# Stops every player now, and starts nothing after. The mixer releases a
# stopped playback on its next mix, so a caller about to quit under Movie
# Maker -- whose mixing ends with the recording -- gives it a few frames
# first, or the playbacks leak at exit.
func quiesce() -> void:
	_silent = true
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
	for pair in [[_wind, &"wind_loop"], [_rush, &"rush_loop"], [_drone, &"drone_loop"]]:
		pair[0].stream = _bank.pick(pair[1])
	_wind.volume_db = -60.0
	_rush.volume_db = -60.0
	_drone.volume_db = -60.0
	for player in [_wind, _rush, _drone]:
		player.play()
	for player in _machine_loops:
		player.stream = _bank.pick(&"groan_loop")
		player.volume_db = -80.0
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


# Once per rendered frame, with the native state main.gd already read.
func update(delta: float, position: Vector3, velocity: Vector3, grounded: bool,
		support_entity: int, traversal: int, chute: bool, deaths: int, crouched: bool,
		support_point_velocity: Vector3 = Vector3.ZERO) -> void:
	# Strides are the feet moving over what they stand on: a rider carried by a
	# lift, a cab or a turning beam is not walking.
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

	if grounded and not _was_grounded:
		var impact := -_last_vy
		if impact >= LAND_MIN_IMPACT and deaths == _last_deaths:
			landings += 1
			var hard := impact >= LAND_HARD_IMPACT
			_play(&"land_hard" if hard else &"land_soft",
				linear_to_db(clampf(impact / 12.0, 0.3, 1.0)), randf_range(0.95, 1.05))
	if not grounded and _was_grounded and velocity.y > JUMP_MIN_RISE and traversal == 0:
		jumps += 1
		_play(&"jump", -4.0, randf_range(0.95, 1.08))

	if traversal != _last_traversal:
		if traversal == TRAVERSAL_HANGING:
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
		_play(&"impact_lethal", 0.0, 1.0)
	_update_voice(delta, position, velocity, grounded, traversal, chute, deaths)

	_was_grounded = grounded
	_last_vy = velocity.y
	_last_traversal = traversal
	_last_chute = chute
	_last_deaths = deaths
	_last_crouched = crouched
	_update_air(position, velocity, grounded, delta)
	_update_birds(position, delta)


# Once per rendered frame, with every machine's motion as main.gd sampled it:
# {body, speed, turning, at (its origin), tip_at (its farthest point)}.
func update_machines(delta: float, machines: Array) -> void:
	var moving: Array = []
	for machine in machines:
		var body: int = machine["body"]
		var speed: float = machine["speed"]
		var state: Dictionary = _machine_state.get(body, {"peak": 0.0, "creak": 0.0})
		var peak := maxf(speed, float(state["peak"]) - MACHINE_PEAK_FALL * delta)
		if speed < MACHINE_MOVE_MPS and peak >= MACHINE_CLANG_MPS:
			clangs += 1
			_play_at(&"clang", machine["tip_at"], linear_to_db(clampf(peak / MACHINE_FULL_MPS, 0.3, 1.0)),
				randf_range(0.85, 1.0))
			peak = 0.0
		var creak: float = float(state["creak"]) - delta
		if speed >= MACHINE_MOVE_MPS and bool(machine["turning"]) and creak <= 0.0:
			creaks += 1
			_play_at(&"creak", machine["at"],
				linear_to_db(clampf(speed / MACHINE_FULL_MPS, 0.25, 1.0)) - 4.0, randf_range(0.8, 1.0))
			creak = MACHINE_CREAK_GAP
		state["peak"] = peak
		state["creak"] = maxf(creak, 0.0)
		_machine_state[body] = state
		if speed >= MACHINE_MOVE_MPS:
			moving.append(machine)
	if not _bank_ready or _silent:
		return
	moving.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		return float(a["speed"]) > float(b["speed"]))
	for i in _machine_loops.size():
		var player := _machine_loops[i]
		if i < moving.size():
			var level := clampf(float(moving[i]["speed"]) / MACHINE_FULL_MPS, 0.05, 1.0)
			player.position = moving[i]["at"]
			player.volume_db = linear_to_db(level) - 6.0
			player.pitch_scale = 0.8 + 0.3 * level
		else:
			player.volume_db = -80.0


# Wind rises with altitude; a falling body hears the air tear past. The
# yard's distant industrial bed thins out as the climb leaves it below.
func _update_air(position: Vector3, velocity: Vector3, grounded: bool, delta: float) -> void:
	if not _bank_ready or _silent:
		return
	var altitude := clampf(position.y / 150.0, 0.0, 1.0)
	_wind_db = lerpf(_wind_db, lerpf(-12.0, -4.0, altitude), 1.0 - exp(-2.0 * delta))
	_drone_db = lerpf(_drone_db, lerpf(-13.0, -24.0, altitude), 1.0 - exp(-1.5 * delta))
	var fall := 0.0 if grounded else clampf((-velocity.y - 6.0) / 24.0, 0.0, 1.0)
	var rush_db := lerpf(-60.0, -2.0, sqrt(fall)) if fall > 0.0 else -60.0
	_rush_db = lerpf(_rush_db, rush_db, 1.0 - exp(-6.0 * delta))
	_duck_db = lerpf(_duck_db, VOICE_DUCK_DB if _voice_tier != &"" else 0.0, 1.0 - exp(-8.0 * delta))
	_wind.volume_db = _wind_db - _duck_db
	_drone.volume_db = _drone_db - _duck_db
	_rush.volume_db = _rush_db - _duck_db
	_rush.pitch_scale = lerpf(0.9, 1.35, fall)


# The fear voice's state machine, once per frame. Between lines of one fall
# `_voice_left` runs negative through a short breath before the next.
func _update_voice(delta: float, position: Vector3, velocity: Vector3, grounded: bool,
		traversal: int, chute: bool, deaths: int) -> void:
	var supported := grounded or traversal != 0 or chute
	var voiced := _fall_lines > 0
	if _last_deaths >= 0 and deaths > _last_deaths:
		if _voice_tier != &"":
			voice_cut += 1
		_voice_stop()
		_fall_reset(position)
		return
	if chute and not _last_chute and voiced:
		_voice_say(&"relief")
		_fall_reset(position)
		return
	if traversal != 0 and _last_traversal == 0 and voiced:
		_voice_say(&"relief")
		_fall_reset(position)
		return
	if grounded and not _was_grounded and voiced:
		if -_last_vy >= LAND_HARD_IMPACT:
			_voice_say(&"pain")
		elif _fall_worst == &"terror":
			_voice_say(&"relief")
		else:
			_voice_stop()
		_fall_reset(position)
		return
	_voice_left -= delta
	if _voice_left <= 0.0 and _voice_tier != &"":
		_voice_tier = &""
		_voice_left = -voice_rng.randf_range(VOICE_GAP_SECONDS.x, VOICE_GAP_SECONDS.y)
	if supported:
		_fall_reset(position)
		return
	_fall_top = maxf(_fall_top, position.y)
	if _voice_tier != &"" or _voice_left > 0.0 or -velocity.y < VOICE_YELP_MPS:
		return
	var tier := &"yelp"
	if _fall_lines > 0:
		tier = &"terror" if _fall_top - position.y >= VOICE_TERROR_DROP_M else &"panic"
	_voice_say(tier)
	_fall_lines += 1
	if tier == &"terror" or (tier == &"panic" and _fall_worst != &"terror"):
		_fall_worst = tier


func _fall_reset(position: Vector3) -> void:
	_fall_top = position.y
	_fall_lines = 0
	_fall_worst = &""


# Starts a line from `tier`, cutting any line still running. Counted and
# timed whether or not a device mixes it, so the headless runs can follow it.
func _voice_say(tier: StringName) -> void:
	var clips: Array = _voice_clips.get(tier, [])
	if clips.is_empty():
		return
	var bag: Array = _voice_bags.get(tier, [])
	if bag.is_empty():
		for i in clips.size():
			if clips.size() == 1 or i != int(_voice_last.get(tier, -1)):
				bag.append(i)
		for i in range(bag.size() - 1, 0, -1):
			var j := voice_rng.randi_range(0, i)
			var held: int = bag[i]
			bag[i] = bag[j]
			bag[j] = held
	var index: int = bag.pop_back()
	_voice_bags[tier] = bag
	_voice_last[tier] = index
	var clip: AudioStream = clips[index]
	voice_lines[tier] = int(voice_lines.get(tier, 0)) + 1
	_voice_tier = tier
	_voice_left = clip.get_length()
	if _silent:
		return
	_voice.stream = clip
	_voice.play()


func _voice_stop() -> void:
	_voice_tier = &""
	_voice_left = 0.0
	if not _silent:
		_voice.stop()


# The spoken lines (presentation/audio/voice/, listed by voice_lines.gd).
# Ogg pages are read here and decoded as they play: a few hundred kB, so
# they load with the director, not on the bank's worker.
func _load_voice() -> void:
	var count := 0
	for tier in VoiceLines.TIERS:
		var clips: Array = []
		for file in VoiceLines.TIERS[tier]:
			var clip := AudioStreamOggVorbis.load_from_file(VoiceLines.DIR + String(file))
			if clip == null:
				push_error("fear voice: cannot read %s%s" % [VoiceLines.DIR, file])
				continue
			clips.append(clip)
		_voice_clips[tier] = clips
		count += clips.size()
	print("SCRAPERX_AUDIO_VOICE lines=%d" % count)


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
	var voice := _voices_3d[_next_voice_3d]
	_next_voice_3d = (_next_voice_3d + 1) % _voices_3d.size()
	voice.bus = bus
	voice.position = at
	voice.stream = _bank.pick(name)
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
