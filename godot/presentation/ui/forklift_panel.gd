extends RefCounted

# Readouts observe the native vehicle; operator selection only labels intent.
static func describe(truck: Dictionary, operator: Dictionary, device: int) -> Dictionary:
	var gear := int(operator.get("gear", 0))
	var pending := int(operator.get("pending_direction", 0))
	var parked := bool(operator.get("parking", true))
	var selector := "F" if gear > 0 else "R" if gear < 0 else "N"
	if parked:
		selector = "PARK / " + selector
	if pending != 0:
		selector = "RELEASE ACCEL" if bool(operator.get("throttle_release_required", false)) else "STOP TO SELECT " + ("F" if pending > 0 else "R")
	var cutoff := bool(truck.get("energy_cutoff", false))
	var speed := float(truck.get("forward_speed_mps", 0.0))
	var tilt := rad_to_deg(float(truck.get("tilt_angle_rad", 0.0)))
	var hydraulic := "LIFT / TILT: ARROWS"
	var driving := "W ACCEL / S BRAKE / A-D STEER / SHIFT INCH"
	var directions := "F / N / R SELECT / P PARK"
	if device == 1:
		driving = "RT ACCEL / LT BRAKE / LEFT STICK STEER"
		directions = "RB F / Y N / LB R / LEFT CLICK PARK"
		hydraulic = "D-PAD: LIFT / TILT · RIGHT CLICK: HOLD INCH"
	elif device == 2:
		driving = "LEFT: STEER / ACCEL / BRAKE · RIGHT: LOOK"
		directions = "SELECT F / N / R · HOLD INCH FOR FINE APPROACH"
		hydraulic = "LIFT AND TILT: RIGHT-HAND BUTTONS"
	var wheels: Array = truck.get("wheels", [])
	var unsupported := false
	for wheel in wheels:
		unsupported = unsupported or not bool(wheel.get("contact", false))
	return {"title": "FORKLIFT", "subtitle": driving,
		"rows": [["SELECTOR", selector, 2 if pending != 0 else 1 if parked else 0],
			["SPEED", "%.1f KM/H %s" % [absf(speed) * 3.6, "REV" if speed < -0.05 else ""], 0],
			["LIFT / TILT", "%.2f M / %.1f° %s" % [float(truck.get("lift_position_m", 0.0)), absf(tilt), "BACK" if tilt >= 0.0 else "FWD"], 0],
			["HYDRAULIC", "%.1f KN" % (absf(float(truck.get("lift_force_n", 0.0))) / 1000.0), 0],
			["BATTERY", "EMPTY" if cutoff else "%.0f%%" % (100.0 * float(truck.get("battery_j", 0.0)) / maxf(1.0, float(truck.get("capacity_j", 1.0)))), 2 if cutoff else 0],
			["SUPPORT", "WHEEL OFF GROUND" if unsupported else "WHEELS IN CONTACT", 2 if unsupported else 0]],
		"help_lines": [directions, hydraulic], "hoist_analog": false,
		"verbs": [[&"hoist", "LIFT"], [&"slew", "TILT"], [&"leave", "LEAVE CONTROLS"]]}
