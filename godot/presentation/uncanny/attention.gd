extends RefCounted
## What the player's eyes rest on, for her chart (HER_CHART.md). Ten times a
## second the camera's aim is cast into the native world (the bridge's
## get_look_target: the same bodies the simulation owns, the player's own
## body and the load in the hands seen through), and while the aim rests on
## one thing -- the view turning slower than REST_TURN_RAD_S -- the time is
## added to that thing's dwell.
##
## Kit bodies and loose stock are kept by their entity; the Matron, her
## locker, the crew and the player's own bodies by what they are; the
## structure by where the look goes: down off a height, up the tower, out at
## the mountains or the sky, at a wall or the floor close in front. At most
## MAX_ENTRIES things are kept; a new one pushes out the least looked at.
## Presentation only: nothing here is simulation state.

const SAMPLE_S := 0.1
const REACH_M := 90.0
const MAX_ENTRIES := 40
const REST_TURN_RAD_S := 1.0
# The native's entity ranges (simulation.hpp, mechanism_kit.hpp, crew.hpp,
# matron.hpp, physics_world_support.hpp).
const KIT_STATIC_MIN := 1000
const KIT_DYNAMIC_MIN := 2000
const KIT_MAX := 3000
const STOCK_MIN := 5000
const STOCK_MAX := 6000
const CREW_MIN := 3100
const CREW_MAX := 3200
const MATRON := 3200
const LOCKER := 3201
const MATRON_PLANKS_MIN := 3210
const CORPSE_MIN := 3300
const CORPSE_MAX := 3300 + 0x10000
# Looking down off a height: the eyes this far above what they meet, the
# look this steep. Up the tower: what is met this far above the eyes.
const DOWN_DROP_M := 6.0
const DOWN_PITCH_Y := -0.25
const UP_RISE_M := 12.0
const OUT_M := 60.0
# A wall or the floor counts only this close, and only standing.
const CLOSE_M := 4.0

const KIND_THING := &"thing"
const KIND_MATRON := &"matron"
const KIND_LOCKER := &"locker"
const KIND_CREW := &"crew"
const KIND_BODY := &"body"
const KIND_DOWN := &"down"
const KIND_UP := &"up"
const KIND_OUT := &"out"
const KIND_SKY := &"sky"
const KIND_WALL := &"wall"
const KIND_FLOOR := &"floor"

# key -> {key, kind, entity, total, longest, looks, first_at, last_at, where,
# feet_y}. `where` is where the look last met it; `feet_y` where the player
# stood.
var entries := {}
# What the aim rests on now ("" when nothing, or when it is moving), and for
# how long without a break.
var current_key := ""
var current_s := 0.0
var samples := 0
var _clock := 0.0
var _last_forward := Vector3.ZERO


static func is_thing(entity: int) -> bool:
	return (entity >= KIT_DYNAMIC_MIN and entity < KIT_MAX) or (entity >= STOCK_MIN and entity < STOCK_MAX)


# One frame. `now` is the uncanny clock (seconds of play); `attending` false
# (the view riding a death, a menu) breaks any look.
func update(delta: float, native: Object, eye: Vector3, forward: Vector3, now: float, attending: bool,
		standing: bool, feet_y: float) -> void:
	_clock += delta
	if _clock < SAMPLE_S:
		return
	var dt := _clock
	_clock = 0.0
	sample(native, eye, forward, now, dt, attending, standing, feet_y)


func sample(native: Object, eye: Vector3, forward: Vector3, now: float, dt: float, attending: bool,
		standing: bool, feet_y: float) -> void:
	samples += 1
	var turned := 0.0 if _last_forward == Vector3.ZERO else _last_forward.angle_to(forward)
	_last_forward = forward
	if not attending or native == null or forward.length() < 0.5 or turned / maxf(dt, 0.001) > REST_TURN_RAD_S:
		current_key = ""
		current_s = 0.0
		return
	var seen := classify(native, eye, forward, standing, feet_y)
	if seen.is_empty():
		current_key = ""
		current_s = 0.0
		return
	var key: String = seen["key"]
	if not entries.has(key):
		_make_room(key)
		entries[key] = {"key": key, "kind": seen["kind"], "entity": int(seen["entity"]), "total": 0.0,
			"longest": 0.0, "looks": 0, "first_at": now, "last_at": now, "where": seen["where"], "feet_y": feet_y}
	var entry: Dictionary = entries[key]
	if key != current_key:
		current_key = key
		current_s = 0.0
		entry["looks"] = int(entry["looks"]) + 1
	current_s += dt
	entry["total"] = float(entry["total"]) + dt
	entry["longest"] = maxf(float(entry["longest"]), current_s)
	entry["last_at"] = now
	entry["where"] = seen["where"]
	entry["feet_y"] = feet_y


# What a look from `eye` along `forward` rests on: {key, kind, entity,
# where}, or empty when it is nothing the chart could name truly.
func classify(native: Object, eye: Vector3, forward: Vector3, standing: bool, feet_y: float) -> Dictionary:
	var band := int(floorf(feet_y / 4.0))
	var hit: Dictionary = native.get_look_target(eye, forward, REACH_M)
	if not bool(hit.get("hit", false)):
		if forward.y < DOWN_PITCH_Y:
			return {"key": "down:%d" % band, "kind": KIND_DOWN, "entity": 0, "where": eye}
		if forward.y > 0.35:
			return {"key": "sky", "kind": KIND_SKY, "entity": 0, "where": eye}
		return {"key": "out", "kind": KIND_OUT, "entity": 0, "where": eye}
	var entity := int(hit["entity"])
	var point: Vector3 = hit["point"]
	var normal: Vector3 = hit["normal"]
	var distance := float(hit["distance"])
	if is_thing(entity):
		return {"key": "e:%d" % entity, "kind": KIND_THING, "entity": entity, "where": point}
	if entity == MATRON:
		return {"key": "matron", "kind": KIND_MATRON, "entity": entity, "where": point}
	if entity == LOCKER:
		return {"key": "locker", "kind": KIND_LOCKER, "entity": entity, "where": point}
	if entity >= CREW_MIN and entity < CREW_MAX:
		return {"key": "crew", "kind": KIND_CREW, "entity": entity, "where": point}
	if entity >= CORPSE_MIN and entity < CORPSE_MAX:
		return {"key": "body", "kind": KIND_BODY, "entity": entity, "where": point}
	# The structure: named by where the look goes, never by a guess at what
	# it is.
	if eye.y - point.y > DOWN_DROP_M and forward.y < DOWN_PITCH_Y:
		return {"key": "down:%d" % band, "kind": KIND_DOWN, "entity": 0, "where": point}
	if point.y - eye.y > UP_RISE_M:
		return {"key": "up:%d" % band, "kind": KIND_UP, "entity": 0, "where": point}
	if distance > OUT_M:
		return {"key": "out", "kind": KIND_OUT, "entity": 0, "where": point}
	if standing and distance < CLOSE_M:
		var cell := Vector3i(floori(point.x / 2.0), floori(point.y / 2.0), floori(point.z / 2.0))
		if normal.y > 0.7:
			return {"key": "floor:%d:%d:%d" % [cell.x, cell.y, cell.z], "kind": KIND_FLOOR, "entity": 0, "where": point}
		if absf(normal.y) < 0.4:
			return {"key": "wall:%d:%d:%d" % [cell.x, cell.y, cell.z], "kind": KIND_WALL, "entity": 0, "where": point}
	return {}


# The thing looked at longest, in all, of those looked at for at least
# `min_total` seconds and last looked at no later than `before` (the chart
# writes late), skipping `written` keys. Empty when there is none.
func longest(min_total: float, before: float, written: Dictionary) -> Dictionary:
	var best := {}
	for key in entries:
		var entry: Dictionary = entries[key]
		if written.has("look:" + String(key)) or float(entry["total"]) < min_total or float(entry["last_at"]) > before:
			continue
		if best.is_empty() or float(entry["total"]) > float(best["total"]):
			best = entry
	return best


# A new key at a full table pushes out the entry looked at least (never the
# one the aim rests on now).
func _make_room(incoming: String) -> void:
	while entries.size() >= MAX_ENTRIES:
		var weakest := ""
		for key in entries:
			if key == current_key or key == incoming:
				continue
			if weakest.is_empty() or float(entries[key]["total"]) < float(entries[weakest]["total"]):
				weakest = key
		if weakest.is_empty():
			return
		entries.erase(weakest)
