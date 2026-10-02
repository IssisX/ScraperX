extends RefCounted

# Advice for the documented full-draw, nominal 82-degree roof shot.
# These are instructions, never a promise of landing or an automatic brake.
static func message(state: Dictionary, position: Vector3, velocity: Vector3) -> String:
	if bool(state.get("seated", false)) and not bool(state.get("released", false)):
		if float(state.get("draw_m", 0.0)) < float(state.get("max_draw_m", 12.0)) - 0.06:
			return "+352 M RECEIVER: FULL DRAW / AIM 82° TOWARD THE TOWER"
		if absf(float(state.get("yaw_rad", 0.0))) > 0.08 or absf(float(state.get("elevation_rad", 0.0)) - deg_to_rad(82.0)) > deg_to_rad(1.5):
			return "+352 M RECEIVER: AIM 82° TOWARD THE TOWER"
		return "FULL DRAW / 82° AIM · BRAKE WITH CHUTE JUST ABOVE +352 M"
	if bool(state.get("can_retrieve", false)) or bool(state.get("recovering", false)):
		return ""
	if not bool(state.get("launch_flight", false)) or not bool(state.get("released", false)) or bool(state.get("seated", false)) or bool(state.get("player_grounded", false)):
		return ""
	if bool(state.get("chute_deployed", false)):
		return "CHUTE OPEN · STEER TOWARD SOLID FOOTING"
	if velocity.y > 0.0:
		if position.y >= 354.0:
			return "TAP CHUTE NOW · BRAKE ABOVE THE +352 M RECEIVER"
		if position.y >= 330.0:
			return "+352 M RECEIVER · GET READY TO TAP CHUTE"
		return "TARGET +352 M · SAVE CHUTE FOR JUST ABOVE THE RECEIVER"
	if position.y < 354.0:
		return "BELOW THE RECEIVER · TAP CHUTE TO SLOW YOUR DESCENT"
	return "TAP CHUTE TO CONTROL YOUR DESCENT"
