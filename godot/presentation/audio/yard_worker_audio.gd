extends Node3D

# Environmental welding, driven by the native activity and the posed electrode.
# A cached broadband arc loop has no musical notes or frame-clock retriggers.
const STEPS := [
	preload("res://assets/audio/footsteps/concrete_01.wav"),
	preload("res://assets/audio/footsteps/concrete_02.wav"),
	preload("res://assets/audio/footsteps/concrete_03.wav"),
	preload("res://assets/audio/footsteps/concrete_04.wav"),
]
static var _arc_clip: AudioStreamWAV
var _arc: AudioStreamPlayer3D
var _step: AudioStreamPlayer3D
var _episode := -1
var _serial := -1

func _ensure_sources() -> void:
	if _arc != null:
		return
	if _arc_clip == null:
		_arc_clip = _make_arc()
	_arc = _voice("WeldingArc", 16.0)
	_arc.stream = _arc_clip
	_arc.volume_db = -15.0
	_step = _voice("WorkerBoot", 10.0)
	_step.volume_db = -18.0

func _voice(label: String, reach: float) -> AudioStreamPlayer3D:
	var voice := AudioStreamPlayer3D.new()
	voice.name = label
	voice.bus = &"Effects"
	voice.unit_size = 2.5
	voice.max_distance = reach
	voice.max_polyphony = 1
	voice.attenuation_filter_cutoff_hz = 6000.0
	add_child(voice)
	return voice

func present(state: Dictionary, welding: bool, tip: Vector3, serial: int) -> void:
	_ensure_sources()
	var point: Vector3 = state.get("position", Vector3.ZERO)
	if not bool(state.get("present", false)) or bool(state.get("paused", false)) or not point.is_finite():
		reset()
		return
	var episode := int(state.get("episode", 0))
	if episode != _episode or serial < _serial:
		reset()
		_episode = episode
		_serial = serial
	var supported := bool(state.get("supported", false))
	if supported and int(state.get("activity", -1)) == 0 and welding and tip.is_finite():
		_arc.global_position = tip
		if not _arc.playing:
			_arc.play()
	else:
		_arc.stop()
	if supported and serial > _serial:
		_step.global_position = point + Vector3.DOWN * 0.86
		_step.stream = STEPS[serial % STEPS.size()]
		_step.play()
	elif not supported:
		_step.stop()
	_serial = serial

func reset() -> void:
	_episode = -1
	_serial = -1
	for voice in [_arc, _step]:
		if is_instance_valid(voice):
			voice.stop()

func _exit_tree() -> void:
	reset()

static func _make_arc() -> AudioStreamWAV:
	const RATE := 16000
	var rng := RandomNumberGenerator.new()
	rng.seed = 74819
	var data := PackedByteArray()
	data.resize(RATE * 2)
	var low := 0.0
	var pop := 0.0
	for index in RATE:
		var noise := rng.randf_range(-1.0, 1.0)
		low += (noise - low) * 0.14
		pop *= 0.76
		if rng.randf() < 0.012:
			pop = rng.randf_range(-0.8, 0.8)
		var ripple := 0.74 + 0.26 * absf(sin(TAU * 50.0 * float(index) / RATE))
		var value := ((noise - low) * 0.31 + low * 0.48 + pop) * ripple
		data.encode_s16(index * 2, int(clampf(value, -0.92, 0.92) * 32767.0))
	var clip := AudioStreamWAV.new()
	clip.format = AudioStreamWAV.FORMAT_16_BITS
	clip.mix_rate = RATE
	clip.data = data
	clip.loop_mode = AudioStreamWAV.LOOP_FORWARD
	clip.loop_end = RATE
	return clip
