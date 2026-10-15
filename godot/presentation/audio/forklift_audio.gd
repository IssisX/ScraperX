extends Node3D

# Four finite, spatial presentation sources. Only native motion, commands,
# force/torque and work receipts change them. No contact prediction or
# gameplay state is owned here. Main gates the held horn and resets on pause.
const BUS := &"Effects"
const SAMPLE_RATE := 16000
static var _clips: Array[AudioStreamWAV] = []
var _drive: AudioStreamPlayer3D
var _hydraulics: AudioStreamPlayer3D
var _reverse: AudioStreamPlayer3D
var _horn: AudioStreamPlayer3D
var _previous_work := -1.0


func _ready() -> void:
	_ensure_sources()


func _ensure_sources() -> void:
	if is_instance_valid(_drive):
		return
	if _clips.is_empty():
		for kind in 4:
			_clips.append(_make_clip(kind))
	_drive = _voice("Drive", _clips[0], 32.0)
	_hydraulics = _voice("Hydraulics", _clips[1], 28.0)
	_reverse = _voice("ReverseWarning", _clips[2], 44.0)
	_horn = _voice("Horn", _clips[3], 60.0)


func _voice(label: String, clip: AudioStreamWAV, distance: float) -> AudioStreamPlayer3D:
	var voice := AudioStreamPlayer3D.new()
	voice.name = label
	voice.bus = BUS
	voice.stream = clip
	voice.attenuation_model = AudioStreamPlayer3D.ATTENUATION_INVERSE_DISTANCE
	voice.unit_size = 4.0
	voice.max_distance = distance
	voice.max_db = 0.0
	voice.volume_db = -80.0
	voice.max_polyphony = 1
	voice.attenuation_filter_cutoff_hz = 6500.0
	add_child(voice)
	# The chassis is below the standing listener; no eye-attached source.
	voice.position.y = .15
	return voice


func update_from_native(state: Dictionary, chassis_position: Vector3, horn_held: bool, delta: float = 0.0) -> void:
	_ensure_sources()
	if not bool(state.get("available", false)) or bool(state.get("paused", false)) or not chassis_position.is_finite():
		reset()
		return
	global_position = chassis_position
	var speed := _number(state, "forward_speed_mps")
	var drive := clampf(_number(state, "applied_drive"), -1.0, 1.0)
	var travel := clampf(absf(speed) / 3.5, 0.0, 1.0)
	var drive_effort := maxf(travel, absf(drive) * .65)
	_set_voice(_drive, absf(speed) > .04 or absf(drive) > .02,
		lerpf(-29.0, -18.0, drive_effort), lerpf(.72, 1.8, travel) + absf(drive) * .12)
	# Targets gate the pump; standing load held by a brake is silent. Actual
	# force/torque and positive-work change make a loaded actuator work harder.
	var lift := clampf(absf(_number(state, "lift_target_mps")) / .32, 0.0, 1.0)
	var tilt := clampf(absf(_number(state, "tilt_target_rad_s")) / .1, 0.0, 1.0)
	var hydraulic_rate := maxf(lift, tilt)
	var load := maxf(clampf(absf(_number(state, "lift_force_n")) / 18000.0, 0.0, 1.0) * lift,
		clampf(absf(_number(state, "tilt_torque_nm")) / 14000.0, 0.0, 1.0) * tilt)
	var work := maxf(_number(state, "positive_work_j"), 0.0)
	var power := 0.0
	if _previous_work >= 0.0 and is_finite(delta) and delta > .0001:
		power = clampf(maxf(work - _previous_work, 0.0) / delta / 6000.0, 0.0, 1.0)
	_previous_work = work
	var effort := maxf(load, power)
	_set_voice(_hydraulics, hydraulic_rate > .02,
		-30.0 + hydraulic_rate * 5.0 + effort * 7.0, .85 + hydraulic_rate * .3 + effort * .15)
	_set_voice(_reverse, speed < -.1 or drive < -.05, -17.0, 1.0)
	_set_voice(_horn, horn_held, -9.0, 1.0)


func reset() -> void:
	_previous_work = -1.0
	for voice in [_drive, _hydraulics, _reverse, _horn]:
		if is_instance_valid(voice):
			voice.stop()
			voice.volume_db = -80.0


func _exit_tree() -> void:
	reset()


static func _number(state: Dictionary, key: String) -> float:
	var value := float(state.get(key, 0.0))
	return value if is_finite(value) else 0.0


func _set_voice(voice: AudioStreamPlayer3D, active: bool, volume: float, pitch: float) -> void:
	if not active:
		voice.stop()
		voice.volume_db = -80.0
		return
	voice.volume_db = volume
	voice.pitch_scale = pitch
	if not voice.playing:
		voice.play()


# Tiny deterministic mono PCM resources, built once per script lifetime.
# Periods contain integer cycles, so sustained motor/horn loops have no
# discontinuous waveform seam. The warning has a baked pulse and silence;
# rendering cadence never sets its beep clock.
static func _make_clip(kind: int) -> AudioStreamWAV:
	var seconds := 1.0 if kind == 2 else .25
	var count := int(seconds * SAMPLE_RATE)
	var pcm := PackedByteArray()
	pcm.resize(count * 2)
	for i in count:
		var t := float(i) / SAMPLE_RATE
		var value := 0.0
		match kind:
			0:
				value = .44 * (sin(TAU * 220.0 * t) + .3 * sin(TAU * 440.0 * t) + .12 * sin(TAU * 880.0 * t))
			1:
				value = .4 * (sin(TAU * 168.0 * t) + .34 * sin(TAU * 504.0 * t) + .14 * sin(TAU * 1008.0 * t))
			2:
				if t < .32:
					var envelope := minf(clampf(t / .012, 0.0, 1.0), clampf((.32 - t) / .012, 0.0, 1.0))
					value = .52 * envelope * (sin(TAU * 920.0 * t) + .12 * sin(TAU * 1840.0 * t))
			3:
				value = .34 * (sin(TAU * 340.0 * t) + sin(TAU * 440.0 * t) + .18 * sin(TAU * 680.0 * t))
		pcm.encode_s16(i * 2, int(clampf(value, -.95, .95) * 32767.0))
	var clip := AudioStreamWAV.new()
	clip.format = AudioStreamWAV.FORMAT_16_BITS
	clip.mix_rate = SAMPLE_RATE
	clip.stereo = false
	clip.loop_mode = AudioStreamWAV.LOOP_FORWARD
	clip.loop_begin = 0
	clip.loop_end = count
	clip.data = pcm
	return clip
