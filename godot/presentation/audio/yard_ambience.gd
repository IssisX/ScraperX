extends Node3D
# Authored environmental air, not operating-machine or contact feedback.
# The open repair shed shelters a soft roof-level wash; the salvage corner
# has brighter wind through stored material. Native machinery stays owned
# by AudioDirector. Two mono spatial voices share its user-controlled bus.

const BUS := &"Ambience"
const RATE := 16000
const LOOP_SECONDS := 6.0
const FADE_SECONDS := 0.5
const MAX_DISTANCE := 65.0
const GROUND_FADE_START := 12.0
const GROUND_CEILING := 32.0

var _positions := [Vector3(-36, 2.4, -30), Vector3(36, 1.6, -56)]
var _voices: Array[AudioStreamPlayer3D] = []
var _streams: Array[AudioStreamWAV] = []
var _task := -1
var _built := false
var _silent := false
var _stopped := false
var _gains := [0.0, 0.0]


func configure(shed_position: Vector3, salvage_position: Vector3) -> void:
	_positions = [shed_position, salvage_position]
	for i in _voices.size():
		_voices[i].global_position = _positions[i]


func _ready() -> void:
	_silent = AudioServer.get_driver_name() == "Dummy" and Engine.get_write_movie_path().is_empty()
	for i in 2:
		var voice := AudioStreamPlayer3D.new()
		voice.name = "ShelteredAir" if i == 0 else "SalvageAir"
		voice.bus = BUS
		voice.unit_size = 14.0
		voice.max_distance = MAX_DISTANCE
		voice.attenuation_filter_cutoff_hz = 4200.0
		voice.volume_db = -80.0
		add_child(voice)
		voice.global_position = _positions[i]
		_voices.append(voice)
	# Generate only these two small loops, once, away from the render thread.
	_task = WorkerThreadPool.add_task(_build)


func _build() -> void:
	_streams.append(_air_loop(0))
	_streams.append(_air_loop(1))
	_finish_build.call_deferred()


func _finish_build() -> void:
	if not is_inside_tree():
		return
	for i in _voices.size():
		_voices[i].stream = _streams[i]
	_built = true


# Called with the same player position already read by Main. No simulation
# receipts, random gameplay state, timers, or additional process loop.
func update(delta: float, listener_position: Vector3) -> void:
	if not _built or _stopped:
		return
	var audible := not _silent
	for bus in [&"Master", BUS]:
		var index := AudioServer.get_bus_index(bus)
		if index < 0 or AudioServer.is_bus_mute(index):
			audible = false
	var height_gain := 1.0 - smoothstep(GROUND_FADE_START, GROUND_CEILING, listener_position.y)
	for i in _voices.size():
		var voice := _voices[i]
		var distance := listener_position.distance_to(voice.global_position)
		if not audible or height_gain <= 0.0 or distance >= MAX_DISTANCE:
			voice.stop()
			voice.volume_db = -80.0
			_gains[i] = 0.0
			continue
		# Fade at the cull boundary before dropping the playback allocation.
		var target := height_gain * (1.0 - smoothstep(45.0, MAX_DISTANCE, distance))
		_gains[i] = move_toward(_gains[i], target, maxf(delta, 0.0) * 1.5)
		voice.volume_db = linear_to_db(maxf(_gains[i], 0.0001)) + (-15.0 if i == 0 else -17.0)
		if not voice.playing:
			voice.play()


func quiesce() -> void:
	_stopped = true
	for voice in _voices:
		voice.stop()


func _exit_tree() -> void:
	quiesce()
	if _task >= 0:
		WorkerThreadPool.wait_for_task_completion(_task)
		_task = -1
	for voice in _voices:
		voice.stream = null
	_streams.clear()


func _air_loop(kind: int) -> AudioStreamWAV:
	var rng := RandomNumberGenerator.new()
	rng.seed = 0x59415244 + kind
	var body := int(LOOP_SECONDS * RATE)
	var fade := int(FADE_SECONDS * RATE)
	var samples := PackedFloat32Array()
	samples.resize(body + fade)
	var fast := 0.0
	var slow := 0.0
	var soft := 0.0
	for i in samples.size():
		var t := float(i) / RATE
		var noise := rng.randf_range(-1.0, 1.0)
		fast += (0.38 if kind == 0 else 0.78) * (noise - fast)
		slow += (0.035 if kind == 0 else 0.12) * (noise - slow)
		soft += 0.015 * (noise - soft)
		# A broad breath under the roof versus dry, fluttering high air at
		# the stored salvage. No motor tone, rhythm, impacts or false voices.
		var breath := 0.72 + 0.18 * sin(TAU * t / 3.7) + 0.10 * sin(TAU * t / 1.9)
		var flutter := 1.0 if kind == 0 else 0.78 + 0.22 * sin(TAU * t * 7.3 + sin(t * 2.1))
		samples[i] = ((fast - slow) * flutter + soft * 0.4) * breath
	# Crossfade the continuation into the head. The wrap then joins adjacent
	# original samples instead of jumping between independent noise values.
	for i in fade:
		var blend := float(i) / fade
		samples[i] = lerpf(samples[body + i], samples[i], blend)
	samples.resize(body)
	var energy := 0.0
	var peak := 0.0
	for value in samples:
		energy += value * value
		peak = maxf(peak, absf(value))
	var gain := minf(0.18 / maxf(sqrt(energy / body), 0.00001), 0.85 / maxf(peak, 0.00001))
	var data := PackedByteArray()
	data.resize(body * 2)
	for i in body:
		data.encode_s16(i * 2, int(samples[i] * gain * 32767.0))
	# Godot 4.7 includes loop_end in decoding, then resumes at sample 1.
	data.resize(data.size() + 2)
	data.encode_s16(body * 2, data.decode_s16(0))
	var stream := AudioStreamWAV.new()
	stream.resource_name = "yard_sheltered_air" if kind == 0 else "yard_salvage_air"
	stream.format = AudioStreamWAV.FORMAT_16_BITS
	stream.mix_rate = RATE
	stream.data = data
	stream.loop_mode = AudioStreamWAV.LOOP_FORWARD
	stream.loop_end = body
	return stream
