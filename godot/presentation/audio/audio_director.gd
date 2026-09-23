extends Node
# Turns what the native simulation reports into sound. It reads state; it
# never writes it. Every cue comes from a change the native already
# decided -- feet on a surface, a takeoff, a landing's real impact speed, a
# grab, a mantle, a canopy, a death -- so what is heard is what happened.
#
# Buses (created here, all into Master): Effects, Ambience, Interface; each
# has a player volume on the AUDIO page.

const SoundBank := preload("res://presentation/audio/sound_bank.gd")

const BUS_EFFECTS := &"Effects"
const BUS_AMBIENCE := &"Ambience"
const BUS_INTERFACE := &"Interface"
const EFFECT_VOICES := 6

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
# The machine hum's source: the plant's firebox.
const PLANT_POSITION := Vector3(30.5, 2.0, -100.0)

# Counted whether or not the bank has finished building, so the scripted
# UI runs can prove the cues fire.
var steps := 0
var jumps := 0
var landings := 0
var grabs := 0

var _bank := SoundBank.new()
var _bank_ready := false
# No output device (CI, headless): the Dummy driver never mixes, so a started
# playback is never torn down and leaks at exit (observed: ten AudioStreamWAV
# / AudioStreamPlaybackWAV). Cues are still counted; nothing is started.
var _silent := false
var _bank_task := -1
var _voices: Array[AudioStreamPlayer] = []
var _next_voice := 0
var _wind: AudioStreamPlayer
var _rush: AudioStreamPlayer
var _hum: AudioStreamPlayer3D
var _stride := 0.0
var _was_grounded := true
var _last_vy := 0.0
var _last_traversal := 0
var _last_chute := false
var _last_deaths := -1


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	_silent = AudioServer.get_driver_name() == "Dummy"
	for bus in [BUS_EFFECTS, BUS_AMBIENCE, BUS_INTERFACE]:
		if AudioServer.get_bus_index(bus) < 0:
			AudioServer.add_bus()
			var index := AudioServer.bus_count - 1
			AudioServer.set_bus_name(index, bus)
			AudioServer.set_bus_send(index, &"Master")
	for i in EFFECT_VOICES:
		var voice := AudioStreamPlayer.new()
		voice.bus = BUS_EFFECTS
		add_child(voice)
		_voices.append(voice)
	_wind = _looping_player(BUS_AMBIENCE)
	_rush = _looping_player(BUS_AMBIENCE)
	_hum = AudioStreamPlayer3D.new()
	_hum.bus = BUS_AMBIENCE
	_hum.position = PLANT_POSITION
	_hum.unit_size = 6.0
	_hum.max_distance = 90.0
	_hum.volume_db = -8.0
	add_child(_hum)
	# ~0.3 s of GDScript synthesis: off the main thread, so the first frames
	# are not held up; cues before it finishes are counted but silent.
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


func _build_bank() -> void:
	_bank.build()
	_on_bank_ready.call_deferred()


func _on_bank_ready() -> void:
	_bank_ready = true
	print("SCRAPERX_AUDIO_BANK clips=%d msec=%d driver=%s" % [_bank.clips.size(), _bank.build_msec,
		AudioServer.get_driver_name()])
	if _silent:
		return
	_wind.stream = _bank.pick(&"wind_loop")
	_rush.stream = _bank.pick(&"rush_loop")
	_hum.stream = _bank.pick(&"hum_loop")
	_wind.volume_db = -60.0
	_rush.volume_db = -60.0
	_wind.play()
	_rush.play()
	_hum.play()


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
	voice.volume_db = -10.0
	add_child(voice)
	voice.finished.connect(voice.queue_free)
	voice.play()


# Once per rendered frame, with the native state main.gd already read.
func update(delta: float, position: Vector3, velocity: Vector3, grounded: bool,
		support_entity: int, traversal: int, chute: bool, deaths: int) -> void:
	var horizontal := Vector2(velocity.x, velocity.z).length()

	if grounded and traversal == 0 and horizontal > STEP_MIN_SPEED:
		_stride += horizontal * delta
		if _stride >= STRIDE_METERS:
			_stride -= STRIDE_METERS
			steps += 1
			_play(_surface(support_entity, position.y),
				linear_to_db(clampf(0.35 + horizontal / 8.0, 0.35, 1.0)) - 4.0,
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
		_play(&"jump", -6.0, randf_range(0.95, 1.08))

	if traversal != _last_traversal:
		if traversal == TRAVERSAL_HANGING:
			grabs += 1
			_play(&"grab", -3.0, randf_range(0.95, 1.05))
		elif traversal == TRAVERSAL_MANTLING or traversal == TRAVERSAL_VAULTING:
			_play(&"scrape", -5.0, randf_range(0.92, 1.08))
	if chute and not _last_chute:
		_play(&"chute", -2.0, 1.0)
	if _last_deaths >= 0 and deaths > _last_deaths:
		_play(&"impact_lethal", 0.0, 1.0)

	_was_grounded = grounded
	_last_vy = velocity.y
	_last_traversal = traversal
	_last_chute = chute
	_last_deaths = deaths
	_update_air(position, velocity, grounded, delta)


# Wind rises with altitude; a falling body hears the air tear past.
func _update_air(position: Vector3, velocity: Vector3, grounded: bool, delta: float) -> void:
	if not _bank_ready or _silent:
		return
	var altitude := clampf(position.y / 150.0, 0.0, 1.0)
	var wind_db := lerpf(-24.0, -9.0, altitude)
	_wind.volume_db = lerpf(_wind.volume_db, wind_db, 1.0 - exp(-2.0 * delta))
	var fall := 0.0 if grounded else clampf((-velocity.y - 6.0) / 24.0, 0.0, 1.0)
	var rush_db := lerpf(-60.0, -3.0, sqrt(fall)) if fall > 0.0 else -60.0
	_rush.volume_db = lerpf(_rush.volume_db, rush_db, 1.0 - exp(-6.0 * delta))
	_rush.pitch_scale = lerpf(0.9, 1.35, fall)


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


func _looping_player(bus: StringName) -> AudioStreamPlayer:
	var player := AudioStreamPlayer.new()
	player.bus = bus
	add_child(player)
	return player
