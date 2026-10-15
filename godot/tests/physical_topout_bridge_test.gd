extends SceneTree

# One initial production approach stage; all subsequent movement is native Action.
const STEP := 1.0 / 90.0
const TOWER := 11
var _native: RefCounted
var _failed := false
var _checks := 0

func _initialize() -> void:
	_run.call_deferred()

func _check(ok: bool, message: String) -> bool:
	_checks += 1
	if not ok:
		_failed = true
		push_error("SCRAPERX_PHYSICAL_TOPOUT_BRIDGE FAIL " + message)
	return ok

func _tick() -> Dictionary:
	_check(_native.advance_frame(STEP) == 1, "one accepted native fixed step")
	var state: Dictionary = _native.get_landing_state()
	_check(float(state["player_gravity_factor"]) == 1.0 and _native.get_death_count() == 0,
		"native gravity active and no recovery death")
	return state

func _run() -> void:
	if not _check(ClassDB.class_exists("ScraperXSimulation"), "native extension loaded"):
		quit(1)
		return
	_native = ClassDB.instantiate("ScraperXSimulation")
	var initial: Dictionary = _native.get_landing_state()
	for key in ["traversal_phase", "traversal_plan_kind", "traversal_rejection_reason",
		"traversal_plan_generation", "traversal_receiver_entity_id", "foot_contact_tick",
		"foot_push_source_contact_tick", "hand_foot_overlap_active", "hand_foot_overlap_solved_ticks",
		"actual_hand_load_n", "actual_foot_load_n", "actual_hand_load_fraction", "actual_foot_load_fraction",
		"hand_foot_last_solve_tick", "hand_foot_last_hand_load_n", "hand_foot_last_foot_load_n"]:
		_check(initial.has(key), "new native dictionary field " + key)
	if _failed:
		quit(1)
		return
	_check(_native.debug_restart_at(Vector3(26.68, 87.92, -132)), "single existing Tower88 approach stage")
	for tick in 45:
		_tick()
	_check(_native.is_player_grounded(), "real initial approach footing")
	_check(_native.set_facing(-1.0, 0.0) and _native.set_move_input(0.0, 0.0), "ordinary facing and neutral input")
	var before: Dictionary = _native.get_landing_state()
	var accepted_before := int(_native.get_accepted_traversal_count())
	_check(_native.request_traversal(), "ordinary Action accepted")
	var state := _tick()
	_check(int(state["traversal_plan_kind"]) == 1 and int(state["traversal_plan_generation"]) > 0
		and int(state["traversal_hand_constraint_count"]) == 2 and not bool(state["foot_push_active"]),
		"native plan starts with real hands and no fabricated foot")
	_check(int(state["traversal_phase"]) > 0 and int(state["traversal_receiver_entity_id"]) == TOWER,
		"native phase and intended receiver read back")
	_check(int(state["left_hand_generation"]) > 0 and int(state["right_hand_generation"]) > 0,
		"existing arm attachment generations preserved")
	_check(_native.get_traversal_left_hand().is_finite() and _native.get_traversal_right_hand().is_finite(),
		"existing native hand positions remain finite")
	var shared := false
	var positive_loads := false
	var exit_found := false
	var phases := [int(state["traversal_phase"])]
	for elapsed in range(1, 153):
		state = _tick()
		var phase := int(state["traversal_phase"])
		if not phases.has(phase):
			phases.append(phase)
		if bool(state["foot_push_active"]):
			_check(int(state["foot_push_source_contact_tick"]) > int(before["foot_contact_tick"]), "foot uses new solved post-Action contact")
			_check(int(state["foot_push_support_entity_id"]) == TOWER, "foot matches real receiving body")
		if int(state["hand_foot_overlap_solved_ticks"]) > int(before["hand_foot_overlap_solved_ticks"]):
			shared = true
			var solved := int(state["hand_foot_last_solve_tick"])
			var solved_hands := float(state["hand_foot_last_hand_load_n"])
			var solved_feet := float(state["hand_foot_last_foot_load_n"])
			_check(solved > int(before["foot_contact_tick"]) and solved <= _native.get_tick_index(), "durable shared receipt is a completed post-Action solve")
			_check(is_finite(solved_hands) and is_finite(solved_feet), "finite durable shared loads")
			positive_loads = positive_loads or (solved_hands > 0.0 and solved_feet > 0.0)
			print("SCRAPERX_TOPOUT_SOLVED_RECEIPT " + JSON.stringify({"published_tick": _native.get_tick_index(),
				"solve_tick": solved, "hand_n": solved_hands, "foot_n": solved_feet,
				"current_overlap": state["hand_foot_overlap_active"], "current_hands": state["traversal_hand_constraint_count"]}))
		_check(bool(state["hand_foot_overlap_active"]) == (int(state["traversal_hand_constraint_count"]) > 0 and bool(state["foot_push_active"])), "current overlap reflects actual published attachment topology")
		if bool(state["hand_foot_overlap_active"]):
			var hands := float(state["actual_hand_load_n"])
			var feet := float(state["actual_foot_load_n"])
			_check(int(state["hand_foot_overlap_solved_ticks"]) > int(before["hand_foot_overlap_solved_ticks"]), "shared state completed actual solve")
			_check(is_finite(hands) and is_finite(feet), "finite actual hand and foot loads")
			if hands + feet > 0.0:
				_check(absf(float(state["actual_hand_load_fraction"]) + float(state["actual_foot_load_fraction"]) - 1.0) < 0.00001,
					"actual load fractions normalize")
			print("SCRAPERX_TOPOUT_SHARED " + JSON.stringify({"tick": _native.get_tick_index(), "phase": phase,
				"contact_tick": state["foot_push_source_contact_tick"], "hand_n": hands, "foot_n": feet}))
		if int(state["traversal_hand_constraint_count"]) == 0 and int(_native.get_accepted_traversal_count()) > accepted_before:
			exit_found = true
			_check(not bool(state["hand_foot_overlap_active"]) and float(state["actual_hand_load_n"]) == 0.0, "completed handoff reports released hands without stale current shared load")
			_check(bool(state["foot_push_active"]), "hand release preserves valid finite foot")
			_check(_native.is_player_grounded() and _native.get_support_entity_id() == TOWER
				and _native.get_player_position().y > 88.7, "hands release on intended actual Tower88 footing")
			break
	_check(exit_found and shared and positive_loads, "bounded native physical hand-foot top-out")
	for tick in 90:
		state = _tick()
	_check(bool(state["checkpoint_footing_valid"]) and not bool(state["foot_push_active"])
		and int(state["traversal_plan_kind"]) == 0 and _native.is_player_grounded()
		and _native.get_support_entity_id() == TOWER, "finite foot expires and actual exit stays firm")
	print("SCRAPERX_PHYSICAL_TOPOUT_BRIDGE %s checks=%d phases=%s final_tick=%d position=%s" % [
		"FAIL" if _failed else "PASS", _checks, str(phases), _native.get_tick_index(), str(_native.get_player_position())])
	quit(1 if _failed else 0)
