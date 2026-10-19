extends "res://presentation/ui/ui_test_chase.gd"
# The Matron's first act (src/sim/matron.hpp), played on touch from the 154 m
# deck to the 220 m ring: touch_matron_act.
#
# Asleep, the climber rigs Stage A's cage and carries a toolbox from the north
# band's rail to her locker door; its clank set down there wakes her. While
# she strains, the line down to 143 m, and crouched round under her post. Out,
# a pest, unseen: she takes the toolbox, carries it off, and at her post
# undoes the cage's rope. Sprints along 143's shaft edge under her make her
# dangerous; crouched and still, she is a pest again. Through the gate in 143's
# rail and up the riser to her deck: she throws the toolbox at the climber,
# and her hands shove them away. A chase up and down her east band makes her
# dangerous: by the rail her shove throws the climber at the shaft. More
# noise and she is fully changed: her hands kill. Back at the riser's foot
# (the checkpoint, and her a pest again), up again while she looks for the
# climber where she killed them, quietly to her ladder and up it to the 176
# ring, her Turn bringing her up after; on by the climbing route (boards and
# standpipe, catwalk and scaffold), where she cannot follow, to the 220 ring.
#
# With --capture, the screen at each stage: her carrying and throwing, the
# cage she unrigged, her coming dangerous and her shove at the rail, her
# coming fully changed, her coming up the ladder after, and the 220 ring. Between those the drawing is
# switched off (the simulation runs on), so the capture is quick.

const MATRON_STATE_GUARDING := 7
const STAGE_PEST := 0
const STAGE_DANGEROUS := 1
const STAGE_CHANGED := 2
const ACT_TOOLBOX_PICK := Vector2(-6.5, -125.75)
const ACT_TOOLBOX_DOOR := Vector2(6.0, -126.4)
const ACT_143_EDGE_Z := -132.6
const ACT_GATE := Vector2(-6.8, -132.2)
const ACT_PLATE := Vector2(-6.8, -134.1)
const ACT_RISER_FOOT := Vector2(-6.0, -133.85)
const ACT_RIM := Vector2(18.0, -139.0)
const ACT_LAP_NORTH := Vector2(18.4, -134.5)
const ACT_LAP_SOUTH := Vector2(18.4, -143.5)
const WELL_A_CAGE_ENTITY := 2000

var _act_box := 0
var _act_stages_seen := {}
var _act_drawing := true
# Deaths the act means at this point (her fully changed hands: one).
var _act_deaths_allowed := 0


func _act_capturing() -> bool:
	return not _capture_prefix.is_empty() and DisplayServer.get_name() != "headless"


func _act_draw(on: bool) -> void:
	if _act_capturing() and _act_drawing != on:
		_act_drawing = on
		RenderingServer.render_loop_enabled = on


# Every screenshot (the act's, and those the routes it reuses take) with the
# drawing on for it, and as it was after: a screenshot waits on a drawn frame.
func _pose(pose: String) -> void:
	var was := _act_drawing
	if not was:
		_act_draw(true)
		await _frames(3)
	await super._pose(pose)
	_act_draw(was)


func _act_pose(pose: String) -> void:
	await _pose(pose)


func _act_log(tag: String) -> void:
	var m := _matron_state()
	var at := _position()
	var her: Vector3 = m["position"]
	var box := Vector3.ZERO
	if _act_box != 0:
		box = _native().get_kit_body_transform(_native().get_kit_body_index(_act_box)).origin
	print("SCRAPERX_ACT %s t=%.1f me=(%.1f,%.1f,%.1f) her=(%.1f,%.1f,%.1f) state=%d stage=%d rage=%.3f turn=%d carrying=%d takes=%d throws=%d shoves=%d unrigs=%d deaths=%d sees=%s speed=%.2f box=(%.2f,%.2f,%.2f)" % [
		tag, float(Engine.get_process_frames()) / 60.0, at.x, at.y, at.z, her.x, her.y, her.z, int(m["state"]),
		int(m["stage"]), float(m["rage"]), int(m["turn"]), int(m["carrying"]), int(m["takes"]), int(m["throws"]),
		int(m["shoves"]), int(m["unrigs"]), int(_native().get_death_count()), str(m["sees_player"]), float(m["speed"]),
		box.x, box.y, box.z])


# _wait_until, noting every stage she passes through and logging now and then.
func _act_wait(condition: Callable, timeout: float, tag: String) -> bool:
	var waited := 0.0
	var next_log := 0.0
	while true:
		_act_stages_seen[int(_matron_state()["stage"])] = true
		if bool(condition.call()):
			return true
		if waited >= timeout or int(_native().get_death_count()) > _act_deaths_allowed:
			_act_log(tag + ("_timeout" if waited >= timeout else "_died"))
			return false
		if waited >= next_log:
			_act_log(tag)
			next_log += 3.0
		await get_tree().process_frame
		waited += get_process_delta_time()
	return false


# Walks to `target` with the stick held short of `amount` of its throw: under
# the pace whose footfalls carry (3.5 m/s), so a sleeper is not woken.
func _act_creep_to(device: int, target: Vector2, tolerance: float, budget: float, amount: float) -> bool:
	var waited := 0.0
	while waited < budget:
		var at := _position()
		var to := target - Vector2(at.x, at.z)
		if to.length() <= tolerance:
			_move_dir(device, Vector2.ZERO)
			await _seconds(0.2)
			return true
		var turn := 4.0 * get_process_delta_time()
		if to.length() > 1.0:
			_main._yaw = float(_main._yaw) + clampf(wrapf(atan2(-to.x, -to.y) - float(_main._yaw), -PI, PI), -turn, turn)
		var yaw := float(_main._yaw)
		var forward := Vector2(-sin(yaw), -cos(yaw))
		var right := Vector2(cos(yaw), -sin(yaw))
		_move_dir(device, Vector2(to.dot(right), to.dot(forward)).normalized() * clampf(to.length() / 0.8, 0.2, amount))
		await get_tree().process_frame
		waited += get_process_delta_time()
	_move_dir(device, Vector2.ZERO)
	return false


func _act_her_xz() -> Vector2:
	var her: Vector3 = _matron_state()["position"]
	return Vector2(her.x, her.z)


func _act_me_xz() -> Vector2:
	return Vector2(_position().x, _position().z)


# Sprints back and forth between `a` and `b` until `done` holds or `budget`
# seconds pass; the stick let go at the end.
func _act_laps(device: int, a: Vector2, b: Vector2, done: Callable, budget: float, tag: String) -> bool:
	var waited := 0.0
	var leg := 0
	var start := Engine.get_process_frames()
	while waited < budget:
		_act_stages_seen[int(_matron_state()["stage"])] = true
		if bool(done.call()):
			_move(device, 0.0)
			return true
		if int(_native().get_death_count()) > _act_deaths_allowed:
			_move(device, 0.0)
			_act_log(tag + "_died")
			print("SCRAPERX_ACT death %s" % str(_native().get_death_state()))
			return false
		var target := a if leg % 2 == 0 else b
		await _sprint_to(device, target, 1.2, 6.0)
		leg += 1
		_act_log(tag)
		waited = float(Engine.get_process_frames() - start) / 60.0
	_move(device, 0.0)
	_act_log(tag + "_timeout")
	return bool(done.call())


func _matron_act_touch(device: int) -> bool:
	_act_draw(false)
	if not await _act_asleep(device):
		return false
	if not await _act_wake_and_go_below(device):
		return false
	if not await _act_pest_unseen(device):
		return false
	if not await _act_rage_below(device):
		return false
	if not await _act_pest_throws(device):
		return false
	if not await _act_dangerous_and_changed(device):
		return false
	if not await _act_way_up(device):
		return false
	_act_draw(true)
	_detail = "stages_seen=%s deaths=%d she_at=%s" % [str(_act_stages_seen.keys()), int(_native().get_death_count()),
		str(_matron_state()["position"])]
	return true


# Asleep: Stage A's cage rigged, and a toolbox from the north band's rail
# carried to her locker door at a pace that does not wake her; set down
# there, it does.
func _act_asleep(device: int) -> bool:
	await _wait_until(func() -> bool: return bool(_ctx()["grounded"]), 2.0)
	if not await _rig_stage_a(device):
		return false
	for leg in [Vector2(-10.2, -130.6), Vector2(-10.0, -129.2)]:
		if not await _walk_to(device, leg, 0.2):
			return _fail("the walk out of the cage stalled at %s" % str(_position()))
	if not await _act_creep_to(device, ACT_TOOLBOX_PICK, 0.08, 10.0, 0.5):
		return _fail("the walk to the toolbox stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"pick_up", "PICK UP"):
		return _fail("Action read '%s' at the toolbox, not PICK UP" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_carrying_entity_id()) != 0, 1.0):
		return _fail("PICK UP did not put the toolbox in the hands")
	_act_box = int(_native().get_carrying_entity_id())
	if not await _act_creep_to(device, ACT_TOOLBOX_DOOR, 0.2, 14.0, 0.5):
		return _fail("the carry to her locker door stalled at %s" % str(_position()))
	if int(_matron_state()["state"]) != MATRON_STATE_DORMANT:
		return _fail("rigging and the carry woke her (state %d)" % int(_matron_state()["state"]))
	# Set down at her door, its clank wakes her.
	if not await _let_go_if_held(device):
		return _fail("LET GO did not set the toolbox down")
	var woke: bool = await _wait_until(func() -> bool: return int(_matron_state()["state"]) == MATRON_STATE_STRAINING, 2.0)
	_act_log("woke_by_the_toolbox")
	if not woke:
		return _fail("the toolbox set down at her door did not wake her (state %d)" % int(_matron_state()["state"]))
	return true


# While she strains, the line down to 143, and crouched round its deck to the
# shaft's north edge, under her post.
func _act_wake_and_go_below(device: int) -> bool:
	if not (await _walk_to(device, Vector2(3.0, -132.36), 0.3, 6.0, true) and \
			await _walk_to(device, Vector2(6.0, -132.36), 0.15, 6.0, true)) or _position().y < 155.0:
		return _fail("up the ramp onto the line's stand stalled at %s" % str(_position()))
	_move_dir(device, Vector2.ZERO)
	await _face(Vector2(0.0, -1.0))
	_press(&"jump", 1)
	if not await _wait_until(func() -> bool: return bool(_native().get_zipline_state()["riding"]), 1.5):
		return _fail("the jump from the stand never took the trolley's bar (at %s)" % str(_position()))
	if bool(_matron_state()["released"]):
		return _fail("she was out before the climber was on the line")
	var line := int(_native().get_zipline_state()["line"])
	var length := float(_native().get_zipline_lines()[line]["length"])
	var holding_back := false
	var stopped := false
	var waited := 0.0
	while waited < 16.0 and not stopped:
		await get_tree().process_frame
		waited += get_process_delta_time()
		var zip: Dictionary = _native().get_zipline_state()
		if not bool(zip["riding"]):
			return _fail("the hands came off the bar mid-ride (at %s)" % str(_position()))
		var speed := absf(float(zip["speed_mps"]))
		var brake := float(zip["along_m"]) > length - 12.0 and speed > 6.0
		if brake and not holding_back:
			_stick_push(Vector2(0.0, 1.0))
		elif not brake and holding_back:
			_move_dir(device, Vector2.ZERO)
		holding_back = brake
		stopped = float(zip["along_m"]) > length - 1.5 and speed < 0.3
	_move_dir(device, Vector2.ZERO)
	if not stopped:
		return _fail("the ride never stopped at the bottom")
	await _frames(2)
	_press(&"drop", 1)
	if not await _wait_until(func() -> bool: return bool(_native().is_player_grounded()) and _position().y < 145.0, 2.5):
		return _fail("DROP at the bottom did not put the body down on 143 m (at %s)" % str(_position()))
	if not await _crouch_on(device, true):
		return _fail("CROUCH did not crouch the climber")
	for leg in [Vector2(-MATRON_BAND_X, -171.5), Vector2(-MATRON_BAND_X, MATRON_NORTH_Z), Vector2(-16.0, ACT_143_EDGE_Z)]:
		if not await _go(device, leg, 0.6, 40.0):
			return _fail("crouched round 143 stalled at %s" % str(_position()))
	_act_log("under_her_post")
	return true


# Out, a pest: unseen, she takes the toolbox from her door and carries it
# off, and at her post undoes Stage A's rope.
func _act_pest_unseen(device: int) -> bool:
	var seen := {"carried": false}
	var done: bool = await _act_wait(func() -> bool:
		var m := _matron_state()
		seen["carried"] = bool(seen["carried"]) or int(m["carrying"]) == _act_box
		return int(m["unrigs"]) >= 1 and int(m["state"]) == MATRON_STATE_GUARDING, 150.0, "pest_unseen")
	if not done:
		return _fail("she never stood guard having undone the cage's rope (state %d, unrigs %d)" % [
			int(_matron_state()["state"]), int(_matron_state()["unrigs"])])
	if not bool(seen["carried"]) or int(_matron_state()["takes"]) < 1:
		return _fail("a pest, she never took the toolbox from her door (takes %d)" % int(_matron_state()["takes"]))
	if int(_native().get_well_a_rope_end_entity_id()) == WELL_A_CAGE_ENTITY:
		return _fail("her unrigging left the rope's end on the cage's eye")
	if int(_matron_state()["stage"]) != STAGE_PEST or int(_native().get_death_count()) != 0:
		return _fail("out and unseen, she was not a pest (stage %d)" % int(_matron_state()["stage"]))
	_act_log("pest_unrigged")
	return true


# Sprints along 143's shaft edge under her post, heard up the shaft, make her
# dangerous; crouched and still, she is a pest again.
func _act_rage_below(device: int) -> bool:
	await _crouch_on(device, false)
	var raged: bool = await _act_laps(device, Vector2(-4.0, ACT_143_EDGE_Z), Vector2(-16.0, ACT_143_EDGE_Z),
		func() -> bool: return int(_matron_state()["stage"]) == STAGE_DANGEROUS, 150.0, "rage_below")
	if not raged:
		return _fail("sprints under her never made her dangerous (rage %.3f)" % float(_matron_state()["rage"]))
	_act_log("dangerous_below")
	await _crouch_on(device, true)
	var calmed: bool = await _act_wait(func() -> bool: return int(_matron_state()["stage"]) == STAGE_PEST, 150.0,
		"quiet_below")
	if not calmed:
		return _fail("crouched and still, she never came back down to a pest (rage %.3f)" % float(_matron_state()["rage"]))
	_act_log("pest_again")
	return true


# Up the riser to her deck: she fetches the toolbox, throws it, and her hands
# shove the climber away.
func _act_pest_throws(device: int) -> bool:
	var throws0 := int(_matron_state()["throws"])
	if not await _act_up_riser(device):
		return false
	_act_log("on_her_deck")
	# The eyes on her: heard on the rungs she has the toolbox, and throws it.
	var thrown: bool = await _act_watch_her(func() -> bool: return int(_matron_state()["throws"]) > throws0, 25.0,
		"pest_throw")
	if not thrown:
		return _fail("she never threw the toolbox at the climber (takes %d, carrying %d)" % [
			int(_matron_state()["takes"]), int(_matron_state()["carrying"])])
	# Back off the riser's head, away from the open rim beside its bridge.
	if not await _walk_to(device, Vector2(-6.0, -128.2), 0.2, 4.0):
		return _fail("the step back along the runway stalled at %s" % str(_position()))
	var cage_eye := Vector3(-11.35, MATRON_DECK_Y + 1.0, -131.4)
	await _look_at_point(cage_eye)
	await _act_pose("act_pest_unrigged")
	var shoves0 := int(_matron_state()["shoves"])
	var shoved: bool = await _act_watch_her(func() -> bool: return int(_matron_state()["shoves"]) > shoves0, 25.0,
		"pest_shove")
	if not shoved or int(_native().get_death_count()) != 0 or int(_matron_state()["stage"]) > STAGE_PEST:
		return _fail("a pest, her hands did not shove the climber (shoves %d, deaths %d, stage %d)" % [
			int(_matron_state()["shoves"]), int(_native().get_death_count()), int(_matron_state()["stage"])])
	_act_log("pest_shoved")
	return true


# _act_wait with the eyes kept on her (turned at a thumb's 4 rad/s), and the
# screen taken the first time she is seen carrying the toolbox, and just after
# each throw.
func _act_watch_her(condition: Callable, timeout: float, tag: String, near_pose := "", near_m := 0.0) -> bool:
	var waited := 0.0
	var next_log := 0.0
	var carried := false
	var near_posed := near_pose.is_empty()
	var throws := int(_matron_state()["throws"])
	while true:
		var m := _matron_state()
		_act_stages_seen[int(m["stage"])] = true
		if bool(condition.call()):
			return true
		if waited >= timeout or int(_native().get_death_count()) > _act_deaths_allowed:
			_act_log(tag + ("_timeout" if waited >= timeout else "_died"))
			return false
		if waited >= next_log:
			_act_log(tag)
			next_log += 3.0
		var to := _act_her_xz() - _act_me_xz()
		var off := 0.0
		if to.length() > 0.3:
			var turn := 4.0 * get_process_delta_time()
			off = wrapf(atan2(-to.x, -to.y) - float(_main._yaw), -PI, PI)
			_main._yaw = float(_main._yaw) + clampf(off, -turn, turn)
		if not near_posed and to.length() < near_m and absf(off) < 0.3:
			near_posed = true
			await _act_pose(near_pose)
		if not carried and int(m["carrying"]) == _act_box and absf(off) < 0.25:
			carried = true
			await _act_pose("act_pest_carrying")
		if int(m["throws"]) > throws:
			throws = int(m["throws"])
			await _seconds(0.15)
			await _act_pose("act_pest_throw")
		await get_tree().process_frame
		waited += get_process_delta_time()
	return false


func _act_up_riser(device: int, off_its_head := true) -> bool:
	await _crouch_on(device, false)
	# From 143's band through the gate in the shaft's rail onto deck 13's
	# walkway plate, to the riser's foot.
	if not (await _go(device, ACT_GATE, 0.2, 12.0) and await _walk_to(device, ACT_PLATE, 0.15, 4.0)):
		return _fail("through the rail's gate onto the plate stalled at %s" % str(_position()))
	if not await _walk_to(device, ACT_RISER_FOOT, 0.08, 6.0):
		return _fail("the step to the riser's foot stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("Action read '%s' facing the riser, not CLIMB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("CLIMB did not take hold of the riser")
	_move(device, 1.0)
	var up: bool = await _act_wait(func() -> bool: return _standing_above(154.5), 30.0, "riser")
	_move(device, 0.0)
	if not up:
		return _fail("the riser to her deck failed (y %.2f)" % _position().y)
	if off_its_head and not await _walk_to(device, Vector2(-6.0, -130.0), 0.15, 4.0):
		return _fail("the step off the riser's head stalled at %s" % str(_position()))
	return true


# A chase round her deck makes her dangerous; by the shaft's rim her shove
# throws the climber at it. More noise, and fully changed her hands kill.
func _act_dangerous_and_changed(device: int) -> bool:
	# Round onto the east band, and sprints up and down it beside the shaft's
	# rail (whatever her hands do there throws the climber at the rail, not
	# off the deck's open outer edge).
	if not (await _sprint_to(device, Vector2(16.0, -128.6), 1.0, 8.0) and \
			await _sprint_to(device, ACT_LAP_NORTH, 1.0, 6.0)):
		return _fail("the run round onto the east band stalled at %s" % str(_position()))
	var raged: bool = await _act_laps(device, ACT_LAP_SOUTH, ACT_LAP_NORTH,
		func() -> bool: return int(_matron_state()["stage"]) >= STAGE_DANGEROUS, 150.0, "chase_to_dangerous")
	if not raged:
		return _fail("the chase never made her dangerous (rage %.3f)" % float(_matron_state()["rage"]))
	_act_log("dangerous")
	# By the rail, still: she comes, and shoves at the shaft.
	if not await _go(device, ACT_RIM, 0.3, 10.0):
		return _fail("the walk to the shaft's rail stalled at %s" % str(_position()))
	var shoves := int(_matron_state()["shoves"])
	var x0 := _position().x
	var watch := {"nearest": x0}
	var shoved: bool = await _act_watch_her(func() -> bool:
		watch["nearest"] = minf(float(watch["nearest"]), _position().x)
		return int(_matron_state()["shoves"]) > shoves and int(_matron_state()["stage"]) == STAGE_DANGEROUS, 30.0,
		"dangerous_at_rail", "act_dangerous_coming", 4.0)
	if not shoved:
		return _fail("dangerous, she never shoved the climber at the rail (stage %d)" % int(_matron_state()["stage"]))
	await _act_wait(func() -> bool:
		watch["nearest"] = minf(float(watch["nearest"]), _position().x)
		return false, 0.6, "rail")
	await _act_pose("act_dangerous_shove")
	_act_log("rail_shove dx=%.2f" % (float(watch["nearest"]) - x0))
	if float(watch["nearest"]) > x0 - 0.3:
		return _fail("dangerous, her shove did not throw the climber at the shaft (dx %.2f)" % (
			float(watch["nearest"]) - x0))
	var raged_more: bool = await _act_laps(device, ACT_LAP_SOUTH, ACT_LAP_NORTH,
		func() -> bool: return int(_matron_state()["stage"]) == STAGE_CHANGED, 150.0, "chase_to_changed")
	if not raged_more:
		return _fail("the chase never changed her fully (rage %.3f)" % float(_matron_state()["rage"]))
	_act_log("changed")
	_act_deaths_allowed = 1
	var killed: bool = await _act_watch_her(func() -> bool: return int(_native().get_death_count()) >= 1, 30.0,
		"changed_kill", "act_changed_coming", 3.0)
	if not killed:
		return _fail("fully changed, she never caught the climber")
	if int(_matron_state()["catches"]) < 1:
		return _fail("the climber died, but not in her hands (%s)" % str(_native().get_death_state()))
	await _seconds(0.5)
	_act_log("killed")
	return true


# Back at the riser's foot, the checkpoint: up again while she looks for the
# climber where she killed them, quietly to her ladder and up it to the 176
# ring; on by the climbing route, no lift, to the 220 ring (its boards are
# narrower than she is: she stays on the ring).
func _act_way_up(device: int) -> bool:
	if _position().y > 150.0:
		return _fail("the death did not put the climber back below her deck (at %s)" % str(_position()))
	if not await _act_up_riser(device):
		return false
	_act_log("back_on_her_deck")
	for leg in [Vector2(3.5, -128.2), Vector2(3.5, -129.8), Vector2(5.0, -129.6)]:
		if not await _act_creep_to(device, leg, 0.08 if leg.x > 4.0 else 0.15, 10.0, 0.5):
			return _fail("the creep to the ladder's foot stalled at %s (deaths %d)" % [str(_position()),
				int(_native().get_death_count())])
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"climb", "CLIMB", "HOLD"):
		return _fail("Action read '%s' facing the ladder, not CLIMB HOLD" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("CLIMB did not take hold of the ladder")
	_move(device, 1.0)
	var on_ring: bool = await _act_wait(func() -> bool: return _standing_above(176.5), 40.0, "ladder")
	_move(device, 0.0)
	if not on_ring or int(_native().get_death_count()) != 1:
		return _fail("the climb did not top out onto the 176 ring (y %.2f, deaths %d, she climbing %s)" % [
			_position().y, int(_native().get_death_count()), str(_matron_state()["climbing"])])
	_act_log("on_176")
	# A look back down the ladder: in her Turn she took hold, and comes up it.
	if _act_her_xz().distance_to(Vector2(5.0, -129.55)) < 2.5 and _act_capturing():
		await _face(Vector2(0.0, -1.0))
		await _tilt(-1.35)
		await _act_pose("act_up_the_ladder_after_you")
		await _tilt(0.0)
	# On by the climbing route, no lift: the boards out from the ring to the
	# standpipe and up it to 198; the catwalk under the scaffold panel, a jump
	# for it, and up it to 220.
	for leg in [Vector2(7.05, -128.4), Vector2(7.05, -130.9), Vector2(7.6, -130.9)]:
		if not await _walk_to(device, leg, 0.08 if leg.x > 7.5 else 0.1, 8.0, true):
			return _fail("along the boards to the standpipe stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not await _offered(&"climb", "CLIMB"):
		return _fail("Action read '%s' facing the standpipe, not CLIMB" % _action_label())
	_act(device)
	if not await _wait_until(func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 0.5):
		return _fail("CLIMB did not take hold of the standpipe")
	_move(device, 1.0)
	var on_198: bool = await _act_wait(func() -> bool: return _standing_above(198.5), 40.0, "standpipe")
	_move(device, 0.0)
	if not on_198:
		return _fail("the standpipe did not top out onto the 198 ring (y %.2f)" % _position().y)
	if not (await _walk_to(device, Vector2(11.0, -129.3), 0.1, 16.0, true) and \
			await _walk_to(device, Vector2(11.0, -131.4), 0.08, 6.0, true)):
		return _fail("out along the catwalk under the scaffold stalled at %s" % str(_position()))
	await _face(Vector2(0.0, 1.0))
	if not await _leap(device, 0.6, func() -> bool: return int(_native().get_traversal_state()) == TRAVERSAL_CLIMBING, 3.0):
		return _fail("the jump for the scaffold panel never caught it (at %s)" % str(_position()))
	_move(device, 1.0)
	var on_220: bool = await _act_wait(func() -> bool: return _standing_above(220.5), 40.0, "scaffold")
	_move(device, 0.0)
	if not on_220:
		return _fail("the scaffold panel did not top out onto the 220 ring (y %.2f)" % _position().y)
	_act_log("on_220")
	if int(_native().get_death_count()) != 1:
		return _fail("the climb on from her deck cost a death (deaths %d)" % int(_native().get_death_count()))
	for stage in [STAGE_PEST, STAGE_DANGEROUS, STAGE_CHANGED]:
		if not _act_stages_seen.has(stage):
			return _fail("she never reached stage %d" % stage)
	await _act_pose("act_220")
	return true
