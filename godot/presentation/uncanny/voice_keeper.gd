extends Node
## Your voice (HER_CHART.md, build step 3; the owner approved RECORD_AUDIO on
## 2026-10-20). Only with the player's consent (ui/consent_screen.gd, the
## MICROPHONE setting) and the phone's permission is the microphone heard,
## through Godot's own input: an AudioStreamMicrophone on a muted bus, read
## back by an AudioEffectCapture. Clips are cut from it by loudness alone --
## a gate over a tracked noise floor, plain signal processing, no model --
## and kept in user:// as WAV: at most MAX_CLIPS, MAX_BYTES in all, none older
## than the profile's clip_max_age_s. Nothing leaves the phone. Turning the
## setting off stops the microphone and deletes every clip.
##
## Echoes: in a quiet stretch, a clip at least the profile's echo_min_age_s
## old (15 minutes in the game) plays once, faint, through a pipe's filter
## (band-limited, a short metallic ring), from structure a few meters from
## the player and out of their view. Never captioned, never explained.
##
## With no consent, the permission refused, or no input device (a headless
## run: the Dummy driver), nothing is heard, kept or played, and nothing
## fails. With no mixing device the echo is still decided and recorded, so the
## headless tests can follow it, but nothing is started.

signal clip_kept(record: Dictionary)
signal echo_played(record: Dictionary)
# Whether the player stopped to listen (stood still STOP_HOLD_S within
# REACTION_S of an echo).
signal echo_reaction(stopped: bool)
signal denied

const PERMISSION := "android.permission.RECORD_AUDIO"
const BUS_MIC := &"HerMic"
const BUS_ECHO := &"HerEcho"
const STATE_OFF := &"off"
const STATE_ASKING := &"asking"
const STATE_DENIED := &"denied"
const STATE_UNAVAILABLE := &"unavailable"
const STATE_LISTENING := &"listening"
# The cut: 20 ms windows at about 16 kHz (the mix rate divided down by
# whole numbers, each kept sample the mean of those it stands for).
const KEEP_RATE_TARGET := 16000.0
const WINDOW_S := 0.02
const GATE_FLOOR_DB := -46.0       # the gate never opens below this
const GATE_OVER_FLOOR_DB := 14.0   # it opens this far over the noise floor
const GATE_CLOSE_DB := 6.0         # and closes this far under where it opened
const GATE_HANG_S := 0.45          # once quiet that long
const FLOOR_RISE_DB_S := 1.5       # the floor creeps up under steady sound
const PRE_ROLL_S := 0.2
const TAIL_S := 0.1
const CLIP_MIN_S := 0.5
const CLIP_MAX_S := 6.0
# Voiced: a 40 ms window (at 4 kHz) whose normalized autocorrelation peaks
# at a pitch period of 80-400 Hz this strongly. A clip's share of voiced
# windows among its loud ones says whether it is a voice.
const VOICE_RATE := 4000.0
const VOICE_WINDOW := 160
const VOICE_PEAK := 0.5
const VOICED_MIN := 0.35
# Kept.
const MAX_CLIPS := 24
const MAX_BYTES := 3 * 1024 * 1024
const INDEX := "index.json"
# The echo: its loudest moment at ECHO_PEAK_DB at a unit's distance, and the
# source ECHO_SOURCE_MIN_M to ECHO_SOURCE_MAX_M off, never ahead in the view.
const ECHO_PEAK_DB := -14.0
const ECHO_UNIT_SIZE := 1.5
const ECHO_MAX_DISTANCE := 14.0
const ECHO_SOURCE_MIN_M := 2.0
const ECHO_SOURCE_MAX_M := 8.0
const ECHO_AHEAD_DOT := 0.3
const REACTION_S := 6.0
const STOP_SPEED := 0.3
const STOP_HOLD_S := 1.5

var profile: RefCounted
var state := STATE_OFF
# Tests: "grant" or "deny" stands for the phone's answer.
var permission_override := ""
# Seconds added to the phone's clock (tests fast-forward time).
var time_offset_s := 0.0
var dir := ""
var index: Array = []         # {file, at, seconds, bytes, peak_db, voiced, echoed}
var echoes_played := 0
var last_echo := {}
var clips_cut := 0
var _wanted := false
var _silent := false
var _mic_player: AudioStreamPlayer
var _capture: AudioEffectCapture
var _echo_player: AudioStreamPlayer3D
var _last_echo_clock := -INF
var _quiet_for := 0.0
var _watch := {}              # the reaction to the last echo, while it is watched
var _pending: Array = []      # worker tasks cutting clips
# The gate.
var _decim := 3
var _keep_rate := 16000.0
var _feed_rate := 0.0
var _acc := 0.0
var _acc_n := 0
var _window := PackedFloat32Array()
var _window_len := 320
var _pre := PackedFloat32Array()
var _clip := PackedFloat32Array()
var _open := false
var _open_db := 0.0
var _floor_db := -50.0
var _below_s := 0.0


func setup(the_profile: RefCounted, storage_dir: String) -> void:
	name = "VoiceKeeper"
	profile = the_profile
	dir = storage_dir.path_join("voice")
	_silent = AudioServer.get_driver_name() == "Dummy" and Engine.get_write_movie_path().is_empty()
	_make_buses()
	_echo_player = AudioStreamPlayer3D.new()
	_echo_player.name = "Echo"
	_echo_player.bus = BUS_ECHO
	_echo_player.unit_size = ECHO_UNIT_SIZE
	_echo_player.max_distance = ECHO_MAX_DISTANCE
	_echo_player.attenuation_model = AudioStreamPlayer3D.ATTENUATION_INVERSE_DISTANCE
	add_child(_echo_player)
	_load_index()
	_enforce_caps()


func _exit_tree() -> void:
	for task in _pending:
		WorkerThreadPool.wait_for_task_completion(task)
	_pending.clear()
	_stop_capture()
	if _echo_player != null:
		_echo_player.stop()
		_echo_player.stream = null


# The microphone's bus (muted: heard by the capture, never by the speaker)
# and the echo's pipe (into Effects).
func _make_buses() -> void:
	if AudioServer.get_bus_index(BUS_MIC) < 0:
		AudioServer.add_bus()
		var mic := AudioServer.bus_count - 1
		AudioServer.set_bus_name(mic, BUS_MIC)
		AudioServer.set_bus_send(mic, &"Master")
		AudioServer.set_bus_mute(mic, true)
		var capture := AudioEffectCapture.new()
		capture.buffer_length = 1.0
		AudioServer.add_bus_effect(mic, capture)
	var mic_index := AudioServer.get_bus_index(BUS_MIC)
	for i in AudioServer.get_bus_effect_count(mic_index):
		if AudioServer.get_bus_effect(mic_index, i) is AudioEffectCapture:
			_capture = AudioServer.get_bus_effect(mic_index, i)
	if AudioServer.get_bus_index(BUS_ECHO) < 0:
		AudioServer.add_bus()
		var echo := AudioServer.bus_count - 1
		AudioServer.set_bus_name(echo, BUS_ECHO)
		AudioServer.set_bus_send(echo, &"Effects" if AudioServer.get_bus_index(&"Effects") >= 0 else &"Master")
		var low_cut := AudioEffectHighPassFilter.new()
		low_cut.cutoff_hz = 350.0
		AudioServer.add_bus_effect(echo, low_cut)
		var high_cut := AudioEffectLowPassFilter.new()
		high_cut.cutoff_hz = 2400.0
		AudioServer.add_bus_effect(echo, high_cut)
		# A pipe's ring: a short feedback delay, a comb at its length.
		var ring := AudioEffectDelay.new()
		ring.dry = 1.0
		ring.tap1_active = true
		ring.tap1_delay_ms = 7.0
		ring.tap1_level_db = -8.0
		ring.tap2_active = false
		ring.feedback_active = true
		ring.feedback_delay_ms = 7.0
		ring.feedback_level_db = -7.0
		ring.feedback_lowpass = 3000.0
		AudioServer.add_bus_effect(echo, ring)
		var room := AudioEffectReverb.new()
		room.room_size = 0.3
		room.damping = 0.55
		room.spread = 0.4
		room.wet = 0.3
		room.dry = 0.85
		AudioServer.add_bus_effect(echo, room)


func now_unix() -> float:
	return Time.get_unix_time_from_system() + time_offset_s


# --- consent and the phone's permission -----------------------------------------


# The MICROPHONE setting. On asks the phone (once) and listens when it is
# allowed; off stops listening and deletes every clip kept.
func set_wanted(on: bool) -> void:
	_wanted = on
	if not on:
		_stop_capture()
		delete_all()
		_set_state(STATE_OFF)
		return
	if state in [STATE_LISTENING, STATE_UNAVAILABLE, STATE_ASKING]:
		return
	if permission_override == "deny":
		_on_permission(PERMISSION, false)
		return
	if permission_override == "grant" or _granted():
		_start_capture()
		return
	_set_state(STATE_ASKING)
	var tree := get_tree()
	if tree != null and not tree.on_request_permissions_result.is_connected(_on_permission):
		tree.on_request_permissions_result.connect(_on_permission)
	# True when already granted; otherwise the phone asks the player and
	# answers through on_request_permissions_result.
	if OS.request_permission(PERMISSION):
		_on_permission(PERMISSION, true)


func _granted() -> bool:
	if OS.get_name() != "Android":
		return true
	return PERMISSION in OS.get_granted_permissions()


func _on_permission(permission: String, granted: bool) -> void:
	if not permission.ends_with("RECORD_AUDIO"):
		return
	if granted and _wanted:
		_start_capture()
		return
	if granted:
		return
	_wanted = false
	_stop_capture()
	delete_all()
	_set_state(STATE_DENIED)
	denied.emit()


func _set_state(next: StringName) -> void:
	state = next


func _start_capture() -> void:
	# No input device here (a headless run's Dummy driver), or the project's
	# input off: nothing to hear, and nothing fails.
	if AudioServer.get_driver_name() == "Dummy" or not bool(ProjectSettings.get_setting("audio/driver/enable_input", false)):
		_set_state(STATE_UNAVAILABLE)
		return
	if _capture == null:
		_set_state(STATE_UNAVAILABLE)
		return
	if _mic_player == null:
		_mic_player = AudioStreamPlayer.new()
		_mic_player.name = "Microphone"
		_mic_player.stream = AudioStreamMicrophone.new()
		_mic_player.bus = BUS_MIC
		add_child(_mic_player)
	_capture.clear_buffer()
	_mic_player.play()
	_set_state(STATE_LISTENING)


func _stop_capture() -> void:
	if _mic_player != null:
		_mic_player.stop()
		_mic_player.queue_free()
		_mic_player = null
	if _capture != null:
		_capture.clear_buffer()
	_open = false
	_clip.clear()
	_pre.clear()
	_window.clear()


func listening() -> bool:
	return state == STATE_LISTENING and _mic_player != null and _mic_player.playing


# Only while playing: the game paused (a menu, the title) or sent to the
# background lets the microphone go, and playing again takes it back.
func _notification(what: int) -> void:
	match what:
		NOTIFICATION_PAUSED, NOTIFICATION_APPLICATION_PAUSED:
			if _mic_player != null and _mic_player.playing:
				_mic_player.stop()
				_capture.clear_buffer()
		NOTIFICATION_UNPAUSED:
			if state == STATE_LISTENING and _mic_player != null and not _mic_player.playing:
				_capture.clear_buffer()
				_mic_player.play()


# --- the cut ---------------------------------------------------------------------


# One frame: what the microphone heard since the last, through the gate.
func _process(_delta: float) -> void:
	if state != STATE_LISTENING or _capture == null:
		return
	var available := _capture.get_frames_available()
	if available <= 0:
		return
	var frames := _capture.get_buffer(available)
	var mono := PackedFloat32Array()
	mono.resize(frames.size())
	for i in frames.size():
		mono[i] = (frames[i].x + frames[i].y) * 0.5
	feed_samples(mono, AudioServer.get_mix_rate())


# Samples at `rate` through the loudness gate, as the microphone gives them
# (and as the tests feed a synthetic voice).
func feed_samples(mono: PackedFloat32Array, rate: float) -> void:
	if rate != _feed_rate:
		_feed_rate = rate
		_decim = maxi(1, roundi(rate / KEEP_RATE_TARGET))
		_keep_rate = rate / float(_decim)
		_window_len = maxi(1, roundi(_keep_rate * WINDOW_S))
		_acc = 0.0
		_acc_n = 0
	for x in mono:
		_acc += x
		_acc_n += 1
		if _acc_n < _decim:
			continue
		_window.append(_acc / float(_decim))
		_acc = 0.0
		_acc_n = 0
		if _window.size() >= _window_len:
			_window_done()


func _window_done() -> void:
	var energy := 0.0
	for x in _window:
		energy += x * x
	var level := linear_to_db(maxf(sqrt(energy / float(_window.size())), 1.0e-7))
	if not _open:
		_floor_db = level if level < _floor_db else minf(level, _floor_db + FLOOR_RISE_DB_S * WINDOW_S)
		var open_at := maxf(GATE_FLOOR_DB, _floor_db + GATE_OVER_FLOOR_DB)
		if level >= open_at:
			_open = true
			_open_db = open_at
			_below_s = 0.0
			_clip = _pre.duplicate()
			_clip.append_array(_window)
		else:
			_pre.append_array(_window)
			var keep := roundi(PRE_ROLL_S * _keep_rate)
			if _pre.size() > keep:
				_pre = _pre.slice(_pre.size() - keep)
	else:
		_clip.append_array(_window)
		_below_s = _below_s + WINDOW_S if level < _open_db - GATE_CLOSE_DB else 0.0
		if _below_s >= GATE_HANG_S or float(_clip.size()) / _keep_rate >= CLIP_MAX_S:
			_close_clip()
	_window.clear()


func _close_clip() -> void:
	_open = false
	var trailing := roundi(maxf(0.0, _below_s - TAIL_S) * _keep_rate)
	var samples := _clip.slice(0, maxi(0, _clip.size() - trailing))
	_clip = PackedFloat32Array()
	_pre.clear()
	_below_s = 0.0
	if float(samples.size()) / _keep_rate < CLIP_MIN_S:
		return
	clips_cut += 1
	var rate := _keep_rate
	var at := now_unix()
	# The voicing and the WAV are worked out off the main thread; the clip is
	# kept when they are done.
	var task := WorkerThreadPool.add_task(_prepare_clip.bind(samples, rate, at))
	_pending.append(task)


# On a worker: the clip's peak, its share of voiced windows, and its 16-bit
# PCM. Touches nothing but its arguments.
func _prepare_clip(samples: PackedFloat32Array, rate: float, at: float) -> void:
	var peak := 0.0
	for x in samples:
		peak = maxf(peak, absf(x))
	var voiced := voiced_share(samples, rate)
	var pcm := PackedByteArray()
	pcm.resize(samples.size() * 2)
	for i in samples.size():
		pcm.encode_s16(i * 2, clampi(roundi(samples[i] * 32767.0), -32768, 32767))
	_keep_clip.call_deferred(pcm, rate, at, linear_to_db(maxf(peak, 1.0e-7)), voiced,
		float(samples.size()) / rate)


# The share of a clip's loud windows that are voiced (a pitch period found by
# plain autocorrelation): a voice's vowels are, a knock or the wind is not.
static func voiced_share(samples: PackedFloat32Array, rate: float) -> float:
	var step := maxi(1, roundi(rate / VOICE_RATE))
	var low := PackedFloat32Array()
	var acc := 0.0
	var n := 0
	for x in samples:
		acc += x
		n += 1
		if n == step:
			low.append(acc / float(step))
			acc = 0.0
			n = 0
	var low_rate := rate / float(step)
	var min_lag := maxi(2, floori(low_rate / 400.0))
	var max_lag := mini(VOICE_WINDOW - 8, ceili(low_rate / 80.0))
	var peak := 0.0
	for x in low:
		peak = maxf(peak, absf(x))
	var loud_floor := peak * 0.1
	var loud := 0
	var voiced := 0
	var w := 0
	while w + VOICE_WINDOW <= low.size():
		var energy := 0.0
		for i in VOICE_WINDOW:
			energy += low[w + i] * low[w + i]
		if sqrt(energy / float(VOICE_WINDOW)) >= loud_floor and energy > 0.0:
			loud += 1
			var best := 0.0
			for lag in range(min_lag, max_lag + 1):
				var sum := 0.0
				var e1 := 0.0
				var e2 := 0.0
				for i in VOICE_WINDOW - lag:
					var a := low[w + i]
					var b := low[w + i + lag]
					sum += a * b
					e1 += a * a
					e2 += b * b
				if e1 > 0.0 and e2 > 0.0:
					best = maxf(best, sum / sqrt(e1 * e2))
			if best >= VOICE_PEAK:
				voiced += 1
		w += VOICE_WINDOW
	return float(voiced) / float(loud) if loud > 0 else 0.0


func _keep_clip(pcm: PackedByteArray, rate: float, at: float, peak_db: float, voiced: float, seconds: float) -> void:
	_pending = _pending.filter(func(task: int) -> bool: return not WorkerThreadPool.is_task_completed(task))
	# Turned off (or refused) while it was being worked out: not kept.
	if not _wanted:
		return
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(dir))
	var file := "clip_%d.wav" % int(at * 1000.0)
	var wav := AudioStreamWAV.new()
	wav.format = AudioStreamWAV.FORMAT_16_BITS
	wav.mix_rate = roundi(rate)
	wav.stereo = false
	wav.data = pcm
	if wav.save_to_wav(dir.path_join(file)) != OK:
		return
	var record := {"file": file, "at": at, "seconds": seconds, "bytes": pcm.size() + 44, "peak_db": peak_db,
		"voiced": voiced, "echoed": false}
	index.append(record)
	_enforce_caps()
	_save_index()
	clip_kept.emit(record)


# --- kept -------------------------------------------------------------------------


func _enforce_caps() -> void:
	var now := now_unix()
	var keep: Array = []
	for record in index:
		if now - float(record["at"]) > float(profile.clip_max_age_s) or \
				not FileAccess.file_exists(dir.path_join(String(record["file"]))):
			_remove_file(String(record["file"]))
		else:
			keep.append(record)
	keep.sort_custom(func(a: Dictionary, b: Dictionary) -> bool: return float(a["at"]) < float(b["at"]))
	var total := 0
	for record in keep:
		total += int(record["bytes"])
	while not keep.is_empty() and (keep.size() > MAX_CLIPS or total > MAX_BYTES):
		var oldest: Dictionary = keep.pop_front()
		total -= int(oldest["bytes"])
		_remove_file(String(oldest["file"]))
	index = keep


func bytes_kept() -> int:
	var total := 0
	for record in index:
		total += int(record["bytes"])
	return total


# Every clip and the index, gone (the setting turned off, or refused).
func delete_all() -> void:
	for record in index:
		_remove_file(String(record["file"]))
	index.clear()
	var listing := DirAccess.open(ProjectSettings.globalize_path(dir))
	if listing != null:
		for file in listing.get_files():
			if file.begins_with("clip_") and file.ends_with(".wav"):
				listing.remove(file)
		if listing.file_exists(INDEX):
			listing.remove(INDEX)


func _remove_file(file: String) -> void:
	var path := ProjectSettings.globalize_path(dir.path_join(file))
	if FileAccess.file_exists(path):
		DirAccess.remove_absolute(path)


func _load_index() -> void:
	index.clear()
	var path := dir.path_join(INDEX)
	if not FileAccess.file_exists(path):
		return
	var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(path))
	if not parsed is Array:
		return
	for record in parsed:
		if record is Dictionary and record.get("file") is String and (record.get("at") is float or record.get("at") is int) \
				and String(record["file"]).begins_with("clip_") and not String(record["file"]).contains("/"):
			index.append({"file": String(record["file"]), "at": float(record["at"]),
				"seconds": float(record.get("seconds", 0.0)), "bytes": int(record.get("bytes", 0)),
				"peak_db": float(record.get("peak_db", -20.0)), "voiced": float(record.get("voiced", 0.0)),
				"echoed": bool(record.get("echoed", false))})


func _save_index() -> void:
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(dir))
	var file := FileAccess.open(dir.path_join(INDEX), FileAccess.WRITE)
	if file != null:
		file.store_string(JSON.stringify(index))


# --- echoes -----------------------------------------------------------------------


# Once a frame while playing: `clock` is the uncanny clock (seconds of play),
# `quiet` whether nothing is going on, `listener` the player, `speed` how fast
# they move over what they stand on.
func update(delta: float, clock: float, quiet: bool, native: Object, listener: Vector3, forward: Vector3,
		speed: float) -> void:
	_quiet_for = _quiet_for + delta if quiet else 0.0
	if not _watch.is_empty():
		_watch["left"] = float(_watch["left"]) - delta
		if speed < STOP_SPEED:
			_watch["still"] = float(_watch["still"]) + delta
		if float(_watch["left"]) <= 0.0 or float(_watch["still"]) >= STOP_HOLD_S:
			var stopped := float(_watch["still"]) >= STOP_HOLD_S
			last_echo["stopped"] = stopped
			_watch = {}
			echo_reaction.emit(stopped)
	if not _watch.is_empty() or native == null:
		return
	# Late: only a clip at least echo_min_age_s old (_echo_candidate); rare:
	# echo_gap_s of play between one echo and the next.
	if clock - _last_echo_clock < float(profile.echo_gap_s):
		return
	if _quiet_for < float(profile.quiet_s):
		return
	var record := _echo_candidate()
	if record.is_empty():
		return
	var source := echo_source(native, listener, forward)
	if source.is_empty():
		return
	var stream := AudioStreamWAV.load_from_file(dir.path_join(String(record["file"])))
	if stream == null:
		_remove_file(String(record["file"]))
		index.erase(record)
		_save_index()
		return
	_echo_player.stream = stream
	_echo_player.global_position = source["point"]
	# Its loudest moment at ECHO_PEAK_DB, however loud it was said.
	_echo_player.volume_db = ECHO_PEAK_DB - float(record["peak_db"])
	if not _silent:
		_echo_player.play()
	record["echoed"] = true
	_save_index()
	echoes_played += 1
	_last_echo_clock = clock
	last_echo = {"file": record["file"], "clock": clock, "age_s": now_unix() - float(record["at"]),
		"point": source["point"], "distance": float(source["distance"]), "volume_db": _echo_player.volume_db,
		"peak_db": ECHO_PEAK_DB, "bus": String(_echo_player.bus), "stopped": null, "listener": listener}
	_watch = {"left": REACTION_S, "still": 0.0}
	echo_played.emit(last_echo)


# The oldest clip old enough, voiced and not yet echoed.
func _echo_candidate() -> Dictionary:
	var now := now_unix()
	var best := {}
	for record in index:
		if bool(record["echoed"]) or float(record["voiced"]) < VOICED_MIN:
			continue
		if now - float(record["at"]) < float(profile.echo_min_age_s):
			continue
		if best.is_empty() or float(record["at"]) < float(best["at"]):
			best = record
	return best


# Where the echo comes from: structure ECHO_SOURCE_MIN_M to ECHO_SOURCE_MAX_M
# off, never ahead in the view (a pipe, a wall, the steel); else the floor a
# few meters behind. {point, distance}, empty when there is neither.
static func echo_source(native: Object, listener: Vector3, forward: Vector3) -> Dictionary:
	var flat := Vector2(forward.x, forward.z)
	flat = flat.normalized() if flat.length() > 0.01 else Vector2(0.0, -1.0)
	var from := listener + Vector3(0.0, 0.4, 0.0)
	var best := {}
	for k in 12:
		var a := TAU * float(k) / 12.0
		var d := Vector3(sin(a), 0.0, cos(a))
		if Vector2(d.x, d.z).dot(flat) > ECHO_AHEAD_DOT:
			continue
		var hit: Dictionary = native.get_look_target(from, d, ECHO_SOURCE_MAX_M)
		if not bool(hit.get("hit", false)) or float(hit["distance"]) < ECHO_SOURCE_MIN_M:
			continue
		var entity := int(hit["entity"])
		if not (entity in [1, 11, 51, 3201] or (entity >= 1000 and entity < 2000)):
			continue
		if best.is_empty() or float(hit["distance"]) < float(best["distance"]):
			best = {"point": (hit["point"] as Vector3) + (hit["normal"] as Vector3) * 0.1,
				"distance": float(hit["distance"])}
	if not best.is_empty():
		return best
	var behind := listener - Vector3(flat.x, 0.0, flat.y) * 3.0
	var floor := float(native.get_surface_below(behind + Vector3(0.0, 1.0, 0.0), 4.0))
	if is_nan(floor):
		return {}
	return {"point": Vector3(behind.x, floor + 0.15, behind.z), "distance": 3.0}
