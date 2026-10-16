extends RefCounted

const TOOL_REFUSALS := ["READY", "TOOL UNAVAILABLE", "STAND AT CONTROLS", "REMOVE CRANE SLINGS",
	"FINISH MOVEMENT", "PARK / RELEASE CONTROLS", "WAIT FOR SOLVE", "ALREADY LOCKED",
	"ALREADY RELEASED", "ALIGN / APPROACH SLOWLY", "REST ON PARKING BED", "CLEAR COUPLER", "RELIEVE COUPLER LOAD"]

static func tool_status(truck: Dictionary) -> String:
	if bool(truck.get("can_detach", false)):
		return "READY TO UNCOUPLE"
	if bool(truck.get("can_couple", false)):
		return "READY TO COUPLE"
	return TOOL_REFUSALS[clampi(int(truck.get("tool_refusal", 1)), 0, TOOL_REFUSALS.size() - 1)]

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
	var driving := "W ACCEL / S BRAKE / A-D STEER / SHIFT FINE"
	var directions := "F / N / R SELECT / P PARK"
	var tool_help := "G: UNCOUPLE FORKS" if bool(truck.get("coupled", true)) else "G: COUPLE FORKS"
	if device == 1:
		driving = "RT ACCEL / LT BRAKE / LEFT STICK STEER"
		directions = "RB F / Y N / LB R / LEFT CLICK PARK"
		hydraulic = "D-PAD: LIFT / TILT · RIGHT CLICK: FINE CONTROL"
		tool_help = "VIEW: UNCOUPLE FORKS" if bool(truck.get("coupled", true)) else "VIEW: COUPLE FORKS"
	elif device == 2:
		driving = "LEFT: STEER / ACCEL / BRAKE · RIGHT: LOOK"
		directions = "SELECT F / N / R · HOLD INCH FOR FINE CONTROL"
		hydraulic = "LIFT AND TILT: RIGHT-HAND BUTTONS"
		tool_help = "UNCOUPLE AT PARKING BED" if bool(truck.get("coupled", true)) else "ALIGN CARRIAGE / COUPLE"
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
			["SUPPORT", "WHEEL OFF GROUND" if unsupported else "WHEELS IN CONTACT", 2 if unsupported else 0],
			["FORKS", tool_status(truck), 1 if bool(truck.get("can_detach", false)) or bool(truck.get("can_couple", false)) else 0]],
		"help_lines": [directions, hydraulic, tool_help], "hoist_analog": false,
		"verbs": [[&"hoist", "LIFT"], [&"slew", "TILT"], [&"leave", "LEAVE CONTROLS"]]}
