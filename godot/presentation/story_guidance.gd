extends RefCounted

# Narrative observation only. Native contacts, machinery and checkpoints remain
# authoritative. Heard lines are session presentation history, not saved progress.
const DoubleEncounter := preload("res://presentation/story_double_encounter.gd")
const TICKS_PER_SECOND := 90
const INTRO_TICKS := 7 * TICKS_PER_SECOND
const TRANSMISSION_TICKS := 11 * TICKS_PER_SECOND
const CUES := {
	&"receiving_iona_come_look": "Ro. You’re in Receiving. Your name’s still on the handover. Come look at what it calls empty.",
	&"receiving_iona_freight_one": "Freight One is eleven metres up. Find firm footing there. The crane yard is ahead.",
	&"freight_one_iona_crown": "Our freight line still shares the construction return at the Crown. They remove that, we lose the way up. We need it separated.",
	&"receiving_iona_keep_the_height": "You’ve found your way up. Keep the height. The Crown is still above you.",
	&"receiving_iona_recovery": "Find your footing. You can try another way up.",
	&"vale_contact_ro": "You kept me as your emergency contact?",
	&"vale_contact_iona": "They asked for next of kin. Not a fucking recommendation."
}
const SERVICE_WALKS := [
	[33.0, "EXTERIOR SERVICE WALK"], [44.0, "SWINGING STAIR LANDING"],
	[66.0, "UPPER SERVICE FRAME"], [88.0, "BRACED WORKSITE"],
	[110.0, "NORTH FRAME"], [121.0, "SUSPENDED LADDER LANDING"],
	[143.0, "SERVICE LIFT LANDING"], [165.0, "BALANCE DECK"],
	[198.0, "CROWN GONDOLA LANDING"], [231.0, "TRACTION TRAM LANDING"],
	[253.0, "BARREL HELIX LANDING"], [286.0, "CASCADE MAST LANDING"],
	[308.0, "PITMAN LANDING"]
]

class Observation:
	var position := Vector3.ZERO
	var grounded := false
	var firm := false
	var support := 0
	var deaths := 0
	var station := &""
	var operating := &""
	var hanging := false

	func read(context: Dictionary) -> bool:
		var supplied: Variant = context.get("position")
		if not supplied is Vector3 or not supplied.is_finite():
			return false
		position = supplied
		grounded = bool(context.get("grounded", false))
		firm = grounded and bool(context.get("checkpoint_footing_valid", false))
		support = int(context.get("support_entity", 0))
		deaths = int(context.get("deaths", 0))
		station = StringName(context.get("station", &""))
		operating = StringName(context.get("operating", &""))
		hanging = bool(context.get("hanging", false)) or bool(context.get("climbing", false))
		return support >= 0 and deaths >= 0

var _observation := Observation.new()
var _start_tick := -1
var _last_tick := -1
var _last_deaths := 0
var _intro_at_arrival := false
var _origin := Vector3.ZERO
var _heard: Dictionary = {}
var _transmission_id := 0
var _transmission_until := -1
var _speaker := ""
var _line := ""
var _arrival_seen := false
var _last_supported_height := 0.0
var _cue_key := &""
var _voice_lengths: Dictionary = {}
var _contact_pending := false
var _contact_phase := 0
var _double_encounter := DoubleEncounter.new()

func configure_voice_lengths(lengths: Dictionary) -> void:
	_voice_lengths = lengths.duplicate()

func reset() -> void:
	_start_tick = -1
	_last_tick = -1
	_last_deaths = 0
	_intro_at_arrival = false
	_heard.clear()
	# Keep serials unique across a native retry: the audio owner must be able
	# to distinguish the new introduction from a previously started line.
	_transmission_until = -1
	_speaker = ""
	_line = ""
	_arrival_seen = false
	_last_supported_height = 0.0
	_cue_key = &""
	_contact_pending = false
	_contact_phase = 0
	_double_encounter.reset()

func _say(key: StringName, tick: int) -> bool:
	if _heard.has(key) or (not CUES.has(key) and not DoubleEncounter.CUES.has(key)):
		return false
	_heard[key] = true
	_transmission_id += 1
	var voice_seconds := float(_voice_lengths.get(key, 0.0))
	var duration := maxi(TRANSMISSION_TICKS, ceili((voice_seconds + .75) * TICKS_PER_SECOND)) if is_finite(voice_seconds) else TRANSMISSION_TICKS
	if key == &"vale_contact_ro" or key == &"vale_contact_iona" or DoubleEncounter.CUES.has(key):
		duration = ceili((voice_seconds + .35) * TICKS_PER_SECOND) if is_finite(voice_seconds) and voice_seconds > 0.0 else TRANSMISSION_TICKS
	_transmission_until = tick + duration
	_speaker = String(DoubleEncounter.SPEAKERS.get(key, "RO" if key == &"vale_contact_ro" else "IONA"))
	_line = String(CUES.get(key, DoubleEncounter.CUES.get(key, "")))
	_cue_key = key
	return true

func _on_tower_access() -> bool:
	var p := _observation.position
	if not _observation.firm or _observation.support == 0 or p.y < 11.65:
		return false
	# These footprints are source-authored receivers. A flying apex, truck
	# altitude or unrelated roof at the same height cannot complete arrival.
	if _observation.support == 2952:
		return p.x >= 17.5 and p.x <= 22.5 and p.z >= -120.8 and p.z <= -118.0
	if _observation.support == 1451:
		return p.x >= -36.0 and p.x <= -25.5 and p.z >= -144.0 and p.z <= -136.0
	# A real player-built support or higher landing also counts as access.
	# This does not assert that a vehicle or cargo arrived with the player.
	return p.x >= -27.0 and p.x <= 27.0 \
		and p.z >= -177.0 and p.z <= -123.0

func sample(context: Dictionary, tick: int) -> Dictionary:
	if tick < 0 or not _observation.read(context):
		_double_encounter.cancel_attention()
		return {}
	if _last_tick >= 0 and tick < _last_tick:
		reset()
	var fresh := tick != _last_tick
	if _start_tick < 0:
		_start_tick = tick
		_origin = _observation.position
		_last_deaths = _observation.deaths
		_intro_at_arrival = _origin.distance_to(Vector3(-94, 1.2, -44)) < 4.0
		if _intro_at_arrival:
			_say(&"receiving_iona_come_look", tick)
	if fresh and _observation.deaths != _last_deaths:
		_last_deaths = _observation.deaths
		_double_encounter.cancel_attention()
		_transmission_until = -1
		_say(&"receiving_iona_recovery", tick)
	if fresh and _on_tower_access():
		_arrival_seen = true
		_last_supported_height = _observation.position.y - .9
		_say(&"receiving_iona_keep_the_height" if _observation.position.y > 34.0 else &"freight_one_iona_crown", tick)
	elif fresh and not _arrival_seen and _intro_at_arrival and tick > _transmission_until:
		_say(&"receiving_iona_freight_one", tick)
	if fresh and _contact_phase == 0 and context.get("story_discovery", &"") == &"vale_emergency_contact":
		_contact_pending = true
	if fresh and _contact_pending and tick > _transmission_until \
			and _observation.firm and _observation.operating.is_empty():
		if _contact_phase == 0:
			_say(&"vale_contact_ro", tick)
			_contact_phase = 1
		elif _contact_phase == 1:
			_say(&"vale_contact_iona", tick)
			_contact_phase = 2
			_contact_pending = false
	var double_key: StringName = _double_encounter.observe(context, tick)
	if fresh and not double_key.is_empty() and tick > _transmission_until:
		if _say(double_key, tick):
			_double_encounter.accept(double_key)
	_last_tick = tick

	var objective_id := &"freight_one"
	var objective := "REACH FREIGHT ONE  /  +11 M"
	var detail := "Crane yard ahead, toward the tower. Reach its first freight ring; choose your route."
	if _arrival_seen:
		objective_id = &"crown"
		objective = "REACH THE CROWN SERVICE DECK"
		detail = "Keep finding real ways up. The shared freight return ends at the Crown."
		for destination in SERVICE_WALKS:
			if float(destination[0]) > _last_supported_height + .2:
				detail = "NEARBY: %s  /  +%d M" % [destination[1], int(destination[0])]
				break
		if not _on_tower_access() and _observation.position.y < 10.0:
			objective_id = &"return_to_spine"
			objective = "RETURN TO THE FREIGHT SPINE"
			detail = "Your route changed. Find safe footing, then choose a way back up."
	if _observation.operating == &"forklift":
		detail = "Move cargo to open a route. The crane can carry this truck to the tower dock."
	elif _observation.station == &"yard_crane" or _observation.operating == &"yard_crane":
		detail = "The crane can transfer the truck to the tower dock. Rig all four lifting eyes."
	elif bool(context.get("grapple_line_attached", false)):
		detail = "REEL IN draws you toward the hook. DROP releases; steer toward clear footing."
	elif not _observation.grounded and not _observation.hanging and _observation.position.y > 4.0:
		objective_id = &"landing"
		objective = "FIND CLEAR FOOTING"
		detail = "Steer toward a clear landing. Deploy your canopy for a long fall."
	var intro_visible := _intro_at_arrival and tick - _start_tick < INTRO_TICKS \
		and _observation.position.distance_to(_origin) < 3.0
	return {"intro_title": "SCRAPERX", "intro_subtitle": "THE EXCHANGE  /  RECEIVING", "intro_visible": intro_visible,
		"chapter": "THE COMMON WAY" if _arrival_seen else "1 / EMPTY ON PAPER", "objective_id": objective_id, "objective": objective, "detail": detail,
		"transmission_key": _cue_key,
		"transmission_id": _transmission_id, "speaker": _speaker if tick <= _transmission_until else "",
		"transmission": _line if tick <= _transmission_until else ""}
