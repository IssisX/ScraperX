extends Node

# The current story sample is the only queue. Never own story progression.
const MANIFEST_PATH := "res://audio/story_radio/manifest.json"
const RADIO_DIRECTORY := "res://audio/story_radio/"
const BUS := &"Effects"
# Owner voice rule: unapproved stock recordings may never play.
const VOICE_RECORDINGS_APPROVED := false

var _scene_voice: AudioStreamPlayer3D
var _scene_position := Vector3.ZERO
var _has_scene_position := false
var _record_position := Vector3.ZERO
var _has_record_position := false
var _voice: AudioStreamPlayer
var _configured := false
var _cues: Dictionary = {}
var _streams: Dictionary = {}
var _played_ids: Dictionary = {}
var _missing_keys: Dictionary = {}
var _manifest_error := ""
var _status := "idle"
var _active_id: Variant = null
var _active_key := &""
var _scheduled_plays := 0
var _quiesced := false


func _ready() -> void:
	_build_voice()
	if not _configured:
		configure()


func _build_voice() -> void:
	if is_instance_valid(_voice):
		return
	_voice = AudioStreamPlayer.new()
	_voice.name = "StoryRadioVoice"
	_voice.bus = BUS
	_voice.max_polyphony = 1
	_voice.volume_db = -3.0
	_voice.process_mode = Node.PROCESS_MODE_PAUSABLE
	add_child(_voice)
	_voice.finished.connect(_on_finished)


func set_scene_voice_position(world_position: Vector3) -> void:
	_has_scene_position = world_position.is_finite()
	if not _has_scene_position:
		return
	_scene_position = world_position
	if is_instance_valid(_scene_voice) and _active_speaker() == "ARI":
		_scene_voice.global_position = world_position

func clear_scene_voice_position() -> void:
	_has_scene_position = false
	if is_instance_valid(_scene_voice) and _scene_voice.playing and _active_speaker() == "ARI":
		_stop_voice()

func set_record_voice_position(world_position: Vector3) -> void:
	_has_record_position = world_position.is_finite()
	if not _has_record_position:
		return
	_record_position = world_position
	if is_instance_valid(_scene_voice) and _active_speaker() in ["RECORD", "SOURCE"]:
		_scene_voice.global_position = world_position

func _active_speaker() -> String:
	return String(_cues.get(String(_active_key), {}).get("speaker", ""))

func _build_scene_voice() -> void:
	if is_instance_valid(_scene_voice):
		return
	_scene_voice = AudioStreamPlayer3D.new()
	_scene_voice.name = "StoryActorVoice"
	_scene_voice.bus = BUS
	_scene_voice.max_polyphony = 1
	_scene_voice.volume_db = -3
	_scene_voice.unit_size = 4
	_scene_voice.max_distance = 20
	_scene_voice.attenuation_filter_cutoff_hz = 12000
	_scene_voice.process_mode = Node.PROCESS_MODE_PAUSABLE
	add_child(_scene_voice)
	_scene_voice.global_position = _scene_position
	_scene_voice.finished.connect(_on_finished)

func _playing_voice() -> Node:
	if _active_speaker() in ["ARI", "RECORD", "SOURCE"]:
		return _scene_voice
	return _voice

func playback_seconds_for(speaker: StringName) -> float:
	var player := _playing_voice()
	if _active_speaker() != String(speaker) or not is_instance_valid(player) or not player.playing:
		return 0.0
	return maxf(0, player.get_playback_position())

func speech_level_for(speaker: StringName) -> float:
	var cue: Dictionary = _cues.get(String(_active_key), {})
	var player := _playing_voice()
	if String(cue.get("speaker", "")) != String(speaker) or not is_instance_valid(player) or not player.playing:
		return 0
	var envelope: Array = cue.get("rms_30hz", [])
	if envelope.is_empty():
		return 0
	var at: float = maxf(0, player.get_playback_position() * 30)
	var index := mini(int(at), envelope.size() - 1)
	return lerpf(float(envelope[index]), float(envelope[mini(index + 1, envelope.size() - 1)]), at - floorf(at))


# Reload is explicit so missing resources are retryable without disk work in
# present(). Already started IDs remain consumed across configuration changes.
func configure(manifest_path: String = MANIFEST_PATH) -> bool:
	cancel()
	_configured = true
	_cues.clear()
	_streams.clear()
	_missing_keys.clear()
	_manifest_error = ""
	var document: Variant = _read_manifest(manifest_path)
	if not document is Dictionary or document.get("schema_version") != 1 or not document.get("cues") is Dictionary:
		_manifest_error = "Invalid story radio manifest: " + manifest_path + "; expected schema_version 1 and cues dictionary"
		_status = "manifest_error"
		return false
	for key_value: Variant in document.cues:
		var key := String(key_value)
		var cue: Variant = document.cues[key_value]
		if not cue is Dictionary or not cue.get("file") is String or not cue.get("speaker") is String or not cue.get("text") is String:
			_missing_keys[key] = "invalid cue metadata"
			continue
		var path: String = cue.file
		# A cue may only name its own packaged recording, never arbitrary files.
		if key.is_empty() or "/" in key or "\\" in key or path != RADIO_DIRECTORY + key + ".ogg":
			_missing_keys[key] = "expected " + RADIO_DIRECTORY + key + ".ogg"
			continue
		var duration: Variant = cue.get("duration_s")
		if not (duration is float or duration is int) or not is_finite(float(duration)) or float(duration) <= 0.0:
			_missing_keys[key] = "invalid duration_s"
			continue
		if cue.has("rms_30hz"):
			var envelope: Variant = cue.rms_30hz
			var valid := envelope is Array
			if valid:
				valid = not envelope.is_empty() and envelope.size() <= ceili(float(duration) * 30) + 2
				for value: Variant in envelope:
					if not (value is float or value is int) or not is_finite(float(value)) or float(value) < 0 or float(value) > 1:
						valid = false
			if not valid:
				_missing_keys[key] = "invalid recording speech envelope"
				continue
		_cues[key] = cue.duplicate(true)
		if VOICE_RECORDINGS_APPROVED:
			var stream := _load_voice_stream(path)
			if stream == null:
				_missing_keys[key] = "recording unavailable: " + path
			else:
				_streams[key] = stream
	_status = "ready" if VOICE_RECORDINGS_APPROVED else "voice_unapproved"
	return true


func _read_manifest(path: String) -> Variant:
	if not FileAccess.file_exists(path):
		return null
	return JSON.parse_string(FileAccess.get_file_as_string(path))


func _load_voice_stream(path: String) -> AudioStream:
	if not ResourceLoader.exists(path, "AudioStream"):
		return null
	return load(path) as AudioStream


func present(state: Dictionary, _delta: float) -> void:
	_build_voice()
	if not _configured:
		configure()
	if _quiesced:
		return
	var id: Variant = state.get("transmission_id")
	var key_value: Variant = state.get("transmission_key")
	var line: Variant = state.get("transmission")
	var speaker: Variant = state.get("speaker")
	if not id is int or not (key_value is String or key_value is StringName) or not line is String or line.is_empty() or not speaker is String:
		cancel()
		return
	var key := String(key_value)
	# A changed ID, key, or copy cancels old audio before validating the next.
	if _active_id != id or String(_active_key) != key or not _matches_copy(key, speaker, line):
		_stop_voice()
	if state.get("suppress_voice", false) == true:
		_stop_voice()
		_status = "suppressed" if _played_ids.has(id) else "deferred"
		return
	if not _matches_copy(key, speaker, line):
		_status = "copy_mismatch" if _cues.has(key) else "missing_cue"
		if not _cues.has(key):
			_missing_keys[key] = "cue absent from manifest"
		return
	if not VOICE_RECORDINGS_APPROVED:
		_status = "voice_unapproved"
		return
	if _played_ids.has(id):
		if _active_id != id:
			_status = "already_started"
		return
	if not _streams.has(key):
		_missing_keys[key] = "recording unavailable; configure again after assets are supplied"
		_status = "missing_stream"
		return
	if speaker in ["ARI", "RECORD", "SOURCE"]:
		if not (_has_scene_position if speaker == "ARI" else _has_record_position):
			_status = "scene_position_unavailable"
			return
		_build_scene_voice()
		_scene_voice.global_position = _scene_position if speaker == "ARI" else _record_position
		_scene_voice.stream = _streams[key]
		_scene_voice.play()
	else:
		_voice.stream = _streams[key]
		_voice.play()
	_played_ids[id] = true
	_active_id = id
	_active_key = StringName(key)
	_scheduled_plays += 1
	_status = "scheduled"


func _matches_copy(key: String, speaker: String, line: String) -> bool:
	return _cues.has(key) and _cues[key].speaker == speaker and _cues[key].text == line


func cancel() -> void:
	_stop_voice()
	_status = "idle"


func quiesce() -> void:
	cancel()
	_quiesced = true


func _stop_voice() -> void:
	if is_instance_valid(_voice):
		_voice.stop()
		_voice.stream = null
	if is_instance_valid(_scene_voice):
		_scene_voice.stop()
		_scene_voice.stream = null
	_active_id = null
	_active_key = &""


func _on_finished() -> void:
	_stop_voice()
	_status = "finished"


func _exit_tree() -> void:
	_stop_voice()
	_streams.clear()


func readback() -> Dictionary:
	var player := _playing_voice()
	return {"active_id": _active_id, "active_key": _active_key,
		"stream": player.stream if is_instance_valid(player) else null,
		"playing": player.playing if is_instance_valid(player) else false,
		"spatial": player == _scene_voice and is_instance_valid(_scene_voice),
		"scheduled_plays": _scheduled_plays, "missing_keys": _missing_keys.duplicate(),
		"manifest_error": _manifest_error, "status": _status, "quiesced": _quiesced}


func cue_metadata(key: StringName) -> Dictionary:
	return (_cues.get(String(key), {}) as Dictionary).duplicate(true)


# Only available validated recordings contribute to caption lifetimes. The
# producer measures duration_s from each final encoded asset in the manifest.
func cue_durations() -> Dictionary:
	var durations := {}
	for key: String in _streams:
		durations[StringName(key)] = float(_cues[key].duration_s)
	return durations
