extends RefCounted

# Orbit wall time drives lens, focus and the original rising time-stretch
# sound. The slowdown curve receives time since measured recoil, while
# body inertia uses interpolated native time. Native forces/tick stay fixed.
const ORBIT_SECONDS := 2.40
const TOTAL_SECONDS := 3.0

static func orbit_phase(clock: float) -> float:
	var t := clampf(clock / ORBIT_SECONDS, 0.0, 1.0)
	return (exp(3.0 * t) - 1.0) / (exp(3.0) - 1.0)

static func soft_blur(clock: float) -> float:
	return 1.0 - smoothstep(0.02, 0.84, orbit_phase(clock))

static func simulation_scale(clock: float) -> float:
	# This clock starts only after observed spring recoil. Ease into a bounded
	# inspection beat; an immediate near-freeze used to hide the real impulse.
	if clock < 0.10:
		return lerpf(1.0, 0.28, smoothstep(0.0, 0.10, clock))
	return lerpf(0.28, 1.0, smoothstep(0.35, 1.80, clock))
