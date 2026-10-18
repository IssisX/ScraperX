extends RefCounted
## One bounded friction voice follows native slide/contact readback.

const FULL_SPEED_MPS := 8.0
const MIN_SPEED_MPS := 0.3
var active := false
var strength := 0.0
var surface := 0
var starts := 0
var _last_tick := -1
var _last_deaths := -1
var _last_count := -1
var _generic: AudioStreamWAV
var _metal: AudioStreamWAV


func configure(generic: AudioStreamWAV, metal: AudioStreamWAV) -> void:
	_generic = generic
	_metal = metal


func reset(voice: AudioStreamPlayer) -> void:
	_stop(voice)
	voice.stream = null
	_last_tick = -1
	_last_deaths = -1
	_last_count = -1


func update(voice: AudioStreamPlayer, state: Dictionary, grounded: bool,
		support: int, native_tick: int, deaths: int, audible: bool) -> void:
	var sliding: Variant = state.get("player_sliding")
	var speed: Variant = state.get("slide_speed_mps")
	var count: Variant = state.get("slide_count")
	if not sliding is bool or not (speed is float or speed is int) or not count is int:
		reset(voice)
		return
	var slide_speed := float(speed)
	var slide_count: int = count
	var is_sliding: bool = sliding
	if not is_finite(slide_speed) or slide_speed < 0.0 or slide_count < 0 or native_tick < 0 or deaths < 0:
		reset(voice)
		return
	var baseline := _last_tick < 0 or native_tick < _last_tick or deaths != _last_deaths or slide_count < _last_count
	var repeated := native_tick == _last_tick
	_last_tick = native_tick
	_last_deaths = deaths
	_last_count = slide_count
	# Lost contact, mute and lifecycle boundaries stop immediately, including
	# a duplicate render of the same native tick. No airborne release tail.
	if baseline or not audible or not grounded or support <= 0 or not is_sliding or slide_count == 0 or slide_speed <= MIN_SPEED_MPS:
		_stop(voice)
		return
	if repeated:
		return
	var contact: Variant = state.get("support_sound_surface", 0)
	var sound_surface: int = contact if contact is int else 0
	var metal := sound_surface == 2
	var stream := _metal if metal else _generic
	if stream == null:
		_stop(voice)
		return
	strength = clampf(slide_speed / FULL_SPEED_MPS, 0.0, 1.0)
	surface = 2 if metal else 0
	voice.volume_db = lerpf(-26.0, -13.0, strength)
	voice.pitch_scale = lerpf(0.85, 1.15, strength)
	if voice.stream != stream:
		voice.stop()
		voice.stream = stream
	if not voice.playing:
		voice.play()
		starts += 1
	active = true


func _stop(voice: AudioStreamPlayer) -> void:
	active = false
	strength = 0.0
	surface = 0
	voice.volume_db = -80.0
	if voice.playing:
		voice.stop()
