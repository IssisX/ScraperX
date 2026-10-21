extends "res://tests/mounted_winch_crossing_input_case.gd"

# Shipping Main, real touch/router/native commands, ordinary starting footing.
# No actor/player staging, contact injection or hook pose changes.
const STEP := 1.0 / 90.0

func step(count: int = 1) -> void:
	for i in count:
		main._native.set_grapple_aim(-sin(main._yaw) * cos(main._pitch), sin(main._pitch), -cos(main._yaw) * cos(main._pitch))
		super.step(1)

func select_tool(id: StringName) -> void:
	if not main._touch._tool_tray_open:
		tap(&"tools")
	tap(id)
	step(2)

func run(shipping_main: Node) -> Dictionary:
	main = shipping_main
	main.set_process(false)
	main._router.capture_mouse = false
	main._router.gameplay_active = true
	step(90)
	var native: Object = main._native
	var bodies := int(native.get_kit_body_count())
	var gun: Dictionary = native.get_grapple_gun_state()
	check(bool(native.is_player_grounded()), "normal starting world has actual player footing")
	check(int(gun.shot_profile) == 1 and float(gun.shot_energy_j) == 900.0, "ordinary carried gun starts with long-range power")
	check(main._touch.is_button_shown(&"tools") and not main._touch.is_button_shown(&"grapple"), "normal view shows compact equipment selector")
	select_tool(&"grapple")
	check(bool(native.get_grapple_gun_state().equipped) and main._ctx.operating == &"grappling_gun", "touch draws actual carried grapple")
	check(main._touch.button_label(&"grapple_range") == "LONG SHOT", "selected native long profile reaches actual touch label")
	check(not main._touch.is_button_shown(&"pendant_up") and not main._touch.is_button_shown(&"pendant_down"), "empty gun offers no useless reel controls")
	tap(&"grapple_range")
	step()
	gun = native.get_grapple_gun_state()
	check(int(gun.shot_profile) == 0 and float(gun.shot_energy_j) == 180.0 and int(gun.cartridges_remaining) == 4,
		"one touch changes real next-shot profile without firing or spending stock")
	check(main._touch.button_label(&"grapple_range") == "NEAR SHOT", "near-shot label waits for actual native acknowledgement")
	select_tool(&"toggle_rigging")
	check(bool(native.get_rigging_state().equipped) and not bool(native.get_grapple_gun_state().equipped), "selecting rigging stows grapple in native state")
	check(not main._touch.is_button_shown(&"grapple_range") and not main._touch.is_button_shown(&"auto_reel"), "rigging cannot show grapple-only controls")
	select_tool(&"grapple")
	check(bool(native.get_grapple_gun_state().equipped) and not bool(native.get_rigging_state().equipped), "selecting grapple stows rigging in native state")
	select_tool(&"toggle_traversal_line")
	check(bool(native.get_traversal_line_state().equipped) and not bool(native.get_grapple_gun_state().equipped), "line selection has one actual tool owner")
	check(not main._touch.is_button_shown(&"grapple_range") and not main._touch.is_button_shown(&"rigging_cut"), "line cannot inherit grapple or cutting actions")
	select_tool(&"grapple")
	check(not bool(native.get_traversal_line_state().equipped) and bool(native.get_grapple_gun_state().equipped), "returning to grapple stows the line")
	check(int(native.get_kit_body_count()) == bodies and int(native.get_grapple_gun_state().cartridges_remaining) == 4,
		"tool changes create no support or projectile and consume no cartridge")
	if int(native.get_grapple_gun_state().shot_profile) == 0:
		tap(&"grapple_range")
		step()
	main._pitch = PI * .5 - .001
	step()
	var from: Vector3 = native.get_player_position()
	tap(&"action")
	step()
	gun = native.get_grapple_gun_state()
	check(bool(gun.line_attached) and float(gun.launch_energy_j) > 899.0 and float(gun.launch_energy_j) <= 900.001,
		"actual primary touch launches the selected finite long-range cartridge")
	check(not main._touch.is_button_shown(&"grapple_range"), "attached tool shows line controls instead of shot selector")
	step(45)
	gun = native.get_grapple_gun_state()
	var rise: float = (gun.rope_end as Vector3).y - from.y
	check(bool(gun.line_attached) and rise > 12.0, "actual touch-fired hook exceeds the former nine-metre apex")
	tap(&"drop")
	step()
	check(not bool(native.get_grapple_gun_state().line_attached) and int(native.get_grapple_gun_state().cartridges_remaining) == 3,
		"deliberate touch release preserves spent stock and the real projectile")
	var before: Vector3 = native.get_player_position()
	stick(Vector2(1, 0))
	step(18)
	stick(Vector2.ZERO)
	step()
	var after: Vector3 = native.get_player_position()
	check(Vector2(after.x-before.x, after.z-before.z).length() > .05, "ordinary touch movement remains available after tool use")
	return {"checks": checks, "failures": failures, "rise_m": rise, "done": true, "actual_native": true}
