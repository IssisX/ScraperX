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
# Each band of the tower sounds of what it is, a one-shot now and then
# somewhere around the listener: [name, floor m, clip, gap s, reach m,
# rise m, volume dB, pitch]. Wet Isolation drips; the Plate Shop's plate
# clanks far off; the Facade Crane Stack's cables creak; the machinery bands
# above boom low through the frame. A band speaks soon after it is entered.
const REGIONS := [
	[&"wet", 220.0, &"drip", Vector2(0.9, 3.0), Vector2(4.0, 14.0), Vector2(1.0, 6.0), -9.0, Vector2(0.85, 1.2)],
	[&"shop", 340.0, &"clang", Vector2(3.5, 9.0), Vector2(18.0, 55.0), Vector2(-10.0, 10.0), -14.0, Vector2(0.75, 1.0)],
	[&"crane", 484.0, &"creak", Vector2(2.5, 7.0), Vector2(8.0, 30.0), Vector2(2.0, 14.0), -10.0, Vector2(0.65, 0.85)],
	[&"machine", 640.0, &"clang", Vector2(5.0, 12.0), Vector2(25.0, 70.0), Vector2(-20.0, 5.0), -15.0, Vector2(0.5, 0.65)],
]
const REGION_ENTRY_SECONDS := Vector2(0.4, 1.5)

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
# Stone pouring: the streams nearest the listener, each heard from its fall.
var _pour_loops: Array[AudioStreamPlayer3D] = []
# Welders' arcs: WELD_LOOPS crackle loops, each following one burning arc
# (the worker's index on it, -1 free); more arcs than loops go unheard.
const WELD_LOOPS := 2
var _weld_loops: Array[AudioStreamPlayer3D] = []
var _weld_owner: Array[int] = []
var welds_heard := 0
# The Matron's music box, a loop that follows her; her one-shots, counted by
# name for the scripted runs.
var _matron_loop: AudioStreamPlayer3D
var matron_cues := {}
# The forklift, heard where it stands: its engine, its pump and its valve
# (the relief blowing off, or the lowering valve letting the load down), each
# a loop; its one-shots, counted by name for the scripted runs.
var _fork_engine: AudioStreamPlayer3D
var _fork_pump: AudioStreamPlayer3D
var _fork_valve: AudioStreamPlayer3D
var _zip_roll: AudioStreamPlayer3D
var _zip_brake: AudioStreamPlayer3D
var _fork_rpm := 0.0
var forklift_cues := {}
var pours_heard := 0
var _voice: AudioStreamPlayer
var _voice_clips := {}   # tier -> Array[AudioStreamOggVorbis]
var _voice_bags := {}    # tier -> clip indices not yet played this round
var _voice_last := {}    # tier -> index played last
var _voice_left := 0.0   # seconds of the current line still to run
# The line being said, as written (VoiceLines.CAPTIONS), while it is said.
var caption := ""
var _voice_names := {}   # tier -> clip file names, as _voice_clips
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
var region_sounds := {}   # region name -> one-shots played there
var _region := &""
var _region_clock := 0.0


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
	for i in 2:
		_pour_loops.append(_positional_player(BUS_EFFECTS, Vector3.ZERO, 5.0, 90.0))
	for i in WELD_LOOPS:
		_weld_loops.append(_positional_player(BUS_AMBIENCE, Vector3.ZERO, 4.0, 70.0))
		_weld_owner.append(-1)
	_matron_loop = _positional_player(BUS_EFFECTS, Vector3.ZERO, 5.0, 90.0)
	_fork_engine = _positional_player(BUS_EFFECTS, Vector3.ZERO, 6.0, 120.0)
	_fork_pump = _positional_player(BUS_EFFECTS, Vector3.ZERO, 5.0, 90.0)
	_fork_valve = _positional_player(BUS_EFFECTS, Vector3.ZERO, 5.0, 90.0)
	_zip_roll = _positional_player(BUS_EFFECTS, Vector3.ZERO, 3.0, 60.0)
	_zip_brake = _positional_player(BUS_EFFECTS, Vector3.ZERO, 3.0, 60.0)
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
	for player in _pour_loops:
		player.stream = _bank.pick(&"pour_loop")
		player.volume_db = -80.0
		player.play()
	for player in _weld_loops:
		player.stream = _bank.pick(&"arc_loop")
		player.volume_db = -80.0
		player.play()
	_matron_loop.stream = _bank.pick(&"matron_lullaby_loop")
	_matron_loop.volume_db = -80.0
	_matron_loop.play()


func set_volumes(master: float, effects: float, ambience: float, interface: float, voice: float = 1.0) -> void:
	for pair in [[&"Master", master], [BUS_EFFECTS, effects], [BUS_AMBIENCE, ambience],
			[BUS_INTERFACE, interface], [BUS_VOICE, voice]]:
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
	_sound_cracks(delta)
	_update_voice(delta, position, velocity, grounded, traversal, chute, deaths)

	_was_grounded = grounded
	_last_vy = velocity.y
	_last_traversal = traversal
	_last_chute = chute
	_last_deaths = deaths
	_last_crouched = crouched
	_update_air(position, velocity, grounded, delta)
	_update_birds(position, delta)
	_update_region(position, delta)


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


# Stone pouring this frame: [midpoint of the fall, its length], nearest the
# listener first. Each is heard from the middle of its fall, louder the
# longer the drop.
func update_pours(pours: Array, listener: Vector3) -> void:
	pours.sort_custom(func(a: Array, b: Array) -> bool:
		return (a[0] as Vector3).distance_to(listener) < (b[0] as Vector3).distance_to(listener))
	if not pours.is_empty():
		pours_heard += 1
	if not _bank_ready or _silent:
		return
	for i in _pour_loops.size():
		var player := _pour_loops[i]
		if i < pours.size():
			player.position = pours[i][0]
			player.volume_db = linear_to_db(clampf(0.4 + float(pours[i][1]) / 12.0, 0.4, 1.0)) - 3.0
		else:
			player.volume_db = -80.0


# Wind rises with altitude; a falling body hears the air tear past. The
# yard's distant industrial bed thins out as the climb leaves it below.
var _gust_clock := 0.0
# The native wind off the west face where C7 goes out (m/s, 0 elsewhere):
# there the wind heard is the wind that pushes.
var wind_mps := 0.0


func _update_air(position: Vector3, velocity: Vector3, grounded: bool, delta: float) -> void:
	if not _bank_ready or _silent:
		return
	# The height heard: the wind grows over the first 150 m and keeps growing
	# to 600 m, and comes in gusts, slow swells that deepen with the height.
	var altitude := clampf(position.y / 150.0, 0.0, 1.0)
	var exposure := clampf((position.y - 150.0) / 450.0, 0.0, 1.0)
	_gust_clock += delta
	var gust := 0.5 + 0.5 * sin(_gust_clock * 0.37) * sin(_gust_clock * 0.11 + 1.3)
	var wind_target := lerpf(-12.0, -4.0, altitude) + 3.0 * exposure + lerpf(1.0, 5.0, exposure) * (gust - 0.5)
	if wind_mps > 0.0:
		wind_target = maxf(wind_target, -10.0 + 12.0 * clampf(wind_mps / 16.0, 0.0, 1.0))
	_wind_db = lerpf(_wind_db, wind_target, 1.0 - exp(-2.0 * delta))
	_wind.pitch_scale = lerpf(1.0, 0.86, exposure) + 0.06 * (gust - 0.5)
	_drone_db = lerpf(_drone_db, lerpf(-13.0, -24.0, altitude), 1.0 - exp(-1.5 * delta))
	var fall := 0.0 if grounded else clampf((-velocity.y - 6.0) / 24.0, 0.0, 1.0)
	# A run gathered past the sprint into flow hears the air tear past too,
	# softer than a fall's.
	var flow := clampf((Vector2(velocity.x, velocity.z).length() - 7.5) / 1.5, 0.0, 1.0)
	var rush := maxf(sqrt(fall), 0.45 * flow)
	var rush_db := lerpf(-60.0, -2.0, rush) if rush > 0.0 else -60.0
	_rush_db = lerpf(_rush_db, rush_db, 1.0 - exp(-6.0 * delta))
	_duck_db = lerpf(_duck_db, VOICE_DUCK_DB if _voice_tier != &"" else 0.0, 1.0 - exp(-8.0 * delta))
	_wind.volume_db = _wind_db - _duck_db
	_drone.volume_db = _drone_db - _duck_db
	_rush.volume_db = _rush_db - _duck_db
	_rush.pitch_scale = lerpf(0.9, 1.35, maxf(fall, 0.3 * flow))


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
		if _voice_tier == &"pain":
			pass   # a break on this landing has already cried out
		elif -_last_vy >= LAND_HARD_IMPACT:
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
	caption = String(VoiceLines.CAPTIONS.get((_voice_names[tier] as Array)[index], ""))
	_voice_tier = tier
	_voice_left = clip.get_length()
	if _silent:
		return
	_voice.stream = clip
	_voice.play()


func _voice_stop() -> void:
	_voice_tier = &""
	caption = ""
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
		var names: Array = []
		for file in VoiceLines.TIERS[tier]:
			var clip := AudioStreamOggVorbis.load_from_file(VoiceLines.DIR + String(file))
			if clip == null:
				push_error("fear voice: cannot read %s%s" % [VoiceLines.DIR, file])
				continue
			clips.append(clip)
			names.append(String(file))
		_voice_clips[tier] = clips
		_voice_names[tier] = names
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


func _region_at(height: float) -> Array:
	var found: Array = []
	for region in REGIONS:
		if height >= float(region[1]):
			found = region
	return found


func _update_region(position: Vector3, delta: float) -> void:
	var region := _region_at(position.y)
	var name: StringName = region[0] if not region.is_empty() else &""
	if name != _region:
		_region = name
		_region_clock = randf_range(REGION_ENTRY_SECONDS.x, REGION_ENTRY_SECONDS.y)
	if region.is_empty():
		return
	_region_clock -= delta
	if _region_clock > 0.0:
		return
	var gap: Vector2 = region[3]
	var reach: Vector2 = region[4]
	var rise: Vector2 = region[5]
	var pitch: Vector2 = region[7]
	_region_clock = randf_range(gap.x, gap.y)
	region_sounds[name] = int(region_sounds.get(name, 0)) + 1
	var angle := randf() * TAU
	var away := randf_range(reach.x, reach.y)
	_play_at(region[2], position + Vector3(cos(angle) * away, randf_range(rise.x, rise.y),
		sin(angle) * away), float(region[6]), randf_range(pitch.x, pitch.y), BUS_AMBIENCE)


# One of the yard crew's footsteps, heard from where it fell.
var worker_steps := 0
var worker_strikes := 0


# The body's own mishaps, heard: the hands torn off a hold (a scrape and a
# grab that fails), a stumble's scuff of the feet.
var slips_heard := 0
var stumbles_heard := 0


func body_slip() -> void:
	slips_heard += 1
	_play(&"scrape", 0.0, randf_range(0.78, 0.88))


func body_stumble(loss: float) -> void:
	stumbles_heard += 1
	_play(&"scrape", linear_to_db(clampf(0.4 + loss, 0.4, 1.0)) - 4.0, randf_range(1.05, 1.18))


func worker_step(at: Vector3) -> void:
	worker_steps += 1
	_play_at(&"step_concrete", at, -11.0, randf_range(0.85, 1.0), BUS_AMBIENCE)


# A worker's tool striking its work.
func worker_strike(at: Vector3) -> void:
	worker_strikes += 1
	_play_at(&"clang", at, -13.0, randf_range(1.1, 1.35), BUS_AMBIENCE)


# A welder's arc (worker `w`) burning at `at` within earshot (`on`), or
# not: a crackle loop follows it while it burns.
func worker_weld(w: int, at: Vector3, on: bool) -> void:
	var slot := _weld_owner.find(w)
	if not on:
		if slot >= 0:
			_weld_owner[slot] = -1
			if _bank_ready and not _silent:
				_weld_loops[slot].volume_db = -80.0
		return
	if slot < 0:
		slot = _weld_owner.find(-1)
		if slot < 0:
			return
		_weld_owner[slot] = w
		welds_heard += 1
	if not _bank_ready or _silent:
		return
	_weld_loops[slot].position = at
	_weld_loops[slot].volume_db = -7.0


# The Matron's music box, heard from `at` at `volume_db` (below -60: off), its
# pitch dragging as she nears (`pitch`, 1.0 true).
func matron_music(at: Vector3, volume_db: float, pitch: float) -> void:
	if not _bank_ready or _silent:
		return
	_matron_loop.position = at
	_matron_loop.volume_db = volume_db
	_matron_loop.pitch_scale = pitch


# One of her sounds (locker_knock, bolt_pop, locker_burst, matron_breath,
# matron_step, matron_shriek, plank_crack), from `at`.
# The forklift this frame (sim/forklift.hpp, its state as the native gives
# it; `body` its frame in the world). Its engine runs while it is driven:
# idling, it climbs with the throttle and with the lever, and lugs down as
# the pump's pressure rises; the pump whines while it lifts, higher in pitch
# with the engine, harder with the pressure; at the relief valve's pressure
# the valve screams; letting a load down, the lowering valve hisses with its
# weight.
func update_forklift(delta: float, state: Dictionary, body: Transform3D, strain: float) -> void:
	var driving := bool(state["driving"])
	var lever := float(state["lift"])
	var raising := driving and lever > 0.05
	var lowering := driving and lever < -0.05
	var demand := maxf(absf(float(state["drive"])), lever if raising else 0.0) if driving else 0.0
	var target := 0.0
	if driving:
		target = 750.0 + 1650.0 * demand - 650.0 * strain * demand
	var rate := 2.5 if target > _fork_rpm else 4.0
	_fork_rpm += (target - _fork_rpm) * minf(1.0, rate * delta)
	if not _bank_ready or _silent:
		return
	var engine_at := body * Vector3(0.0, 0.5, -0.4)
	var pump_at := body * Vector3(0.0, 0.4, 0.9)
	_loop_level(_fork_engine, &"engine_loop", engine_at,
		0.35 + 0.55 * demand + 0.2 * strain if _fork_rpm > 100.0 else 0.0, maxf(_fork_rpm, 300.0) / 1500.0, -4.0)
	_loop_level(_fork_pump, &"pump_loop", pump_at,
		lever * (0.4 + 0.6 * strain) if raising else 0.0, maxf(_fork_rpm, 300.0) / 1500.0 * (1.0 - 0.08 * strain), -6.0)
	var held := clampf(float(state["lift_n"]) / float(state["lift_max_n"]), 0.0, 1.0)
	if raising and strain > 0.97:
		_loop_level(_fork_valve, &"relief_loop", pump_at, 1.0, 1.0, -3.0)
	elif lowering:
		_loop_level(_fork_valve, &"relief_loop", pump_at, -lever * (0.15 + 0.5 * held), 0.62, -8.0)
	else:
		_loop_level(_fork_valve, &"relief_loop", pump_at, 0.0, 1.0, 0.0)


# A zipline ride (sim/zipline.hpp), heard at the trolley over the rider's
# hands: its sheaves rolling the cable's strands, a buzz that climbs with
# the speed, and the brake block's clamp dragged down the cable once the
# trolley has run into it.
func update_zipline(riding: bool, speed_mps: float, braking: bool, bar: Vector3) -> void:
	if not _bank_ready or _silent:
		return
	var at := bar + Vector3(0.0, 0.25, 0.0)
	var pace := clampf(speed_mps / 12.0, 0.0, 1.4)
	_loop_level(_zip_roll, &"zip_loop", at, 0.9 * pace if riding and speed_mps > 0.3 else 0.0, 0.5 + pace, -6.0)
	_loop_level(_zip_brake, &"zip_brake_loop", at,
		0.4 + 0.6 * pace if riding and braking and speed_mps > 0.5 else 0.0, 0.8 + 0.4 * pace, -5.0)


# A loop at `level` (0 silent): started when first heard, its stream set once.
func _loop_level(player: AudioStreamPlayer3D, clip: StringName, at: Vector3, level: float,
		pitch: float, trim_db: float) -> void:
	player.position = at
	if level <= 0.01:
		player.volume_db = -80.0
		return
	if player.stream == null:
		player.stream = _bank.pick(clip)
	if not player.playing:
		player.play()
	player.volume_db = linear_to_db(clampf(level, 0.0, 1.0)) + trim_db
	player.pitch_scale = clampf(pitch, 0.2, 3.0)


# The forklift's moments: the check valve shutting on a load (`clunk`), the
# rear wheels lifting off (`unload`) and slamming back down (`slam`), the
# mast groaning under strain (`groan`); `strength` 0..1.
func forklift_cue(name: StringName, at: Vector3, strength: float) -> void:
	forklift_cues[name] = int(forklift_cues.get(name, 0)) + 1
	var db := linear_to_db(clampf(strength, 0.2, 1.0))
	match name:
		&"clunk":
			_play_at(&"clang", at, db - 2.0, randf_range(0.5, 0.58))
		&"slam":
			_play_at(&"land_hard", at, db + 2.0, randf_range(0.55, 0.65))
			_play_at(&"clang", at, db - 4.0, randf_range(0.42, 0.5))
		&"unload", &"groan":
			_play_at(&"creak", at, db - 3.0, randf_range(0.6, 0.72))


func matron_cue(name: StringName, at: Vector3, volume_db: float, pitch: float = 1.0) -> void:
	matron_cues[name] = int(matron_cues.get(name, 0)) + 1
	_play_at(name, at, volume_db, pitch)


# A flock flushed at `at`: its wings, heard from where it rose.
func flutter(at: Vector3) -> void:
	_play_at(&"flutter", at, -4.0, randf_range(0.9, 1.1), BUS_AMBIENCE)


func _surface(entity: int, height: float) -> StringName:
	if entity == 51 and height < 0.5:
		return &"step_earth"      # the valley floor, off the grade
	if entity in [1, 35, 43, 45, 51]:
		return &"step_concrete"   # grade, apron, bay, handoff deck, dressing
	return &"step_metal"


# A death's bones: the run of cracks at the body, and over it a second,
# lower run the harder it hit.
var _cracks_due: Array = []   # [seconds until, place, loudness, pitch]

func bone_cracks(at: Vector3, speed_mps: float, lethal_mps: float) -> void:
	var hard := clampf(speed_mps / maxf(lethal_mps, 1.0), 1.0, 3.0)
	_cracks_due.append([0.0, at, 1.0, randf_range(0.95, 1.1)])
	if hard >= 1.5:
		_cracks_due.append([0.11, at, clampf(0.4 * hard, 0.6, 1.0), randf_range(0.75, 0.85)])

# An ankle broken on landing: the crack at the feet, and the cry (one, if
# the landing's own pain line has not already said it).
func ankle_break(body: Vector3) -> void:
	_cracks_due.append([0.0, body - Vector3(0.0, 0.85, 0.0), 1.0, randf_range(1.05, 1.2)])
	if _voice_tier != &"pain":
		_voice_say(&"pain")

func _sound_cracks(delta: float) -> void:
	var keep: Array = []
	for crack in _cracks_due:
		crack[0] -= delta
		if crack[0] <= 0.0:
			_play_at(&"bone_crack", crack[1], linear_to_db(float(crack[2])), float(crack[3]))
		else:
			keep.append(crack)
	_cracks_due = keep


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
