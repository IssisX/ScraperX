extends RefCounted

static func describe(crane: Dictionary, device: int) -> Dictionary:
	var driving := "A-D SLEW / W-S TROLLEY / UP-DOWN HOIST"
	var rigging := "LEAVE CAB · WALK TO EACH EYE · G RIG / R UNRIG"
	if device == 1:
		driving = "LEFT STICK SLEW / TROLLEY · RT UP / LT DOWN"
		rigging = "LEAVE CAB · WALK TO EACH EYE · X RIG / Y UNRIG"
	elif device == 2:
		driving = "LEFT STICK SLEW / TROLLEY · RIGHT SIDE LOOK"
		rigging = "DONE TO LEAVE CAB · RIG / UNRIG AT EACH TRUCK EYE"
	var mask := int(crane.get("attached_mask", 0))
	var count := 0
	for leg in 4:
		if mask & (1 << leg):
			count += 1
	var failure := int(crane.get("failure", 0))
	var status: String = ["READY", "OBSTRUCTED", "NOT ENOUGH CHAIN", "SLING NOT READY"][clampi(failure, 0, 3)]
	if failure == 0 and count > 0 and count < 4:
		status = "RIG ALL FOUR LEGS"
	return {"title": "YARD CRANE", "subtitle": driving,
		"rows": [["RIGGING", "%d / 4 LEGS" % count, 1 if count == 4 else 0],
			["HOIST", "%.2f M / %.1f KN" % [float(crane.get("hoist_actual_length_m", 0.0)), float(crane.get("hoist_force_n", 0.0)) / 1000.0], 0],
			["TROLLEY", "%.2f M" % float(crane.get("trolley_position_m", 0.0)), 0],
			["SLEW", "%.1f°" % rad_to_deg(float(crane.get("slew_angle_rad", 0.0))), 0],
			["HOIST ENERGY", "%.0f%%" % (100.0 * maxf(0.0, 1.0 - float(crane.get("hoist_source_used_j", 0.0)) / maxf(1.0, float(crane.get("hoist_source_capacity_j", 1.0))))), 0],
			["STATUS", status, 2 if failure != 0 else 0]],
		"help_lines": [rigging, "RELEASE CONTROLS TO STOP · DONE TO LEAVE CAB"], "hoist_analog": false,
		"verbs": [[&"hoist", "HOIST"], [&"slew", "SLEW / TROLLEY"], [&"leave", "LEAVE CAB"]]}
