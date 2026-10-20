extends Node

# The current story sample is the only queue. Never own story progression.
const MANIFEST_PATH := "res://audio/story_radio/manifest.json"
const RADIO_DIRECTORY := "res://audio/story_radio/"
const BUS := &"Effects"

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
		_cues[key] = cue.duplicate(true)
		var stream := _load_voice_stream(path)
		if stream == null:
			_missing_keys[key] = "recording unavailable: " + path
		else:
			_streams[key] = stream
	_status = "ready"
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
	if _played_ids.has(id):
		if _active_id != id:
			_status = "already_started"
		return
	if not _streams.has(key):
		_missing_keys[key] = "recording unavailable; configure again after assets are supplied"
		_status = "missing_stream"
		return
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
	_active_id = null
	_active_key = &""


func _on_finished() -> void:
	_stop_voice()
	_status = "finished"


func _exit_tree() -> void:
	_stop_voice()
	_streams.clear()


func readback() -> Dictionary:
	return {"active_id": _active_id, "active_key": _active_key,
		"stream": _voice.stream if is_instance_valid(_voice) else null,
		"playing": _voice.playing if is_instance_valid(_voice) else false,
		"scheduled_plays": _scheduled_plays, "missing_keys": _missing_keys.duplicate(),
		"manifest_error": _manifest_error, "status": _status, "quiesced": _quiesced}


# Only available validated recordings contribute to caption lifetimes. The
# producer measures duration_s from each final encoded asset in the manifest.
func cue_durations() -> Dictionary:
	var durations := {}
	for key: String in _streams:
		durations[StringName(key)] = float(_cues[key].duration_s)
	return durations
