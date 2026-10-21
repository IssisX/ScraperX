extends SceneTree

const Record := preload("res://presentation/story_handover_record.gd")
const Dialogue := preload("res://presentation/story_handover_dialogue.gd")
const Guide := preload("res://presentation/story_guidance.gd")
var checks := 0
var failures: Array[String] = []

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures.append(why)
		push_error("STORY_HANDOVER " + why)

func context() -> Dictionary:
	return {"position": Vector3(12, 110.9, -181.95), "grounded": true,
		"checkpoint_footing_valid": true, "support_entity": Record.RECEIVER_ID, "deaths": 0}

func view(at: Vector3, pixel_y: float = 480, pixel_x: float = 320) -> Transform3D:
	var target := Record.SCREEN_POSITION + Vector3((pixel_x / Record.PIXELS.x - .5) * Record.SCREEN_SIZE.x, (.5 - pixel_y / Record.PIXELS.y) * Record.SCREEN_SIZE.y, 0)
	return Transform3D(Basis.IDENTITY, at + Vector3(0, .62, 0)).looking_at(target, Vector3.UP)

func rejected(ctx: Dictionary, camera: Transform3D, why: String) -> void:
	var record := Record.new()
	root.add_child(record)
	var quiet := true
	for tick in 100:
		quiet = record.observe(camera, ctx, tick).is_empty() and quiet
	check(quiet, why)
	record.queue_free()

func _run() -> void:
	var ctx := context()
	var camera := view(ctx.position)
	var record := Record.new()
	root.add_child(record)
	record.observe(camera, ctx, 0)
	var quiet := true
	for i in 100:
		quiet = record.observe(camera, ctx, 0).is_empty() and quiet
	check(quiet, "duplicate render ticks cannot earn attention")
	for tick in range(1, Record.FOCUS_TICKS):
		quiet = record.observe(camera, ctx, tick).is_empty() and quiet
	check(quiet, "partial attention cannot begin playback")
	check(record.observe(camera, ctx, Record.FOCUS_TICKS) == &"handover", "continuous screen attention selects first handover")
	quiet = true
	for tick in range(55, 155):
		quiet = record.observe(camera, ctx, tick).is_empty() and quiet
	check(quiet, "holding gaze never queues repeated playback")
	var away := Transform3D(Basis(Vector3.UP, PI), camera.origin)
	record.observe(away, ctx, 155)
	var source := view(ctx.position, 840)
	var source_events: Array[StringName] = []
	for tick in range(156, 210):
		var event: StringName = record.observe(source, ctx, tick)
		if not event.is_empty():
			source_events.append(event)
	check(source_events == [&"source"], "physical original-survey tab selects source exactly once after first play")
	record.observe(away, ctx, 211)
	var handover := view(ctx.position, 730)
	var handover_events: Array[StringName] = []
	for tick in range(212, 266):
		var event: StringName = record.observe(handover, ctx, tick)
		if not event.is_empty():
			handover_events.append(event)
	check(handover_events == [&"handover"], "physical handover tab explicitly selects exactly one replay")
	var fresh := Record.new()
	root.add_child(fresh)
	fresh.observe(camera, ctx, 0)
	fresh.observe(camera, ctx, 30)
	check(fresh.observe(camera, ctx, 1000).is_empty() and fresh._focus == 0, "unobserved native-time gaps reset attention")
	var bad := ctx.duplicate()
	bad.grounded = false
	rejected(bad, camera, "airborne proximity cannot activate recording")
	bad = ctx.duplicate()
	bad.checkpoint_footing_valid = false
	rejected(bad, camera, "grounded flag without firm footing is insufficient")
	bad = ctx.duplicate()
	bad.support_entity = 51
	rejected(bad, camera, "nearby unrelated support cannot activate recording")
	bad = ctx.duplicate()
	bad.position = Vector3(11.4, 110.9, -181.95)
	rejected(bad, view(bad.position), "same receiver outside clear viewing passage cannot activate")
	bad = ctx.duplicate()
	bad.operating = &"winch"
	rejected(bad, camera, "machine operation cannot earn terminal attention")
	var behind := Transform3D(Basis.IDENTITY, Vector3(12,111.52,-183.4)).looking_at(Record.SCREEN_POSITION)
	rejected(ctx, behind, "back of cabinet cannot activate the front screen")
	rejected(ctx, away, "passing without screen attention cannot activate")
	check_guide()
	check_deliberate_choice()
	print("STORY_HANDOVER %s checks=%d failures=%d supplied_native_facts_only=1" % ["PASS" if failures.is_empty() else "FAIL", checks, failures.size()])
	record.queue_free()
	fresh.queue_free()
	await process_frame
	quit(0 if failures.is_empty() else 1)

func check_guide() -> void:
	var guide := Guide.new()
	# Supplied observations exercise queue policy only, never native footing.
	guide.sample({"position": Vector3(0, 34.9, -150), "grounded": true,
		"checkpoint_footing_valid": true, "support_entity": 51, "deaths": 0}, 0)
	var ctx := context()
	ctx.story_handover_near = true
	ctx.story_handover = &"handover"
	var tick := 2000
	var state: Dictionary = guide.sample(ctx, tick)
	ctx.erase("story_handover")
	for key in Dialogue.KEYS:
		check(state.transmission_key == key and state.transmission == Dialogue.CUES[key] and state.speaker == Dialogue.SPEAKERS[key], "single Guide queue speaks exact authored cue " + str(key))
		var serial: int = state.transmission_id
		check(guide.sample(ctx, tick).transmission_id == serial, "same native tick cannot duplicate cue " + str(key))
		tick = guide._transmission_until + 1
		state = guide.sample(ctx, tick)
	check(state.transmission.is_empty() and state.objective_id == &"crown" and "+253 M" in state.detail and "ARI WARD" in state.detail, "completed exchange adds witness detail while Crown remains goal")
	ctx.story_handover = &"source"
	state = guide.sample(ctx, tick + 1)
	check(state.transmission_key == &"handover_source_survey" and state.transmission == Dialogue.CUES[&"handover_source_survey"], "source comparison retains original qualification")
	var source_id: int = state.transmission_id
	ctx.erase("story_handover")
	tick = guide._transmission_until + 1
	guide.sample(ctx, tick)
	ctx.story_handover = &"handover"
	state = guide.sample(ctx, tick + 1)
	check(state.transmission_key == &"handover_record_assurance" and state.transmission_id > source_id, "explicit handover replay receives fresh serial")
	ctx.erase("story_handover")
	state = guide.sample(ctx, guide._transmission_until + 1)
	check(state.transmission.is_empty() and guide._record_phase == Dialogue.KEYS.size(), "replay never repeats the personal exchange")
	var decision_tick: int = guide._last_tick
	check(guide.handover_choice_available(decision_tick), "completed exchange and source comparison unlock a deliberate challenge")
	check(not guide.choose_handover(&"unknown", decision_tick), "unrecognized choices never change knowledge")
	check(not guide.choose_handover(&"sent_to_dev", decision_tick - 1), "stale choice ticks cannot submit evidence")
	check(guide.choose_handover(&"sent_to_dev", decision_tick), "accepted send commits the single narrative disposition")
	check(not guide.choose_handover(&"ari_first", decision_tick), "a second press cannot unsend the dispatched evidence")
	state = guide.sample(ctx, decision_tick + 1)
	check(state.transmission_key == Dialogue.SEND_KEYS[0] and state.handover_disposition == &"sent_to_dev", "send starts Ro's recorded request with the retained knowledge state")
	state = guide.sample(ctx, guide._transmission_until + 1)
	check(state.transmission_key == Dialogue.SEND_KEYS[1] and state.speaker == "DEV", "Dev acknowledges the actual chosen dispatch without declaring machinery safe")
	check("ORIGINAL PRESERVED" in state.handover_receipt, "dispatch receipt preserves original evidence")
	ctx.deaths = 1
	state = guide.sample(ctx, guide._last_tick + 1)
	check(state.handover_disposition == &"sent_to_dev", "death cannot erase accepted narrative knowledge")
	state = guide.sample(ctx, 0)
	check(state.handover_disposition == &"undecided" and not state.handover_choice_available, "native-time restart resets decision and comparison prerequisites")
	var incomplete := Guide.new()
	incomplete.sample(context(), 0)
	check(not incomplete.choose_handover(&"ari_first", 0), "arrival alone cannot claim comparison or select a witness route")

func check_deliberate_choice() -> void:
	var ctx := context()
	for choice in [&"sent_to_dev", &"ari_first"]:
		var record := Record.new()
		root.add_child(record)
		# Supplied presentation state isolates the screen's press/geometry law;
		# the actual Main path earns this state through Guide and packaged audio.
		record._first_play = false
		record._choice_available = true
		var camera := view(ctx.position, 638, 168 if choice == &"sent_to_dev" else 472)
		record.observe(camera, ctx, 0)
		for tick in range(1, Record.FOCUS_TICKS):
			check(record.observe(camera, ctx, tick).is_empty(), "choice gaze never submits " + str(choice))
		check(record.choice_offer().is_empty(), "partial attention cannot offer a commit " + str(choice))
		check(record.observe(camera, ctx, Record.FOCUS_TICKS).is_empty() and record.choice_offer() == choice, "completed attention highlights without submitting " + str(choice))
		var airborne := ctx.duplicate()
		airborne.grounded = false
		check(record.confirm_choice(camera, airborne, Record.FOCUS_TICKS).is_empty(), "cached highlight cannot submit after footing loss " + str(choice))
		check(record.confirm_choice(camera, ctx, Record.FOCUS_TICKS - 1).is_empty(), "stale Action cannot commit " + str(choice))
		check(record.confirm_choice(camera, ctx, Record.FOCUS_TICKS) == choice, "fresh physical Action returns the aimed choice " + str(choice))
		check(record.confirm_choice(camera, ctx, Record.FOCUS_TICKS).is_empty(), "held or duplicated Action cannot submit twice " + str(choice))
		record.queue_free()
