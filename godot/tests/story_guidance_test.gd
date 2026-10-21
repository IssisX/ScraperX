extends SceneTree

const Guide := preload("res://presentation/story_guidance.gd")
var checks := 0
var failures: Array[String] = []

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		failures.append(message)
		push_error("STORY_GUIDANCE " + message)

func observation(at: Vector3, support: int = 51, firm: bool = true) -> Dictionary:
	return {"position": at, "support_entity": support, "grounded": firm,
		"checkpoint_footing_valid": firm, "deaths": 0}

func _run() -> void:
	var guide := Guide.new()
	var spawn := observation(Vector3(-94, 1.2, -44))
	spawn.merge({"yard_worker_present": true, "yard_worker_supported": true, "yard_worker_activity": 0})
	var state: Dictionary = guide.sample(spawn, 0)
	check(state.intro_visible and state.objective_id == &"freight_one", "live Receiving starts with clear first destination")
	check(state.transmission_key == &"arrival_iona_empty_record", "actual working witness starts the recorded story and first destination")
	check(not state.objective_visible, "ordinary guidance leaves the permanent mission card hidden")
	var witness := Guide.new()
	var absent := observation(Vector3(-94, 1.2, -44))
	check(witness.sample(absent, 0).transmission.is_empty(), "missing worker cannot be described as still working")
	absent.merge({"yard_worker_present": true, "yard_worker_supported": false, "yard_worker_activity": 0})
	check(witness.sample(absent, 1).transmission.is_empty(), "unsupported worker cannot start the witness line")
	absent.yard_worker_supported = true
	absent.yard_worker_activity = 2
	check(witness.sample(absent, 2).transmission.is_empty(), "a pointing worker is not reported as welding")
	absent.yard_worker_activity = 0
	check(witness.sample(absent, 3).transmission_key == &"arrival_iona_empty_record", "late real work starts the introduction once")
	var id: int = state.transmission_id
	check(guide.sample(spawn, 0).transmission_id == id, "same native tick never repeats dialogue")
	var air := observation(Vector3(0, 352.9, -150), 0, false)
	state = guide.sample(air, 5)
	check(state.objective_id == &"landing" and state.transmission_id == id, "flight apex neither earns arrival nor fabricates a transmission")
	var fake := observation(Vector3(20, 11.9, -119), 2952)
	fake.checkpoint_footing_valid = false
	state = guide.sample(fake, 6)
	check(state.objective_id == &"freight_one", "grounded flag alone cannot complete supported arrival")
	var wrong := observation(Vector3(-100, 12, -94), 1440)
	check(guide.sample(wrong, 7).objective_id == &"freight_one", "unrelated yard height does not claim tower access")
	var net := observation(Vector3(20, 11.9, -119), 2952)
	state = guide.sample(net, 8)
	check(state.objective_id == &"crown" and state.transmission_key == &"arrival_iona_first_ring", "real personnel receiver starts its recorded onward direction")
	check("+22 M" in state.detail and "Cabinet" in state.detail, "first ring names the existing cabinet duct and vent route to22m")
	var higher := Guide.new()
	state = higher.sample(observation(Vector3(6, 352.9, -150), 51), 0)
	check(state.objective_id == &"crown" and not "+33 M" in state.detail, "higher supported bypass never sends player backward to33m")
	check(state.transmission_key == &"receiving_iona_keep_the_height", "higher bypass preserves essential purpose without claiming cargo delivery")
	var dock := Guide.new()
	check(dock.sample(observation(Vector3(-30, 11.9, -140), 1451), 1).objective_id == &"crown", "actual dock personnel arrival is accepted")
	var low := observation(Vector3(0, .9, -150), 51)
	state = guide.sample(low, 9)
	check(state.objective_id == &"return_to_spine", "survived fall gives a current recovery objective")
	low.deaths = 1
	state = guide.sample(low, 10)
	check(state.transmission_key == &"receiving_iona_recovery", "native death change selects one recovery line")
	check(guide.sample(low, 11).transmission_id == state.transmission_id, "recovery line is not spammed every frame")
	var previous_id: int = state.transmission_id
	state = guide.sample(spawn, 0)
	check(state.intro_visible and state.objective_id == &"freight_one", "native time reset clears session narrative history")
	check(state.transmission_id > previous_id, "native retry gets a new serial so its recorded introduction can play")
	guide.configure_voice_lengths({&"arrival_iona_empty_record": 20.0})
	guide.reset()
	state = guide.sample(spawn, 0)
	check(not guide.sample(spawn, 15 * 90).transmission.is_empty(), "long real recording retains caption lifetime")
	check(guide.sample({}, 2000).is_empty(), "missing native observation clears presentation")
	var personal := Guide.new()
	personal.configure_voice_lengths({&"vale_contact_ro": 3.485667, &"vale_contact_iona": 4.2})
	var gallery := observation(Vector3(-101.5, 6.8, -95), 1440)
	check(personal.sample(gallery, 0).transmission.is_empty(), "gallery arrival alone does not imply reading a contact card")
	gallery.story_discovery = &"vale_emergency_contact"
	state = personal.sample(gallery, 1)
	check(state.transmission_key == &"vale_contact_ro" and state.speaker == "RO", "actual discovery begins the authored personal question")
	gallery.erase("story_discovery")
	state = personal.sample(gallery, 347)
	check(state.transmission_key == &"vale_contact_ro", "Ro's actual recorded duration finishes before the reply")
	state = personal.sample(gallery, 348)
	check(state.transmission_key == &"vale_contact_iona" and state.speaker == "IONA" and "fucking recommendation" in state.transmission, "Iona replies in character with the authored adult copy")
	check(state.objective_id == &"freight_one", "conversation never awards tower access or replaces the route objective")
	check(personal.sample(gallery, 900).transmission.is_empty(), "short exchange ends without a replay loop")
	var delayed := Guide.new()
	var starting := spawn.duplicate()
	starting.story_discovery = &"vale_emergency_contact"
	state = delayed.sample(starting, 0)
	check(state.transmission_key == &"arrival_iona_empty_record", "personal discovery does not interrupt necessary introductory speech")
	starting.erase("story_discovery")
	starting.grounded = false
	starting.checkpoint_footing_valid = false
	check(delayed.sample(starting, 1200).transmission_key != &"vale_contact_ro", "queued banter waits through an unsupported fall")
	var approach := Guide.new()
	approach.sample(spawn, 0)
	var aisle := observation(Vector3(-94, .9, -75))
	state = approach.sample(aisle, 1200)
	check(state.transmission_key == &"arrival_iona_personnel_way", "actual ground approach describes the existing mesh route")
	check(approach.sample(aisle, 2400).transmission_id == state.transmission_id, "ground direction is heard once rather than looping")
	var climbing := observation(Vector3(20, 6, -117.5), 0, false)
	check(approach.sample(climbing, 2401).transmission_id == state.transmission_id, "being airborne near mesh cannot claim the player is climbing it")
	climbing.climbing = true
	state = approach.sample(climbing, 2402)
	check(state.transmission_key == &"arrival_iona_mesh_lip", "actual native climb receives the lip cue")
	check(state.objective_id == &"freight_one", "hanging on mesh does not award receiving footing")
	check(approach.sample(climbing, 3600).transmission_id == state.transmission_id, "mesh direction is not repeated during a long hang")
	print("STORY_GUIDANCE %s checks=%d failures=%d supplied_native_facts_only=1" % ["PASS" if failures.is_empty() else "FAIL", checks, failures.size()])
	quit(0 if failures.is_empty() else 1)
