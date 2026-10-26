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

# Footfalls at the gait's cadence (presentation/gait.gd), the same rhythm as
# the head's dip, the arms and the legs: a run strikes about three times a
# second, not eight.
const Gait := preload("res://presentation/gait.gd")
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
# The climber's breath follows the effort: a load from what the body is doing
# (the feet's speed over what they stand on, past a walk; a hang, a mantle or
# a vault), which the effort climbs toward over BREATH_RISE_S and leaves
# over BREATH_FALL_S, so a sprint's breath goes on for a while after it.
# Heard past BREATH_HEARD; a breath every 4 s at that, every 1.1 s spent.
const BREATH_RISE_S := 5.0
const BREATH_FALL_S := 9.0
const BREATH_HEARD := 0.2
const BREATH_CYCLE_S := Vector2(4.0, 1.1)
const BREATH_HANG_LOAD := 0.7
# Hits (sim/impacts.hpp): each heard where it happened, as what was struck
# sounds (ImpactSound's order), as loud as its energy: 0 dB at its kind's
# full energy, 10 dB down for each tenth of it; deeper the heavier it is
# than its kind's usual weight. A loud hit carries farther. At most
# IMPACTS_PER_FRAME are started a frame, the hardest first.
const IMPACT_CLIPS := [&"hit_timber", &"hit_hollow", &"hit_heavy", &"hit_container", &"hit_machine"]
const IMPACT_FULL_J := [400.0, 300.0, 4000.0, 6000.0, 3000.0]
const IMPACT_USUAL_KG := [160.0, 55.0, 1500.0, 3750.0, 3000.0]
const IMPACT_FLOOR_DB := -36.0
const IMPACT_PLAYERS := 6
const IMPACTS_PER_FRAME := 3
const IMPACT_REACH_M := 160.0

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
var radio_cues := 0      # the foreman's radio opened or closed, counted for the tests
# The forklift, heard where it stands: its engine, its pump and its valve
# (the relief blowing off, or the lowering valve letting the load down), each
# a loop; its one-shots, counted by name for the scripted runs.
var _fork_engine: AudioStreamPlayer3D
var _fork_pump: AudioStreamPlayer3D
var _fork_valve: AudioStreamPlayer3D
var _fork_tires: AudioStreamPlayer3D
var _fork_scrape: AudioStreamPlayer3D
var _fork_chain: AudioStreamPlayer3D
var _zip_roll: AudioStreamPlayer3D
var _zip_brake: AudioStreamPlayer3D
# The yard crane, heard from its parts 60 m up: the slew drive at the ring,
# the trolley's wheels on the jib's chords, the hoist winch's drum on the
# counter-jib and, at its full pull with nothing rising, its motor stalling.
var _crane_slew: AudioStreamPlayer3D
var _crane_trolley: AudioStreamPlayer3D
var _crane_hoist: AudioStreamPlayer3D
var _crane_stall: AudioStreamPlayer3D
# Its lifting magnet's coil, humming while it is on.
var _crane_magnet: AudioStreamPlayer3D
var crane_cues := {}
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
var _voice_frame := -1   # the frame it started
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
# The effort, 0 rested to 1 spent, and the breaths taken (halves counted as
# one, at the inhale), for the scripted runs.
var effort := 0.0
var breaths := 0
var _breath_phase := 0.0
# Hits heard, by ImpactSound, counted whether or not the bank is ready.
var impacts_heard := {}
var _impact_seen := 0
var _impact_players: Array[AudioStreamPlayer3D] = []
var _next_impact := 0


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
	for i in IMPACT_PLAYERS:
		_impact_players.append(_positional_player(BUS_EFFECTS, Vector3.ZERO, 6.0, IMPACT_REACH_M))
	_matron_loop = _positional_player(BUS_EFFECTS, Vector3.ZERO, 5.0, 90.0)
	_fork_engine = _positional_player(BUS_EFFECTS, Vector3.ZERO, 6.0, 120.0)
	_fork_pump = _positional_player(BUS_EFFECTS, Vector3.ZERO, 5.0, 90.0)
	_fork_valve = _positional_player(BUS_EFFECTS, Vector3.ZERO, 5.0, 90.0)
	_fork_tires = _positional_player(BUS_EFFECTS, Vector3.ZERO, 4.0, 80.0)
	_fork_scrape = _positional_player(BUS_EFFECTS, Vector3.ZERO, 5.0, 100.0)
	_fork_chain = _positional_player(BUS_EFFECTS, Vector3.ZERO, 3.0, 60.0)
	_zip_roll = _positional_player(BUS_EFFECTS, Vector3.ZERO, 3.0, 60.0)
	_zip_brake = _positional_player(BUS_EFFECTS, Vector3.ZERO, 3.0, 60.0)
	_crane_slew = _positional_player(BUS_EFFECTS, Vector3.ZERO, 12.0, 260.0)
	_crane_trolley = _positional_player(BUS_EFFECTS, Vector3.ZERO, 10.0, 220.0)
	_crane_hoist = _positional_player(BUS_EFFECTS, Vector3.ZERO, 14.0, 280.0)
	_crane_stall = _positional_player(BUS_EFFECTS, Vector3.ZERO, 12.0, 240.0)
	_crane_magnet = _positional_player(BUS_EFFECTS, Vector3.ZERO, 4.0, 70.0)
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


# The foreman's radio (life/foreman_radio.gd): its squelch opening a line and
# the channel's click closing it, on the interface bus. A machine's sound; his
# voice, a person's, waits to be recorded.
func radio(open: bool) -> void:
	radio_cues += 1
	if not _bank_ready or _silent:
		return
	var voice := AudioStreamPlayer.new()
	voice.bus = BUS_INTERFACE
	voice.stream = _bank.pick(&"radio_open" if open else &"radio_close")
	voice.volume_db = -6.0
	add_child(voice)
	voice.finished.connect(voice.queue_free)
	voice.play()


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
	if not _silent:
		_update_dread(delta)
	# Strides are the feet moving over what they stand on: a rider carried by a
	# lift, a cab or a turning beam is not walking.
	var foot_velocity := velocity - support_point_velocity if grounded else velocity
	var horizontal := Vector2(foot_velocity.x, foot_velocity.z).length()

	if grounded and traversal == 0 and horizontal > STEP_MIN_SPEED:
		_stride += Gait.cadence(horizontal) * delta
		if _stride >= 1.0:
			_stride -= 1.0
			steps += 1
			# Crouched feet are placed, not planted: the same step, softer. A
			# run's footfall lands harder and deeper than a walk's.
			var weight := smoothstep(1.5, 8.0, horizontal)
			_play(_surface(support_entity, position.y),
				linear_to_db(clampf(0.35 + horizontal / 8.0, 0.35, 1.0)) - (9.0 if crouched else 3.0),
				randf_range(0.95, 1.05) * lerpf(1.0, 0.86, weight))
	elif not grounded:
		_stride = 0.5  # the first step after landing comes quickly

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
	_update_breath(delta, grounded, horizontal, traversal, deaths)
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


func _update_breath(delta: float, grounded: bool, foot_speed: float, traversal: int, deaths: int) -> void:
	if _last_deaths >= 0 and deaths > _last_deaths:
		effort = 0.0
		_breath_phase = 0.0
		return
	var load := effort   # in the air the effort holds
	if traversal == TRAVERSAL_HANGING or traversal == TRAVERSAL_MANTLING or traversal == TRAVERSAL_VAULTING:
		load = BREATH_HANG_LOAD
	elif grounded:
		load = smoothstep(2.0, 9.0, foot_speed)
	var tau := BREATH_RISE_S if load > effort else BREATH_FALL_S
	effort = clampf(effort + (load - effort) * (1.0 - exp(-delta / tau)), 0.0, 1.0)
	if effort < BREATH_HEARD:
		_breath_phase = 0.0
		return
	var before := _breath_phase
	_breath_phase += delta / lerpf(BREATH_CYCLE_S.x, BREATH_CYCLE_S.y,
		smoothstep(BREATH_HEARD, 1.0, effort))
	# A fear line is breath enough: none is taken over one.
	var level := lerpf(-24.0, -11.0, smoothstep(BREATH_HEARD, 1.0, effort))
	if before == 0.0 or floorf(_breath_phase) > floorf(before):
		breaths += 1
		if _voice_tier == &"":
			_play(&"breath_in", level, randf_range(0.95, 1.05))
	elif fmod(before, 1.0) < 0.42 and fmod(_breath_phase, 1.0) >= 0.42 and _voice_tier == &"":
		_play(&"breath_out", level + 1.0, randf_range(0.93, 1.03))


# Once per rendered frame, with every machine's motion as main.gd sampled it:
# {body, speed, turning, at (its origin), tip_at (its farthest point)}.
func update_machines(delta: float, machines: Array) -> void:
	var moving: Array = []
	for machine in machines:
		if not bool(machine.get("heard", true)):
			continue   # sounded by its own parts (the forklifts, the yard crane)
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


# The hits since the last frame, from the native's kept list (oldest first):
# {sequence, entity, at, energy_j, speed_mps, mass_kg, sound}. A new world
# counts its hits from 1 again.
func update_impacts(impacts: Array, listener: Vector3) -> void:
	if impacts.is_empty():
		return
	if int(impacts.back()["sequence"]) < _impact_seen:
		_impact_seen = 0
	var fresh: Array = []
	for hit in impacts:
		if int(hit["sequence"]) <= _impact_seen:
			continue
		var sound := int(hit["sound"])
		impacts_heard[sound] = int(impacts_heard.get(sound, 0)) + 1
		if (hit["at"] as Vector3).distance_to(listener) <= IMPACT_REACH_M:
			fresh.append(hit)
	_impact_seen = int(impacts.back()["sequence"])
	if not _bank_ready or _silent or fresh.is_empty():
		return
	fresh.sort_custom(func(a: Dictionary, b: Dictionary) -> bool:
		return float(a["energy_j"]) > float(b["energy_j"]))
	for i in mini(fresh.size(), IMPACTS_PER_FRAME):
		var hit: Dictionary = fresh[i]
		var sound := clampi(int(hit["sound"]), 0, IMPACT_CLIPS.size() - 1)
		var energy := maxf(float(hit["energy_j"]), 0.001)
		var level_db := clampf(10.0 * log(energy / float(IMPACT_FULL_J[sound])) / log(10.0), IMPACT_FLOOR_DB, 3.0)
		var heft := float(IMPACT_USUAL_KG[sound]) / maxf(float(hit["mass_kg"]), 1.0)
		var player := _impact_players[_next_impact]
		_next_impact = (_next_impact + 1) % _impact_players.size()
		player.position = hit["at"]
		player.stream = _bank.pick(IMPACT_CLIPS[sound])
		player.volume_db = level_db
		player.unit_size = lerpf(3.0, 12.0, inverse_lerp(IMPACT_FLOOR_DB, 0.0, level_db))
		player.pitch_scale = clampf(pow(heft, 0.15), 0.7, 1.3) * randf_range(0.96, 1.04)
		player.play()


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
		if _voice_tier in [&"pain", &"break_leg", &"break_arm"]:
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
	# A line cut in the frame it started (a landing's pain line taken over by
	# the break it caused) was never heard: it does not count.
	if _voice_tier != &"" and _voice_frame == Engine.get_process_frames():
		voice_lines[_voice_tier] = maxi(0, int(voice_lines.get(_voice_tier, 1)) - 1)
	_voice_frame = Engine.get_process_frames()
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


# The forklift this frame (sim/forklift.hpp, its state as the native gives
# it; `body` its frame in the world), every part heard from what the native
# measured it doing:
# - the engine at the native's own rpm (ticking over at FORK_IDLE_RPM while
#   driven: the native's engine has no idle), working with the throttle and
#   lugging down as the pump's pressure rises;
# - the tires rolling at the front wheels' spin, on concrete or steel by what
#   they roll on;
# - the blades scraping only while the native finds them on what is under
#   them and sliding over it;
# - the mast's chains running at the forks' measured speed (silent stalled,
#   whatever the lever asks);
# - the pump whining while it lifts, the relief valve screaming at its
#   pressure, the lowering valve hissing with the weight let down;
# - backing, its reverse alarm, a beep every FORK_BEEP_SECONDS.
const FORK_IDLE_RPM := 750.0
const FORK_BEEP_SECONDS := 0.9
const FORK_TOP_MPS := 3.2
const FORK_LIFT_MPS := 0.4
var _fork_beep_clock := 0.0
# Both front wheels off the floor this long, the tires go quiet.
const FORK_TIRE_AIR_HOLD_S := 0.2
var _fork_tires_air := 0.0
# What was heard, counted whether or not the bank plays, for the scripted
# runs: frames each part was heard running, and beeps.
var forklift_heard := {}
var crane_heard := {}


func _count(heard: Dictionary, part: StringName) -> void:
	heard[part] = int(heard.get(part, 0)) + 1


func update_forklift(delta: float, state: Dictionary, body: Transform3D, strain: float) -> void:
	var driving := bool(state["driving"])
	var lever := float(state["lift"])
	var raising := driving and lever > 0.05
	var lowering := driving and lever < -0.05
	var rpm := maxf(float(state["engine_rpm"]), FORK_IDLE_RPM) if driving else 0.0
	var throttle := float(state["throttle"])
	var spins: PackedFloat32Array = state["wheel_spin_rad_s"]
	var radii: PackedFloat32Array = state["wheel_radius"]
	var grounds: PackedInt64Array = state["wheel_ground"]
	# The front wheels' rolling, from those on the floor: a wheel spun in the
	# air rolls on nothing. With both off it for a moment (a curb, a bump)
	# the sound holds; longer, it stops.
	var rolling := 0.0
	var touching := 0
	for i in 2:
		if int(grounds[i]) != 0:
			rolling += absf(spins[i]) * radii[i]
			touching += 1
	_fork_tires_air = 0.0 if touching > 0 else _fork_tires_air + delta
	var roll := rolling / float(touching) if touching > 0 else \
		(0.5 * (absf(spins[0]) * radii[0] + absf(spins[1]) * radii[1]) if _fork_tires_air < FORK_TIRE_AIR_HOLD_S else 0.0)
	var scraping := float(state["fork_gap_m"]) <= FORK_TOUCH_M and float(state["fork_drag_mps"]) > 0.05
	var chain := absf(float(state["lift_mps"]))
	var backing := driving and int(state["gear"]) < 0
	if rpm > 0.0:
		_count(forklift_heard, &"engine")
	if roll > 0.1:
		_count(forklift_heard, &"tires")
	if scraping:
		_count(forklift_heard, &"scrape")
	if chain > 0.02:
		_count(forklift_heard, &"chain")
	var beep := false
	if backing:
		_fork_beep_clock -= delta
		if _fork_beep_clock <= 0.0:
			_fork_beep_clock += FORK_BEEP_SECONDS
			beep = true
			_count(forklift_heard, &"beep")
	else:
		_fork_beep_clock = 0.0
	if not _bank_ready or _silent:
		return
	var wheels: Array = state["wheels"]
	var front := ((wheels[0] as Transform3D).origin + (wheels[1] as Transform3D).origin) * 0.5
	var lift_m := float(state["lift_m"])
	var engine_at := body * Vector3(0.0, 0.5, -0.4)
	var pump_at := body * Vector3(0.0, 0.4, 0.9)
	var tips_at := body * Vector3(0.0, 0.1 + lift_m, 2.2)
	var mast_at := body * Vector3(0.0, 1.4 + 0.5 * lift_m, 1.0)
	_loop_level(_fork_engine, &"engine_loop", engine_at,
		0.3 + 0.45 * throttle + 0.2 * strain if rpm > 0.0 else 0.0, rpm / 1500.0 * (1.0 - 0.06 * strain), -4.0)
	# The floor from a front wheel on it; with both off it for a moment, the
	# sound keeps to the floor it was on.
	var ground := int(grounds[0]) if int(grounds[0]) != 0 else int(grounds[1])
	var tire_clip: StringName = _fork_tires.get_meta(&"clip", &"tire_concrete_loop")
	if ground != 0:
		tire_clip = &"tire_concrete_loop" if _floor_is_concrete(ground) else &"tire_steel_loop"
	_loop_level(_fork_tires, tire_clip,
		front, clampf(roll / FORK_TOP_MPS, 0.0, 1.0) if roll > 0.1 else 0.0,
		0.6 + 0.5 * clampf(roll / FORK_TOP_MPS, 0.0, 1.5), -5.0)
	_loop_level(_fork_scrape, &"fork_scrape_loop", tips_at,
		clampf(float(state["fork_drag_mps"]) / 1.2, 0.2, 1.0) if scraping else 0.0,
		0.8 + 0.3 * clampf(float(state["fork_drag_mps"]) / 2.0, 0.0, 1.0), -3.0)
	_loop_level(_fork_chain, &"lift_chain_loop", mast_at, clampf(chain / FORK_LIFT_MPS, 0.0, 1.0) if chain > 0.02 else 0.0,
		0.5 + 0.5 * clampf(chain / FORK_LIFT_MPS, 0.0, 1.5), -7.0)
	_loop_level(_fork_pump, &"pump_loop", pump_at,
		lever * (0.4 + 0.6 * strain) if raising else 0.0, maxf(rpm, 300.0) / 1500.0 * (1.0 - 0.08 * strain), -6.0)
	var held := clampf(float(state["lift_n"]) / float(state["lift_max_n"]), 0.0, 1.0)
	if raising and strain > 0.97:
		_loop_level(_fork_valve, &"relief_loop", pump_at, 1.0, 1.0, -3.0)
	elif lowering:
		_loop_level(_fork_valve, &"relief_loop", pump_at, -lever * (0.15 + 0.5 * held), 0.62, -8.0)
	else:
		_loop_level(_fork_valve, &"relief_loop", pump_at, 0.0, 1.0, 0.0)
	if beep:
		_play_at(&"beep", body * Vector3(0.0, 1.0, -1.45), -2.0, 1.0)


# The yard crane (sim/yard_crane.hpp), each part heard from where it is and
# from what the native measured it doing: the slewing ring's motors and gears
# as the jib turns; the trolley's wheels as it runs; the hoist's winch as the
# rope comes in or pays out, deeper under load, groaning stalled. The rope
# creaks as its pull changes fast (a load taken up, a swing); the hook's
# chain jingles as the block is jerked. The lifting magnet's coil hums where
# it hangs while it is on.
const CRANE_CREAK_NPS := 25000.0
const CRANE_JINGLE_MPS2 := 0.8
var _crane_last_n := -1.0
var _crane_last_hook := 0.0
var _crane_creak_clock := 0.0
var _crane_jingle_clock := 0.0


func update_crane(delta: float, state: Dictionary, jib: Transform3D, magnet: Vector3) -> void:
	var slew := clampf(absf(float(state["slew_measured"])) / 0.08, 0.0, 1.0)
	var magnet_on := bool(state["magnet_on"])
	if magnet_on:
		_count(crane_heard, &"magnet")
	var trolley := clampf(absf(float(state["trolley_mps"])) / 1.2, 0.0, 1.0)
	# The drum as driven (kHoistMps 1 m/s), not the rope's length changing
	# as the block swings under a stopped drum.
	var rope := clampf(absf(float(state["hoist_rate"])) / 1.0, 0.0, 1.0)
	var hoist_n := float(state["hoist_n"])
	var load := clampf(hoist_n / float(state["hoist_max_n"]), 0.0, 1.0)
	var stalled := bool(state["hoist_stalled"])
	var hook_mps := float(state["hook_mps"])
	if slew > 0.02:
		_count(crane_heard, &"slew")
	if trolley > 0.02:
		_count(crane_heard, &"trolley")
	if rope > 0.02 or stalled:
		_count(crane_heard, &"winch")
	var creak := false
	var jingle := false
	_crane_creak_clock = maxf(0.0, _crane_creak_clock - delta)
	_crane_jingle_clock = maxf(0.0, _crane_jingle_clock - delta)
	if delta > 0.0 and _crane_last_n >= 0.0:
		if absf(hoist_n - _crane_last_n) / delta > CRANE_CREAK_NPS and _crane_creak_clock <= 0.0:
			creak = true
			_crane_creak_clock = 0.4
			_count(crane_heard, &"creak")
		if absf(hook_mps - _crane_last_hook) / delta > CRANE_JINGLE_MPS2 and _crane_jingle_clock <= 0.0:
			jingle = true
			_crane_jingle_clock = 0.18
			_count(crane_heard, &"jingle")
	_crane_last_n = hoist_n
	_crane_last_hook = hook_mps
	if not _bank_ready or _silent:
		return
	var ring := jib * Vector3(0.0, 0.4, 0.0)
	var drum := jib * Vector3(-6.0, 2.65, 0.0)
	_loop_level(_crane_slew, &"slew_loop", ring, 0.85 * slew, 0.6 + 0.4 * slew, -2.0)
	_loop_level(_crane_trolley, &"trolley_loop", state["rope_top"], 0.85 * trolley, 0.55 + 0.6 * trolley, -3.0)
	_loop_level(_crane_hoist, &"winch_loop", drum, (0.4 + 0.5 * load) * rope if not stalled else 0.0,
		0.55 + 0.45 * rope - 0.15 * load, -1.0)
	_loop_level(_crane_stall, &"winch_loop", drum, 1.0 if stalled else 0.0, 0.32, -1.0)
	_loop_level(_crane_magnet, &"magnet_loop", magnet, 0.8 if magnet_on else 0.0, 1.0, -6.0)
	if creak:
		_play_at(&"creak", drum, linear_to_db(clampf(0.4 + load, 0.4, 1.0)) - 2.0, randf_range(0.45, 0.6))
	if jingle:
		_play_at(&"jingle", state["hook"], linear_to_db(clampf(hook_mps / 1.5, 0.3, 1.0)) - 4.0, randf_range(0.9, 1.1))


# A crane's brake taking hold as a drive stops, or the hook block coming to
# its upper limit: a heavy clank where it happens.
func crane_cue(name: StringName, at: Vector3, strength: float) -> void:
	crane_cues[name] = int(crane_cues.get(name, 0)) + 1
	var db := linear_to_db(clampf(strength, 0.25, 1.0))
	match name:
		&"brake":
			_play_at(&"clang", at, db + 1.0, randf_range(0.36, 0.42))
		&"limit":
			_play_at(&"clang", at, db + 3.0, randf_range(0.3, 0.34))
			_play_at(&"creak", at, db - 3.0, randf_range(0.5, 0.6))
		&"landed":
			_play_at(&"land_hard", at, db + 2.0, randf_range(0.5, 0.6))
		&"magnet_take":
			# The steel drawn hard against the magnet's face.
			_play_at(&"hit_heavy", at, db + 1.0, randf_range(0.75, 0.85))
		&"magnet_drop":
			# Let go: the contactor dropping out and the steel parting from it.
			_play_at(&"clang", at, db - 2.0, randf_range(0.55, 0.65))
		&"twistlocks":
			# The spreader's four locks turning in the box's castings together:
			# a short, high steel clack and the cams grinding home.
			_play_at(&"clang", at, db, randf_range(0.85, 0.95))
			_play_at(&"creak", at, db - 6.0, randf_range(0.9, 1.0))


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
	if player.stream == null or player.get_meta(&"clip", &"") != clip:
		player.stream = _bank.pick(clip)
		player.set_meta(&"clip", clip)
		player.play()
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


# One of her sounds (locker_knock, bolt_pop, locker_burst, matron_breath,
# matron_step, matron_shriek, plank_crack), from `at`.
func matron_cue(name: StringName, at: Vector3, volume_db: float, pitch: float = 1.0) -> void:
	matron_cues[name] = int(matron_cues.get(name, 0)) + 1
	_play_at(name, at, volume_db, pitch)


# Her waking dread (sound_bank _matron_wake) from `at`, `distance` metres from
# the player. The ambience and her music box drop away first, so the world
# goes quiet around it; nearer is louder and the duck is deeper. A fresh
# variant and pitch every time, so it never repeats identically.
const DREAD_DUCK_SECONDS := 3.6
func matron_dread(at: Vector3, distance: float) -> void:
	matron_cues[&"matron_wake"] = int(matron_cues.get(&"matron_wake", 0)) + 1
	var near := 1.0 - clampf(distance / 40.0, 0.0, 1.0)
	_dread_depth_db = lerpf(-8.0, -22.0, near)
	_dread_s = DREAD_DUCK_SECONDS
	_play_at(&"matron_wake", at, lerpf(-6.0, 4.0, near), randf_range(0.92, 1.04))


var _dread_s := 0.0
var _dread_depth_db := 0.0
var _dread_amp: AudioEffectAmplify


# The duck under her dread: in fast, held, then back slowly.
func _update_dread(delta: float) -> void:
	if _dread_amp == null:
		var index := AudioServer.get_bus_index(BUS_AMBIENCE)
		if index < 0:
			return
		_dread_amp = AudioEffectAmplify.new()
		AudioServer.add_bus_effect(index, _dread_amp)
	_dread_s = maxf(0.0, _dread_s - delta)
	var u := _dread_s / DREAD_DUCK_SECONDS
	var target := _dread_depth_db * clampf(u * 3.0, 0.0, 1.0)
	_dread_amp.volume_db = move_toward(_dread_amp.volume_db, target, delta * (80.0 if target < _dread_amp.volume_db else 10.0))
	if _matron_loop != null and _dread_s > 0.0:
		_matron_loop.volume_db = minf(_matron_loop.volume_db, -60.0)


# A flock flushed at `at`: its wings, heard from where it rose.
func flutter(at: Vector3) -> void:
	_play_at(&"flutter", at, -4.0, randf_range(0.9, 1.1), BUS_AMBIENCE)


# The forklift's blades touch what is under them at this clearance or less
# (sim/forklift.hpp kForkTouchM).
const FORK_TOUCH_M := 0.004


# Whether a floor entity is concrete (the grade, the apron, the bay, the
# handoff deck, the valley floor), as the footsteps take it; else steel.
func _floor_is_concrete(entity: int) -> bool:
	return entity in [1, 35, 43, 45, 51]


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

# A blow the body felt and lived (sim kHitFeltSpeed): the thud of it, as
# loud as it was hard, and from 8 m/s the cry.
func body_hit(speed_mps: float) -> void:
	var hard := clampf((speed_mps - 6.0) / 4.0, 0.0, 1.0)
	_play(&"land_hard", linear_to_db(lerpf(0.5, 1.0, hard)), randf_range(0.8, 0.95))
	if speed_mps >= 8.0 and not (_voice_tier in [&"pain", &"break_leg", &"break_arm"]):
		_voice_say(&"pain")

# An ankle broken on landing: the crack at the feet, and the cry for the
# break (over the landing's own pain line, if that has started).
func ankle_break(body: Vector3) -> void:
	_cracks_due.append([0.0, body - Vector3(0.0, 0.85, 0.0), 1.0, randf_range(1.05, 1.2)])
	if _voice_tier != &"break_leg":
		_voice_say(&"break_leg")

# An arm broken by a blow or a wrenched catch: the crack at the shoulder, and
# the cry for it (over the blow's own pain line, if that has started).
func arm_break(body: Vector3) -> void:
	_cracks_due.append([0.0, body + Vector3(0.0, 0.45, 0.0), 1.0, randf_range(1.2, 1.35)])
	if _voice_tier != &"break_arm":
		_voice_say(&"break_arm")

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


# The bay on the 154 m deck (src/sim/bay.hpp), from get_bay_state(): the
# leaf's drive humming while it holds the leaf up; the fan's roar, coasting
# down when the district stops; the leaf groaning on her palms while she
# holds it, louder with the load. Its moments, counted for the scripted runs:
# `trip` (hands in it: the district's contactors dropping out), `hook` (the
# chain made fast on the leaf's eye) and `restart` (the district starting
# again by itself). The leaf on its stops is update_machines' clang.
const BAY_HOLD_FULL_N := 930.0
var bay_cues := {}
var _bay_drive: AudioStreamPlayer3D
var _bay_fan: AudioStreamPlayer3D
var _bay_groan: AudioStreamPlayer3D
var _bay_trips := -1
var _bay_restarts := -1
var _bay_hooked := false

func update_bay(state: Dictionary, drive: Vector3, fan: Vector3, tip: Vector3, eye: Vector3) -> void:
	if state.is_empty():
		return
	if _bay_drive == null:
		_bay_drive = _positional_player(BUS_EFFECTS, drive, 3.0, 40.0)
		_bay_fan = _positional_player(BUS_EFFECTS, fan, 4.0, 50.0)
		_bay_groan = _positional_player(BUS_EFFECTS, tip, 3.0, 40.0)
	var trips := int(state["trips"])
	var restarts := int(state["restarts"])
	var hooked := bool(state["hooked"])
	# The first sample only sets the counts: a load arrives mid-story.
	var tripped := _bay_trips >= 0 and trips > _bay_trips
	var restarted := _bay_restarts >= 0 and restarts > _bay_restarts
	var hooked_on := hooked and not _bay_hooked and _bay_trips >= 0
	_bay_trips = trips
	_bay_restarts = restarts
	_bay_hooked = hooked
	if tripped:
		_count(bay_cues, &"trip")
	if restarted:
		_count(bay_cues, &"restart")
	if hooked_on:
		_count(bay_cues, &"hook")
	if not _bank_ready or _silent:
		return
	var running := bool(state["leaf_running"])
	var lifting := clampf(absf(float(state["leaf_speed"])) / 0.35, 0.0, 1.0)
	_loop_level(_bay_drive, &"motor_loop", drive, (0.35 + 0.45 * lifting) if running else 0.0,
		0.75 + 0.35 * lifting, -10.0)
	var spin := clampf(float(state["fan_fraction"]), 0.0, 1.0)
	_loop_level(_bay_fan, &"air_roar_loop", fan, 0.75 * spin, 0.45 + 0.6 * spin, -4.0)
	var hold := clampf(float(state["support_n"]) / BAY_HOLD_FULL_N, 0.0, 1.0) if bool(state["hands"]) else 0.0
	_loop_level(_bay_groan, &"groan_loop", tip, 0.8 * hold, 0.55 + 0.1 * hold, -4.0)
	if tripped:
		_play_at(&"clang", drive, -2.0, randf_range(0.42, 0.48))
	if restarted:
		_play_at(&"clang", drive, -6.0, randf_range(0.6, 0.66))
		_play_at(&"creak", drive, -8.0, randf_range(0.7, 0.8))
	if hooked_on:
		_play_at(&"jingle", eye, -3.0, randf_range(0.85, 0.95))
		_play_at(&"clang", eye, -6.0, randf_range(0.9, 1.0))


# The chiller drop (AS-007 E), machines only, from get_wet_state(): the air
# roaring through the throttle under the car's floor, louder with its flow
# and higher with the duct's pressure; the collar's blast at the shaft's mouth
# with the vent's flow; a bang at the trip lever when the chiller's catch lets
# go; a deep thump when the chiller seats on its stop. (The leaves banging
# shut and the deck's slam are update_machines' clangs.) Counted for the
# scripted runs: air_heard each frame the roar plays above WET_E_HEARD_DB.
const WET_E_ROAR_FULL_M3S := 20.0
const WET_E_BLAST_FULL_M3S := 12.0
const WET_E_DUCT_FULL_PA := 7000.0
const WET_E_HEARD_DB := -30.0
# Where the chiller comes to rest on its stop, its middle.
const WET_E_CHILLER_SEAT := Vector3(10.0, 252.75, -136.65)
const WET_E_ROAR_TRIM_DB := -2.0
var air_heard := 0
var wet_e_cues := {}
var _wet_e_roar: AudioStreamPlayer3D
var _wet_e_blast: AudioStreamPlayer3D
var _wet_e_latched := true
var _wet_e_seated := false

func update_wet_e(_delta: float, state: Dictionary, floor_at: Vector3, mouth: Vector3, lever: Vector3) -> void:
	if state.is_empty():
		return
	if _wet_e_roar == null:
		_wet_e_roar = _positional_player(BUS_EFFECTS, floor_at, 6.0, 120.0)
		_wet_e_blast = _positional_player(BUS_EFFECTS, mouth, 10.0, 220.0)
	var flow := maxf(0.0, float(state.get("e_throttle_m3s", 0.0)))
	var roar := clampf(flow / WET_E_ROAR_FULL_M3S, 0.0, 1.0)
	if roar > 0.01 and linear_to_db(roar) + WET_E_ROAR_TRIM_DB > WET_E_HEARD_DB:
		air_heard += 1
	var duct := clampf((float(state.get("e_duct_pa", 101325.0)) - 101325.0) / WET_E_DUCT_FULL_PA, 0.0, 1.5)
	var vent := clampf(absf(float(state.get("e_vent_m3s", 0.0))) / WET_E_BLAST_FULL_M3S, 0.0, 1.0)
	if vent > 0.01:
		_count(wet_e_cues, &"blast")
	var latched := bool(state.get("e_catch_latched", true))
	var bang := _wet_e_latched and not latched
	_wet_e_latched = latched
	if bang:
		_count(wet_e_cues, &"catch")
	var seated := float(state.get("e_chiller_travel", 0.0)) <= -50.0 + 0.02
	var thump := seated and not _wet_e_seated
	_wet_e_seated = seated
	if thump:
		_count(wet_e_cues, &"seat")
	if not _bank_ready or _silent:
		return
	_loop_level(_wet_e_roar, &"air_roar_loop", floor_at, roar, 0.7 + 0.6 * duct, WET_E_ROAR_TRIM_DB)
	_loop_level(_wet_e_blast, &"vent_blast_loop", mouth, vent, 0.85 + 0.3 * vent, 0.0)
	if bang:
		_play_at(&"catch_bang", lever, 0.0, randf_range(0.95, 1.05))
	if thump:
		_play_at(&"seat_thump", WET_E_CHILLER_SEAT, 0.0, 1.0)


# Water (sim physics_world_water.cpp). The body going in at `speed_mps`
# (louder and lower the harder), and a stroke through it.
var water_cues := {}   # name -> count (counted in CI too)
var _underwater_filters := {}   # bus name -> AudioEffectLowPassFilter
var _underwater_mix := 0.0

func water_splash(at: Vector3, speed_mps: float) -> void:
	water_cues[&"splash"] = int(water_cues.get(&"splash", 0)) + 1
	var hard := clampf(speed_mps / 12.0, 0.0, 1.0)
	_play_at(&"splash", at, lerpf(-10.0, 0.0, hard), lerpf(1.15, 0.8, hard))


func swim_stroke(at: Vector3) -> void:
	water_cues[&"stroke"] = int(water_cues.get(&"stroke", 0)) + 1
	_play_at(&"swim_stroke", at, -12.0, randf_range(0.9, 1.1))


# Under the surface the world is heard through water: a low-pass on the
# effects and the ambience, eased in and out so it never clicks (the voice
# and the interface stay clear: captions and menus are not in the water).
func set_underwater(under: bool, delta: float) -> void:
	_underwater_mix = move_toward(_underwater_mix, 1.0 if under else 0.0, delta * 6.0)
	for bus_name in [BUS_EFFECTS, BUS_AMBIENCE]:
		var index := AudioServer.get_bus_index(bus_name)
		if index < 0:
			continue
		var filter: AudioEffectLowPassFilter = _underwater_filters.get(bus_name)
		if filter == null:
			filter = AudioEffectLowPassFilter.new()
			filter.cutoff_hz = 20000.0
			AudioServer.add_bus_effect(index, filter)
			_underwater_filters[bus_name] = filter
		var effect_index := -1
		for i in AudioServer.get_bus_effect_count(index):
			if AudioServer.get_bus_effect(index, i) == filter:
				effect_index = i
		if effect_index < 0:
			continue
		# 20 kHz dry to 450 Hz under: exponential in the mix, as heard.
		filter.cutoff_hz = 20000.0 * pow(450.0 / 20000.0, _underwater_mix)
		AudioServer.set_bus_effect_enabled(index, effect_index, _underwater_mix > 0.001)
