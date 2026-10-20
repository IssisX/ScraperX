extends SceneTree

const Radio := preload("res://presentation/audio/story_radio_player.gd")
var checks := 0
var failures := 0

# Silence is a test-only resource used to exercise real AudioStreamPlayer
# scheduling and cleanup. No synthetic voice or fixture enters shipping assets.
class FixtureRadio extends Radio:
	var reads := 0
	var loads := 0
	var available := false
	var invalid_manifest := false
	var sample: AudioStreamWAV

	func _init() -> void:
		sample = AudioStreamWAV.new()
		sample.mix_rate = 8000
		sample.format = AudioStreamWAV.FORMAT_16_BITS
		var bytes := PackedByteArray()
		bytes.resize(16000)
		sample.data = bytes

	func _read_manifest(_path: String) -> Variant:
		reads += 1
		if invalid_manifest:
			return {"schema_version": 2, "cues": {}}
		return {"schema_version": 1, "voices": {"test": "fixture only"}, "cues": {
			"opening": {"file": "res://audio/story_radio/opening.ogg", "speaker": "FOREMAN", "text": "Take the service walkway.", "duration_s": 1.0},
			"landing": {"file": "res://audio/story_radio/landing.ogg", "speaker": "FOREMAN", "text": "The landing is ahead.", "duration_s": 1.0},
			"escape": {"file": "user://outside.ogg", "speaker": "FOREMAN", "text": "Invalid.", "duration_s": 1.0}}}

	func _load_voice_stream(_path: String) -> AudioStream:
		loads += 1
		return sample if available else null


func _initialize() -> void:
	_run.call_deferred()


func check(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("STORY_RADIO FAIL " + message)


func cue(id: int = 1, key: StringName = &"opening") -> Dictionary:
	return {"transmission_id": id, "transmission_key": key, "speaker": "FOREMAN",
		"transmission": "Take the service walkway." if key == &"opening" else "The landing is ahead."}


func _run() -> void:
	# Shipping AudioDirector creates this settings bus before adding radio.
	var added_effects_bus := AudioServer.get_bus_index(&"Effects") < 0
	if added_effects_bus:
		AudioServer.add_bus()
		AudioServer.set_bus_name(AudioServer.bus_count - 1, &"Effects")
	var radio := FixtureRadio.new()
	root.add_child(radio)
	check(radio.reads == 1 and radio.loads == 2, "configure loads manifest and eligible streams once")
	radio.present(cue(), 0.0)
	var receipt := radio.readback()
	check(receipt.status == "missing_stream" and receipt.scheduled_plays == 0 and receipt.stream == null, "missing asset stays explicit and does not consume ID")
	check(radio.cue_durations().is_empty(), "missing recordings cannot advertise spoken duration")
	check(receipt.missing_keys.has("opening") and receipt.missing_keys.has("escape"), "missing recording and invalid path have readback")
	for sample in 30:
		radio.present(cue(), 1.0 / 90.0)
	check(radio.reads == 1 and radio.loads == 2, "per-frame sampling never reloads disk resources")
	radio.available = true
	check(radio.configure(), "explicit configure retries newly available recordings")
	check(radio.cue_durations() == {&"opening": 1.0, &"landing": 1.0}, "validated available recording durations serve caption timing")
	radio.present(cue(), 0.0)
	receipt = radio.readback()
	check(receipt.scheduled_plays == 1 and receipt.active_id == 1 and receipt.stream == radio.sample, "same previously missing ID schedules actual stream after reload")
	check(radio.get_child_count() == 1 and radio._voice.max_polyphony == 1 and radio._voice.bus == &"Effects", "one voice follows existing Effects settings bus")
	for sample in 30:
		radio.present(cue(), 0.1)
	check(radio.readback().scheduled_plays == 1, "duplicate ID never restarts")
	radio.present(cue(2, &"landing"), 0.0)
	check(radio.readback().active_id == 2 and radio.readback().scheduled_plays == 2, "new ID replaces old recording")
	var mismatch := cue(3)
	mismatch.transmission = "Different words."
	radio.present(mismatch, 0.0)
	check(radio.readback().stream == null and radio.readback().status == "copy_mismatch" and radio.readback().scheduled_plays == 2, "copy mismatch cancels old voice and refuses wrong words")
	mismatch = cue(3)
	mismatch.speaker = "OTHER"
	radio.present(mismatch, 0.0)
	check(radio.readback().status == "copy_mismatch", "speaker must match recording metadata")
	var suppressed := cue(3)
	suppressed.suppress_voice = true
	radio.present(suppressed, 0.0)
	check(radio.readback().status == "deferred" and radio.readback().stream == null, "unstarted suppressed current cue defers without queue")
	radio.present(cue(4, &"landing"), 0.0)
	check(radio.readback().active_id == 4 and radio.readback().scheduled_plays == 3, "new current cue discards stale deferred cue")
	suppressed = cue(4, &"landing")
	suppressed.suppress_voice = true
	radio.present(suppressed, 0.0)
	check(radio.readback().status == "suppressed" and radio.readback().stream == null, "suppression stops active speech immediately")
	radio.present(cue(4, &"landing"), 0.0)
	check(radio.readback().scheduled_plays == 3 and radio.readback().stream == null, "interrupted started ID is never replayed")
	suppressed = cue(5)
	suppressed.suppress_voice = true
	radio.present(suppressed, 0.0)
	radio.present(cue(5), 0.0)
	check(radio.readback().active_id == 5 and radio.readback().scheduled_plays == 4, "current unstarted cue plays when suppression ends")
	radio._on_finished()
	check(radio.readback().stream == null and radio.readback().active_id == null and radio.readback().status == "finished", "finished signal releases stream and active identity")
	radio.present(cue(5), 0.0)
	check(radio.readback().scheduled_plays == 4, "finished cue cannot replay from repeated samples")
	radio.present(cue(6), 0.0)
	radio.present({}, 0.0)
	check(radio.readback().stream == null and radio.readback().active_id == null and radio.readback().status == "idle", "empty state stops and neutralizes")
	radio.present(cue(6), 0.0)
	check(radio.readback().scheduled_plays == 5, "neutral state does not erase consumed identity")
	radio.invalid_manifest = true
	check(not radio.configure() and not radio.readback().manifest_error.is_empty(), "bad manifest fails explicitly")
	radio.present(cue(7), 0.0)
	check(radio.readback().stream == null and radio.readback().scheduled_plays == 5, "invalid manifest cannot play old cached recording")
	radio.invalid_manifest = false
	radio.configure()
	radio.present(cue(7), 0.0)
	check(radio.readback().scheduled_plays == 6, "unstarted ID remains retryable after configuration repair")
	radio.quiesce()
	radio.present(cue(8), 0.0)
	check(radio.readback().stream == null and radio.readback().quiesced and radio.readback().scheduled_plays == 6, "quiesce prevents late scheduled speech")
	radio.free()
	# Let the dummy mixer release stopped playback references before exit.
	for frame in 4:
		await process_frame
	if added_effects_bus:
		AudioServer.remove_bus(AudioServer.get_bus_index(&"Effects"))
	print("STORY_RADIO %s checks=%d failures=%d fixture_stream_only=1 audible_voice_quality=unverified" % ["PASS" if failures == 0 else "FAIL", checks, failures])
	quit(0 if failures == 0 else 1)
