extends Node3D

# Presentation only: native targets gate motors, N / N m and positive J deltas
# modulate load. A stationary brake, failure or disconnected hoist adds no clang.
const BUS := &"Effects"
const SAMPLE_RATE := 16000
# Native yard_crane_geometry bearing root; not attached to the camera.
const BEARING := Vector3(-60, 36, -150)
static var _clips: Array[AudioStreamWAV] = []
var _winch: AudioStreamPlayer3D
var _trolley: AudioStreamPlayer3D
var _slew: AudioStreamPlayer3D
var _previous_hoist := -1.0
var _previous_transport := -1.0

func _ready() -> void:
	_ensure_sources()

func _ensure_sources() -> void:
	if is_instance_valid(_winch):
		return
	if _clips.is_empty():
		for kind in 3:
			_clips.append(_make_clip(kind))
	_winch = _voice("Winch", _clips[0])
	_trolley = _voice("Trolley", _clips[1])
	_slew = _voice("Slew", _clips[2])

func _voice(label: String, clip: AudioStreamWAV) -> AudioStreamPlayer3D:
	var voice := AudioStreamPlayer3D.new()
	voice.name = label
	voice.bus = BUS
	voice.stream = clip
	voice.attenuation_model = AudioStreamPlayer3D.ATTENUATION_INVERSE_DISTANCE
	voice.unit_size = 6.0
	voice.max_distance = 100.0
	voice.max_polyphony = 1
	voice.max_db = 0.0
	voice.volume_db = -80.0
	voice.attenuation_filter_cutoff_hz = 6000.0
	add_child(voice)
	return voice

func update_from_native(state: Dictionary, delta: float = 0.0) -> void:
	_ensure_sources()
	if not bool(state.get("available", false)) or bool(state.get("paused", false)):
		reset()
		return
	var sheave := BEARING
	var lines: Array = state.get("lines", [])
	if not lines.is_empty() and lines[0] is Dictionary:
		var point: Variant = lines[0].get("from", BEARING)
		if point is Vector3 and point.is_finite():
			sheave = point
	_winch.global_position = sheave
	_trolley.global_position = sheave
	_slew.global_position = BEARING
	var hoist_work := maxf(_number(state, "hoist_work_j"), 0.0)
	var transport_work := maxf(_number(state, "transport_work_j"), 0.0)
	var hoist_power := _power(hoist_work, _previous_hoist, delta, 30000.0)
	var transport_power := _power(transport_work, _previous_transport, delta, 60000.0)
	_previous_hoist = hoist_work
	_previous_transport = transport_work
	var hoist := clampf(absf(_number(state, "hoist_target_mps")), 0.0, 1.0)
	var trolley := clampf(absf(_number(state, "trolley_target_mps")), 0.0, 1.0)
	var slew := clampf(absf(_number(state, "slew_target_rad_s")) / .12, 0.0, 1.0)
	_set_voice(_winch, bool(state.get("hoist_connected", false)) and hoist > .01, hoist,
		maxf(hoist_power, clampf(absf(_number(state, "hoist_force_n")) / 100000.0, 0.0, 1.0)))
	_set_voice(_trolley, trolley > .01, trolley,
		maxf(transport_power, clampf(absf(_number(state, "trolley_force_n")) / 180000.0, 0.0, 1.0)))
	_set_voice(_slew, slew > .01, slew,
		maxf(transport_power, clampf(absf(_number(state, "slew_torque_nm")) / 1800000.0, 0.0, 1.0)))

func reset() -> void:
	_previous_hoist = -1.0
	_previous_transport = -1.0
	for voice in [_winch, _trolley, _slew]:
		if is_instance_valid(voice):
			voice.stop()
			voice.volume_db = -80.0

func _exit_tree() -> void:
	reset()

static func _number(state: Dictionary, key: String) -> float:
	var value := float(state.get(key, 0.0))
	return value if is_finite(value) else 0.0

static func _power(work: float, previous: float, delta: float, watts: float) -> float:
	return clampf(maxf(work - previous, 0.0) / delta / watts, 0.0, 1.0) if previous >= 0.0 and is_finite(delta) and delta > .0001 else 0.0

func _set_voice(voice: AudioStreamPlayer3D, active: bool, rate: float, effort: float) -> void:
	if not active:
		voice.stop()
		voice.volume_db = -80.0
		return
	voice.volume_db = -31.0 + rate * 5.0 + effort * 8.0
	voice.pitch_scale = .75 + rate * .4 + effort * .12
	if not voice.playing:
		voice.play()

# Three deterministic .25 s mono PCM loops cached for the script lifetime.
# Integer cycles keep seams continuous; no synthesis or resource churn per frame.
static func _make_clip(kind: int) -> AudioStreamWAV:
	var count := SAMPLE_RATE / 4
	var pcm := PackedByteArray()
	pcm.resize(count * 2)
	var hz := [156.0, 244.0, 96.0][kind] as float
	for i in count:
		var t := float(i) / SAMPLE_RATE
		var value := .42 * (sin(TAU * hz * t) + .3 * sin(TAU * hz * 3.0 * t) + .12 * sin(TAU * hz * 7.0 * t))
		pcm.encode_s16(i * 2, int(value * 32767.0))
	var clip := AudioStreamWAV.new()
	clip.format = AudioStreamWAV.FORMAT_16_BITS
	clip.mix_rate = SAMPLE_RATE
	clip.loop_mode = AudioStreamWAV.LOOP_FORWARD
	clip.loop_end = count
	clip.data = pcm
	return clip
