extends "res://tests/grapple_toolbelt_input_case.gd"

func run(shipping_main: Node) -> Dictionary:
	main = shipping_main
	main.set_process(false)
	main._router.capture_mouse = false
	main._router.gameplay_active = true
	step(90)
	select_tool(&"grapple")
	var native: Object = main._native
	var initial: Dictionary = native.get_grapple_gun_state()
	check(bool(native.is_player_grounded()), "ordinary yard footing before charged use")
	check(main._touch.button_label(&"action") == "HOLD TO AIM" and not main._touch.is_button_shown(&"grapple_range"),
		"one hold action replaces separate near/long switch")
	main._pitch = PI * .5 - .001
	step()
	finger(&"action", true)
	step(45)
	var held: Dictionary = native.get_grapple_gun_state()
	check(bool(held.charging) and float(held.charge_fraction) > .49 and float(held.charge_fraction) < .51,
		"real touch hold advances native collision-time charge")
	check(int(held.cartridges_remaining) == int(initial.cartridges_remaining) and float(held.total_launch_work_j) == 0.0,
		"holding does not fire or consume stock")
	check(main._touch.button_label(&"action") == "RELEASE TO FIRE", "native charge acknowledgement reaches primary action")
	var preview: Dictionary = native.get_grapple_aim_preview()
	check((preview.points as PackedVector3Array).size() >= 2 and (preview.points as PackedVector3Array).size() <= 33,
		"actual native ballistic preview has bounded samples")
	main._render_snapshot()
	check(main._grapple_view._aim_guide.visible and main._grapple_view._aim_mesh.get_surface_count() == 1,
		"shipping render path shows one native-fed world guide surface")
	finger(&"action", false, true)
	step()
	main._render_snapshot()
	check(not bool(native.get_grapple_gun_state().charging) and not main._grapple_view._aim_guide.visible,
		"canceled touch clears native charge and visible guide")
	check(int(native.get_grapple_gun_state().cartridges_remaining) == int(initial.cartridges_remaining), "cancellation preserves cartridge")
	finger(&"action", true)
	step(15)
	main._open_pause()
	check(main._paused, "real pause opens during charge")
	main._resume()
	step()
	check(not bool(native.get_grapple_gun_state().charging) and int(native.get_grapple_gun_state().cartridges_remaining) == int(initial.cartridges_remaining),
		"real pause/resume cancels without discharge")
	finger(&"action", false)
	step()
	check(not bool(native.get_grapple_gun_state().line_attached), "stale release after pause cannot fire")
	finger(&"action", true)
	step(15)
	select_tool(&"toggle_rigging")
	check(not bool(native.get_grapple_gun_state().charging) and not bool(native.get_grapple_gun_state().equipped),
		"actual tool switch cancels and stows charged gun")
	select_tool(&"grapple")
	main._pitch = PI * .5 - .001
	step()
	finger(&"action", true)
	step(120)
	held = native.get_grapple_gun_state()
	check(bool(held.charging) and float(held.charge_fraction) > .999 and float(held.shot_energy_j) == 900.0,
		"long hold caps at demonstrated strong shot without auto-fire")
	check(int(held.cartridges_remaining) == int(initial.cartridges_remaining), "full hold still spends no stock")
	var from: Vector3 = native.get_player_position()
	finger(&"action", false)
	step()
	var fired: Dictionary = native.get_grapple_gun_state()
	check(not bool(fired.charging) and bool(fired.line_attached) and float(fired.launch_energy_j) > 899.0 and float(fired.launch_energy_j) <= 900.0,
		"one real release launches actual finite full-power hook")
	check(int(fired.cartridges_remaining) + 1 == int(initial.cartridges_remaining), "release consumes exactly one cartridge")
	check((native.get_grapple_aim_preview().points as PackedVector3Array).is_empty(), "fired shot has no stale aim prediction")
	step(45)
	var rise: float = (native.get_grapple_gun_state().rope_end as Vector3).y - from.y
	check(rise > 12.0, "touch charged shot retains useful overhead range")
	tap(&"drop")
	step()
	var before: Vector3 = native.get_player_position()
	stick(Vector2(1, 0))
	step(18)
	stick(Vector2.ZERO)
	step()
	var after: Vector3 = native.get_player_position()
	check(Vector2(after.x-before.x, after.z-before.z).length() > .05, "ordinary touch movement continues after charged shot")
	return {"checks": checks, "failures": failures, "rise_m": rise, "done": true, "actual_native": true, "touch": true}
