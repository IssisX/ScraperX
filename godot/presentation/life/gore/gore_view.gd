extends Node3D
## The gore, all of it read from the native's hold state (get_matron_hold_state)
## and her state: strain on the joint she is pulling, the tear edges, the
## severed bodies, the fatality's beats. Nothing here decides anything; it
## shows what the sim did, as badly as it deserves.
##
## Owner calls update_view() each frame with the two dictionaries, the
## camera, and the deck height under the player. Always full gore: there is
## no setting or path that softens it.

const Kit := preload("res://presentation/life/gore/gore_kit.gd")
const BloodFx := preload("res://presentation/life/gore/blood_fx.gd")
const Limb := preload("res://presentation/life/gore/gore_limb.gd")
const Victim := preload("res://presentation/life/gore/gore_victim.gd")

enum { LAY, PIN, LEG0, LEG1, DRAG, KICK, HAUL, FALL, WIPE, BLACK }

## The lens overlay (first person); a studio's outside shots turn it off.
var lens_on := true
var fx: Node3D
var victim: Node3D
var arm_piece: Node3D
var leg_pieces: Array[Node3D] = []
var lens: ColorRect
var _lens_mat: ShaderMaterial
var _arms: Node3D = null      # first_person_arms.gd, for its stumps

# Edges seen.
var _arm_tears := 0
var _leg_tears := 0
var _pins := 0
var _landings := 0
var _edges := PackedInt64Array()
var _claimed := false
# What is showing.
var _lens_blood := 0.0
var _lens_run := 0.0
var _vignette := 0.0
var _redout := 0.0
var _crush := 0.0
var _apron := 0.0
var _hands := 0.0
var _smear := 0.0
var _soak := 0.0
var _drag_last := Vector3.INF
var _arm_last := Vector3.INF
var _arm_vel := Vector3.ZERO
var _arm_trail := Vector3.INF
var _leg_last: Array = [Vector3.INF, Vector3.INF]
var _haul_drip := 0.0
# Counters (tests and the sound): each is an edge the gore answered.
var tears_shown := 0
var crunches := 0
var ruins := 0
var smears := 0


func _ready() -> void:
	fx = BloodFx.new()
	add_child(fx)
	victim = Victim.new()
	add_child(victim)
	victim.build()
	victim.visible = false
	arm_piece = Limb.new()
	add_child(arm_piece)
	arm_piece.build_limb(1, 11)
	arm_piece.visible = false
	for k in 2:
		var leg := Limb.new()
		add_child(leg)
		leg.build_limb(2 + k, 21 + k)
		leg.visible = false
		leg_pieces.append(leg)
	var layer := CanvasLayer.new()
	layer.layer = 40
	add_child(layer)
	lens = ColorRect.new()
	lens.mouse_filter = Control.MOUSE_FILTER_IGNORE
	lens.set_anchors_preset(Control.PRESET_FULL_RECT)
	_lens_mat = ShaderMaterial.new()
	_lens_mat.shader = Kit.shader("lens", Kit.LENS_SHADER)
	_lens_mat.set_shader_parameter("seed", 3.7)
	lens.material = _lens_mat
	lens.visible = false
	layer.add_child(lens)


func attach_arms(arms: Node3D) -> void:
	_arms = arms


## matron_view.gd's look, to bloody her.
var matron_view: Node = null


func update_view(hold: Dictionary, her: Dictionary, delta: float, camera: Camera3D, ground_y: float) -> void:
	if hold.is_empty():
		return
	fx.ground_y = ground_y
	fx.camera_position = camera.global_position if camera != null else Vector3.ZERO
	var position: Vector3 = hold.get("player_position", Vector3.ZERO)
	var body_q: Quaternion = hold.get("body_rotation", Quaternion.IDENTITY)
	var claimed := bool(hold.get("claimed", false))
	var beat := int(hold.get("fatality_beat", -1))
	var lost := [bool(hold.get("left_arm_lost", false)), bool(hold.get("right_arm_lost", false)),
		bool(hold.get("left_leg_lost", false)), bool(hold.get("right_leg_lost", false))]
	# --- strain: the joint being pulled ---
	var strain := [0.0, 0.0, 0.0, 0.0]
	var wrist_side := int(hold.get("wrist_side", -1))
	var arm_s := float(hold.get("wrist_strain_ns", 0.0)) / maxf(float(hold.get("wrist_tear_ns", 600.0)), 1.0)
	if wrist_side >= 0 and wrist_side < 2:
		strain[wrist_side] = arm_s
	var leg_s: Vector2 = hold.get("leg_strain_ns", Vector2.ZERO)
	var leg_t := maxf(float(hold.get("leg_tear_ns", 1200.0)), 1.0)
	strain[2] = leg_s.x / leg_t
	strain[3] = leg_s.y / leg_t
	# Even under the threshold the load shows (the sim's strain only climbs
	# past the hold load): the load's share, a little.
	var load := float(hold.get("leg_load_n", 0.0))
	for k in [2, 3]:
		if not lost[k] and claimed and beat in [LEG0, LEG1] and (k - 2) == (beat - LEG0):
			strain[k] = maxf(strain[k], clampf(load / 6000.0, 0.0, 0.35))
	# --- the victim, seen from outside while claimed ---
	# The first-person eye is in its head: hidden then, shown to any other shot.
	var cam_close := camera != null and camera.global_position.distance_to(victim.head_position()) < 0.3
	victim.visible = claimed and not cam_close
	victim.global_transform = Transform3D(Basis(body_q), position)
	# --- tears ---
	var arm_tears := int(hold.get("arm_tear_count", 0))
	if arm_tears > _arm_tears:
		_arm_tears = arm_tears
		_on_tear(int(hold.get("last_severed_side", wrist_side)), false, hold)
	elif arm_tears < _arm_tears:
		_reset()
	var leg_tears := int(hold.get("leg_tear_count", 0))
	if leg_tears > _leg_tears:
		_leg_tears = leg_tears
		_on_tear(0 if lost[2] and (leg_tears == 1 or not lost[3]) else 1, true, hold)
	elif leg_tears < _leg_tears:
		_leg_tears = leg_tears
	# --- the fatality's beats ---
	if claimed and not _claimed:
		_claimed = true
	if not claimed and _claimed:
		_claimed = false
	var edges: PackedInt64Array = hold.get("fatality_beat_edges", PackedInt64Array())
	if edges.size() == 10:
		if _edges.size() == 10:
			for b in 10:
				if edges[b] > _edges[b]:
					_on_beat(b, hold)
		_edges = edges.duplicate()
	var pins := int(hold.get("knee_pin_count", 0))
	if pins > _pins:
		_pins = pins
		_on_crunch(hold)
	var landings := int(hold.get("fall_landing_count", 0))
	if landings > _landings:
		_landings = landings
		_on_landing(hold)
	# Knee down: the chest goes in.
	var knee_down := bool(hold.get("knee_down", false))
	_crush = move_toward(_crush, 1.0 if knee_down else (_crush if _crush > 0.6 else 0.0), delta * 3.0)
	victim.apply(lost, strain, _crush, _soak)
	if victim.ruined and not claimed:
		victim.unruin()
	# Drag: a smear behind the torso on the deck.
	if claimed and beat == DRAG:
		if _drag_last == Vector3.INF:
			_drag_last = position
		elif _drag_last.distance_to(position) > 0.14:
			fx.splat(Vector3(position.x, ground_y, position.z), Vector3.UP, 0.26, 1.0, position - _drag_last)
			_drag_last = position
			smears += 1
	else:
		_drag_last = Vector3.INF
	# Haul: bleeding on the rope, drips off the stumps, splashing below.
	if claimed and beat in [HAUL, KICK]:
		_haul_drip -= delta
		if _haul_drip <= 0.0:
			_haul_drip = 0.35
			var hit: Array = fx.land(position + Vector3(0, -0.6, 0), Vector3.ZERO)
			if not hit.is_empty():
				fx.splat(hit[0], hit[1], 0.12 + randf() * 0.1)
	# Wipe: her hands smear down the apron.
	if claimed and beat == WIPE:
		_smear = move_toward(_smear, 1.0, delta / 2.0)
		_hands = move_toward(_hands, 0.45, delta * 0.3)
	if not claimed and beat < 0 and _edges.size() == 10 and _smear > 0.0 and lost.count(true) == 0:
		_smear = 0.0
	# --- the severed bodies ---
	_follow_arm(hold, delta)
	_follow_legs(hold)
	# --- the first person's own stumps ---
	if _arms != null and _arms.has_method("set_lost"):
		for side in 2:
			_arms.set_lost(side, bool(lost[side]))
	# --- the lens ---
	_redout = move_toward(_redout, 0.62 if (claimed and beat < BLACK) else 0.0, delta * 0.8)
	_vignette = move_toward(_vignette, 0.0, delta * 0.35)
	_lens_blood = move_toward(_lens_blood, 0.0, delta * 0.03)
	_lens_run += delta
	lens.visible = lens_on and (_lens_blood > 0.01 or _vignette > 0.01 or _redout > 0.01)
	_lens_mat.set_shader_parameter("blood", _lens_blood)
	_lens_mat.set_shader_parameter("vignette", maxf(_vignette, arm_s * 0.5 if wrist_side >= 0 else 0.0))
	_lens_mat.set_shader_parameter("redout", _redout)
	_lens_mat.set_shader_parameter("run", _lens_run)
	# --- her ---
	if matron_view != null and matron_view.has_method("apply_gore"):
		matron_view.apply_gore(_apron, clampf(_apron * 2.5, 0.0, 1.0), _smear, _hands,
			hold.get("matron_hand", Vector3.ZERO), hold.get("matron_hand2", Vector3.ZERO), float(her.get("floor_y", ground_y)))


func _on_tear(side: int, leg: bool, hold: Dictionary) -> void:
	tears_shown += 1
	var k := (2 if leg else 0) + clampi(side, 0, 1)
	var stump: Node3D = victim.stumps[k]
	stump.tear()
	# The stump jets: from the victim's joint, and in first person from the
	# shoulder stub, across the view.
	var from_node: Node3D = stump
	if not leg and _arms != null and _arms.get("stumps") != null:
		_arms.set_lost(side, true)
		var fp: Node3D = _arms.stumps[side]
		if fp != null:
			from_node = fp
	fx.spray(from_node, Vector3.UP, 1.5 if leg else 1.0)
	if from_node != stump:
		fx.spray(stump, Vector3.UP, 1.5 if leg else 1.0)
	_lens_blood = minf(1.0, _lens_blood + (0.45 if not leg else 0.35))
	_lens_run = 0.0
	_vignette = 0.9
	_apron = minf(1.0, _apron + (0.22 if leg else 0.18))
	_hands = minf(1.0, _hands + 0.5)
	_soak = minf(1.0, _soak + 0.2)
	var at: Vector3 = stump.global_position
	fx.burst(at, Vector3.UP, 1.0 if not leg else 1.3)
	var hit: Array = fx.land(at, Vector3.ZERO)
	if not hit.is_empty():
		fx.splat(hit[0], hit[1], 0.55 if leg else 0.4)


# The player's punch landing on her (strike_feel.gd's edge, the point and
# normal the native gave): a burst off her skin at the knuckles, what lands
# of it splatted where it falls, and her blood on his glove. Additive only:
# the tears and the fatality are untouched by it.
var strikes_shown := 0

func _on_strike(at: Vector3, normal: Vector3, impulse_ns: float) -> void:
	strikes_shown += 1
	var k := clampf(maxf(impulse_ns, 45.0) / 50.0, 0.6, 1.4)
	var n := normal.normalized() if normal.length_squared() > 1.0e-6 else Vector3.UP
	fx.burst(at, n, 0.28 * k)
	var hit: Array = fx.land(at, n * 1.5)
	if not hit.is_empty():
		fx.splat(hit[0], hit[1], 0.12 * k)
	_hands = minf(1.0, _hands + 0.1 * k)
	_lens_blood = minf(1.0, _lens_blood + 0.04 * k)
	_lens_run = 0.0


func _on_beat(beat: int, hold: Dictionary) -> void:
	if beat == LAY:
		_soak = maxf(_soak, 0.1)


func _on_crunch(hold: Dictionary) -> void:
	crunches += 1
	# The ribcage gives: blood out of the mouth, a spatter on the plates.
	var mouth: Vector3 = victim.head_position()
	fx.burst(mouth, Vector3.UP + Vector3(0, 0, -0.4), 0.55)
	fx.drip(victim.head, Vector3.DOWN, 8.0)
	var hit: Array = fx.land(mouth, Vector3.ZERO)
	if not hit.is_empty():
		fx.splat(hit[0], hit[1], 0.5)
	_lens_blood = minf(1.0, _lens_blood + 0.3)
	_vignette = 1.0
	_apron = minf(1.0, _apron + 0.08)


func _on_landing(hold: Dictionary) -> void:
	ruins += 1
	victim.ruin()
	var at: Vector3 = hold.get("player_position", Vector3.ZERO)
	var speed := float(hold.get("fall_landing_speed_mps", 10.0))
	var force := clampf(speed / 10.0, 0.8, 2.6)
	fx.burst(at, Vector3.UP, force)
	var ground := Vector3(at.x, fx.ground_y if fx.ground_y > at.y - 2.0 else at.y - 0.3, at.z)
	fx.splat(ground, Vector3.UP, 1.4 * force, 0.0)
	var rng := RandomNumberGenerator.new()
	rng.seed = int(speed * 1000.0)
	for i in 10:
		var a := rng.randf() * TAU
		var d := rng.randf_range(0.6, 1.9) * force
		fx.splat(ground + Vector3(cos(a) * d, 0.0, sin(a) * d), Vector3.UP, rng.randf_range(0.15, 0.5), 0.7,
			Vector3(cos(a), 0.0, sin(a)))
	_soak = 1.0


func _follow_arm(hold: Dictionary, delta: float) -> void:
	var present := bool(hold.get("severed_arm_present", false))
	arm_piece.visible = present
	if not present:
		_arm_last = Vector3.INF
		_arm_trail = Vector3.INF
		return
	var p: Vector3 = hold.get("severed_arm_position", Vector3.ZERO)
	var q: Quaternion = hold.get("severed_arm_rotation", Quaternion.IDENTITY)
	# The sim's arm body is centred: its root half a length up its axis.
	arm_piece.place(p, Basis(q))
	if _arm_last != Vector3.INF and delta > 0.0:
		var v := (p - _arm_last) / delta
		# A hard stop or turn: it hit something (a wall of the shaft, the
		# deck). Splat it there, facing back the way it came.
		if _arm_vel.length() > 3.0 and (v - _arm_vel).length() > 4.0:
			fx.splat(p, -_arm_vel.normalized(), 0.35, 0.3, v)
		# A trail of drops along its flight.
		if _arm_trail == Vector3.INF or _arm_trail.distance_to(p) > 0.8:
			_arm_trail = p
			if v.length() > 1.0:
				var hit: Array = fx.land(p, v * 0.3)
				if not hit.is_empty():
					fx.splat(hit[0], hit[1], 0.12, 0.5, v)
		_arm_vel = v
	_arm_last = p


func _follow_legs(hold: Dictionary) -> void:
	var legs: Array = hold.get("severed_legs", [])
	for k in 2:
		var present: bool = k < legs.size() and bool((legs[k] as Dictionary).get("present", false))
		leg_pieces[k].visible = present
		if present:
			var d: Dictionary = legs[k]
			leg_pieces[k].place(d["position"] as Vector3, Basis(d["rotation"] as Quaternion))


func _reset() -> void:
	_arm_tears = 0
	_apron = 0.0
	_hands = 0.0
	_smear = 0.0
	_soak = 0.0
	_crush = 0.0
	for s in victim.stumps:
		s.heal()


## For a studio: the current lens values.
func lens_state() -> Dictionary:
	return {"blood": _lens_blood, "vignette": _vignette, "redout": _redout}
