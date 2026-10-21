extends SceneTree

# Shipping scene/bridge and viewport touch events; diagnostic clock only.
# Roof and launcher diagnostic staging; no direct native actions or crossing/phone evidence.
# godot --headless --path godot --script res://tests/grappling_gun_test.gd
const TouchControls := preload("res://presentation/ui/touch_controls.gd")
const Router := preload("res://presentation/ui/input_router.gd")
const STEP := 1.0 / 90.0
var _main: Node
var _native: Object
var _checks := 0
var _failed := false

func _initialize() -> void:
	_run.call_deferred()

func _check(ok: bool, label: String) -> bool:
	_checks += 1
	if not ok:
		_failed = true
		push_error("SCRAPERX_GRAPPLE_TOUCH FAIL " + label)
	return ok

func _step(count: int = 1, delta: float = STEP) -> void:
	for i in count:
		await process_frame
		_main._process(delta)

func _touch(at: Vector2, pressed: bool, index: int = 0) -> void:
	var event := InputEventScreenTouch.new()
	event.index = index
	event.position = at
	event.pressed = pressed
	root.push_input(event, true)

func _button(id: StringName, held: bool) -> bool:
	var button = _main._touch._buttons[id]
	if not _check(button.shown and button.enabled and button.appear >= 0.2,
			"real touch control offered: " + String(id)):
		return false
	_touch(_main._touch.button_center(id), held)
	return true

func _tap(id: StringName) -> bool:
	if id in [&"grapple", &"toggle_rigging", &"toggle_traversal_line"] and not _main._touch._tool_tray_open:
		if not _tap(&"tools"):
			return false
		_main._touch.update_context(_main._ctx, 1.0)
	if not _button(id, true):
		return false
	_touch(_main._touch.button_center(id), false)
	return true

func _stick(held: bool, direction: float = 1.0) -> void:
	var home: Vector2 = _main._touch.stick_home()
	_touch(home, held, 6)
	if held:
		var drag := InputEventScreenDrag.new()
		drag.index = 6
		drag.relative = Vector2(direction * 150.0 * _main._touch._u, 0)
		drag.position = home + drag.relative
		root.push_input(drag, true)

func _walk(label: String) -> void:
	var before: Vector3 = _native.get_player_position()
	_stick(true)
	await _step(10)
	_stick(false)
	var after: Vector3 = _native.get_player_position()
	_check(Vector2(after.x - before.x, after.z - before.z).length() > 0.02,
		label + " viewport stick moves actual native player")
	_stick(true, -1.0)
	await _step(10)
	_stick(false)
	await _step(8)

func _chute_hit_contract() -> void:
	# Isolated context fixture; exercise the real hit ordering and router verb.
	var touch := TouchControls.new()
	var router := Router.new()
	root.add_child(touch)
	touch.router = router
	await process_frame
	touch._layout()
	var ctx := {"hanging": false, "jump_ok": false,
		"action": {"id": &"grapple_action", "icon": &"hook", "label": "FIRE HOOK"},
		"drop_ok": true, "chute_ok": true, "chute": false, "danger": 0.7,
		"operating": &"grappling_gun", "crouched": false, "slung": false,
		"grapple_equipped": true, "grapple_line_attached": false}
	for attached in [false, true]:
		ctx["grapple_line_attached"] = attached
		touch.update_context(ctx, 1.0)
		var event := InputEventScreenTouch.new()
		event.index = 9
		event.position = touch.button_center(&"chute")
		event.pressed = true
		touch.handle_touch(event)
		_check(router.frame(STEP)["verbs"] == [&"chute"],
			"visible Chute dispatches chute with grapple attached=" + str(attached))
		event.pressed = false
		touch.handle_touch(event)
	ctx["drop_ok"] = false
	ctx["grapple_equipped"] = false
	ctx["operating"] = &""
	touch.update_context(ctx, 1.0)
	_check(touch.button_center(&"chute").is_equal_approx(touch.button_center(&"drop")),
		"ordinary freefall retains established chute slot")
	touch.queue_free()
	router.free()

func _auto_reel_context_contract() -> void:
	# Real touch/router boundary; native cleanup is exercised in native tests.
	var touch := TouchControls.new()
	var router := Router.new()
	root.add_child(touch)
	touch.router = router
	await process_frame
	var ctx := {"hanging": false, "jump_ok": true,
		"action": {"id": &"grapple_action", "icon": &"hook", "label": "LOCK LINE"},
		"drop_ok": true, "chute_ok": false, "chute": false, "danger": 0.0,
		"operating": &"grappling_gun", "crouched": false, "slung": false,
		"grapple_equipped": true, "grapple_line_attached": true, "grapple_auto_reel": false}
	for running in [false, true]:
		ctx["grapple_auto_reel"] = running
		ctx["action"] = {"id": &"grapple_reel", "icon": &"operate", "label": "STOP REEL" if running else "REEL IN"}
		touch.update_context(ctx, 1.0)
		_check(not touch.is_button_shown(&"auto_reel"), "primary Action has no duplicate automatic reel target")
		_check(touch.button_label(&"action") == ("STOP REEL" if running else "REEL IN"),
			"primary reel label follows native running=" + str(running))
		var event := InputEventScreenTouch.new()
		event.index = 9
		event.position = touch.button_center(&"action")
		event.pressed = true
		touch.handle_touch(event)
		_check(router.frame(STEP)["verbs"] == [&"action"], "reel touch emits one primary semantic action")
		event.pressed = false
		touch.handle_touch(event)
		_check(router.frame(STEP)["verbs"].is_empty(), "reel release emits no stop or repeated toggle")
		_check(not touch._movement_blocked, "primary reel preserves portable movement stick")
	for cleanup in ["grapple_line_attached", "grapple_equipped"]:
		ctx[cleanup] = false
		touch.update_context(ctx, 1.0)
		_check(not touch.is_button_shown(&"auto_reel") and not touch._buttons[&"auto_reel"].enabled,
			"duplicate reel remains absent after native cleanup: " + cleanup)
		ctx[cleanup] = true
	touch.queue_free()
	router.free()

func _pendant_event_contract() -> void:
	# InputRouter samples before Main refreshes context; releases must be current.
	var touch := TouchControls.new()
	var router := Router.new()
	root.add_child(touch)
	touch.router = router
	router.touch = touch
	await process_frame
	var ctx := {"hanging": false, "jump_ok": true,
		"action": {"id": &"grapple_action", "icon": &"hook", "label": "LOCK LINE"},
		"drop_ok": true, "chute_ok": false, "chute": false, "danger": 0.0,
		"operating": &"grappling_gun", "crouched": false, "slung": false,
		"grapple_equipped": true, "grapple_line_attached": true, "grapple_auto_reel": false}
	touch.update_context(ctx, 1.0)
	var event := InputEventScreenTouch.new()
	event.index = 8
	event.position = touch.button_center(&"pendant_up")
	event.pressed = true
	touch.handle_touch(event)
	_check(router.frame(STEP)["pendant"] == Vector2(0, 1), "manual UP press reaches router before context refresh")
	# Reproduce a held manual frame followed by release and immediate AUTO.
	touch.update_context(ctx, 1.0)
	event.pressed = false
	touch.handle_touch(event)
	event.position = touch.button_center(&"action")
	event.pressed = true
	touch.handle_touch(event)
	var immediate: Dictionary = router.frame(STEP)
	_check(immediate["pendant"] == Vector2.ZERO and immediate["verbs"] == [&"action"],
		"UP release then immediate AUTO delivers neutral pendant and one toggle")
	event.pressed = false
	touch.handle_touch(event)
	ctx["operating"] = &"intake"
	touch.update_context(ctx, 1.0)
	for fixture in [[&"pendant_up", Vector2(0, 1)], [&"pendant_down", Vector2(0, -1)],
			[&"pendant_left", Vector2(-1, 0)], [&"pendant_right", Vector2(1, 0)]]:
		event.position = touch.button_center(fixture[0])
		event.pressed = true
		touch.handle_touch(event)
		_check(router.frame(STEP)["pendant"] == fixture[1], "fresh pendant press reaches router: " + String(fixture[0]))
		touch.update_context(ctx, 1.0)
		event.pressed = false
		touch.handle_touch(event)
		_check(router.frame(STEP)["pendant"] == Vector2.ZERO, "pendant release clears router immediately: " + String(fixture[0]))
	event.position = touch.button_center(&"pendant_up")
	event.pressed = true
	touch.handle_touch(event)
	touch.reset_touches()
	_check(router.frame(STEP)["pendant"] == Vector2.ZERO, "touch reset clears manual pendant")
	touch.handle_touch(event)
	ctx["operating"] = &""
	touch.update_context(ctx, 1.0)
	_check(router.frame(STEP)["pendant"] == Vector2.ZERO, "lost machine capability clears manual pendant")
	touch.queue_free()
	router.free()

func _falling_chute() -> void:
	if not _tap(&"action"):
		return
	await _step(8)
	_check(bool(_state()["equipped"]), "draw acquired gun again before actual fall")
	# The diagnostic roof is finite; ordinary lateral stick input walks off it.
	_stick(true, -1.0)
	var offered := false
	for i in 360:
		await _step()
		if _main._touch._buttons[&"chute"].shown and _main._touch._buttons[&"chute"].appear >= 0.2:
			offered = true
			break
	_stick(false)
	if not _check(offered and not _native.is_player_grounded() and bool(_state()["equipped"]),
			"ordinary movement reaches actual equipped freefall with visible Chute"):
		return
	if not _tap(&"chute"):
		return
	await _step(2)
	_check(bool(_main._ctx["chute"]) and bool(_state()["equipped"]),
		"actual falling Chute press opens native canopy without stowing grapple")

func _launcher_pitch() -> void:
	var launcher: Dictionary = _native.get_slingshot_state()
	var station: Vector3 = launcher["neutral_position"]
	if not _check(_native.debug_restart_at(station + Vector3.UP * 0.76), "native launcher diagnostic staging"):
		return
	await _step(2)
	if not _check(_main._ctx["action"]["id"] == &"slingshot", "actual launcher offers touch board"):
		return
	if not _tap(&"action"):
		return
	await _step(2)
	launcher = _native.get_slingshot_state()
	if not _check(bool(launcher["seated"]), "touch Action boards native launcher"):
		return
	var elevation: float = launcher["elevation_rad"]
	_check(elevation > 1.35, "native launcher rail exceeds ordinary look clamp")
	await _step(1, 0.0)
	_check(absf(float(_main._pitch) - elevation) < 0.00001,
		"seated launcher camera preserves actual native rail pitch")
	# Look changes the aim request; it must not also move the rail camera directly.
	var at := root.get_visible_rect().size * Vector2(0.78, 0.35)
	_touch(at, true, 1)
	var drag := InputEventScreenDrag.new()
	drag.index = 1
	drag.position = at + Vector2(0, 10)
	drag.relative = Vector2(0, 10)
	root.push_input(drag, true)
	_touch(drag.position, false, 1)
	await _step(1, 0.0)
	_check(absf(float(_main._pitch) - elevation) < 0.00001,
		"launcher look request leaves zero-tick camera aligned with native rail")

func _nearby_recovery_action() -> void:
	# Isolated roof pose fixture. Tests command/HUD intent, never route recovery.
	if not _check(_native.debug_restart_at(Vector3(12, 485.15, -135.5)), "nearby recovery diagnostic roof staging"):
		return
	await _step(45)
	if not _tap(&"action"):
		return
	await _step(8)
	if int(_state().shot_profile) == 1:
		if not _tap(&"grapple_range"):
			return
		await _step()
	_check(int(_state().shot_profile) == 0, "real touch chooses near profile for local recovery diagnostic")
	var at := root.get_visible_rect().size * Vector2(0.78, 0.35)
	var sensitivity: float = Router.LOOK_RADIANS_PER_UNIT * _main._router.look_sensitivity
	var relative := Vector2((float(_main._yaw) + PI * 0.5) / sensitivity,
		float(_main._pitch) / sensitivity)
	_touch(at, true, 1)
	var drag := InputEventScreenDrag.new()
	drag.index = 1
	drag.position = at + relative
	drag.relative = relative
	root.push_input(drag, true)
	_touch(at + relative, false, 1)
	await _step()
	var stocks := _state()
	_check(stocks.has("action_recovers_hook") and typeof(stocks["action_recovers_hook"]) == TYPE_BOOL,
		"actual native bridge exports Boolean recovery intent")
	if not _tap(&"action"):
		return
	await _step()
	if not _tap(&"drop"):
		return
	await _step()
	var released := _state()
	print("SCRAPERX_GRAPPLE_RECOVERY before=", released)
	_check(not bool(released["line_attached"]) and bool(released["recovery_nearby"]) and not bool(released["action_recovers_hook"]),
		"nearby released native hook selects recovery intent")
	_check(_main._ctx["action"]["label"] == "RECOVER HOOK"
		and _main._touch.button_label(&"action") == "RECOVER HOOK",
		"shipping context and touch HUD offer Recover for detached nearby hook")
	if not _tap(&"action"):
		return
	await _step()
	var attempted := _state()
	print("SCRAPERX_GRAPPLE_RECOVERY after=", attempted)
	_check(attempted["last_failure"] == "HOOK OUT OF REACH", "nearby hook beyond physical grasp retains native failure feedback")
	_check(int(attempted["cartridges_remaining"]) == int(stocks["cartridges_remaining"]) - 1
		and int(attempted["hooks_remaining"]) == int(stocks["hooks_remaining"]) - 1
		and not bool(attempted["line_attached"]),
		"failed nearby recovery neither fires nor refunds finite stocks")

func _state() -> Dictionary:
	return _native.get_grapple_gun_state()

func _view(label: String) -> void:
	var state: Dictionary = _native.get_grapple_gun_render_state()
	var view = _main._grapple_view
	var poses: Array = state["hooks"]
	var active := 0
	var exact := true
	for i in poses.size():
		var mesh: MeshInstance3D = view._hooks[i]
		var pose: Dictionary = poses[i]
		exact = exact and mesh.visible == bool(pose["active"])
		if not bool(pose["active"]):
			continue
		active += 1
		var expected := Transform3D(Basis(pose["rotation"]), pose["position"])
		exact = exact and mesh.global_transform.is_equal_approx(expected)
	_check(active == 1 and exact, label + " pooled hook matches native interpolated body pose")
	var line: MeshInstance3D = view._line
	var attached := bool(state["line_attached"])
	_check(line.visible == attached, label + " cable visibility follows native attachment")
	if attached:
		var start: Vector3 = state["rope_start"]
		var end: Vector3 = state["rope_end"]
		_check((line.global_transform * Vector3(0, -0.5, 0)).distance_to(start) < 0.0001
			and (line.global_transform * Vector3(0, 0.5, 0)).distance_to(end) < 0.0001,
			label + " cable endpoints match native interpolated endpoints")

func _hook_geometry_contract() -> void:
	var parts: Array = _native.get_grapple_hook_geometry()
	if not _check(parts.size() == 66, "native crook exposes all 66 steel prisms"):
		return
	var bounds := AABB()
	var first := true
	for part in parts:
		var polygon: PackedVector3Array = part["polygon"]
		for point in polygon:
			for z in [float(part["z_low"]), float(part["z_high"])]:
				var vertex := Vector3(point.x, point.y, z)
				if first:
					bounds = AABB(vertex, Vector3.ZERO)
					first = false
				else:
					bounds = bounds.expand(vertex)
	_check(absf(bounds.position.x + 0.13) < 0.000001
		and absf(bounds.end.x - 0.13) < 0.000001
		and absf(bounds.end.y - 0.19) < 0.000001
		and absf(bounds.position.z + 0.0915) < 0.000001
		and absf(bounds.end.z - 0.0915) < 0.000001,
		"native crook keeps authored steel envelope")
	var hooks: Array = _main._grapple_view._hooks
	if not _check(hooks.size() == 4, "shipping Main builds four hooks from native geometry"):
		return
	var exact := true
	for hook in hooks:
		var mesh_bounds: AABB = hook.mesh.get_aabb()
		exact = exact and mesh_bounds.position.distance_to(bounds.position) < 0.000001
		exact = exact and mesh_bounds.size.distance_to(bounds.size) < 0.000001
	_check(exact, "all rendered crooks match complete native prism bounds")

func _run() -> void:
	await _chute_hit_contract()
	await _auto_reel_context_contract()
	await _pendant_event_contract()
	_main = load("res://main.tscn").instantiate()
	root.add_child(_main)
	_main.set_process(false) # Invoke the shipping process with an explicit diagnostic clock.
	_native = _main._native
	if not _check(_native != null and not _main._regression_scene and not _main._ci_mode,
			"shipping Main and real bridge with enabled input"):
		quit(1)
		return
	_hook_geometry_contract()
	print("SCRAPERX_GRAPPLE_TOUCH diagnostic_staging=roof484 phone_evidence=0 clock=Main._process")
	if not _check(_native.debug_restart_at(Vector3(12, 485.15, -135.5)), "single native roof staging"):
		quit(1)
		return
	_touch(root.get_visible_rect().size * Vector2(0.78, 0.35), true, 7)
	_touch(root.get_visible_rect().size * Vector2(0.78, 0.35), false, 7)
	await _step(45)
	_check(_native.is_player_grounded() and bool(_state()["station_available"]), "actual roof contact reaches gun rack")
	_check(_main._router.device == Router.Device.TOUCH and _main._ctx["action"]["id"] == &"equip_grapple", "router offers touch equip")
	if not _tap(&"action"):
		quit(1)
		return
	var before_tick: int = _native.get_tick_index()
	await _step(2, 0.0)
	_check(_native.get_tick_index() == before_tick and not bool(_state()["equipped"])
		and _main._operating == &"", "zero-tick frames keep equip pending before native acknowledgment")
	await _step(20)
	_check(bool(_state()["equipped"]) and bool(_state()["acquired"])
		and _main._operating == &"grappling_gun", "touch equip survives pending frames and opens pendant after acknowledgment")
	await _walk("equipped")
	for id in [&"pendant_up", &"pendant_down"]:
		var button = _main._touch._buttons[id]
		_check(button.center.distance_to(_main._touch.stick_home()) > button.radius * 1.22 + 150.0 * _main._touch._u
			and button.center.distance_to(_main._touch.button_center(&"tools")) > (button.radius + _main._touch._buttons[&"tools"].radius) * 1.22,
			"grapple reel control stays outside movement stick: " + String(id))
	var at := root.get_visible_rect().size * Vector2(0.78, 0.35)
	var sensitivity: float = Router.LOOK_RADIANS_PER_UNIT * _main._router.look_sensitivity
	var relative := Vector2((float(_main._yaw) + PI * 0.5) / sensitivity,
		(float(_main._pitch) - 1.45) / sensitivity)
	_touch(at, true, 1)
	var drag := InputEventScreenDrag.new()
	drag.index = 1
	drag.position = at + relative
	drag.relative = relative
	root.push_input(drag, true)
	_touch(at + relative, false, 1)
	await _step()
	_check(absf(float(_main._pitch) - 1.45) < 0.001
		and absf(float(_main._yaw) + PI * 0.5) < 0.001, "viewport look drag reaches high grapple aim through router")
	_check(float(_state().shot_energy_j) == 900.0 and int(_state().shot_profile) == 1, "carried gun defaults to long shot before near-work diagnostic")
	if not _tap(&"grapple_range"):
		quit(1)
		return
	await _step(2)
	_check(int(_state().shot_profile) == 0 and float(_state().shot_energy_j) == 180.0, "touch selects precision for close-work controls")
	var ammo: int = _state()["cartridges_remaining"]
	if not _tap(&"action"):
		quit(1)
		return
	await _step(30)
	var fired := _state()
	_check(int(fired["cartridges_remaining"]) == ammo - 1 and int(fired["state"]) == 1
		and bool(fired["line_attached"]), "touch Action fires one native hook and pays line")
	_check(float(fired["shot_energy_j"]) == 180.0 and float(fired["maximum_shot_energy_j"]) == 900.0 and float(fired["maximum_line_length_m"]) == 60.0
		and float(fired["hook_mass_kg"]) == 2.0, "native bridge reports finite powered-shot contract")
	_check(is_finite(float(fired["launch_energy_j"])) and float(fired["launch_energy_j"]) > 179.0
		and float(fired["launch_energy_j"]) <= float(fired["shot_energy_j"]) + 0.001, "finite native cartridge work")
	_view("flight")
	if not _tap(&"action"):
		quit(1)
		return
	await _step(20)
	_check(int(_state()["state"]) == 2 and int(_state()["cartridges_remaining"]) == ammo - 1, "second touch Action locks without another cartridge")
	var length: float = _state()["paid_out_length"]
	var before_move: Vector3 = _native.get_player_position()
	_stick(true)
	_button(&"pendant_down", true)
	await _step(18)
	_stick(false)
	var after_move: Vector3 = _native.get_player_position()
	_check(Vector2(after_move.x - before_move.x, after_move.z - before_move.z).length() > 0.02,
		"attached viewport stick moves player while a second touch pays out line")
	_touch(_main._touch.button_center(&"pendant_down"), false)
	_check(float(_state()["paid_out_length"]) > length + 0.1, "touch Lower pays native locked line")
	length = _state()["paid_out_length"]
	_button(&"pendant_up", true)
	await _step(18)
	_touch(_main._touch.button_center(&"pendant_up"), false)
	_check(float(_state()["paid_out_length"]) < length - 0.1
		and is_finite(float(_state()["reel_work_j"])), "touch Raise takes up line with finite native work")
	_check(_state().has("auto_reel") and typeof(_state().get("auto_reel")) == TYPE_BOOL,
		"actual bridge exposes Boolean auto reel state")
	length = _state()["paid_out_length"]
	if not _tap(&"action"):
		quit(1)
		return
	await _step(18)
	_check(bool(_state()["auto_reel"]) and float(_state()["paid_out_length"]) < length - 0.1,
		"one touch starts native takeup that persists through neutral pendant frames")
	_check(_main._ctx["grapple_auto_reel"] and _main._touch.button_label(&"action") == "STOP REEL",
		"shipping HUD acknowledges actual native auto reel")
	await _walk("automatic reel")
	if not _tap(&"action"):
		quit(1)
		return
	await _step(2)
	length = _state()["paid_out_length"]
	await _step(18)
	_check(not bool(_state()["auto_reel"]) and absf(float(_state()["paid_out_length"]) - length) < 0.0001,
		"one touch STOP halts native takeup across subsequent neutral frames")
	# The old dedicated Auto button is now the contextual primary Action.
	# Keep this motor-control exercise outside real pickup reach by walking
	# on the existing roof; pickup itself has separate native/public checks.
	for i in 90:
		var gun := _state()
		if (gun.rope_end as Vector3).distance_to(gun.rope_start) >= 2.0:
			break
		_stick(true)
		await _step()
	_stick(false)
	await _step(4)
	_check(_native.is_player_grounded() and not bool(_state().action_recovers_hook), "ordinary roof movement keeps repeated motor commands distinct from pickup")
	if not _tap(&"action"):
		quit(1)
		return
	await _step(2)
	length = _state()["paid_out_length"]
	_button(&"pendant_down", true)
	await _step(18)
	_touch(_main._touch.button_center(&"pendant_down"), false)
	await _step(2)
	_check(not bool(_state()["auto_reel"]) and float(_state()["paid_out_length"]) > length + 0.1
		and _main._touch.button_label(&"action") == "REEL IN",
		"manual pendant overrides automatic reel and HUD acknowledges cancellation")
	if not _tap(&"action"):
		quit(1)
		return
	await _step(2)
	if not _tap(&"drop"):
		quit(1)
		return
	await _step(8)
	_check(not bool(_state()["line_attached"]) and int(_state()["cartridges_remaining"]) == ammo - 1,
		"touch Drop passively releases without refund or another shot")
	_check(not bool(_state()["auto_reel"]) and not _main._touch.is_button_shown(&"auto_reel"),
		"actual line release cancels automatic reel and removes its control")
	_view("released")
	await _walk("released but equipped")
	_check(_main._touch.button_label(&"drop") == "STOW", "detached gun offers touch Stow")
	if not _tap(&"drop"):
		quit(1)
		return
	await _step(8)
	_check(not bool(_state()["equipped"]) and _main._operating == &""
		and _main._ctx["panel"].is_empty() and not _main._touch._buttons[&"pendant_up"].shown
		and int(_state()["cartridges_remaining"]) == ammo - 1,
		"touch Stow reaches native acknowledgment and closes grapple presentation without firing")
	_check(not bool(_state()["auto_reel"]) and not _main._touch.is_button_shown(&"auto_reel"),
		"actual stow keeps automatic reel stopped and hidden")
	await _falling_chute()
	await _launcher_pitch()
	await _nearby_recovery_action()
	print("SCRAPERX_GRAPPLE_TOUCH %s checks=%d phone_evidence=0 route_evidence=0" % ["FAIL" if _failed else "PASS", _checks])
	quit(1 if _failed else 0)
