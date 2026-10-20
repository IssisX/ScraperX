extends Node3D
## The tower that learns the player (03_EXECUTION/UNCANNY/HER_CHART.md,
## build steps 1 and 3): what the eyes rest on (attention.gd), the phone's
## clock on the tower's clocks (tower_clocks.gd), her chart written from what
## was seen (her_chart.gd) and left in the tower late and rarely
## (chart_pages.gd), and, with consent, the player's own voice heard back
## (voice_keeper.gd). Once a frame main.gd hands over what the native already
## reported; nothing here decides anything the simulation owns, and nothing
## here reads anything on the phone but its clock and, with consent, its
## microphone.

signal microphone_denied

const UncannyProfile := preload("res://presentation/uncanny/uncanny_profile.gd")
const Attention := preload("res://presentation/uncanny/attention.gd")
const HerChart := preload("res://presentation/uncanny/her_chart.gd")
const ChartPages := preload("res://presentation/uncanny/chart_pages.gd")
const TowerClocks := preload("res://presentation/uncanny/tower_clocks.gd")
const VoiceKeeper := preload("res://presentation/uncanny/voice_keeper.gd")
const PageReader := preload("res://presentation/uncanny/page_reader.gd")

# A page is read while the eyes rest on it from this close, this near its
# middle, this long (page_reader.gd).
const READ_DISTANCE_M := 2.4
const READ_ANGLE_RAD := 0.3
const READ_DWELL_S := 0.5

const PAGE_CHECK_S := 1.0
# How the player stands at an edge: twice a second, standing still, the
# floor is looked for EDGE_NEAR_M and EDGE_FAR_M out all round; no floor
# within EDGE_DROP_M below is a drop.
const EDGE_SAMPLE_S := 0.5
const EDGE_NEAR_M := 0.9
const EDGE_FAR_M := 2.5
const EDGE_DROP_M := 4.0
const STILL_MPS := 0.5
# Quiet: on the ground or a hold, slower than this, no fear line said, no
# death for a while, not carried by a machine, and the Matron not loose
# nearby.
const QUIET_MOVE_MPS := 3.0
const QUIET_AFTER_DEATH_S := 30.0
const QUIET_SUPPORT_MPS := 0.3
const MATRON_LOUD_STATES := [1, 2, 3, 4, 8, 9, 10, 11]
const MATRON_NEAR_M := 50.0
const MATRON_LOCKER_SHUT := [0, 1]
const STAND_HALF := 0.9
const CROUCH_HALF := 0.6

var profile: RefCounted
var attention: RefCounted
var chart: RefCounted
var pages: Node3D
var clocks: Node3D
var voice: Node
var reader: Control
# The page the eyes rest on, and for how long; pages read, by spot.
var reading_spot := ""
var pages_read := {}
var _reading_s := 0.0
# Seconds of play (the uncanny clock: advanced only while playing).
var clock := 0.0
# Seconds added to the phone's clock (the tests fast-forward time).
var time_offset_s := 0.0
var pages_placed := 0
var _native: Object
var _audio: Object
var _page_check := 0.0
var _edge_check := 0.0
var _deaths_seen := -1
var _death_clock := -INF
var _fall_top := -INF
var _echo_index := -1


# `storage` overrides the profile's own place in user:// (the tests' own).
func setup(native: Object, audio: Object, profile_name: StringName, keep: bool, storage := "") -> void:
	name = "Uncanny"
	_native = native
	_audio = audio
	profile = UncannyProfile.new()
	profile.configure(profile_name)
	var dir := storage if storage != "" else String(profile.storage_dir)
	# A scripted run starts from nothing: what an earlier run left is gone.
	if storage != "" and not keep:
		_wipe(dir)
	attention = Attention.new()
	chart = HerChart.new()
	chart.setup(profile, attention, keep, dir)
	chart.set_stock(native.get_loose_stock(), native)
	pages = ChartPages.new()
	add_child(pages)
	pages.setup(native)
	# Her pages from before stay where she left them.
	for kept in chart.pages:
		pages.place(String(kept["spot"]), PackedStringArray(kept["lines"]))
	clocks = TowerClocks.new()
	add_child(clocks)
	clocks.setup(native)
	# Over the world, under the HUD's menus.
	var layer := CanvasLayer.new()
	layer.name = "PageReaderLayer"
	layer.layer = 2
	add_child(layer)
	reader = PageReader.new()
	layer.add_child(reader)
	reader.setup(pages.font, pages.paper_texture())
	voice = VoiceKeeper.new()
	add_child(voice)
	voice.setup(profile, dir)
	voice.clip_kept.connect(func(record: Dictionary) -> void: chart.note_clip(clock, float(record["voiced"])))
	voice.echo_played.connect(func(record: Dictionary) -> void:
		_echo_index = chart.note_echo(clock, local_now(), (record["listener"] as Vector3).y - STAND_HALF))
	voice.echo_reaction.connect(func(stopped: bool) -> void: chart.note_echo_reaction(_echo_index, stopped))
	voice.denied.connect(func() -> void: microphone_denied.emit())
	chart.begin_session(now_unix(), local_now())


static func _wipe(dir: String) -> void:
	for sub in [dir.path_join("voice"), dir]:
		var listing := DirAccess.open(ProjectSettings.globalize_path(sub))
		if listing == null:
			continue
		for file in listing.get_files():
			listing.remove(file)


func set_microphone(on: bool) -> void:
	if voice != null:
		voice.set_wanted(on)


func now_unix() -> float:
	return Time.get_unix_time_from_system() + time_offset_s


# The phone's local date and time (fast-forwarded in the tests).
func local_now() -> Dictionary:
	if time_offset_s == 0.0:
		return Time.get_datetime_dict_from_system()
	var bias := int(Time.get_time_zone_from_system().get("bias", 0)) * 60
	return Time.get_datetime_dict_from_unix_time(int(now_unix()) + bias)


# Time passing without play (the tests): the uncanny clock and the phone's.
func advance(seconds: float) -> void:
	clock += seconds
	time_offset_s += seconds
	if voice != null:
		voice.time_offset_s = time_offset_s
	_page_check = 0.0


# Her hearing: how loud the player's room is, 0..1, for the native's
# set_room_loudness (voice_keeper.gd).
func room_loudness() -> float:
	return voice.room_loudness() if voice != null else 0.0


# Once a frame while playing. `attending` is false while the view rides a
# death.
func update(delta: float, ctx: Dictionary, camera: Camera3D, attending: bool) -> void:
	if _native == null or camera == null:
		return
	clock += delta
	var eye := camera.global_position
	var forward := -camera.global_transform.basis.z
	var position: Vector3 = ctx["position"]
	var feet_y := position.y - (CROUCH_HALF if bool(ctx.get("crouched", false)) else STAND_HALF)
	var grounded := bool(ctx["grounded"])
	var traversal := int(ctx["traversal"])
	var standing := grounded and traversal == 0
	var velocity: Vector3 = ctx["velocity"]
	var support_velocity: Vector3 = _native.get_support_point_linear_velocity() if grounded else Vector3.ZERO
	var foot := velocity - support_velocity
	var foot_speed := Vector2(foot.x, foot.z).length()
	attention.update(delta, _native, eye, forward, clock, attending, standing, feet_y)
	_watch_deaths(ctx, position, grounded or traversal != 0 or bool(ctx.get("chute", false)))
	_edge_check -= delta
	if _edge_check <= 0.0:
		_edge_check = EDGE_SAMPLE_S
		if standing and foot_speed < STILL_MPS:
			_sample_edge(position, feet_y)
	_page_check -= delta
	if _page_check <= 0.0:
		_page_check = PAGE_CHECK_S
		_try_page(eye, forward, feet_y)
	var quiet := attending and _quiet(ctx, position, grounded, traversal, foot_speed, support_velocity)
	voice.update(delta, clock, quiet, _native, position, forward, foot_speed)
	_update_reading(delta, eye, forward, attending)


# Her page comes up to be read while the eyes rest on it from close by, its
# face toward them and nothing in between; the locker's only once her door is
# gone.
func _update_reading(delta: float, eye: Vector3, forward: Vector3, attending: bool) -> void:
	var seen := ""
	if attending:
		for spot in pages.placed:
			var sheet: Node3D = pages.page_sheet(String(spot))
			if sheet == null or pages.page_text(String(spot)) == "":
				continue
			var to := sheet.global_position - eye
			var distance := to.length()
			if distance > READ_DISTANCE_M or forward.angle_to(to) > READ_ANGLE_RAD or \
					sheet.global_transform.basis.z.dot(-to) <= 0.0:
				continue
			if String(spot) == ChartPages.LOCKER_SPOT:
				if locker_shut():
					continue
			else:
				var hit: Dictionary = _native.get_look_target(eye, to, distance)
				if bool(hit.get("hit", false)) and float(hit["distance"]) < distance - 0.15:
					continue
			seen = String(spot)
			break
	if seen != "" and seen == reading_spot:
		_reading_s += delta
	else:
		reading_spot = seen
		_reading_s = 0.0
	if seen != "" and _reading_s >= READ_DWELL_S:
		reader.show_page(pages.page_text(seen))
		pages_read[seen] = true
	elif seen == "":
		reader.hide_page()


# Deaths and where: the drop is from the highest point since the body last
# had support to where it met what killed it (the native's death point).
func _watch_deaths(ctx: Dictionary, position: Vector3, supported: bool) -> void:
	var deaths := int(ctx["deaths"])
	if _deaths_seen < 0:
		_deaths_seen = deaths
	if deaths > _deaths_seen:
		_deaths_seen = deaths
		var death: Dictionary = _native.get_death_state()
		var point: Vector3 = death["point"]
		var drop := maxf(0.0, _fall_top - STAND_HALF - point.y) if _fall_top > -INF else 0.0
		chart.note_death(clock, local_now(), point.y, drop)
		_death_clock = clock
		_fall_top = position.y
		return
	if supported:
		_fall_top = position.y
	else:
		_fall_top = maxf(_fall_top, position.y)


func _sample_edge(position: Vector3, feet_y: float) -> void:
	var near_drop := false
	var far_drop := false
	for k in 8:
		var a := TAU * float(k) / 8.0
		var d := Vector3(sin(a), 0.0, cos(a))
		for r in [EDGE_NEAR_M, EDGE_FAR_M]:
			var probe := Vector3(position.x, feet_y + 0.6, position.z) + d * float(r)
			var below := float(_native.get_surface_below(probe, EDGE_DROP_M + 0.6))
			if is_nan(below) or below < feet_y - EDGE_DROP_M:
				if float(r) == EDGE_NEAR_M:
					near_drop = true
				else:
					far_drop = true
	if near_drop:
		chart.note_edge(true, clock)
	elif far_drop:
		chart.note_edge(false, clock)


func _quiet(ctx: Dictionary, position: Vector3, grounded: bool, traversal: int, foot_speed: float,
		support_velocity: Vector3) -> bool:
	if not grounded and traversal == 0:
		return false
	if bool(ctx.get("chute", false)) or clock - _death_clock < QUIET_AFTER_DEATH_S:
		return false
	if foot_speed > QUIET_MOVE_MPS or support_velocity.length() > QUIET_SUPPORT_MPS:
		return false
	if _audio != null and String(_audio.caption) != "":
		return false
	var her: Dictionary = _native.get_matron_state()
	if int(her["state"]) in MATRON_LOUD_STATES and ((her["position"] as Vector3) - position).length() < MATRON_NEAR_M:
		return false
	return true


func locker_shut() -> bool:
	return int(_native.get_matron_state()["state"]) in MATRON_LOCKER_SHUT


# Her next page, if one is due and has something old enough to say, at the
# next spot, if it can appear there unseen.
func _try_page(eye: Vector3, forward: Vector3, feet_y: float) -> void:
	if not chart.due(clock):
		return
	var page: Dictionary = chart.compose(clock, local_now(), int(_native.get_death_state()["corpse_count"]))
	if (page["lines"] as PackedStringArray).is_empty():
		return
	var used: Array = chart.pages.map(func(kept: Dictionary) -> String: return String(kept["spot"]))
	var spot: String = pages.spot_for(int(chart.pages_written), feet_y, used)
	if spot == "" or not pages.can_place(spot, eye, forward, locker_shut()):
		return
	if not pages.place(spot, page["lines"]):
		return
	chart.commit(spot, page, clock, now_unix())
	pages_placed += 1
	print("SCRAPERX_HER_CHART page=%d spot=%s profile=%s clock=%.0f lines=%d" % [int(chart.pages_written), spot,
		String(profile.name), clock, (page["lines"] as PackedStringArray).size()])
