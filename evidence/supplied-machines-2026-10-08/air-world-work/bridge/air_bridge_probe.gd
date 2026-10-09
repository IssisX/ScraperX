extends SceneTree

func _initialize() -> void:
	_run.call_deferred()

func _require(ok: bool, message: String) -> bool:
	if not ok:
		push_error("SCRAPERX_AIR_BRIDGE FAIL " + message)
		quit(1)
	return ok

func _run() -> void:
	if not _require(ClassDB.class_exists("ScraperXSimulation"), "native class bound"):
		return
	var native: Object = ClassDB.instantiate("ScraperXSimulation")
	if not _require(native != null, "native authority constructed"):
		return
	if not _require(bool(native.set_move_input(0.0, 0.0)) and bool(native.set_facing(1.0, 0.0)), "ordinary neutral/facing commands accepted"):
		return
	for tick in 90:
		if not _require(int(native.advance_frame(1.0 / 90.0)) == 1, "one90Hz native step"):
			return
	if not _require(bool(native.is_player_grounded()) and int(native.get_support_entity_id()) == 1, "normal grade real support"):
		return
	var initial: Dictionary = native.get_landing_state()
	for field in ["air_control_command_positive_work_j", "air_control_command_absorbed_work_j", "air_control_force_n"]:
		if not _require(initial.has(field), "native dictionary field " + field):
			return
	var positive := float(initial["air_control_command_positive_work_j"])
	var absorbed := float(initial["air_control_command_absorbed_work_j"])
	if not _require(bool(native.request_jump()) and int(native.advance_frame(1.0 / 90.0)) == 1, "ordinary grade Jump accepted"):
		return
	if not _require(not bool(native.is_player_grounded()) and int(native.get_support_entity_id()) == 0 and native.get_player_linear_velocity().y > 5.0, "actual ordinary airborne takeoff"):
		return
	if not _require(bool(native.set_move_input(1.0, 0.0)), "ordinary air side input accepted"):
		return
	var force_seen := false
	var peak_force := 0.0
	for tick in 24:
		if not _require(int(native.advance_frame(1.0 / 90.0)) == 1, "air native step"):
			return
		var state: Dictionary = native.get_landing_state()
		var next_positive := float(state["air_control_command_positive_work_j"])
		var next_absorbed := float(state["air_control_command_absorbed_work_j"])
		var force: Vector3 = state["air_control_force_n"]
		if not _require(not bool(native.is_player_grounded()) and int(native.get_support_entity_id()) == 0 and int(native.get_traversal_state()) == 0 and not bool(native.is_parachute_deployed()), "uncontacted ordinary air sample"):
			return
		if not _require(is_finite(next_positive) and is_finite(next_absorbed) and force.is_finite() and next_positive >= positive and next_absorbed >= absorbed and next_positive - positive <= 3000.1 / 90.0, "finite monotone bounded command readbacks"):
			return
		if not _require(force.x >= 0.0 and absf(force.y) < 0.0001 and absf(force.z) < 0.0001, "actual horizontal air force readback"):
			return
		force_seen = force_seen or force.x > 0.01
		peak_force = maxf(peak_force, force.length())
		positive = next_positive
		absorbed = next_absorbed
	if not _require(force_seen and positive > float(initial["air_control_command_positive_work_j"]) and native.get_player_linear_velocity().x > 0.5, "bound bridge reports real steering force and earned work"):
		return
	if not _require(bool(native.set_move_input(0.0, 0.0)) and int(native.advance_frame(1.0 / 90.0)) == 1, "ordinary air input release"):
		return
	var neutral: Dictionary = native.get_landing_state()
	var neutral_force: Vector3 = neutral["air_control_force_n"]
	if not _require(neutral_force == Vector3.ZERO and float(neutral["air_control_command_positive_work_j"]) == positive and float(neutral["air_control_command_absorbed_work_j"]) == absorbed, "neutral air force resets without inventing work"):
		return
	print("SCRAPERX_AIR_BRIDGE PASS fields=3 grade_jump=1 ordinary_air_input=1 samples=24 finite_monotone=1 peak_force_n=%.6f positive_work_j=%.6f absorbed_work_j=%.6f neutral_force_zero=1 tick=%d player=%s velocity=%s support=%d" % [peak_force, positive, absorbed, native.get_tick_index(), native.get_player_position(), native.get_player_linear_velocity(), native.get_support_entity_id()])
	native = null
	quit(0)
