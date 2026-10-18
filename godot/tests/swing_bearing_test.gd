extends SceneTree
## Tests presentation from supplied native observations; no physics/audio-hearing claim.
const Feedback := preload("res://presentation/swing_bearing_feedback.gd")
const Director := preload("res://presentation/audio/audio_director.gd")
var failures := 0

class NativeSample extends RefCounted:
	var speed := Vector3.RIGHT
	var queries := 0
	var missing := false
	func get_kit_body_index(entity: int) -> int:
		queries += 1
		return -1 if missing else entity - 2480
	func get_kit_body_linear_velocity(body: int) -> Vector3:
		return speed if body == 0 else Vector3.ZERO
	func get_kit_body_transform(body: int) -> Transform3D:
		return Transform3D(Basis.IDENTITY, Vector3(-49.0 + 1.1 * body, 4.2, -59.0))

class RecordedDirector extends Director:
	var played: Array[Dictionary] = []
	func _play_at(sound: StringName, at: Vector3, level: float, pitch: float,
			_bus: StringName = BUS_EFFECTS) -> void:
		played.append({"sound": sound, "at": at, "level": level, "pitch": pitch})

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, why: String) -> void:
	if not ok:
		failures += 1
		push_error("SWING_BEARING FAIL " + why)

func _run() -> void:
	var native := NativeSample.new()
	for group in [1, 2, 3, 9]:
		var feedback := Feedback.new()
		var count := 0
		feedback.sample(native, Feedback.BAY, 0, 0, true)
		for tick in range(group, 181, group):
			var cues := feedback.sample(native, Feedback.BAY, tick, 0, true)
			count += cues.size()
			for cue in cues:
				check(cue.at == Vector3(-49.0, 4.2, -59.0) and cue.strength > 0.0 and cue.strength <= 1.0,
					"cue belongs to actual moving bearing and has bounded strength")
			check(feedback.sample(native, Feedback.BAY, tick, 0, true).is_empty(),
				"repeated render cannot replay a native sample")
		check(count == 3, "same sparse motion cues across native delivery groups")
	var feedback := Feedback.new()
	native.queries = 0
	native.speed = Vector3.ZERO
	for tick in 181:
		check(feedback.sample(native, Feedback.BAY, tick, 0, true).is_empty(), "stationary steel is quiet")
	check(native.queries == 2, "fixed two-body lookup is cached")
	native.speed = Vector3.RIGHT
	check(feedback.sample(native, Feedback.BAY, 190, 0, false).is_empty(), "mute consumes movement")
	check(feedback.sample(native, Feedback.BAY, 191, 0, true).is_empty(), "unmute cannot replay old movement")
	check(feedback.sample(native, Feedback.BAY, 1000, 0, true).is_empty(), "long observation gap stays quiet")
	check(feedback.sample(native, Feedback.BAY, 1001, 1, true).is_empty(), "death resets motion baseline")
	check(feedback.sample(native, Feedback.BAY, 1, 1, true).is_empty(), "rewind resets motion baseline")
	native.queries = 0
	for tick in range(2, 70):
		check(feedback.sample(native, Feedback.BAY + Vector3.RIGHT * 100, tick, 1, true).is_empty(),
			"distant bay is quiet")
	check(native.queries == 0, "distant bay performs no native body queries")
	native.missing = true
	for tick in range(70, 140):
		check(feedback.sample(native, Feedback.BAY, tick, 1, true).is_empty(), "missing native bars stay quiet")
	native.missing = false
	native.speed = Vector3(NAN, 0.0, 0.0)
	feedback.reset()
	for tick in 70:
		check(feedback.sample(native, Feedback.BAY, tick, 1, true).is_empty(), "invalid native speed is rejected")
	_audio_path(native)
	print("SWING_BEARING %s failures=%d native_physics_exercised=0 heard_audio=0" %
		["PASS" if failures == 0 else "FAIL", failures])
	quit(1 if failures else 0)

func _audio_path(native: NativeSample) -> void:
	native.speed = Vector3.RIGHT
	var audio := RecordedDirector.new()
	root.add_child(audio)
	var children := audio.get_child_count()
	for tick in 181:
		audio.update_ground_swing(native, Feedback.BAY, tick, 0)
	check(audio.swing_bearing_cues == 3 and audio.played.size() == 3 and audio.get_child_count() == children,
		"production director routes cues without adding voices")
	for cue in audio.played:
		check(cue.sound == &"creak" and cue.at == Vector3(-49.0, 4.2, -59.0) and
			cue.level >= -24.0 and cue.level <= -13.0 and cue.pitch > 0.0,
			"existing positional steel clip has bounded mix values")
	var effects := AudioServer.get_bus_index(Director.BUS_EFFECTS)
	var muted := AudioServer.is_bus_mute(effects)
	AudioServer.set_bus_mute(effects, true)
	for tick in range(181, 271):
		audio.update_ground_swing(native, Feedback.BAY, tick, 0)
	AudioServer.set_bus_mute(effects, false)
	audio.update_ground_swing(native, Feedback.BAY, 271, 0)
	check(audio.played.size() == 3, "Effects mute and unmute cannot queue a stale creak")
	for tick in range(272, 331):
		audio.update_ground_swing(native, Feedback.BAY, tick, 0)
	check(audio.played.size() == 4, "fresh movement after unmute produces a new cue")
	audio.reset_landing_feedback(0, 0)
	audio.update_ground_swing(native, Feedback.BAY, 400, 0)
	check(audio.played.size() == 4, "restart establishes a quiet motion baseline")
	audio.quiesce()
	for tick in range(401, 481):
		audio.update_ground_swing(native, Feedback.BAY, tick, 0)
	check(audio.played.size() == 4, "shutdown cannot start another voice")
	AudioServer.set_bus_mute(effects, muted)
	if audio._bank_task >= 0:
		WorkerThreadPool.wait_for_task_completion(audio._bank_task)
		audio._bank_task = -1
	audio.free()
