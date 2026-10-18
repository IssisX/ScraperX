extends SceneTree
## Presentation receipts only; native climbing is exercised by its own tests.
const Effort := preload("res://presentation/climb_effort_feedback.gd")
const Director := preload("res://presentation/audio/audio_director.gd")
const Bank := preload("res://presentation/audio/sound_bank.gd")
var failures := 0

class RecordedDirector extends Director:
	var pulls := 0
	func _play(sound: StringName, _level: float, _pitch: float,
			_bus: StringName = BUS_EFFECTS) -> void:
		if sound == &"climb_pull":
			pulls += 1


func _initialize() -> void:
	_run.call_deferred()


func check(ok: bool, why: String) -> void:
	if not ok:
		failures += 1
		push_error("CLIMB_EFFORT FAIL " + why)


func receipt(work: float) -> Dictionary:
	return {"hand_actuator_positive_work_j": work,
		"left_hand_force_n": Vector3.UP * 420.0, "right_hand_force_n": Vector3.UP * 420.0,
		"left_hand_attached": true, "right_hand_attached": true}


func _run() -> void:
	for group in [1, 2, 3, 9]:
		var model := Effort.new()
		var count := 0
		model.sample(receipt(0), 0, 1, 0, true)
		for tick in range(group, 91, group):
			var state := receipt(float(tick) * 6.0)
			var unchanged := state.duplicate(true)
			var strength := model.sample(state, tick, 1, 0, true)
			count += int(strength > 0.0)
			check(is_finite(strength) and strength >= 0.0 and strength <= 1.0 and state == unchanged,
				"read-only bounded response")
			for repeated_render in 4:
				check(model.sample(state, tick, 1, 0, true) == 0.0,
					"repeated rendering cannot duplicate native work")
		check(count == 2, "same two cues for one second of work across native delivery groups")
	var model := Effort.new()
	model.sample(receipt(0), 0, 1, 0, true)
	for tick in range(1, 91):
		check(model.sample(receipt(0), tick, 1, 0, true) == 0.0,
			"steady loaded hang and passive support motion stay quiet")
	model.reset()
	model.sample(receipt(0), 0, 1, 0, true)
	check(model.sample(receipt(60), 10, 1, 0, false) == 0.0 and
		model.sample(receipt(60), 11, 1, 0, true) == 0.0,
		"muted work is consumed and cannot replay on unmute")
	check(model.sample(receipt(120), 20, 0, 0, true) == 0.0 and
		model.sample(receipt(120), 21, 1, 0, true) == 0.0,
		"release consumes work without a delayed pull")
	check(model.sample(receipt(180), 30, 1, 1, true) == 0.0 and
		model.sample(receipt(180), 1, 1, 1, true) == 0.0,
		"death and clock rewind establish quiet baselines")
	check(model.sample(receipt(600), 100, 1, 1, true) == 0.0,
		"long observation gaps do not burst old work")
	var invalid := receipt(NAN)
	check(model.sample(invalid, 101, 1, 1, true) == 0.0 and
		model.sample(receipt(660), 102, 1, 1, true) == 0.0,
		"nonfinite work and next valid baseline stay quiet")
	model.reset()
	model.sample(receipt(0), 0, 1, 0, true)
	var unloaded := receipt(120)
	unloaded.left_hand_force_n = Vector3.ZERO
	unloaded.right_hand_force_n = Vector3.ZERO
	check(model.sample(unloaded, 10, 1, 0, true) == 0.0,
		"command work without actual hand load cannot create a pull cue")
	var detached := receipt(240)
	detached.left_hand_attached = false
	detached.right_hand_attached = false
	check(model.sample(detached, 20, 1, 0, true) == 0.0, "detached hands stay quiet")
	_audio_path()
	var bank := Bank.new()
	bank._rng.seed = 20261018
	var first := bank._climb_pull()
	bank._rng.seed = 20261018
	var second := bank._climb_pull()
	var peak := 0.0
	for value in first:
		check(is_finite(value), "finite cloth PCM")
		peak = maxf(peak, absf(value))
	check(first == second and first.size() == int(Bank.MIX_RATE * 0.24) and peak > 0.001 and peak <= 1.0,
		"bounded nonempty deterministic cloth clip")
	print("CLIMB_EFFORT %s failures=%d native_physics_exercised=0 heard_audio=0" %
		["PASS" if failures == 0 else "FAIL", failures])
	quit(1 if failures else 0)


func _audio_path() -> void:
	var audio := RecordedDirector.new()
	root.add_child(audio)
	var voices := audio.get_child_count()
	for tick in 91:
		audio.update(1.0 / 90.0, Vector3.ZERO, Vector3.ZERO, false, 0, 1, false,
			0, false, Vector3.ZERO, receipt(float(tick) * 6.0), true, tick)
	check(audio.pulls == 2 and audio.climb_effort_cues == 2 and audio.get_child_count() == voices,
		"production audio path selects two pull clips through a bounded existing voice pool")
	audio.reset_landing_feedback(0, 0, receipt(540))
	audio._update_climb_effort(receipt(600), 100, 1, 0)
	check(audio.pulls == 2, "restart consumes existing work quietly")
	var effects := AudioServer.get_bus_index(Director.BUS_EFFECTS)
	var muted := AudioServer.is_bus_mute(effects)
	AudioServer.set_bus_mute(effects, true)
	audio._update_climb_effort(receipt(660), 110, 1, 0)
	AudioServer.set_bus_mute(effects, false)
	audio._update_climb_effort(receipt(660), 111, 1, 0)
	check(audio.pulls == 2, "production Effects mute consumes work without later replay")
	audio._update_climb_effort(receipt(780), 121, 1, 0)
	check(audio.pulls == 3, "fresh loaded work after unmute remains audible")
	audio.quiesce()
	audio._update_climb_effort(receipt(900), 131, 1, 0)
	check(audio.pulls == 3 and audio.last_climb_effort_strength == 0.0,
		"audio shutdown cannot start or queue another effort cue")
	AudioServer.set_bus_mute(effects, muted)
	# The director builds its sound bank asynchronously. Join that owned task
	# before freeing the test node; Godot locks a receiver during its call.
	if audio._bank_task >= 0:
		WorkerThreadPool.wait_for_task_completion(audio._bank_task)
		audio._bank_task = -1
	audio.free()
