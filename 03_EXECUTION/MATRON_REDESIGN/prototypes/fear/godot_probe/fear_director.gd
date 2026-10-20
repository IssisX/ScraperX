extends RefCounted
# PROTOTYPE (overhaul/fear track; not in the repo). The Matron's fear beats,
# decided in the presentation from what the native already gives every frame
# (get_matron_state(), the player's body, the camera, the foreman's radio
# text, the fear voice's last clip, the death count). Nothing here changes the
# simulation; beats that need the simulation to act (her stop on sight, her
# hang on the ladder) are only detected here, to measure how often they come.
#
# Every beat is emitted as a cue {t, kind, line, d, state, stage, ...} and
# printed as one FEAR_CUE line, so a playthrough's log is the beat timeline.

# matron.hpp:95-108 State; matron.hpp:315 Stage.
const DORMANT := 0
const STRAINING := 1
const EMERGING := 2
const HUNTING := 3
const SEARCHING := 4
const SHUFFLING := 5
const DEAD := 6
const GUARDING := 7
const PEERING := 8
const UNRIGGING := 9
const PEST := 0
const DANGEROUS := 1
const CHANGED := 2
# matron_view.gd:16 the locker; matron.hpp:146-151 the shaft.
const LOCKER := Vector3(11.0, 155.1, -126.2)
const SHAFT_HALF := 17.0
const MID_Z := -150.0
const STAND_CENTRE := 1.05   # matron_view.gd:21
const HEAD_ABOVE_CENTRE := 0.62

# The lines (spoken by a recorded voice; the prototype prints them).
const LOCKER_PLEAS := ["Hello? Is someone there?", "I can't get the door, dear.", "Could you fetch Walter for me?"]
const SHH := ["Shh.", "Shh, shh, shh.", "People are sleeping."]
const SHH_DANGEROUS := ["I said quiet.", "Indoor voice, Mr. Navarro."]
const NAME_FORMAL := ["Mr. Navarro?", "Mr. Navarro, you haven't signed in."]
const NAME_FIRST := ["Eli?", "Eli, love. Come down now."]
const SHOVE_PEST := ["Out of the way, dear.", "Excuse me, love."]
const SHOVE_RIM := ["Mind the edge.", "Careful now."]
const PUPIL := ["Let's have a look at you.", "(penlight on: left, right)", "Pupils fixed.", "Time of death, %s."]

var t := 0.0
var cues: Array = []
var counts := {}
# The freeze and the stare.
var _unseen_s := 999.0
var _was_seen := false
var freeze_left := 0.0
# The hush.
var _rage_hist: Array = []      # [t, rage] over the last 3 s
var _rage_last := -1.0
var _shh_at := -999.0
var _shh_i := 0
# The name and the echo.
var name_known := false
var first_name_known := false
var _name_at := -999.0
var _name_i := 0
var _last_radio := ""
var _echo_due := -1.0
var _echo_text := ""
var _ring_line_said := false
var _radio_answered := false
# The shove and the catch.
var _shoves_seen := -1
var _catches_seen := -1
var _shove_i := 0
var _shove_effort := 0          # shoves heard only as her effort (no words)
var _shove_last_t := -999.0
var _shove_line_t := -999.0
var _shove_lines_this := 0      # lines this encounter (an encounter: shoves under 20 s apart)
# Her imitation of the player's own last yell.
var _deaths_seen := -1
var _mimic_clip := ""
var _mimic_line := ""
var _mimic_armed := false
var _mimic_at := -999.0
var _mimic_armed_at := -999.0
# The breath in the hand.
var _breath_in := 0.0
var _haptic_breaths := 0
# The locker.
var _quiet_at_locker := 0.0
var _pleas := 0
var _plea_at := -999.0
# The ladder.
var _ladder_close := false


func _emit(kind: String, line: String, her: Dictionary, d: float, extra := {}) -> void:
	var cue := {"t": snappedf(t, 0.01), "kind": kind, "line": line, "d": snappedf(d, 0.1),
		"state": int(her.get("state", -1)), "stage": int(her.get("stage", 0))}
	cue.merge(extra)
	cues.append(cue)
	counts[kind] = int(counts.get(kind, 0)) + 1
	print("FEAR_CUE t=%.2f kind=%s d=%.1f state=%d stage=%d rage=%.2f line=\"%s\" %s" % [t, kind, d,
		int(her.get("state", -1)), int(her.get("stage", 0)), float(her.get("rage", 0.0)), line,
		JSON.stringify(extra) if not extra.is_empty() else ""])


# ctx: player (Vector3, body centre), velocity (Vector3), camera (Transform3D),
# radio (String, the line showing), deaths (int), fear_clip (String, the fear
# voice's clip playing or last played), fear_line (String, its caption),
# climbing (bool, the player on a ladder), hour (float, the sky's).
func update(delta: float, her: Dictionary, ctx: Dictionary) -> void:
	t += delta
	var state := int(her["state"])
	var stage := int(her.get("stage", 0))
	var released := bool(her.get("released", false))
	var alive := bool(her.get("alive", true))
	var sees := bool(her.get("sees_player", false))
	var centre: Vector3 = her["position"]
	var head := centre + Vector3(0.0, HEAD_ABOVE_CENTRE, 0.0)
	var player: Vector3 = ctx["player"]
	var camera: Transform3D = ctx["camera"]
	var d := centre.distance_to(player)
	var flat := Vector2(centre.x - player.x, centre.z - player.z).length()
	var out := released and alive and state != DEAD
	var rage := float(her.get("rage", 0.0))
	var shoved_now := _shoves_seen >= 0 and int(her.get("shoves", 0)) > _shoves_seen
	var ear := (ctx["camera"] as Transform3D).origin

	# --- 1. The locker: a polite voice behind the steel while you stand quiet by it.
	if state == DORMANT and Vector2(player.x - LOCKER.x, player.z - LOCKER.z).length() < 3.5 \
			and absf(player.y - LOCKER.y) < 2.5 and (ctx["velocity"] as Vector3).length() < 1.2:
		_quiet_at_locker += delta
		if _quiet_at_locker >= 4.0 and _pleas < LOCKER_PLEAS.size() and t - _plea_at >= 20.0 and _voice_free():
			_emit("locker_plea", LOCKER_PLEAS[_pleas], her, Vector2(player.x - LOCKER.x, player.z - LOCKER.z).length(),
				{"knocks": 3, "muffled": true})
			_pleas += 1
			_plea_at = t
			_spoke()
	else:
		_quiet_at_locker = 0.0

	# --- 2. Her name for you: Brenner says it on the radio within her hearing.
	var radio := String(ctx.get("radio", ""))
	if radio != _last_radio:
		_last_radio = radio
		if radio != "":
			# The handset is on the player; she hears it if she is out (or
			# straining in her locker) and near enough (a voice ~ a walk: 16 m
			# open, matron.hpp:207-208; the locker grille 30 m).
			var hears := (out and d < 20.0) or ((state == STRAINING or state == DORMANT) and player.distance_to(LOCKER) < 30.0)
			# Revised after the act run (out/act_run.log t=31: the line came with
			# the player 49 m off, a deck down): Brenner's channel is on the deck
			# intercom too (foreman_radio.gd:79), so awake she hears every line.
			var intercom := state != DORMANT
			if (hears or intercom) and radio.find("Navarro") >= 0 and not name_known:
				name_known = true
				_emit("name_learned", radio, her, d)
			if hears and out:
				# Echolalia: the tail of what she heard, in her voice, later.
				var clause := RegEx.create_from_string("[^.?!,]+")
				var parts := clause.search_all(radio)
				_echo_text = "..." + parts[parts.size() - 1].get_string().strip_edges().to_lower() + "..."
				_echo_due = t + 10.0
			if radio.begins_with("Don't come down"):
				_ring_line_said = true
	if _echo_due > 0.0 and t >= _echo_due:
		_echo_due = -1.0
		if out and not sees and _voice_free() and _audible(head, ear):
			_emit("echo", _echo_text, her, d, {"from": "her mouth"})
			_spoke()

	# --- 3. The freeze: she sees you, and everything about her stops.
	if sees and not _was_seen and out and _unseen_s >= 3.0:
		freeze_left = [1.2, 0.6, 0.25][clampi(stage, 0, 2)]
		_emit("freeze", "", her, d, {"hold_s": freeze_left, "music": "cut", "breath": "held",
			"eyes": "on camera" if d < 3.0 else "head turn only"})
	_unseen_s = 0.0 if sees else _unseen_s + delta
	_was_seen = sees
	freeze_left = maxf(0.0, freeze_left - delta)

	# --- 4. The hush: your noise makes her rage jump while she is near.
	_rage_hist.append([t, rage])
	while not _rage_hist.is_empty() and t - float(_rage_hist[0][0]) > 3.0:
		_rage_hist.pop_front()
	var jump := rage - _rage_last if _rage_last >= 0.0 else 0.0
	var climb3 := rage - float(_rage_hist[0][1]) if not _rage_hist.is_empty() else 0.0
	_rage_last = rage
	if out and stage < CHANGED and not shoved_now and _voice_free() and t - _shh_at >= 40.0 and (jump >= 0.01 or climb3 >= 0.02) \
			and _audible(head, ear):
		var pool: Array = SHH if stage == PEST else SHH_DANGEROUS
		_emit("shh", pool[_shh_i % pool.size()], her, d, {"rage_jump": snappedf(jump, 0.001), "rage_3s": snappedf(climb3, 0.001)})
		_shh_i += 1
		_shh_at = t
		_spoke()

	# --- 5. Calling you by name: looking for you, you unseen, near.
	if out and name_known and not sees and (state == PEERING or state == SEARCHING) \
			and d >= 4.0 and t - _name_at >= 45.0 and _voice_free() and _audible(head, ear):
		var pool2: Array = NAME_FIRST if first_name_known else NAME_FORMAL
		_emit("call_name", pool2[_name_i % pool2.size()], her, d)
		_name_i += 1
		_name_at = t
		_spoke()

	# --- 6. Her hands on you: the shove with a kind word (and which word). Paced
	# (the act run: 51 shoves in 80 s pinned at the rail, kShoveCooldownSeconds
	# 1.5): words on the first shove of an encounter and at most one more 12 s
	# on; every other shove only her effort, a breath through the nose.
	var shoves := int(her.get("shoves", 0))
	if _shoves_seen >= 0 and shoves > _shoves_seen:
		if t - _shove_last_t >= 20.0:
			_shove_lines_this = 0   # a new encounter
		_shove_last_t = t
		if _shove_lines_this < 2 and t - _shove_line_t >= 12.0:
			var rim := Vector2(clampf(player.x, -SHAFT_HALF, SHAFT_HALF) - player.x,
				clampf(player.z, MID_Z - SHAFT_HALF, MID_Z + SHAFT_HALF) - player.z).length()
			var at_rim := stage == DANGEROUS and rim > 0.05 and rim <= 3.0   # matron.cpp:1843-1866
			var pool3: Array = SHOVE_RIM if at_rim else SHOVE_PEST
			_emit("shove_line", pool3[_shove_i % pool3.size()], her, d, {"aimed_at_rim": at_rim, "rim_m": snappedf(rim, 0.1),
				"face_px": roundi(245.5 / maxf(d, 0.1))})
			_shove_i += 1
			_shove_line_t = t
			_shove_lines_this += 1
			_spoke()
		else:
			_shove_effort += 1
		if not first_name_known:
			first_name_known = true   # her hands on him: she has read his badge
	_shoves_seen = shoves

	# --- 7. A catch: the pupil check in the dead eyes.
	var catches := int(her.get("catches", 0))
	if _catches_seen >= 0 and catches > _catches_seen:
		var hour := float(ctx.get("hour", 10.5))
		_emit("pupil_check", PUPIL[3] % _say_time(hour), her, d, {"hour": snappedf(hour, 0.01)})
	_catches_seen = catches

	# --- 8. A death: what the player last yelled is hers to use.
	var deaths := int(ctx.get("deaths", 0))
	if _deaths_seen >= 0 and deaths > _deaths_seen and String(ctx.get("fear_clip", "")) != "":
		_mimic_clip = String(ctx["fear_clip"])
		_mimic_line = String(ctx.get("fear_line", ""))
		_mimic_armed = true
		_mimic_armed_at = t
		_emit("mimic_armed", _mimic_line, her, d, {"clip": _mimic_clip})
	_deaths_seen = deaths
	if _mimic_armed and out and not sees and t - _mimic_armed_at >= 20.0 and t - _mimic_at >= 30.0 and d >= 10.0 \
			and _voice_free() and _audible(head, ear, 55.0) \
			and (state == SEARCHING or state == SHUFFLING or state == GUARDING or (state == HUNTING and _unseen_s > 4.0)):
		var c: Array = carry(head, ear)
		var emitter: Vector3 = c[1]
		var below := emitter != head
		_emit("mimic", _mimic_line, her, d, {"clip": _mimic_clip, "pitch": 0.92, "carried_m": snappedf(float(c[0]), 0.1),
			"emitter": "shaft edge" if below else "her mouth", "emitter_at": str(emitter.snapped(Vector3.ONE * 0.1))})
		_spoke()
		_mimic_armed = false
		_mimic_at = t

	# --- 9. The radio answer: after Brenner's "She's below you", her on his channel.
	if _ring_line_said and not _radio_answered and out and not sees and d > 25.0 and radio == "":
		_radio_answered = true
		var air := 1.0 - float(carry(head, ear)[0]) / VOICE_REACH_M >= SPEAK_LEVEL
		_emit("radio_answer", "I'm not below you, dear.", her, d, {"radio_first": true, "air_twin": air,
			"air_delay_ms": roundi(d / 343.0 * 1000.0)})
		_spoke()

	# --- 10. Her breath in your hand: close, and not in your view.
	if out and d < 2.5 and alive:
		var to_her := (head - camera.origin).normalized()
		var forward := -camera.basis.z
		var behind := forward.dot(to_her) < cos(deg_to_rad(55.0))
		_breath_in -= delta
		if behind and _breath_in <= 0.0:
			# Still, she breathes slow and even (a resting 17 a minute); moving,
			# the view's own rasp (matron_view.gd:1350).
			_breath_in = 3.5 if float(her.get("speed", 0.0)) < 0.3 else lerpf(0.7, 2.0, clampf(d / 24.0, 0.0, 1.0))
			_haptic_breaths += 1
			if _haptic_breaths == 1 or _haptic_breaths % 10 == 0:
				_emit("haptic_breath", "", her, d, {"n": _haptic_breaths, "amp": snappedf(lerpf(0.22, 0.08, d / 2.5), 0.01)})
	else:
		_breath_in = 0.0

	# --- 11. The ladder: her on it below you, close.
	var close := bool(her.get("climbing", false)) and bool(ctx.get("climbing", false)) and flat < 1.5 \
		and player.y - centre.y > 0.5 and player.y - centre.y < 3.0
	if close and not _ladder_close:
		_emit("ladder_hang", "", her, d, {"below_m": snappedf(player.y - centre.y, 0.1)})
	_ladder_close = close


# How far a sound carries from `from` to the ear at `to`: Matron::carry_m's two
# ways (matron.cpp:274-297), the decks counted from the tower's floor list
# (matron.hpp: the stack's decks every kStackPitch 11 m under hers at 154, the
# rings every kRingPitch 22 m from 176.25) instead of a ray. Also where it is
# heard from: her mouth, or the shaft's edge nearest the ear (out/shaft_voice.txt).
static func carry(from: Vector3, to: Vector3) -> Array:
	var lo := minf(from.y, to.y)
	var hi := maxf(from.y, to.y)
	var decks := 0
	for k in range(0, 14):
		var y := 154.0 - 11.0 * float(k)
		if y > lo + 0.3 and y < hi - 0.3:
			decks += 1
	for k in range(0, 10):
		var y := 176.25 + 22.0 * float(k)
		if y > lo + 0.3 and y < hi - 0.3:
			decks += 1
	var straight := from.distance_to(to)
	if decks == 0:
		return [straight, from]
	var through := straight * pow(10.0, float(decks))
	var out_p := Vector3(clampf(from.x, -SHAFT_HALF, SHAFT_HALF), from.y, clampf(from.z, MID_Z - SHAFT_HALF, MID_Z + SHAFT_HALF))
	var in_p := Vector3(clampf(to.x, -SHAFT_HALF, SHAFT_HALF), to.y, clampf(to.z, MID_Z - SHAFT_HALF, MID_Z + SHAFT_HALF))
	var a := from.distance_to(out_p)
	var b := to.distance_to(in_p)
	var round_m := (a + out_p.distance_to(in_p) + b) * (1.4 if a > 0.5 else 1.0) * (1.4 if b > 0.5 else 1.0)
	return [through, from] if through <= round_m else [round_m, in_p]


# Her speaking voice carries VOICE_REACH_M (a walk's noise is heard 16 m, a
# sprint's 44 m: matron.hpp:207-208); a line is only spoken where its level at
# the player's ear would be at least SPEAK_LEVEL (she does not waste words).
const VOICE_REACH_M := 25.0
const SPEAK_LEVEL := 0.2
const VOICE_GAP_S := 2.5
var _voice_free_at := 0.0
var unheard_skipped := 0


func _audible(from: Vector3, ear: Vector3, reach := VOICE_REACH_M) -> bool:
	var c: Array = carry(from, ear)
	var ok := 1.0 - float(c[0]) / reach >= SPEAK_LEVEL
	if not ok:
		unheard_skipped += 1
	return ok


func _voice_free() -> bool:
	return t >= _voice_free_at


func _spoke() -> void:
	_voice_free_at = t + VOICE_GAP_S


static func _say_time(hour: float) -> String:
	var q := int(roundf(hour * 4.0))
	var h := (q / 4) % 12
	h = 12 if h == 0 else h
	var quarter := q % 4
	var names := ["", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven", "twelve"]
	match quarter:
		1:
			return "a quarter past %s" % names[h]
		2:
			return "half past %s" % names[h]
		3:
			return "a quarter to %s" % names[h % 12 + 1]
	return "%s o'clock" % names[h]


func summary() -> String:
	return "FEAR_SUMMARY t=%.1f cues=%d counts=%s name_known=%s first_name=%s haptic_breaths=%d shoves_wordless=%d unheard_skipped=%d" % [t,
		cues.size(), JSON.stringify(counts), str(name_known), str(first_name_known), _haptic_breaths, _shove_effort, unheard_skipped]
