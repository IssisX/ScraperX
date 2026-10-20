extends "res://tests/mounted_winch_input_case.gd"

const SOURCE_A := Vector3(-109.8, 1.08, -37.33)
const SOURCE_B := Vector3(-106.2, 1.08, -37.33)
const OPERATOR_A := Vector3(-109.1, .9, -37.9)
const OPERATOR_B := Vector3(-106.9, .9, -37.9)
var stick_down := false

func step(count: int = 1) -> void:
	for i in count:
		var intent: Dictionary = main._router.frame(1.0 / 90.0)
		var look: Vector2 = intent.look
		main._yaw -= look.x
		main._pitch = clampf(main._pitch - look.y, -1.25, 1.35)
		var forward := Vector2(-sin(main._yaw), -cos(main._yaw))
		var right := Vector2(cos(main._yaw), -sin(main._yaw))
		var desired: Vector2 = intent.move
		var movement: Vector2 = right * desired.x + forward * desired.y
		main._native.set_move_input(movement.x, movement.y)
		main._native.set_facing(forward.x, forward.y)
		main._native.set_rigging_aim(-sin(main._yaw) * cos(main._pitch), sin(main._pitch), -cos(main._yaw) * cos(main._pitch))
		main._dispatch(intent.verbs, 1.0 / 90.0)
		main._deliver_tool_pendant_input(intent.pendant)
		main._native.advance_frame(1.0 / 90.0)
		main._ctx = main._read_context()
		main._router.set_rigging_mode(bool(main._ctx.rigging_equipped))
		main._touch.update_context(main._ctx, 1.0)
		main._rigging_view.update_state(main._native.get_rigging_state(), main._audio)

func stick(value: Vector2) -> void:
	var home: Vector2 = main._touch.stick_home()
	if value.is_zero_approx():
		if stick_down:
			var release := InputEventScreenTouch.new()
			release.index = 42
			release.position = home
			release.pressed = false
			main._router._input(release)
		stick_down = false
		return
	if not stick_down:
		var press := InputEventScreenTouch.new()
		press.index = 42
		press.position = home
		press.pressed = true
		main._router._input(press)
		stick_down = true
	var drag := InputEventScreenDrag.new()
	drag.index = 42
	var magnitude := clampf(value.length(), 0.0, .55)
	var offset := value.normalized() * (.1 + .9 * magnitude) * 150.0 * float(main._touch._u)
	drag.position = home + Vector2(offset.x, -offset.y)
	main._router._input(drag)

func walk_to(at: Vector3) -> bool:
	for i in 1080:
		var position: Vector3 = main._native.get_player_position()
		var delta := Vector2(at.x - position.x, at.z - position.z)
		if delta.length() < .08:
			stick(Vector2.ZERO)
			step(12)
			return true
		var forward := Vector2(-sin(main._yaw), -cos(main._yaw))
		var right := Vector2(cos(main._yaw), -sin(main._yaw))
		var velocity := delta.normalized() * minf(.5, delta.length() * 1.5)
		stick(Vector2(velocity.dot(right), velocity.dot(forward)))
		step()
	stick(Vector2.ZERO)
	step()
	check(false, "touch walking reaches operator waypoint " + str(at))
	return false

func cross_to(at: Vector3) -> bool:
	var position: Vector3 = main._native.get_player_position()
	return walk_to(Vector3(position.x, .9, -38.2)) and walk_to(Vector3(at.x, .9, -38.2)) and walk_to(at)

func endpoint(side: float) -> Vector3:
	var body: int = main._native.get_kit_body_index(2499)
	var transform: Transform3D = main._native.get_kit_body_transform(body)
	return transform * Vector3(side * 1.8, .17, 0)

func side_lug(side: float) -> Vector3:
	var body: int = main._native.get_kit_body_index(2499)
	var transform: Transform3D = main._native.get_kit_body_transform(body)
	return transform * Vector3(0, 0, .5 * side)

func approach_stay(side: float) -> bool:
	var position: Vector3 = main._native.get_player_position()
	if side < 0:
		if position.z > -36:
			if not walk_to(Vector3(-101, .9, -34.1)) or not walk_to(Vector3(-101, .9, -40.6)):
				return false
		return walk_to(Vector3(-108, .9, -40.6)) and walk_to(Vector3(-108, .9, -37.9))
	return walk_to(Vector3(-108, .9, -40.6)) and walk_to(Vector3(-101, .9, -40.6)) and \
		walk_to(Vector3(-101, .9, -34.1)) and walk_to(Vector3(-108, .9, -34.1))

func clip_stay_point(at: Vector3, kind: int) -> bool:
	aim(at)
	var state: Dictionary = main._native.get_rigging_state()
	check(int(state.target) != 0 and int(state.target_kind) == kind,
		"actual passive stay eye or fixed guide offered")
	if not failures.is_empty():
		return false
	tap(&"action")
	step(2)
	check(int(main._native.get_rigging_state().failure) == 0, "touch stay installation step accepted")
	return failures.is_empty()

func install_stay(side: float, count: int) -> int:
	if not approach_stay(side):
		return 0
	var operator_z := -36.0 + 1.9 * side
	if not walk_to(Vector3(-108 + 2.5 * side, .9, operator_z)):
		return 0
	if not clip_stay_point(Vector3(-108 + 3.1 * side, 1.3, -36 + 1.2 * side), 0):
		return 0
	if not walk_to(Vector3(-108, .9, operator_z)):
		return 0
	if not clip_stay_point(Vector3(-108, 2.4, -36 + 1.2 * side), 1) or not clip_stay_point(side_lug(side), 0):
		return 0
	step(30)
	var state: Dictionary = main._native.get_rigging_state()
	check(int(state.installed_count) == count and not bool(state.power_selected),
		"touch installs actual passive side stay alongside powered cables")
	return int(state.selected) if failures.is_empty() else 0

func raise_pair_for_unload(sources: Dictionary) -> bool:
	var position: Vector3 = main._native.get_player_position()
	if position.z > -36 and not approach_stay(-1):
		return false
	for entry in [[SOURCE_A, OPERATOR_A, sources.line_a, "paid_a"], [SOURCE_B, OPERATOR_B, sources.line_b, "paid_b"]]:
		if not cross_to(entry[1]):
			return false
		var previous: int = main._native.get_rigging_state().selected
		if not choose(entry[0], int(entry[2]), previous):
			return false
		var before: Dictionary = main._native.get_rigging_state()
		check(float(before.source_energy_used_j) == float(sources[entry[3]]), "stay recovery preserves source ledger before explicit lift")
		if not failures.is_empty():
			return false
		finger(&"pendant_up", true)
		step(90)
		finger(&"pendant_up", false)
		step(12)
		var raised: Dictionary = main._native.get_rigging_state()
		check(float(raised.rest_length_m) < float(before.rest_length_m) - .02 and
			float(raised.source_energy_used_j) > float(before.source_energy_used_j) and bool(raised.braking),
			"real touch lift raises each end to unload cooperating stays then brakes")
		if not failures.is_empty():
			return false
		sources[entry[3]] = float(raised.source_energy_used_j)
	return true

func remove_stay(side: float, token: int, count: int, sources: Dictionary, can_raise: bool = true) -> bool:
	aim(side_lug(side))
	var state: Dictionary = main._native.get_rigging_state()
	check(int(state.target_line) == token and int(main._ctx.rigging_target_line) == token,
		"unique attached beam lug offers exact passive line")
	check(main._ctx.action.label == "SELECT LINE", "ordinary line exposes contextual SELECT LINE")
	if not failures.is_empty():
		return false
	tap(&"action")
	step(2)
	state = main._native.get_rigging_state()
	check(int(state.selected) == token and not bool(state.power_selected), "touch Action selects exact passive stay")
	for i in 180:
		if float(main._native.get_rigging_state().tension_n) <= 60:
			break
		step()
	if float(main._native.get_rigging_state().tension_n) > 60 and can_raise:
		if not raise_pair_for_unload(sources) or not approach_stay(side):
			return false
		return remove_stay(side, token, count, sources, false)
	check(float(main._native.get_rigging_state().tension_n) <= 60, "real solved stay is unloaded before touch UNCLIP")
	if not failures.is_empty():
		return false
	tap(&"rigging_unclip")
	step(2)
	state = main._native.get_rigging_state()
	check(int(state.failure) == 0 and int(state.installed_count) == count and int(state.target_line) == 0,
		"touch UNCLIP removes only selected unloaded stay")
	check(int(state.selected) == 0, "removed stay leaves no obsolete selection")
	return failures.is_empty()

func outcome() -> Dictionary:
	return {"checks": checks, "failures": failures, "fixture_start_only": true,
		"native_tick": main._native.get_tick_index()}

func install(source: Vector3, side: float, count: int) -> int:
	aim(source)
	var state: Dictionary = main._native.get_rigging_state()
	check(int(state.target_kind) == 2 and int(state.target) != 0 and int(state.target_line) == 0,
		"unoccupied registered mount offers installation")
	check(main._ctx.action.label == "CLIP WINCH", "context names winch installation")
	if not failures.is_empty():
		return 0
	tap(&"action")
	step()
	# Clip within assisted reach, then operate from in front of the low panel.
	if not walk_to(Vector3(-108 + 1.1 * side, .9, -36.95)):
		return 0
	aim(endpoint(side))
	check(bool(main._native.get_rigging_state().draft) and int(main._native.get_rigging_state().target) != 0,
		"actual beam eye offered to pending cable")
	if not failures.is_empty():
		return 0
	tap(&"action")
	step(30)
	check(int(main._native.get_rigging_state().installed_count) == count, "near-beam touch clips install expected cable")
	if not failures.is_empty() or not walk_to(OPERATOR_A if side < 0 else OPERATOR_B):
		return 0
	state = main._native.get_rigging_state()
	check(int(state.installed_count) == count and bool(state.power_operable), "touch clips create expected live powered cables")
	if not failures.is_empty():
		return 0
	return int(state.selected)

func choose(source: Vector3, token: int, previous: int) -> bool:
	aim(source)
	var state: Dictionary = main._native.get_rigging_state()
	check(int(state.target_line) == token and int(main._ctx.rigging_target_line) == token,
		"native mount and Main context offer exact installed line")
	check(int(state.selected) == previous, "aiming preserves previous selection")
	check(main._ctx.action.label == "CONTROL WINCH", "context exposes CONTROL WINCH")
	if not failures.is_empty():
		return false
	tap(&"action")
	step(2)
	state = main._native.get_rigging_state()
	check(int(state.selected) == token and bool(state.power_operable), "touch Action selects offered native cable")
	return failures.is_empty()

func operate_selected() -> bool:
	var before: Dictionary = main._native.get_rigging_state()
	check(main._touch.is_button_shown(&"pendant_up") and main._touch.is_button_shown(&"pendant_down") and
		not main._touch._movement_blocked, "powered pendant retains ordinary touch movement")
	if not failures.is_empty():
		return false
	finger(&"pendant_up", true)
	step(378)
	var raised: Dictionary = main._native.get_rigging_state()
	finger(&"pendant_up", false)
	step(12)
	check(float(raised.rest_length_m) < float(before.rest_length_m) - .02 and
		float(raised.source_energy_used_j) > float(before.source_energy_used_j), "UP shortens selected cable using finite source")
	check(bool(main._native.get_rigging_state().braking) and is_zero_approx(float(main._native.get_rigging_state().target_speed_mps)),
		"released UP applies actual native brake")
	if not failures.is_empty():
		return false
	finger(&"pendant_down", true)
	step(45)
	var lowered: Dictionary = main._native.get_rigging_state()
	finger(&"pendant_down", false, true)
	step(12)
	check(float(lowered.rest_length_m) > float(raised.rest_length_m) + .02 and
		float(lowered.source_energy_used_j) == float(raised.source_energy_used_j), "DOWN pays out selected cable without refilling source")
	check(bool(main._native.get_rigging_state().braking) and is_zero_approx(float(main._native.get_rigging_state().target_speed_mps)),
		"canceled DOWN applies actual native brake")
	return failures.is_empty()

func run(shipping_main: Node) -> Dictionary:
	main = shipping_main
	main.set_process(false)
	main._router.capture_mouse = false
	main._router.gameplay_active = true
	main._operating = &""
	check(main._native.debug_restart_at(OPERATOR_A), "single staged start beside crossing source A")
	if not failures.is_empty():
		return outcome()
	step(10)
	tap(&"toggle_rigging")
	step()
	var line_a := install(SOURCE_A, -1, 1)
	if line_a == 0:
		return outcome()
	var a_paid: float = main._native.get_rigging_state().source_energy_used_j
	if not cross_to(OPERATOR_B):
		return outcome()
	check(main._native.get_player_position().distance_to(OPERATOR_A) > 1.5, "touch stick physically transfers operator between sources")
	var line_b := install(SOURCE_B, 1, 2)
	if line_b == 0 or not choose(SOURCE_B, line_b, line_b) or not operate_selected():
		return outcome()
	var b_paid: float = main._native.get_rigging_state().source_energy_used_j
	if not cross_to(OPERATOR_A) or not choose(SOURCE_A, line_a, line_b):
		return outcome()
	check(float(main._native.get_rigging_state().source_energy_used_j) == a_paid, "B operation preserves source A ledger")
	if not failures.is_empty() or not operate_selected():
		return outcome()
	a_paid = float(main._native.get_rigging_state().source_energy_used_j)
	if not cross_to(OPERATOR_B) or not choose(SOURCE_B, line_b, line_a):
		return outcome()
	check(float(main._native.get_rigging_state().source_energy_used_j) == b_paid, "A operation preserves source B ledger")
	if not failures.is_empty():
		return outcome()
	var sources := {"line_a": line_a, "line_b": line_b, "paid_a": a_paid, "paid_b": b_paid}
	var front := install_stay(-1, 3)
	if front == 0:
		return outcome()
	var rear := install_stay(1, 4)
	if rear == 0:
		return outcome()
	tap(&"toggle_rigging")
	step(30)
	var state: Dictionary = main._native.get_rigging_state()
	check(not bool(state.equipped) and int(state.installed_count) == 4, "stow retains both powered cables and both passive stays")
	tap(&"toggle_rigging")
	step(2)
	check(int(main._native.get_rigging_state().installed_count) == 4, "reequip preserves all four cooperating lines")
	if not failures.is_empty() or not remove_stay(1, rear, 3, sources):
		return outcome()
	if not approach_stay(-1) or not remove_stay(-1, front, 2, sources):
		return outcome()
	if not cross_to(OPERATOR_A) or not choose(SOURCE_A, line_a, 0):
		return outcome()
	check(float(main._native.get_rigging_state().source_energy_used_j) == float(sources.paid_a), "passive line operations preserve source A ledger after explicit lift")
	if not failures.is_empty() or not cross_to(OPERATOR_B) or not choose(SOURCE_B, line_b, line_a):
		return outcome()
	check(float(main._native.get_rigging_state().source_energy_used_j) == float(sources.paid_b), "passive line operations preserve source B ledger after explicit lift")
	return outcome()
