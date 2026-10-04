extends RefCounted

# One presentation clock for the lens, focus and original time-stretch cue.
# Native forces and the fixed tick never depend on this curve.
const ORBIT_SECONDS := 2.40
const TOTAL_SECONDS := 3.0

static func orbit_phase(clock: float) -> float:
	var t := clampf(clock / ORBIT_SECONDS, 0.0, 1.0)
	return (exp(3.0 * t) - 1.0) / (exp(3.0) - 1.0)

static func soft_blur(clock: float) -> float:
	return 1.0 - smoothstep(0.02, 0.84, orbit_phase(clock))

static func simulation_scale(clock: float) -> float:
	# This clock starts only after the shot is moving. Ease into a short
	# inspection beat. An immediate near-freeze hides the impulse.
	if clock < 0.10:
		return lerpf(1.0, 0.28, smoothstep(0.0, 0.10, clock))
	return lerpf(0.28, 1.0, smoothstep(0.35, 1.80, clock))
