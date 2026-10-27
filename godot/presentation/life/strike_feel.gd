extends RefCounted
# The punch, felt (physics_world_strike.cpp decides it; nothing here does).
# From the native's strike state each frame: the edges (a strike thrown, a
# strike landed, a miss), a camera kick back along the line of the blow
# that decays like a head taking a jolt, a short FOV pinch, and the
# hit-stop: for HIT_STOP_S after a landing the drawn arms, the camera's kick
# and her drawn body hold still. The hit-stop is the viewer's only: the
# native keeps stepping at its own rate under it, so nothing is lost or
# slowed in the simulation (no launch_shot time scale is used).

const HIT_STOP_S := 0.07           # 60-80 ms, scaled a little by the blow
const KICK_DEG_PER_NS := 0.03      # ~1.5 deg for a 50 N*s landing
const KICK_MAX_DEG := 3.0
const KICK_DECAY := 14.0           # 1/s: the jolt's spring
const KICK_FREQ := 26.0            # rad/s: one shudder, not a wobble
const FOV_PINCH_DEG := 2.2
const FOV_DECAY := 10.0
const WINDUP_FOV_DEG := 0.6        # the eyes narrowing as the arm draws back

var thrown := 0                    # strikes seen thrown (edges), for the scripted runs
var landed := 0                    # strikes seen land
var missed := 0
var hit_stops := 0

var _strikes := -1
var _hits := -1
var _landed_this := false
var _phase := 0
var _stop_left := 0.0
var _kick_t := 10.0
var _kick_deg := 0.0
var _kick_axis := Vector3.RIGHT
var _fov := 0.0


## Returns the frame's edges: [&"thrown" | &"landed" | &"missed", ...].
func update(strike: Dictionary, camera_basis: Basis, delta: float, reduced_motion: bool) -> Array:
	var edges: Array = []
	var strikes := int(strike.get("strikes", 0))
	var hits := int(strike.get("hits", 0))
	var phase := int(strike.get("phase", 0))
	if _strikes < 0:
		_strikes = strikes
		_hits = hits
	if strikes > _strikes:
		_strikes = strikes
		_landed_this = false
		thrown += 1
		edges.append(&"thrown")
	if hits > _hits:
		_hits = hits
		_landed_this = true
		landed += 1
		hit_stops += 1
		var impulse := float(strike.get("impulse", 0.0))
		# The sim's impulse is the whole contact's; at the first frame it is
		# the first tick's. Size the jolt for the blow's full force-time.
		var blow := maxf(impulse, 45.0)
		_stop_left = HIT_STOP_S * clampf(blow / 50.0, 0.85, 1.15)
		_kick_deg = 0.0 if reduced_motion else minf(KICK_MAX_DEG, blow * KICK_DEG_PER_NS)
		_kick_t = 0.0
		# The head goes back and a little off the striking side: about the
		# camera's right axis (pitch up) tipped toward its forward roll.
		_kick_axis = (camera_basis.x + camera_basis.z * 0.25).normalized()
		_fov = 0.0 if reduced_motion else FOV_PINCH_DEG
		edges.append(&"landed")
	if _phase == 2 and phase == 3 and not _landed_this:
		missed += 1
		edges.append(&"missed")
	_phase = phase
	if _stop_left > 0.0:
		_stop_left = maxf(0.0, _stop_left - delta)
	else:
		_kick_t += delta
		_fov = move_toward(_fov, 0.0, _fov * FOV_DECAY * delta + 1.0e-4)
	return edges


## The drawn world holds still (arms, her body, the kick): the native does not.
func frozen() -> bool:
	return _stop_left > 0.0


## Rotation to apply over the camera's basis this frame.
func kick() -> Basis:
	if _kick_deg <= 0.0:
		return Basis.IDENTITY
	var a := deg_to_rad(_kick_deg) * exp(-KICK_DECAY * _kick_t) * cos(KICK_FREQ * _kick_t)
	if absf(a) < 1.0e-5:
		return Basis.IDENTITY
	return Basis(_kick_axis, a)


func fov_offset(strike: Dictionary) -> float:
	var windup := WINDUP_FOV_DEG if int(strike.get("phase", 0)) == 1 else 0.0
	return -(_fov + windup)
