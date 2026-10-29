extends RefCounted
## The camera while her hands have the body: third person (the owner,
## 2026-10-28, after a playtest: "needs to be a third-person perspective when
## these arm rips and fatalities are taking place ... you can't see what you
## want to see ... and you have no control"). Read-only on the native's
## get_matron_hold_state(): the sim decides everything; this only frames it.
##
## From her grab (her fingers on the ankle, `held`; or the run hers,
## `claimed`) to his release or his death, the view is off his eye and out
## where both bodies are seen whole: about a pivot between them, at the
## distance that fits both in the frame, from the side of the line between
## them that has the most open air. The look (stick, drag) orbits it. The
## camera is kept clear of steel: a sphere cast (a cone of rays through the
## native, her body and the severed pieces seen through) from the pivot pulls
## it in, at once, and it eases back out. The pivot itself is cast to from
## his centre (always in open air: his body is a capsule in the world).
##
## His death in her hand (fatal_release_beat rising at the let-go, the hang
## over the drop): RELEASE_SLOW_S of slow motion (the sim stepped fewer ticks
## a frame, its step unchanged), the view pulled up over her shoulder at the
## edge looking down the drop at him, the fade to black complete FADE_DONE_S after the let-go,
## the sim stepped fast while black until the death's restore lands (the
## native's death count), then the fade in on the restored body's eye. Her
## floor version (claimed, the Black beat) fades the same way.

const ENGAGE_S := 0.35          # eye to third person
const DISENGAGE_S := 0.5        # third person back to the eye (a set-down, a spare)
const PIVOT_HIS_SHARE := 0.55   # the pivot from her chest toward his middle
const MIN_DIST := 2.2
const MAX_DIST := 7.5
const FRAME_FILL := 0.82        # the bodies' extent over the half-FOV's reach
const BASE_PITCH := 0.32        # above the pair, looking a little down
const PITCH_MIN := -0.35
const PITCH_MAX := 1.25
const SIDE_EASE := 1.6          # the side's yaw follows the pair's line
const DIST_OUT_RATE := 2.5      # eased back out after a pull-in (1/s)
const CAM_RADIUS := 0.3         # the sphere cast's radius
const PIVOT_MARGIN := 0.2
const PIVOT_ROOM_M := 0.45      # the pivot pushed off steel nearer than this
const ROOM_PROBES := [Vector3(1, 0, 0), Vector3(-1, 0, 0), Vector3(0, 1, 0), Vector3(0, -1, 0), Vector3(0, 0, 1),
	Vector3(0, 0, -1), Vector3(0.577, 0.577, 0.577), Vector3(-0.577, 0.577, 0.577), Vector3(0.577, 0.577, -0.577),
	Vector3(-0.577, 0.577, -0.577), Vector3(0.577, -0.577, 0.577), Vector3(-0.577, -0.577, 0.577),
	Vector3(0.577, -0.577, -0.577), Vector3(-0.577, -0.577, -0.577)]
const ROOM_MIN_M := 1.4
const ROOM_SNAP_M := 0.6        # less than this: round to the room at once (a cut)         # less room than this behind the camera: look for more round the pair
const DODGE_RATE := 2.5         # the orbit eased round toward more room (1/s)
const DODGE_TRIES := [0.6, -0.6, 1.2, -1.2, 1.9, -1.9, PI]
# Bodies the camera's cast sees through: hers, a torn arm, torn legs, corpses.
const HER_ENTITY := 3200
const SEVERED_FIRST := 3290
const SEVERED_LAST := 3292
const CORPSE_FIRST := 3300      # sim/corpse.cpp kUserDataBase + each death
const CORPSE_LAST := 3399
const HAND_SKIP_M := 0.12       # her hand's own body (no entity): skipped near her hand
# The let-go.
const RELEASE_SLOW_S := 0.5
const RELEASE_SLOW_SCALE := 0.3
const FADE_FROM_S := 0.5
const FADE_DONE_S := 1.0
const PULL_S := 0.7
const PULL_BACK_M := 1.3        # back from the edge, over her shoulder
const PULL_UP_M := 3.4          # over her head
const PULL_RISE_M := 1.8        # risen more over the pull-out
const FAST_FORWARD := 6.0       # the sim's speed while black, waiting on the death
const AWAIT_DEATH_MAX_S := 12.0 # real seconds black before it gives up waiting
const BLACK_HOLD_S := 0.12
const FADE_IN_S := 0.6
const FLOOR_BLACK_BEAT := 9     # Matron::Fatal Black (her floor version)
const FLOOR_FADE_S := 0.35      # her floor version's fade: full inside its 0.5 s Black beat
# The hold's beats, each framed for itself (the owner, 2026-10-29: "not just a
# third person view ... a whole cinematic, dramatic experience"): how far out
# (a share of the distance that fits both bodies), the camera's height over
# the pair, and how far the pivot is drawn to what the beat is about. Eased
# over about BEAT_EASE_S so each beat reads before the next takes the frame.
#   grab:  close and a little high, on her hand closing on his leg;
#   carry: low, so she looms as she walks him to the rail;
#   over:  wide and level, both silhouettes over the drop (the tip, the hang);
#   reach: in on her other hand going for his arm;
#   rip:   closest, on the arm in her hand (the pull, held to her face);
#   throw: back out as she throws it and he hangs one-armed;
#   down:  wider, high, as she sets him back on her deck.
const BEAT_EASE_S := 0.7
const BEATS := {
	&"grab": [0.8, 0.30, 0.45],
	&"carry": [0.95, 0.08, 0.0],
	&"over": [1.1, 0.0, 0.25],
	&"reach": [0.85, 0.18, 0.45],
	&"rip": [0.72, 0.16, 0.65],
	&"throw": [0.95, 0.22, 0.2],
	&"down": [1.05, 0.42, 0.0],
}
const CLOSE_DIST := 1.7         # the closest beat's least distance
const ST_GRAB := 12
const ST_CARRY := 13
const ST_DANGLE := 14
const ST_ARMPLAY := 15
const ST_SPARE := 17

var _w := 0.0                   # third person's share of the view, 0..1
var _engaged := false
var _side := 1.0                # which side of the pair's line
var _side_yaw := 0.0
var _look_ref := Vector2.ZERO   # the look when it engaged: the orbit is the change
var _dist := MAX_DIST
var _pivot := Vector3.ZERO
var _cam := Vector3.ZERO
var _first_frame := true
var _dodge := 0.0               # the orbit turned off its side for room
var _dodge_goal := 0.0
# The fatality's let-go and its death.
var _release_seen := -1
var _since_release := -1.0
var _awaiting_death := false
var _deaths_at := 0
var _black := 0.0
var _black_after := -1.0        # seconds since the death landed (fade in)
var _await_s := 0.0
var _pull_from := Vector3.ZERO
var _edge := Vector3.ZERO
var _out := Vector3.ZERO
var _floor_black := 0.0
var _floor_fade := false        # her floor version's fade under way
var _landed_pending := false    # the death landed before the black was full: the view held till it is
var _last_view := Transform3D()
var _fatal_edges := PackedInt64Array()
var beat_cuts := 0              # fatality beat edges seen: a hook for tests and the sound
# For tests: the last frame's facts.
var clearance := INF            # the cast's free run past the camera
var pivot := Vector3.ZERO
var lets_go_seen := 0
var deaths_waited := 0
var black_full_after := -1.0    # seconds from the let-go to full black
var death_after := -1.0         # seconds from the let-go to the death's restore
var slow_ticks_s := 0.0         # seconds of the let-go's slow motion
var floor_black_full_after := -1.0   # seconds from her floor Black beat to full black
var floor_black_at_death := -1.0     # the black when her floor version's death landed (before the wait to full)
var _floor_s := 0.0
var beat := &""                 # the beat being framed (tests, stills)
var _beat_dist := 1.0
var _beat_pitch := BASE_PITCH
var _beat_focus := 0.0
var _focus := Vector3.ZERO


func active() -> bool:
	return _w > 0.001


## His own body and arms are not drawn while the view is out of his eye.
func third_person() -> bool:
	return _w > 0.001


func fov_offset() -> float:
	return 0.0


func vignette() -> float:
	return 0.0


func black() -> float:
	return maxf(_black, _floor_black)


## The sim's rate this frame: slow at the let-go (fewer native ticks a frame,
## each the same step).
func time_scale() -> float:
	if _since_release >= 0.0 and _since_release < RELEASE_SLOW_S:
		return RELEASE_SLOW_SCALE
	return 1.0


## Seconds since her fingers opened over the drop (-1 when not falling).
func since_let_go() -> float:
	return _since_release


## While black and the death still to come: the sim stepped this many times
## as fast.
func fast_forward() -> float:
	return FAST_FORWARD if _awaiting_death and black() >= 0.999 else 1.0


## The death the view waits on: its ride on the corpse is skipped (black).
func awaiting_death() -> bool:
	return _awaiting_death


## One frame. `hold` the native's get_matron_hold_state(), `her_head` her
## drawn head, `eye` the first-person camera as the body's feel placed it,
## `look` the player's look (yaw, pitch), `deaths` a stamp of the native's
## deaths (its death count and the runs she ended: any change is a death
## landed, the restore with it),
## `cast` a Callable(from, to, skip_near, radius) -> float: the free run
## from `from` toward `to` (to its length if nothing is met) of a cone that
## is `radius` wide at `to`. Returns the camera.
func frame(hold: Dictionary, her_head: Vector3, eye: Transform3D, look: Vector2, vfov_deg: float,
		deaths: int, delta: float, cast: Callable) -> Transform3D:
	var held := bool(hold.get("held", false))
	var claimed := bool(hold.get("claimed", false))
	var him: Vector3 = hold.get("player_position", eye.origin)
	var body_q: Quaternion = hold.get("body_rotation", Quaternion.IDENTITY)
	if not body_q.is_normalized():
		body_q = body_q.normalized() if body_q.length() > 1.0e-6 else Quaternion.IDENTITY
	var hand: Vector3 = hold.get("matron_hand", Vector3.ZERO)
	var hand_on := bool(hold.get("matron_hand_present", false))
	_step_beats(hold, delta)
	# The let-go over the drop: its count rises once.
	var release := int(hold.get("fatal_release_beat", 0))
	if _release_seen < 0 or release < _release_seen:
		# (a new run builds her again, her count from 0)
		_release_seen = release
	if release > _release_seen:
		_release_seen = release
		lets_go_seen += 1
		_since_release = 0.0
		_awaiting_death = true
		_deaths_at = deaths
		_await_s = 0.0
		_pull_from = _cam if _w >= 0.5 else eye.origin
		var e: Vector3 = hold.get("fatal_edge", him)
		var o: Vector2 = hold.get("fatal_out", Vector2.ZERO)
		_edge = e
		_out = Vector3(o.x, 0.0, o.y).normalized() if o.length() > 1.0e-4 else (him - e) * Vector3(1, 0, 1)
		if _out.length() < 1.0e-4:
			_out = Vector3.FORWARD
		_out = _out.normalized()
	# Her floor version: its Black beat is the fade, and the death follows.
	if claimed and int(hold.get("fatality_beat", -1)) == FLOOR_BLACK_BEAT and not _awaiting_death \
			and not _landed_pending and _black_after < 0.0:
		_awaiting_death = true
		_deaths_at = deaths
		_await_s = 0.0
		_floor_fade = true
		_floor_s = 0.0
	if _since_release >= 0.0:
		if _since_release < RELEASE_SLOW_S:
			slow_ticks_s += minf(delta, RELEASE_SLOW_S - _since_release)
		_since_release += delta
	# The death landed (the native's restore with it): black a moment, then in.
	# Landed before the black is full (her floor version's short Black beat,
	# or a short fall), the view holds its last frame while the fade finishes.
	if _awaiting_death:
		_await_s += delta
		if deaths != _deaths_at or (_await_s > AWAIT_DEATH_MAX_S and not held and not claimed):
			if deaths != _deaths_at:
				deaths_waited += 1
				if _since_release >= 0.0:
					death_after = _since_release
				if _floor_fade:
					floor_black_at_death = black()
			_awaiting_death = false
			_landed_pending = true
	var want := (held or claimed or _awaiting_death or _landed_pending) and _black_after < 0.0
	# The fade.
	if _awaiting_death or _landed_pending:
		if _since_release >= 0.0:
			_black = smoothstep(FADE_FROM_S, FADE_DONE_S, _since_release)
			if _since_release >= FADE_DONE_S:
				_black = 1.0
				if black_full_after < 0.0:
					black_full_after = _since_release
		if _floor_fade:
			_floor_s += delta
			_floor_black = move_toward(_floor_black, 1.0, delta / FLOOR_FADE_S)
			if _floor_black >= 1.0 and floor_black_full_after < 0.0:
				floor_black_full_after = _floor_s
		if _landed_pending and black() >= 0.999:
			_landed_pending = false
			_floor_fade = false
			_since_release = -1.0
			_black_after = 0.0
		elif _landed_pending:
			return _last_view if _w > 0.001 else eye
	elif _black_after >= 0.0:
		_black_after += delta
		var k := clampf((_black_after - BLACK_HOLD_S) / FADE_IN_S, 0.0, 1.0)
		_black = 1.0 - k
		_floor_black = minf(_floor_black, _black)
		if k >= 1.0:
			_black_after = -1.0
			_black = 0.0
			_floor_black = 0.0
	else:
		_black = 0.0
		_floor_black = 0.0
	# Engage, hold, let go. Out of black on the restored body, the view is his
	# eye at once.
	if _black_after >= 0.0 and not held and not claimed:
		_w = 0.0
		_engaged = false
	elif want:
		_w = move_toward(_w, 1.0, delta / ENGAGE_S)
	else:
		_w = move_toward(_w, 0.0, delta / DISENGAGE_S)
	if _w <= 0.001:
		_engaged = false
		_first_frame = true
		clearance = INF
		return eye
	# The pair: his middle, her chest; their extent.
	var her_chest := her_head + Vector3(0.0, -0.75, 0.0)
	var his_head := him + body_q * Vector3(0.0, 0.75, 0.0)
	var his_feet: Vector3 = hold.get("held_ankle", Vector3.ZERO) if held else Vector3.ZERO
	if his_feet.length_squared() < 1.0e-6:
		his_feet = him + body_q * Vector3(0.0, -0.85, 0.0)
	var her_feet := her_head + Vector3(0.0, -2.3, 0.0)
	var want_pivot := her_chest.lerp(him, PIVOT_HIS_SHARE)
	# The beat: its framing eased in, the pivot drawn toward what it is about.
	var shape: Array = _beat_shape(hold, him, body_q, hand, hand_on)
	var beat_k := 1.0 - exp(-delta / BEAT_EASE_S) if not _first_frame else 1.0
	_beat_dist = lerpf(_beat_dist, float(shape[0]), beat_k)
	_beat_pitch = lerpf(_beat_pitch, float(shape[1]), beat_k)
	_beat_focus = lerpf(_beat_focus, float(shape[2]), beat_k)
	want_pivot = want_pivot.lerp(_focus, _beat_focus)
	var falling := _since_release >= 0.0 and not held
	if falling:
		# Over the drop, the pivot stays where it was at the let-go.
		want_pivot = _pivot
	var skip := hand if hand_on else Vector3.INF
	var to_pivot := want_pivot - him
	var run: float = cast.call(him, want_pivot, skip, 0.0) if to_pivot.length() > 1.0e-3 else 0.0
	var p := want_pivot
	if run < to_pivot.length():
		p = him + to_pivot.normalized() * maxf(0.0, run - PIVOT_MARGIN)
	if falling:
		p = _pivot
	else:
		# Room round the pivot: pushed off steel nearer than PIVOT_ROOM_M, kept
		# only where his middle still sees it.
		var push := Vector3.ZERO
		for d6 in ROOM_PROBES:
			var f: float = cast.call(p, p + (d6 as Vector3) * PIVOT_ROOM_M, skip, 0.0)
			if f < PIVOT_ROOM_M:
				push -= (d6 as Vector3) * (PIVOT_ROOM_M - f)
		if push.length() > 1.0e-4:
			var moved := p + push
			if float(cast.call(him, moved, skip, 0.0)) >= him.distance_to(moved) - 1.0e-3:
				p = moved
	var extent := 0.0
	for q in [his_head, his_feet, him, her_head, her_feet, her_chest]:
		extent = maxf(extent, (q as Vector3).distance_to(p))
	var half := deg_to_rad(clampf(vfov_deg, 30.0, 120.0)) * 0.5
	var fit := clampf(extent / (tan(half) * FRAME_FILL) + 0.3, MIN_DIST, MAX_DIST)
	if not falling:
		fit = clampf(fit * _beat_dist, CLOSE_DIST, MAX_DIST)
	# The side: across the line between them, the more open one, chosen once.
	var line := Vector3(him.x - her_head.x, 0.0, him.z - her_head.z)
	if line.length() < 0.2:
		line = Vector3(-sin(look.x), 0.0, -cos(look.x))
	line = line.normalized()
	var across := Vector3.UP.cross(line).normalized()
	var across_yaw := atan2(across.x, across.z)
	if not _engaged:
		_engaged = true
		_look_ref = look
		var a: float = cast.call(p, p + across * fit, skip, CAM_RADIUS)
		var b: float = cast.call(p, p - across * fit, skip, CAM_RADIUS)
		_side = 1.0 if a >= b - 0.05 else -1.0
		_side_yaw = across_yaw if _side > 0.0 else wrapf(across_yaw + PI, -PI, PI)
		_dist = fit
		_pivot = p
		_dodge = 0.0
		_dodge_goal = 0.0
	var side_goal := across_yaw if _side > 0.0 else wrapf(across_yaw + PI, -PI, PI)
	_side_yaw = wrapf(_side_yaw + angle_difference(_side_yaw, side_goal) * (1.0 - exp(-SIDE_EASE * delta)), -PI, PI)
	_pivot = _pivot.lerp(p, 1.0 - exp(-10.0 * delta)) if not _first_frame else p
	if not falling and float(cast.call(him, _pivot, skip, 0.0)) < him.distance_to(_pivot) - 1.0e-3:
		_pivot = p
	# The look orbits: its change since the view went out.
	var yaw := _side_yaw + wrapf(look.x - _look_ref.x, -PI, PI)
	var pitch := clampf(_beat_pitch - (look.y - _look_ref.y), PITCH_MIN, PITCH_MAX)
	_dodge = lerp_angle(_dodge, _dodge_goal, 1.0 - exp(-DODGE_RATE * delta))
	var dir := _orbit(yaw + _dodge, pitch)
	var goal := _pivot + dir * fit
	var look_at := _pivot
	if falling:
		# The pull-out down the drop: up from where the camera was to over her
		# shoulder at the edge, rising, looking down the drop at him going.
		var k := smoothstep(0.0, PULL_S, _since_release)
		var over := _edge - _out * PULL_BACK_M + Vector3.UP * (PULL_UP_M + PULL_RISE_M * smoothstep(0.0, FADE_DONE_S, _since_release))
		goal = _pull_from.lerp(over, k)
		look_at = _pivot.lerp(him, k)
	# The sphere cast from the pivot (a camera's radius past the goal): in at
	# once, out eased.
	var to_goal := goal - _pivot
	var reach := to_goal.length()
	var way := to_goal / reach if reach > 1.0e-3 else dir
	var free: float = cast.call(_pivot, _pivot + way * (reach + CAM_RADIUS), skip, CAM_RADIUS)
	var allowed := maxf(0.0, minf(reach, free - CAM_RADIUS))
	if not falling:
		# Cramped: round the pair to where there is room (eased there, the
		# pull-in meanwhile at once); back to its side once that has room.
		if allowed < minf(ROOM_MIN_M, fit):
			var best := allowed
			for off in DODGE_TRIES:
				var d2 := _orbit(yaw + _dodge + float(off), pitch)
				var room := minf(fit, float(cast.call(_pivot, _pivot + d2 * (fit + CAM_RADIUS), skip, CAM_RADIUS)) - CAM_RADIUS)
				if room > best + 0.3:
					best = room
					_dodge_goal = wrapf(_dodge + float(off), -PI, PI)
			if allowed < ROOM_SNAP_M and best > allowed + 0.3:
				_dodge = _dodge_goal
				dir = _orbit(yaw + _dodge, pitch)
				goal = _pivot + dir * fit
				to_goal = goal - _pivot
				reach = to_goal.length()
				way = to_goal / reach if reach > 1.0e-3 else dir
				free = cast.call(_pivot, _pivot + way * (reach + CAM_RADIUS), skip, CAM_RADIUS)
				allowed = maxf(0.0, minf(reach, free - CAM_RADIUS))
		elif absf(_dodge_goal) > 1.0e-3:
			var home := minf(fit, float(cast.call(_pivot, _pivot + _orbit(yaw, pitch) * (fit + CAM_RADIUS), skip, CAM_RADIUS)) - CAM_RADIUS)
			if home >= minf(ROOM_MIN_M, fit):
				_dodge_goal = 0.0
	if _first_frame or allowed < _dist:
		_dist = allowed
	else:
		_dist = minf(allowed, _dist + (allowed - _dist) * (1.0 - exp(-DIST_OUT_RATE * delta)) + 0.02 * delta)
	_first_frame = false
	# The free run past the camera along its line (a test's: CAM_RADIUS or
	# more is clear of steel).
	clearance = free - _dist
	var cam := _pivot + way * _dist
	_cam = cam
	pivot = _pivot
	var to_look := look_at - cam
	var basis := eye.basis
	if to_look.length() > 1.0e-3:
		var up := Vector3.UP if absf(to_look.normalized().dot(Vector3.UP)) < 0.98 else Vector3.FORWARD
		basis = Basis.looking_at(to_look, up)
	var out := Transform3D(basis, cam)
	if _w >= 0.999:
		_last_view = out
		return out
	# Easing between his eye and the third person: the blended point is cast
	# to from the pivot as well, so the way out never passes through steel.
	var s := smoothstep(0.0, 1.0, _w)
	var mid := eye.origin.lerp(cam, s)
	var to_mid := mid - _pivot
	if to_mid.length() > 1.0e-3:
		var f: float = cast.call(_pivot, mid, skip, CAM_RADIUS)
		if f - CAM_RADIUS < to_mid.length():
			mid = _pivot + to_mid.normalized() * maxf(0.0, f - CAM_RADIUS)
	var q := eye.basis.get_rotation_quaternion().slerp(basis.get_rotation_quaternion(), s)
	_last_view = Transform3D(Basis(q), mid)
	return _last_view


## The beat now, its framing [distance share, pitch, pivot pull], and its
## focus (_focus): her hand on his leg, his body over the drop, her other hand
## and his arm, from the hold's own state (never a guess).
func _beat_shape(hold: Dictionary, him: Vector3, body_q: Quaternion, hand: Vector3, hand_on: bool) -> Array:
	var st := int(hold.get("state", -1))
	var hand1: Vector3 = hold.get("hand", hand)
	var hand2: Vector3 = hold.get("hand2", hand1)
	var arm_beat := int(hold.get("armplay_beat", -1))
	var his_chest := him + body_q * Vector3(0.0, 0.35, 0.0)
	_focus = him
	beat = &"carry"
	if st == ST_GRAB:
		beat = &"grab"
		_focus = hand1 if hand1.length_squared() > 1.0e-6 else (hand if hand_on else him)
	elif st == ST_CARRY:
		beat = &"carry"
	elif st == ST_DANGLE:
		beat = &"over"
		_focus = him
	elif st == ST_ARMPLAY:
		var has_arm := int(hold.get("arm_tears", 0)) > 0 and bool(hold.get("severed_arm_present", false))
		if arm_beat <= 0 and not has_arm:
			beat = &"reach"
			_focus = his_chest.lerp(hand2, 0.5) if hand2.length_squared() > 1.0e-6 else his_chest
		elif arm_beat >= 4:
			beat = &"throw"
			_focus = hand2 if hand2.length_squared() > 1.0e-6 else him
		else:
			beat = &"rip"
			_focus = hand2 if hand2.length_squared() > 1.0e-6 else his_chest
	elif st == ST_SPARE:
		beat = &"down"
	return BEATS[beat]


static func _orbit(yaw: float, pitch: float) -> Vector3:
	return Vector3(sin(yaw) * cos(pitch), sin(pitch), cos(yaw) * cos(pitch))


func _step_beats(hold: Dictionary, _delta: float) -> void:
	var edges: PackedInt64Array = hold.get("fatality_beat_edges", PackedInt64Array())
	if _fatal_edges.size() == edges.size():
		for i in edges.size():
			if edges[i] != _fatal_edges[i]:
				beat_cuts += 1
	_fatal_edges = edges.duplicate()


## The cast the camera uses, through the native's look ray: a cone of five
## rays from `from`, the outer four `radius` off the centre one at `to`. Her
## body, the severed pieces and corpses are seen through (the ray goes on
## past them), and her hand's own body near `skip_near`. The free run along
## the centre line, `to`'s distance if nothing is met.
static func cone_cast(native: Object, from: Vector3, to: Vector3, radius: float, skip_near: Vector3) -> float:
	var run := to - from
	var length := run.length()
	if length < 1.0e-4:
		return length
	var dir := run / length
	var side := dir.cross(Vector3.UP)
	if side.length() < 1.0e-3:
		side = dir.cross(Vector3.RIGHT)
	side = side.normalized()
	var up := side.cross(dir).normalized()
	var best := length
	var offs := [Vector3.ZERO, side * radius, -side * radius, up * radius, -up * radius] if radius > 0.0 else [Vector3.ZERO]
	for off in offs:
		var end: Vector3 = to + off
		var ray := end - from
		var ray_length := ray.length()
		var free := _ray_free(native, from, ray / ray_length, ray_length, skip_near)
		# along the centre line
		best = minf(best, free * (ray / ray_length).dot(dir))
	return best


static func _ray_free(native: Object, from: Vector3, dir: Vector3, length: float, skip_near: Vector3) -> float:
	var at := from
	var gone := 0.0
	for pass_i in 5:
		var left := length - gone
		if left <= 1.0e-3:
			return length
		var hit: Dictionary = native.get_look_target(at, dir, left)
		if not bool(hit.get("hit", false)):
			return length
		var d := float(hit["distance"])
		var entity := int(hit["entity"])
		var point: Vector3 = hit["point"]
		var see_through := entity == HER_ENTITY or (entity >= SEVERED_FIRST and entity <= SEVERED_LAST) or \
			(entity >= CORPSE_FIRST and entity <= CORPSE_LAST) or \
			(skip_near.is_finite() and point.distance_to(skip_near) < HAND_SKIP_M)
		if not see_through:
			return gone + d
		gone += d + 0.05
		at = from + dir * gone
	return gone
