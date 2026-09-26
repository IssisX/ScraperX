extends Node
## Recorded human reactions driven only by authoritative falling state.
## Ordinary jumps do not reach both the descent and speed thresholds.

const ALARM: Array[AudioStream] = [
	preload("res://assets/audio/fall/wtf_1.ogg"),
	preload("res://assets/audio/fall/wtf_2.ogg"),
	preload("res://assets/audio/fall/wtf_3.ogg"),
]
const PANIC: Array[AudioStream] = [
	preload("res://assets/audio/fall/fuck_1.ogg"),
	preload("res://assets/audio/fall/fuck_2.ogg"),
]
const COOLDOWN := 3.5
var reactions := 0
var last_clip := ""
var _voice: AudioStreamPlayer
var _silent := false
var _disabled := false
var _airborne := false
var _apex := 0.0
var _episode_cues := 0
var _cooldown := 0.0
var _escalation_gap := 0.0
var _remaining := 0.0
var _last_deaths := -1
var _last_alarm := -1
var _last_panic := -1


func _ready() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	_silent = AudioServer.get_driver_name() == "Dummy" and Engine.get_write_movie_path().is_empty()
	_voice = AudioStreamPlayer.new()
	_voice.bus = &"Effects"
	_voice.volume_db = -6.0
	add_child(_voice)


func _process(_delta: float) -> void:
	if get_tree().paused:
		# Retain the episode: reopening a menu cannot replay the first yell.
		stop_voice()


func stop_voice() -> void:
	_remaining = 0.0
	if is_instance_valid(_voice):
		_voice.stop()


func quiesce() -> void:
	_disabled = true
	stop_voice()


func _exit_tree() -> void:
	stop_voice()
	if is_instance_valid(_voice):
		_voice.stream = null


func speaking() -> bool:
	return _remaining > 0.0


func update(delta: float, position: Vector3, velocity: Vector3, grounded: bool,
		traversal: int, chute: bool, deaths: int) -> void:
	if _disabled or get_tree().paused:
		stop_voice()
		return
	_cooldown = maxf(0.0, _cooldown - delta)
	_escalation_gap = maxf(0.0, _escalation_gap - delta)
	_remaining = maxf(0.0, _remaining - delta)
	var restored := _last_deaths >= 0 and deaths != _last_deaths
	_last_deaths = deaths
	if grounded or traversal != 0 or chute or restored:
		stop_voice()
		_airborne = false
		_episode_cues = 0
		_apex = position.y
		return
	if not _airborne:
		_airborne = true
		_apex = position.y
	_apex = maxf(_apex, position.y)
	if speaking():
		return
	var descent := _apex - position.y
	if _episode_cues == 0 and _cooldown <= 0.0 and velocity.y <= -12.0 and descent >= 4.0:
		_last_alarm = _choose(ALARM, _last_alarm)
	elif _episode_cues == 1 and _escalation_gap <= 0.0 and velocity.y <= -24.0 and descent >= 35.0:
		_last_panic = _choose(PANIC, _last_panic)


func _choose(clips: Array[AudioStream], previous: int) -> int:
	# Uniform among the other takes; no immediate repeat in either pool.
	var index := randi_range(0, clips.size() - 2) if previous >= 0 else randi_range(0, clips.size() - 1)
	if previous >= 0 and index >= previous:
		index += 1
	var stream := clips[index]
	last_clip = stream.resource_path.get_file()
	reactions += 1
	_episode_cues += 1
	_cooldown = COOLDOWN
	_escalation_gap = 1.6
	_remaining = stream.get_length()
	if not _silent:
		_voice.stream = stream
		_voice.play()
	return index
