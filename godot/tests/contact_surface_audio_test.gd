extends SceneTree
## Supplied native state exercises ordinary stride selection, not heard audio.

const Director := preload("res://presentation/audio/audio_director.gd")
var failures := 0
var checks := 0

class RecordedDirector extends Director:
	var sounds: Array[StringName] = []
	func _play(sound: StringName, _level: float, _pitch: float) -> void:
		sounds.append(sound)


func _initialize() -> void:
	_run.call_deferred()


func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("CONTACT_SURFACE_AUDIO FAIL " + why)


func _run() -> void:
	var audio := RecordedDirector.new()
	root.add_child(audio)
	# Each supplied frame travels 0.72 m: one ordinary stride. Expected
	# surfaces are explicit examples, independent of the selector's code.
	for example in [
		[51, 0.4, {"support_sound_surface": 2}, &"step_metal"],
		[51, 9.0, {"support_sound_surface": 2}, &"step_metal"],
		[51, 0.4, {"support_sound_surface": 1}, &"step_concrete"],
		[11, 9.0, {"support_sound_surface": 1}, &"step_concrete"],
		[51, 0.4, {}, &"step_earth"],
		[51, 9.0, {}, &"step_concrete"],
		[11, 9.0, {}, &"step_metal"],
		[51, 0.4, {"support_sound_surface": 0}, &"step_earth"],
		[51, 9.0, {"support_sound_surface": 0}, &"step_concrete"],
		[11, 9.0, {"support_sound_surface": 0}, &"step_metal"],
		[51, 0.4, {"support_sound_surface": -1}, &"step_earth"],
		[51, 9.0, {"support_sound_surface": 3}, &"step_concrete"],
		[11, 9.0, {"support_sound_surface": 99}, &"step_metal"],
		[51, 0.4, {"support_sound_surface": "2"}, &"step_earth"],
		[51, 9.0, {"support_sound_surface": 2.0}, &"step_concrete"],
		[11, 9.0, {"support_sound_surface": true}, &"step_metal"],
		[51, 0.4, {"support_sound_surface": null}, &"step_earth"],
	]:
		audio.sounds.clear()
		var before: int = audio.steps
		var state: Dictionary = example[2]
		var unchanged := state.duplicate(true)
		audio.update(0.18, Vector3(0, example[1], 0), Vector3(4, 0, 0), true,
			example[0], 0, false, 0, false, Vector3.ZERO, state)
		check(audio.sounds == [example[3]] and audio.steps == before + 1,
			"one expected ordinary step for " + str(example))
		check(state == unchanged, "support metadata remains read-only")
	# Native metadata must not create footing or movement. The rider shares
	# the support's world velocity, so relative foot speed is zero.
	for example in [
		[false, Vector3(4, 0, 0), Vector3.ZERO, {}],
		[true, Vector3(4, 0, 0), Vector3.ZERO, {"player_sliding": true}],
		[true, Vector3(4, 0, 0), Vector3(4, 0, 0), {}],
	]:
		audio.sounds.clear()
		var before: int = audio.steps
		var state: Dictionary = example[3].duplicate()
		state["support_sound_surface"] = 2
		for frame in 10:
			audio.update(0.18, Vector3(0, 9, 0), example[1], example[0],
				51, 0, false, 0, false, example[2], state)
		check(audio.sounds.is_empty() and audio.steps == before,
			"airborne, sliding or stationary rider produces no ordinary steps: " + str(example))
	# Omitting the new argument preserves unrelated contact cue selection.
	check(audio._surface(51, 0.4) == &"step_earth" and
		audio._surface(51, 9.0) == &"step_concrete" and
		audio._surface(11, 9.0) == &"step_metal", "legacy callers retain their fallback")
	audio.quiesce()
	if audio._bank_task >= 0:
		WorkerThreadPool.wait_for_task_completion(audio._bank_task)
		audio._bank_task = -1
		audio.free()
	print("CONTACT_SURFACE_AUDIO %s checks=%d native_contacts_exercised=0 heard_audio=0" %
		["PASS" if failures == 0 else "FAIL", checks])
	quit(1 if failures else 0)
