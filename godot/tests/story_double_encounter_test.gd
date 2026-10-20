extends SceneTree

const Encounter := preload("res://presentation/story_double_encounter.gd")
const Guide := preload("res://presentation/story_guidance.gd")
var checks := 0
var failures: Array[String] = []

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		failures.append(message)
		push_error("STORY_DOUBLE " + message)

func context() -> Dictionary:
	var at := Vector3(-26.75, 253.9, -141.2)
	var actor := Vector3(-26.75, 253.9, -143.2)
	var eye := at + Vector3(0, .65, 0)
	return {"position": at, "grounded": true, "checkpoint_footing_valid": true,
		"support_entity": 1993, "deaths": 0, "story_actor_line_of_sight": true,
		"story_camera_transform": Transform3D(Basis.looking_at(actor + Vector3(0, .65, 0) - eye), eye),
		"story_actor": {"present": true, "supported": true, "entity_id": 63200,
			"support_entity_id": 1993, "episode": 1, "position": actor, "velocity": Vector3.ZERO}}

func focus(encounter: RefCounted, ctx: Dictionary, start: int = 0) -> StringName:
	var key := &""
	for tick in range(start, start + 55):
		key = encounter.observe(ctx, tick)
	return key

func _run() -> void:
	var ctx := context()
	var encounter := Encounter.new()
	check(focus(encounter, ctx) == &"double_ro_who", "54 fresh ticks of clear face attention request first line")
	check(encounter.observe(ctx, 54).is_empty(), "repeated tick cannot request another line")
	check(encounter.observe(ctx, 55) == &"double_ro_who", "unaccepted request keeps phase")
	encounter.accept(&"wrong")
	check(encounter.observe(ctx, 56) == &"double_ro_who", "wrong acceptance cannot advance")
	encounter.accept(&"double_ro_who")
	check(encounter.observe(ctx, 57) == &"double_ari_name", "accepted line alone advances")
	ctx.story_actor.present = false
	check(encounter.observe(ctx, 58).is_empty(), "actor loss cancels attention")
	ctx.story_actor.present = true
	ctx.story_actor.episode = 2
	check(encounter.observe(ctx, 59).is_empty(), "new actor episode cannot inherit old attention")
	check(focus(encounter, ctx, 60) == &"double_ari_name", "new attention resumes without replaying accepted speech")
	check(encounter.observe(ctx, 0).is_empty(), "native reset clears attention")
	check(focus(encounter, ctx, 1) == &"double_ro_who", "native reset starts a fresh conversation")
	var stale := Encounter.new()
	for iteration in range(100):
		check(stale.observe(context(), 1).is_empty(), "stale tick cannot accumulate attention")
	check(stale.observe(context(), 1000).is_empty(), "large unseen tick gap cannot fabricate attention")
	for invalid in ["support", "firm", "air", "actor_support", "far", "look", "occluded", "camera", "operating", "hanging", "flying"]:
		var bad := context()
		match invalid:
			"support": bad.support_entity = 1440
			"firm": bad.checkpoint_footing_valid = false
			"air": bad.grounded = false
			"actor_support": bad.story_actor.supported = false
			"far": bad.story_actor.position += Vector3(0, 0, -10)
			"look": bad.story_camera_transform.basis = Basis(Vector3.UP, PI)
			"occluded": bad.story_actor_line_of_sight = false
			"camera": bad.story_camera_transform.origin += Vector3(0, 5, 0)
			"operating": bad.operating = &"yard_crane"
			"hanging": bad.hanging = true
			"flying": bad.story_actor.velocity = Vector3(0, 3, 0)
		check(focus(Encounter.new(), bad).is_empty(), "reject " + invalid)
	var guide := Guide.new()
	var lengths := {}
	for key in Encounter.KEYS:
		lengths[key] = .1
	guide.configure_voice_lengths(lengths)
	ctx = context()
	var seen: Array[StringName] = []
	var last_id := 0
	var previous_tick := -1
	for tick in range(1800):
		# Actual guidance arrival cue is busy through tick990; no double cue
		# may replace it. Dialogue then uses each clip +0.35 second gap.
		var state: Dictionary = guide.sample(ctx, tick)
		if state.transmission_id != last_id:
			last_id = state.transmission_id
			var key: StringName = state.transmission_key
			if Encounter.CUES.has(key):
				seen.append(key)
				check(state.speaker == Encounter.SPEAKERS[key] and state.transmission == Encounter.CUES[key], "exact authored speaker/text " + String(key))
				check(tick > 990, "double waits for existing cue lifetime")
				if previous_tick >= 0:
					check(tick - previous_tick == 42, "natural clip duration + gap expires before next line")
				previous_tick = tick
				check(guide.sample(ctx, tick).transmission_id == last_id, "same tick does not duplicate accepted cue")
	check(seen == Encounter.KEYS, "all thirteen lines accepted exactly once in order")
	var paused := Guide.new()
	paused.configure_voice_lengths(lengths)
	for tick in range(1000):
		paused.sample(context(), tick)
	ctx = context()
	ctx.operating = &"yard_crane"
	for tick in range(1000, 1100):
		check(paused.sample(ctx, tick).transmission_key != &"double_ari_name", "operating pauses future dialogue")
	ctx = context()
	var resumed: Dictionary
	for tick in range(1100, 1155):
		resumed = paused.sample(ctx, tick)
	check(resumed.transmission_key == &"double_ari_name", "return and fresh attention resume the next accepted phase")
	print("STORY_DOUBLE %s checks=%d failures=%d supplied_native_facts_only=1" % ["PASS" if failures.is_empty() else "FAIL", checks, failures.size()])
	quit(0 if failures.is_empty() else 1)
