extends RefCounted
## Her chart (03_EXECUTION/UNCANNY/HER_CHART.md): she was the ward's matron,
## and matrons keep charts. What the tower has seen of the player -- what
## the eyes rested on longest and how long (attention.gd), when they play by
## the phone's own clock, where they died, how they stand at an edge, their
## own voice heard back -- written as ward notes in her hand, plain and exact.
##
## The rules (the owner's): accurate or silent -- a line is written only from
## what was recorded, and a thing is named only when it is known for what it
## is; late, never instant -- nothing is written about a moment until the
## profile's late_s has passed; rare -- the first page after first_page_s of
## play, the next no sooner than page_gap_s after it. Early pages are vague,
## later ones exact. Only what the player gives the game: their play, their
## clock, their voice with consent; nothing else on the phone.
##
## This session's observations live here; across sessions only a little is
## kept (persistent runs only, in the profile's storage_dir): when the player
## has played, how many pages she has written, and the pages left in the
## tower, so they are where they were.

const StockView := preload("res://presentation/stock_view.gd")
const Attention := preload("res://presentation/uncanny/attention.gd")

const FILE := "chart.json"
const PAGE_LINES_MAX := 5
const DEATHS_KEPT := 16
const ECHOES_KEPT := 8
const CLIPS_KEPT := 32
const SESSIONS_KEPT := 12
const PAGES_KEPT := 8
# A clip counts as the player's voice from this share of voiced windows
# (voice_keeper.gd's plain autocorrelation, no model).
const VOICED_MIN := 0.35
# Said within this long after a death: said when the climb went badly.
const BAD_WINDOW_S := 30.0
# A death is a fall from this drop.
const FALL_MIN_M := 2.5
const NIGHT_HOURS := [0, 1, 2, 3, 4]
const WEEKDAYS := ["Sun.", "Mon.", "Tues.", "Wed.", "Thurs.", "Fri.", "Sat."]
const MONTHS := ["Jan.", "Feb.", "March", "April", "May", "June", "July", "Aug.", "Sept.", "Oct.", "Nov.", "Dec."]
# The levels a place is named by: [height of the floor, its name]. The
# Stack's decks every 11 m to 154, then the rings, plates and decks the
# route stands on.
const LEVELS := [
	[11.0, "the 11 m deck"], [22.0, "the 22 m deck"], [33.0, "the 33 m deck"], [44.0, "the 44 m deck"],
	[55.0, "the 55 m deck"], [66.0, "the 66 m deck"], [77.0, "the 77 m deck"], [88.0, "the 88 m deck"],
	[99.0, "the 99 m deck"], [110.0, "the 110 m deck"], [121.0, "the 121 m deck"], [132.0, "the 132 m deck"],
	[143.0, "the 143 m deck"], [154.0, "the 154 m deck"], [176.25, "the 176 m ring"], [198.25, "the 198 m ring"],
	[220.25, "the 220 m ring"], [242.25, "the 242 m ring"], [264.25, "the 264 m ring"],
	[291.25, "the 291 m walkway"], [340.25, "the 340 m plate"], [374.25, "the 374 m ring"],
	[418.25, "the 418 m ring"], [484.25, "the 484 m ring"], [640.25, "the 640 m plate"],
	[662.1, "the 662 m deck"], [684.1, "the 684 m deck"], [706.1, "the 706 m deck"],
	[728.1, "the 728 m deck"], [750.1, "the 750 m deck"], [803.5, "the top"],
]
# Loose stock by kind (src/sim/loose_stock.hpp LooseStock::Kind), as she
# names it, and the colors by which its paint is told apart (stock_view.gd).
const STOCK_NAMES := ["crate", "dolly of ingots", "timber beam", "oil drum", "pallet", "shipping container",
	"steel beam", "sleeper", "pipe cradle", "stack of steel plate", "slab of steel plate", "length of pipe",
	"stillage of steel billets", "coil of steel", "gas bottle", "reel of cable", "toolbox", "bundle of planks",
	"crate", "reel of cable", "slab of plate", "cage of gas bottles", "pig of iron"]
const STOCK_COLORS := {3: StockView.DRUM_COLOR_NAMES, 5: StockView.CONTAINER_COLOR_NAMES,
	6: StockView.BEAM_COLOR_NAMES, 14: StockView.BOTTLE_COLOR_NAMES, 16: StockView.TOOLBOX_COLOR_NAMES}
# The machines' parts she can name truly (main_context.gd's carry names and
# the kit's own entities); any other part of a machine is "something".
const KIT_NAMES := {2000: "the skip lift's cage", 2001: "the skip", 2010: "the cage", 2011: "the derrick's boom",
	2020: "the platform", 2021: "the dumpster", 2031: "the pipe spool", 2042: "the door", 2200: "the lift cage",
	2958: "the pig of iron", 2959: "the pig of iron", 2960: "the pig of iron", 2961: "the pig of iron",
	2962: "the pig of iron", 2963: "the pig of iron", 2982: "the forklift", 2989: "the forklift",
	2990: "the forklift", 2991: "the forklift", 2987: "the crane's sling", 2995: "the crane's magnet",
	2996: "the crane's spreader", 2970: "the hook's gun"}

var profile: RefCounted
var attention: RefCounted
var persistent := false
var storage_dir := ""
# Loose stock entity -> [kind, variant] (the bridge's get_loose_stock).
var stock := {}

# This session.
var deaths: Array = []          # {at, hhmm, y, drop}
var edge_at := 0
var edge_back := 0
var edge_ready_at := INF        # the clock when enough edge samples were in
var echoes: Array = []          # {at, hhmm, y, stopped (-1 unknown, 0, 1)}
var clips: Array = []           # {at, voiced}
var written := {}               # what is already in a page, by fact id
var session_pages := 0
var last_page_at := -INF
var last_lines := PackedStringArray()
var last_facts := PackedStringArray()

# Kept across sessions (persistent runs).
var sessions: Array = []        # {unix, night}
var pages_written := 0
var pages: Array = []           # {spot, lines, unix}


func setup(the_profile: RefCounted, the_attention: RefCounted, keep: bool, dir: String) -> void:
	profile = the_profile
	attention = the_attention
	persistent = keep
	storage_dir = dir
	_load_kept()


func set_stock(packed: PackedInt32Array, native: Object) -> void:
	stock.clear()
	for i in range(0, packed.size() - 2, 3):
		var entity := int(native.get_kit_body_entity_id(packed[i]))
		stock[entity] = [packed[i + 1], packed[i + 2]]


# A sitting at the game: a NEW CLIMB or a restart within SESSION_GAP_S of the
# last is the same one.
const SESSION_GAP_S := 4.0 * 3600.0


func begin_session(unix: float, local: Dictionary) -> void:
	if not sessions.is_empty() and unix - float(sessions[-1]["unix"]) < SESSION_GAP_S:
		return
	sessions.append({"unix": unix, "night": int(local["hour"]) in NIGHT_HOURS})
	while sessions.size() > SESSIONS_KEPT:
		sessions.pop_front()
	_save_kept()


# --- what is seen --------------------------------------------------------------


func note_death(at: float, local: Dictionary, y: float, drop: float) -> void:
	deaths.append({"at": at, "hhmm": hhmm(local), "y": y, "drop": drop, "id": "death:%d" % _serial()})
	if deaths.size() > DEATHS_KEPT:
		deaths.pop_front()


func note_edge(at_edge: bool, now: float) -> void:
	if at_edge:
		edge_at += 1
	else:
		edge_back += 1
	if edge_at + edge_back >= int(profile.edge_samples_min) and edge_ready_at == INF:
		edge_ready_at = now


func note_echo(at: float, local: Dictionary, y: float) -> int:
	echoes.append({"at": at, "hhmm": hhmm(local), "y": y, "stopped": -1, "id": "echo:%d" % _serial()})
	if echoes.size() > ECHOES_KEPT:
		echoes.pop_front()
	return echoes.size() - 1


func note_echo_reaction(index: int, stopped: bool) -> void:
	if index >= 0 and index < echoes.size():
		echoes[index]["stopped"] = 1 if stopped else 0


func note_clip(at: float, voiced: float) -> void:
	clips.append({"at": at, "voiced": voiced})
	if clips.size() > CLIPS_KEPT:
		clips.pop_front()


var _serials := 0
func _serial() -> int:
	_serials += 1
	return _serials


# --- the page ------------------------------------------------------------------


# A page is due: the first after first_page_s of play, each after that
# page_gap_s after the one before.
func due(now: float) -> bool:
	if session_pages == 0:
		return now >= float(profile.first_page_s)
	return now - last_page_at >= float(profile.page_gap_s)


# The page she would write now: {lines, facts}, its lines empty when there is
# nothing yet old enough to write (accurate or silent: the date alone is not
# a page). `bodies` is how many of the player's bodies lie in the tower now.
func compose(now: float, local: Dictionary, bodies: int) -> Dictionary:
	var index := pages_written
	var before := now - float(profile.late_s)
	var lines := PackedStringArray()
	var facts := PackedStringArray()
	for make in [_look_line, _death_line, _edge_line, _echo_line, _talk_line, _session_line]:
		if lines.size() >= PAGE_LINES_MAX - 2:
			break
		var made: Array = make.call(index, before, bodies)
		if made.is_empty():
			continue
		lines.append(made[0])
		facts.append_array(made[1])
	if lines.is_empty():
		return {"lines": PackedStringArray(), "facts": facts}
	var page := PackedStringArray([date_line(local), time_line(local)])
	page.append_array(lines)
	return {"lines": page, "facts": facts}


# The page is in the tower: what it says is written, and kept.
func commit(spot: String, page: Dictionary, now: float, unix: float) -> void:
	for fact in page["facts"]:
		written[fact] = true
	last_lines = page["lines"]
	last_facts = page["facts"]
	session_pages += 1
	last_page_at = now
	pages_written += 1
	pages.append({"spot": spot, "lines": Array(page["lines"]), "unix": unix})
	while pages.size() > PAGES_KEPT:
		pages.pop_front()
	_save_kept()


static func hhmm(local: Dictionary) -> String:
	return "%02d:%02d" % [int(local["hour"]), int(local["minute"])]


static func date_line(local: Dictionary) -> String:
	return "%s %s %d" % [WEEKDAYS[clampi(int(local.get("weekday", 0)), 0, 6)],
		MONTHS[clampi(int(local.get("month", 1)) - 1, 0, 11)], int(local.get("day", 1))]


static func time_line(local: Dictionary) -> String:
	var hour := int(local["hour"])
	if hour in NIGHT_HOURS:
		return "%s. Still awake." % hhmm(local)
	if hour == 5 or hour == 6:
		return "%s. Up early." % hhmm(local)
	if hour >= 22:
		return "%s. Up late." % hhmm(local)
	return "%s. On the tower." % hhmm(local)


# The level a height stands on ("the 154 m deck"), or "" off every level.
static func level_name(y: float) -> String:
	if y < 3.0:
		return "the yard"
	for level in LEVELS:
		if y >= float(level[0]) - 0.6 and y < float(level[0]) + 3.5:
			return String(level[1])
	return ""


# Where a height is, as she writes it: "on the 154 m deck", "in the yard",
# "at 150 m".
static func place_phrase(y: float) -> String:
	var name := level_name(y)
	if name == "the yard":
		return "in the yard"
	if name != "":
		return "on " + name
	return "at %d m" % roundi(y)


# A loose piece or a machine's part as she names it ("the yellow oil drum"),
# or "" when it is not known for what it is.
func thing_name(entity: int, with_color: bool) -> String:
	if stock.has(entity):
		var kind := int(stock[entity][0])
		if kind < 0 or kind >= STOCK_NAMES.size():
			return ""
		var noun: String = STOCK_NAMES[kind]
		if with_color and STOCK_COLORS.has(kind):
			var colors: Array = STOCK_COLORS[kind]
			return "the %s %s" % [colors[int(stock[entity][1]) % colors.size()], noun]
		return "the " + noun
	return String(KIT_NAMES.get(entity, ""))


func _look_line(index: int, before: float, _bodies: int) -> Array:
	if attention == null:
		return []
	var entry: Dictionary = attention.longest(float(profile.look_min_s), before, written)
	if entry.is_empty():
		return []
	var exact := index >= 2
	var seconds := roundi(float(entry["total"]))
	var how_long := ("for %d seconds" % seconds) if exact else "for a long time"
	var line := ""
	match StringName(entry["kind"]):
		Attention.KIND_THING:
			var name := thing_name(int(entry["entity"]), exact)
			var there := place_phrase((entry["where"] as Vector3).y)
			if name == "":
				line = "Looked at something %s %s." % [there, how_long] if index >= 1 else \
					"Looked at something for a long time."
			elif index == 0:
				line = "Looked at %s %s." % [name, how_long]
			else:
				line = "Looked at %s %s %s." % [name, there, how_long]
		Attention.KIND_MATRON:
			line = "Watched me %s." % how_long
		Attention.KIND_LOCKER:
			line = "Looked at my locker %s." % how_long
		Attention.KIND_CREW:
			line = "Watched the crew %s." % how_long
		Attention.KIND_BODY:
			line = "Looked at own body %s." % how_long
		Attention.KIND_DOWN:
			var from := level_name(float(entry["feet_y"]))
			if from == "":
				from = "%d m" % roundi(float(entry["feet_y"]))
			line = "Looked down %s." % how_long if index == 0 else "Looked down from %s %s." % [from, how_long]
		Attention.KIND_UP:
			line = "Looked up the tower %s." % how_long
		Attention.KIND_OUT:
			line = "Looked out at the mountains %s." % how_long
		Attention.KIND_SKY:
			line = "Looked at the sky %s." % how_long
		Attention.KIND_WALL:
			line = "Stood facing the wall %s." % how_long
		Attention.KIND_FLOOR:
			line = "Looked at the floor %s." % how_long
	if line == "":
		return []
	return [line, PackedStringArray(["look:" + String(entry["key"])])]


func _death_line(index: int, before: float, bodies: int) -> Array:
	var fresh: Array = deaths.filter(func(d: Dictionary) -> bool:
		return not written.has(String(d["id"])) and float(d["at"]) <= before)
	if fresh.is_empty():
		return []
	var ids := PackedStringArray()
	var all_falls := true
	for d in fresh:
		ids.append(String(d["id"]))
		all_falls = all_falls and float(d["drop"]) >= FALL_MIN_M
	var last: Dictionary = fresh[-1]
	var fell := float(last["drop"]) >= FALL_MIN_M
	var line := ""
	if index == 0:
		var verb := "Fell" if all_falls else "Died"
		line = "%s." % verb if fresh.size() == 1 else ("%s twice." % verb if fresh.size() == 2 else
			"%s %d times." % [verb, fresh.size()])
	else:
		var level := level_name(float(last["y"]))
		var where := ("to " + level) if level != "" else ("at %d m" % roundi(float(last["y"])))
		if fell:
			line = "Fell %s." % where if index == 1 else "Fell %d m %s at %s." % [roundi(float(last["drop"])), where,
				String(last["hhmm"])]
		else:
			line = "Died %s." % place_phrase(float(last["y"])) if index == 1 else "Died %s at %s." % [
				place_phrase(float(last["y"])), String(last["hhmm"])]
		if index >= 2 and bodies > 0:
			line += " Left the body there."
	return [line, ids]


func _edge_line(index: int, before: float, _bodies: int) -> Array:
	var count := edge_at + edge_back
	if written.has("edge") or count < int(profile.edge_samples_min) or edge_ready_at > before:
		return []
	var back := float(edge_back) / float(count)
	if back >= 0.85:
		return ["Keeps back from the edge." if index == 0 else "Afraid of the edge (stands well back).",
			PackedStringArray(["edge"])]
	if float(edge_at) / float(count) >= 0.5:
		return ["Stands right at the edge.", PackedStringArray(["edge"])]
	return []


func _echo_line(index: int, before: float, _bodies: int) -> Array:
	for e in echoes:
		if written.has(String(e["id"])) or float(e["at"]) > before or int(e["stopped"]) < 0:
			continue
		var reaction := "Stopped to listen." if int(e["stopped"]) == 1 else "Kept going."
		var line := "Heard own voice. %s" % reaction
		if index >= 2:
			line = "Heard own voice %s at %s. %s" % [place_phrase(float(e["y"])), String(e["hhmm"]), reaction]
		return [line, PackedStringArray([String(e["id"])])]
	return []


func _talk_line(_index: int, before: float, _bodies: int) -> Array:
	var voiced := 0
	var after_bad := 0
	for c in clips:
		if float(c["voiced"]) < VOICED_MIN or float(c["at"]) > before:
			continue
		voiced += 1
		for d in deaths:
			if float(c["at"]) >= float(d["at"]) and float(c["at"]) - float(d["at"]) <= BAD_WINDOW_S:
				after_bad += 1
				break
	if after_bad >= 2 and not written.has("talk_bad"):
		return ["Talks to self when the climb goes badly.", PackedStringArray(["talk_bad", "talk"])]
	if voiced >= 3 and not written.has("talk"):
		return ["Talks to self.", PackedStringArray(["talk"])]
	return []


func _session_line(_index: int, _before: float, _bodies: int) -> Array:
	if written.has("session") or sessions.size() < 2:
		return []
	var now: Dictionary = sessions[-1]
	var then: Dictionary = sessions[-2]
	var gap := float(now["unix"]) - float(then["unix"])
	var days := floori(gap / 86400.0)
	if days >= 2:
		return ["Back after %d days." % days, PackedStringArray(["session"])]
	# Another night: the last sitting was in a night too, and not this one.
	if bool(now["night"]) and bool(then["night"]) and gap >= 12.0 * 3600.0:
		return ["Up at night again.", PackedStringArray(["session"])]
	return []


# --- kept ----------------------------------------------------------------------


func _path() -> String:
	return storage_dir.path_join(FILE)


func _load_kept() -> void:
	if not persistent or not FileAccess.file_exists(_path()):
		return
	var parsed: Variant = JSON.parse_string(FileAccess.get_file_as_string(_path()))
	if not parsed is Dictionary:
		return
	var kept: Dictionary = parsed
	if kept.get("sessions") is Array:
		for s in kept["sessions"]:
			if s is Dictionary and (s.get("unix") is float or s.get("unix") is int):
				sessions.append({"unix": float(s["unix"]), "night": bool(s.get("night", false))})
	if kept.get("pages_written") is float or kept.get("pages_written") is int:
		pages_written = maxi(0, int(kept["pages_written"]))
	if kept.get("pages") is Array:
		for p in kept["pages"]:
			if p is Dictionary and p.get("spot") is String and p.get("lines") is Array:
				var lines: Array = []
				for line in p["lines"]:
					lines.append(str(line))
				pages.append({"spot": String(p["spot"]), "lines": lines, "unix": float(p.get("unix", 0.0))})
	while sessions.size() > SESSIONS_KEPT:
		sessions.pop_front()
	while pages.size() > PAGES_KEPT:
		pages.pop_front()


func _save_kept() -> void:
	if not persistent:
		return
	DirAccess.make_dir_recursive_absolute(ProjectSettings.globalize_path(storage_dir))
	var temporary := _path() + ".tmp"
	var file := FileAccess.open(temporary, FileAccess.WRITE)
	if file == null:
		return
	file.store_string(JSON.stringify({"sessions": sessions, "pages_written": pages_written, "pages": pages}))
	file.close()
	DirAccess.rename_absolute(ProjectSettings.globalize_path(temporary), ProjectSettings.globalize_path(_path()))
