extends SceneTree

const Encounter := preload("res://presentation/story_double_encounter.gd")
const Guide := preload("res://presentation/story_guidance.gd")
const Dialogue := preload("res://presentation/story_handover_dialogue.gd")
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

func reach_branch(encounter: RefCounted, ctx: Dictionary) -> int:
	var tick := 54
	check(focus(encounter, ctx) == Encounter.KEYS[0], "branch approach retains deliberate attention")
	for phase in range(4):
		var key: StringName = Encounter.KEYS[phase]
		check(encounter.observe(ctx, tick + 1) == key, "common opening phase " + str(phase))
		encounter.accept(key)
		tick += 1
	return tick

func branch_checks() -> void:
	for disposition in [&"undecided", &"ari_first", &"sent_to_dev", &"unrecognized", &""]:
		var ctx := context()
		ctx.handover_disposition = disposition
		var encounter := Encounter.new()
		var tick := reach_branch(encounter, ctx)
		var expected: Array = Encounter.KEYS.duplicate()
		if Encounter.BRANCH_KEYS.has(disposition):
			expected[4] = Encounter.BRANCH_KEYS[disposition][0]
			expected[5] = Encounter.BRANCH_KEYS[disposition][1]
		for phase in range(4, expected.size()):
			tick += 1
			check(encounter.observe(ctx, tick) == expected[phase], "branch and shared close " + String(disposition) + " phase " + str(phase))
			encounter.accept(expected[phase])
		check(encounter.observe(ctx, tick + 1).is_empty(), "branch ends after thirteen accepted lines")
	# A busy Guide observes but does not accept. Only the latest accepted
	# request decides which knowledge the conversation commits to.
	var ctx := context()
	ctx.handover_disposition = &"ari_first"
	var encounter := Encounter.new()
	var tick := reach_branch(encounter, ctx)
	check(encounter.observe(ctx, tick + 1) == &"double_ro_original_scope", "survey branch offered while Guide is busy")
	encounter.accept(&"double_ro_dev_has_scope")
	ctx.handover_disposition = &"sent_to_dev"
	check(encounter.observe(ctx, tick + 2) == &"double_ro_dev_has_scope", "unaccepted branch follows changed knowledge")
	encounter.accept(&"double_ro_original_scope")
	check(encounter.observe(ctx, tick + 3) == &"double_ro_dev_has_scope", "stale branch acceptance cannot advance")
	encounter.accept(&"double_ro_dev_has_scope")
	ctx.handover_disposition = &"ari_first"
	check(encounter.observe(ctx, tick + 4) == &"double_ari_dev_has_scope", "accepted first line freezes matching reply")
	encounter.cancel_attention()
	encounter.accept(&"double_ari_dev_has_scope")
	ctx.story_actor.episode = 2
	check(focus(encounter, ctx, tick + 5) == &"double_ari_dev_has_scope", "attention cancellation and new episode retain branch and accepted phase")
	ctx.story_actor.present = false
	check(encounter.observe(ctx, tick + 60).is_empty(), "actor loss pauses selected branch")
	ctx.story_actor.present = true
	check(focus(encounter, ctx, tick + 61) == &"double_ari_dev_has_scope", "actor return retains selected branch")
	check(encounter.observe(ctx, 0).is_empty(), "native restart clears selected branch attention")
	check(focus(encounter, ctx, 1) == Encounter.KEYS[0], "native restart clears accepted conversation")
	for phase in range(4):
		encounter.observe(ctx, 56 + phase)
		encounter.accept(Encounter.KEYS[phase])
	check(encounter.observe(ctx, 60) == &"double_ro_original_scope", "native restart selects current knowledge anew")
	# The bypass is a committed branch too; later discovery cannot rewrite a
	# question already heard into a mismatched answer.
	ctx = context()
	encounter = Encounter.new()
	tick = reach_branch(encounter, ctx)
	check(encounter.observe(ctx, tick + 1) == &"double_ro_empty", "missing knowledge uses original bypass")
	encounter.accept(&"double_ro_empty")
	ctx.handover_disposition = &"ari_first"
	check(encounter.observe(ctx, tick + 2) == &"double_ari_told", "accepted bypass retains original answer after discovery")
	encounter.reset()
	tick = reach_branch(encounter, ctx)
	check(encounter.observe(ctx, tick + 1) == &"double_ro_original_scope", "explicit reset clears bypass latch")

func choose_at_archive(guide: RefCounted, disposition: StringName) -> int:
	# Guide owns the accepted knowledge state. Earn it through the archive
	# events rather than injecting its derived field into the actor context.
	var terminal := {"position": Vector3(12, 110.9, -181.95), "grounded": true,
		"checkpoint_footing_valid": true, "support_entity": 1960, "deaths": 0,
		"story_handover_near": true, "story_handover": &"handover"}
	guide.sample(terminal, 0)
	terminal.erase("story_handover")
	var tick: int = guide._transmission_until + 1
	for key in Dialogue.KEYS:
		check(guide.sample(terminal, tick).transmission_key == key, "archive prerequisite uses authored sequence " + String(key))
		tick = guide._transmission_until + 1
	terminal.story_handover = &"source"
	check(guide.sample(terminal, tick).transmission_key == &"handover_source_survey", "original scope comparison precedes the choice")
	terminal.erase("story_handover")
	tick = guide._transmission_until + 1
	guide.sample(terminal, tick)
	check(guide.choose_handover(disposition, tick), "Guide accepts the explicit archive decision " + String(disposition))
	var responses: Array = Dialogue.SEND_KEYS if disposition == &"sent_to_dev" else Dialogue.ARI_KEYS
	for key in responses:
		tick += 1
		check(guide.sample(terminal, tick).transmission_key == key, "chosen archive response precedes the witness encounter " + String(key))
		tick = guide._transmission_until + 1
	guide.sample(terminal, tick)
	return tick + 1

func _run() -> void:
	branch_checks()
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
	for key in Encounter.CUES:
		lengths[key] = .1
	for key in Dialogue.CUES:
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
	for disposition in Encounter.BRANCH_KEYS:
		var branch_guide := Guide.new()
		branch_guide.configure_voice_lengths(lengths)
		var start_tick := choose_at_archive(branch_guide, disposition)
		ctx = context()
		var accepted: Array[StringName] = []
		var branch_id := 0
		var death_tick := -1
		for tick in range(start_tick, start_tick + 3000):
			var branch_state: Dictionary = branch_guide.sample(ctx, tick)
			if branch_state.transmission_id != branch_id:
				branch_id = branch_state.transmission_id
				var key: StringName = branch_state.transmission_key
				if Encounter.CUES.has(key):
					accepted.append(key)
					check(branch_state.speaker == Encounter.SPEAKERS[key] and branch_state.transmission == Encounter.CUES[key], "authored branch speaker/text " + String(key))
					if key == Encounter.BRANCH_KEYS[disposition][0]:
						ctx.deaths = 1
						ctx.handover_disposition = &"undecided"
						death_tick = tick
					if key == Encounter.BRANCH_KEYS[disposition][1]:
						check(tick >= death_tick + Encounter.FOCUS_TICKS, "death requires fresh attention before branch reply")
		var expected: Array = Encounter.KEYS.duplicate()
		expected[4] = Encounter.BRANCH_KEYS[disposition][0]
		expected[5] = Encounter.BRANCH_KEYS[disposition][1]
		check(accepted == expected, "Guide retains accepted branch across death " + String(disposition))
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
