extends RefCounted
## Which of her takes plays (03_EXECUTION/MATRON_REDESIGN/HER_SOUND_VARIATION_SPEC.md,
## "Selection and event contract"): the one owner of her selection history,
## kept by the director (audio_director.gd _her_cue) for every one-shot of
## her own bank.
##
## Each family (a cue name of hers) keeps a shuffled bag of its takes and the
## last H it started (H at most N-1 for N takes). The families of her mouth,
## voice and torn body also share one ring of the last RING takes started
## within RING_S: no take in it is chosen again, whatever cue asks. So no take
## of hers plays twice running, and none twice within the ring. A family with
## no take left to choose is silent (an optional cue), never a repeat; a
## required one (a tear, a blow) takes its least recent take that is not the
## one she started last, else is silent and counted short.
##
## Only a start commits: choose() reserves, commit() records once a player has
## started it (or, with no audio device, once it would have), so a cue with
## no free player uses up nothing. A family's C is the least time between two
## of its optional starts; P its pitch and G its gain in dB, applied once.
## Nothing here is cleared when her state changes: history outlives a hold, a
## restore and a camera move.

const RING := 4
const RING_S := 8.0
# Family -> [H, C s, P range, G dB range, ring, required]. The spec's
# starting values; her step, joints and plants (contacts, not her mouth) keep
# their own bag and history only, and the pitch their caller gives them.
const FAMILIES := {
	&"matron_breath": [3, 1.8, Vector2(0.96, 1.04), Vector2(-2.0, 0.0), true, false],
	&"matron_effort": [3, 1.2, Vector2(0.96, 1.04), Vector2(-1.5, 0.0), true, false],
	&"matron_shriek": [2, 4.0, Vector2(0.97, 1.03), Vector2(-1.0, 0.0), true, false],
	&"matron_swallow": [2, 8.0, Vector2(0.97, 1.03), Vector2(-2.0, 0.0), true, false],
	&"matron_laugh": [2, 12.0, Vector2(0.97, 1.02), Vector2(-2.0, 0.0), true, false],
	&"matron_rip": [2, 0.25, Vector2(0.97, 1.03), Vector2(-1.0, 0.0), true, true],
	&"matron_hit": [2, 0.35, Vector2(0.97, 1.04), Vector2(-1.5, 0.0), true, true],
	&"matron_croak": [2, 7.0, Vector2(0.96, 1.02), Vector2(-2.0, 0.0), true, false],
	&"matron_cluck": [2, 6.0, Vector2(0.98, 1.03), Vector2(-2.0, 0.0), true, false],
	&"matron_sniff": [2, 5.0, Vector2(0.97, 1.04), Vector2(-2.0, 0.0), true, false],
	&"matron_step": [3, 0.065, Vector2.ZERO, Vector2.ZERO, false, false],
	&"matron_plant": [3, 0.065, Vector2.ZERO, Vector2.ZERO, false, false],
	&"matron_joints": [3, 0.7, Vector2.ZERO, Vector2.ZERO, false, false],
}
# Why a cue played nothing (counted in suppressed).
const NO_FAMILY := &"no_family"
const EMPTY := &"empty"
const COOLDOWN := &"cooldown"
const HISTORY := &"history"
const SHORT := &"short"

var rng := RandomNumberGenerator.new()
# Every take started, oldest first: {take, family, index, t, enc}; `enc` the
# hold it was in (0: none). Kept to LOG_MAX, the oldest dropped.
const LOG_MAX := 2048
var started: Array = []
var started_total := 0
var suppressed := {}   # reason -> count

var _bag := {}       # family -> Array[int] takes not yet drawn this round
var _history := {}   # family -> Array[int] its last starts, newest last
var _last_t := {}    # family -> the time of its last start
var _ring: Array[Dictionary] = []   # {family, index, t}, newest last
var _last_take := &""   # the take she started last, of any family


func _init(seed_value: int = 0x4D4154) -> void:
	rng.seed = seed_value


static func is_hers(family: StringName) -> bool:
	return FAMILIES.has(family)


static func take_id(family: StringName, index: int) -> StringName:
	return StringName("%s_%d" % [family, index])


static func in_ring(family: StringName) -> bool:
	return FAMILIES.has(family) and bool(FAMILIES[family][4])


# A take of `family`'s `count` to start now (`clock` her presentation time),
# or -1: silent, the reason counted. Never the take she started last, of any
# family. A required family (a tear's, a blow's) is held back by no cooldown.
func choose(family: StringName, count: int, clock: float) -> int:
	if not FAMILIES.has(family):
		_suppress(NO_FAMILY)
		return -1
	if count <= 0:
		_suppress(EMPTY)
		return -1
	var spec: Array = FAMILIES[family]
	var required := bool(spec[5])
	if not required and clock - float(_last_t.get(family, -INF)) < float(spec[1]):
		_suppress(COOLDOWN)
		return -1
	var history: Array = _history.get(family, [])
	var keep := mini(int(spec[0]), count - 1)
	var recent := history.slice(maxi(0, history.size() - keep))
	var ringed := _ringed(family, clock) if bool(spec[4]) else []
	var eligible: Array[int] = []
	for k in count:
		if not (k in recent) and not (k in ringed) and take_id(family, k) != _last_take:
			eligible.append(k)
	if eligible.is_empty():
		if not required:
			_suppress(HISTORY)
			return -1
		# A tear or a blow is heard: her least recent take, never the one she
		# started last.
		for k in _by_age(family, count):
			if take_id(family, k) != _last_take:
				return k
		_suppress(SHORT)
		return -1
	# From the bag first; an emptied bag refills, history still applied.
	var bag: Array = _bag.get(family, [])
	var from_bag: Array[int] = []
	for k in eligible:
		if k in bag:
			from_bag.append(k)
	if from_bag.is_empty():
		bag = range(count)
		_bag[family] = bag
		from_bag = eligible
	return from_bag[rng.randi_range(0, from_bag.size() - 1)]


# `family`'s take `index` started at `clock` (in hold `encounter`, 0 none).
func commit(family: StringName, index: int, clock: float, encounter: int) -> void:
	var bag: Array = _bag.get(family, [])
	bag.erase(index)
	_bag[family] = bag
	var history: Array = _history.get(family, [])
	history.append(index)
	if history.size() > 8:
		history.pop_front()
	_history[family] = history
	_last_t[family] = clock
	if in_ring(family):
		_ring.append({"family": family, "index": index, "t": clock})
		if _ring.size() > RING:
			_ring.pop_front()
	var id := take_id(family, index)
	_last_take = id
	started_total += 1
	started.append({"take": id, "family": family, "index": index, "t": clock, "enc": encounter})
	if started.size() > LOG_MAX:
		started = started.slice(started.size() - LOG_MAX / 2)


# Her pitch for a take of `family` (1.0 for a family that keeps its caller's).
func pitch(family: StringName) -> float:
	var p: Vector2 = FAMILIES[family][2] if FAMILIES.has(family) else Vector2.ZERO
	return 1.0 if p == Vector2.ZERO else rng.randf_range(p.x, p.y)


# Her gain in dB for a take of `family`, under its nominal level.
func gain_db(family: StringName) -> float:
	var g: Vector2 = FAMILIES[family][3] if FAMILIES.has(family) else Vector2.ZERO
	return 0.0 if g == Vector2.ZERO else rng.randf_range(g.x, g.y)


# Whether `family` keeps its caller's pitch (a contact, not her voice).
static func keeps_pitch(family: StringName) -> bool:
	return not FAMILIES.has(family) or FAMILIES[family][2] == Vector2.ZERO


func _ringed(family: StringName, clock: float) -> Array:
	var out: Array = []
	for entry in _ring:
		if entry["family"] == family and clock - float(entry["t"]) < RING_S:
			out.append(int(entry["index"]))
	return out


# `family`'s takes, least recently started first (never started first of all).
func _by_age(family: StringName, count: int) -> Array:
	var history: Array = _history.get(family, [])
	var order: Array = range(count)
	order.sort_custom(func(a: int, b: int) -> bool: return history.rfind(a) < history.rfind(b))
	return order


func _suppress(reason: StringName) -> void:
	suppressed[reason] = int(suppressed.get(reason, 0)) + 1
