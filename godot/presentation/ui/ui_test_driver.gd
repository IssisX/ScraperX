extends "res://presentation/ui/ui_test_uncanny.gd"

# Scripted proof that every control path reaches the native authority. Each
# scenario injects events through the real viewport input pipeline -- the
# same _input the devices feed -- and asserts a NATIVE state change (tick,
# traversal, velocity, carry and rig state), never a presentation variable
# alone.
#
# One scenario per process, because the native spawn is fixed at tick 0:
#   godot --headless --fixed-fps 60 --path godot -- --uitest=<scenario>
# Prints "SCRAPERX_UITEST PASS|FAIL <scenario> ..." and exits 0 or 31. With
# --capture=<prefix> under a real renderer, each scenario also saves the
# screen at its most telling moment as <prefix>_<pose>.png.

# (Scenarios: ui_test_base.gd .. ui_test_act.gd, one family per file.)

const SCENARIOS := {
	"touch_jump": 8,
	"touch_move_look": 8,
	"touch_gyro_aim": 8,
	"touch_climb": 4,
	"touch_vault": 3,
	"touch_double_tap_vault": 3,
	"touch_speed_vault": 3,
	"touch_precision": 3,
	"touch_wall_run": 41,
	"touch_wall_kick": 41,
	"touch_wall_climb": 0,
	"touch_tuck": 0,
	"touch_flow": 8,
	"touch_springboard": 8,
	"touch_quickturn": 8,
	"touch_crouch": 0,
	"touch_body": 0,
	"touch_slide": 0,
	"touch_hang_drop": 5,
	"touch_hang_climb": 5,
	"touch_monkey_bars": 39,
	"touch_swing_launch": 39,
	"touch_zipline": 25,
	"touch_zipline_slam": 25,
	"touch_pipe_gallery": 40,
	"touch_hook": 39,
	"touch_hook_rope": 16,
	"touch_birds": 8,
	"touch_workers": 8,
	"touch_works": 27,
	"touch_chute": 9,
	"touch_lethal_feedback": 9,
	# A broken arm: flat out into the Matron's locker, then the ladder one-armed.
	"touch_arm_break": 11,
	"touch_ladder": 11,
	# The faint route hint: off by default, the ladder's hold marked when on.
	"touch_route_hint": 11,
	"touch_pause": 8,
	"pad_core": 8,
	"keyboard_core": 8,
	# AS-006 Stage A, rigged and ridden on each device from the 154 m deck.
	"touch_rig": 11,
	"pad_rig": 11,
	"keyboard_rig": 11,
	# AS-006 Stage B, the derrick boom rigged and ridden on each device from the 176 m ring.
	"touch_boom": 12,
	"pad_boom": 12,
	"keyboard_boom": 12,
	# AS-006 Stage C, the chute cleared and the platform freed on each device.
	"touch_debris": 13,
	"pad_debris": 13,
	"keyboard_debris": 13,
	# AS-007, Wet Isolation, on its three machines from the 220 ring to TP-340.
	"touch_wet": 16,
	"pad_wet": 16,
	"keyboard_wet": 16,
	# AS-008, the Plate Shop, on its three machines from TP-340 to the 484 ring.
	"touch_shop": 19,
	"pad_shop": 19,
	"keyboard_shop": 19,
	# AS-009, the Facade Crane Stack, on its three machines from the 484 ring to TP-640.
	"touch_crane": 22,
	"pad_crane": 22,
	"keyboard_crane": 22,
	# AS-010, Midstack Service, its service lift from TP-640 to the 662 deck.
	"touch_service": 30,
	"pad_service": 30,
	"keyboard_service": 30,
	# AS-010's C4, the service gantry, climbed from the 662 deck to the 684 deck
	# on touch, the device the game is played on.
	"touch_c4": 31,
	# AS-010's stage N, the granular discharge hoist, from the 684 deck to the
	# 706 deck on touch.
	"touch_n": 32,
	# AS-010's C5, the cooling plant, climbed from the 706 deck to the 728 deck
	# on touch.
	"touch_c5": 33,
	# AS-010's stage O, the gravel wheel, from the 728 deck to the 750 deck on
	# touch.
	"touch_o": 34,
	# The ground slingshot from the game's start: into the pouch, the stick
	# pulled back to draw, RELEASE, the chute over the tower and the stick
	# steering down onto the 220 ring.
	"touch_slingshot": 8,
	# AS-012, the swing, from the 220 ring where the slingshot and the route
	# from grade arrive: out the gangway into the seat, STRAP IN, KICK THE
	# TRIP, the ram's blow and the swing up to the 242 ring, UNBUCKLE and off.
	"touch_swing": 16,
	# C6, the west band, from the 242 ring where the swing sets its rider down
	# to the 264 ring: no ladder, no standpipe.
	"touch_c6": 35,
	# C7, from the 264 ring's west band where C6 tops out: the exposed way to
	# the walkway at 291.25 m without the cascade mast -- two pipe galleries
	# swung out over the drop, four hang-ups and the block.
	"touch_c7": 36,
	"touch_summit": 42,
	# The Matron on the 154 m deck: wake her, creep away while she strains,
	# run her over the rotten catwalk (look_matron_*: renders only, not in CI).
	"touch_matron": 11,
	"touch_matron_ride": 11,
	"touch_matron_line": 11,
	"touch_matron_climb": 11,
	# Her first act on touch, from the 154 m deck to the 220 m ring: a pest,
	# dangerous, fully changed and back (ui_test_act.gd).
	"touch_matron_act": 11,
	# Knock her down, then throw what you carry: a toolbox thrown at a pest
	# coming for the climber puts her down (ui_test_act.gd).
	"touch_matron_heave": 11,
	"touch_bay": 11,
	# Her corner pipe: she comes down the shaft after the climber, heard on it
	# 4 s and more before she is there; a drum shoved off the edge onto her as
	# she climbs back up knocks her off it, alive (ui_test_act.gd).
	"touch_matron_pipe": 11,
	"look_matron_locker": 11,
	"look_matron_hunt": 11,
	"look_matron_catwalk": 11,
	"look_matron_gallery": 11,
	"look_matron_stock": 11,
	# Loose stock in the yard: a drum carried and set down (look_stock_yard:
	# renders only, not in CI).
	"touch_stock": 8,
	"touch_forklift": 8,
	"touch_forklift_strain": 8,
	# The yard crane west of the stack: its remote taken, worked, let go
	# (look_yard_crane: renders only, not in CI).
	"touch_yard_crane": 8,
	# The yard crane's lifting magnet, on each device: rigged by HOOK, MAGNET
	# ON from the remote, a slab taken and lifted, MAGNET OFF; CRANE by the
	# button, R and LB.
	"touch_crane_magnet": 8,
	"pad_crane_magnet": 8,
	"keyboard_crane_magnet": 8,
	# The yard crane's container spreader: rigged by HOOK, landed on the
	# laydown's container, LOCK, lifted, set down, UNLOCK.
	"touch_crane_spreader": 8,
	"look_yard_crane": 8,
	"look_stock_yard": 8,
	# Close up, the smooth finishes and the rough ones (renders only, not in CI).
	"look_finishes": 8,
	"look_night_lamps": 8,
	"look_tower_stock_deck2": 25,
	"look_tower_stock_176": 14,
	# Look only (renders, not in CI): the summit lit at dusk, from its deck and from the ground.
	"look_summit": 42,
	"look_summit_ground": 8,
	# The owner's vertical machines, from the 264 ring's west band where C6
	# tops out, on their own motors: the cascade mast 27 m up, the walkway out
	# the west face, the pitman lift 23.6 m, the barrel helix 24 m, the walkway
	# back east and a mantle onto TP-340.
	"touch_machines": 36,
	# From the 750 deck where stage O sets its rider down: a vault over the
	# east parapet, the crown gondola's cabin through a half turn of its 28 m
	# crown to 778 m, and the luffing derrick's cradle up and in to 802.6 m.
	"touch_high": 37,
	"perf_top": 37,
	# The cloud layer (the height felt): from the yard, a broken ceiling far
	# overhead round the tower, looked at up and out in four directions.
	"touch_clouds": 8,
	"perf_grade": 8,
	# The owner's slab incline from the game's start: across the yard east of
	# the slingshot, up the ramp onto its platform, onto the trolley, RAISE,
	# 30 m up the 60-degree incline, and off onto deck 3.
	"touch_incline": 8,
	# The owner's stone wheel from deck 2: down the ramp, into the bucket at
	# the bottom, RAISE, carried over the top of the 26 m wheel by the poured
	# stone's weight, and down the ramp onto deck 4.
	"touch_wheel": 25,
	# The traction tram from deck 6: out along the east bridge, off its end,
	# down the gangway onto the carrier, RAISE, 26 m up and 44 m west on its
	# driven wheels, and down onto deck 8.
	"touch_tram": 27,
	# Checkpoint continuation and lethal rollback proof from Deck 4 (+44 m).
	"touch_checkpoint": 26,
	# Continue: the climb saved to storage while playing, loaded back, a cut
	# save refused and set aside, NEW CLIMB's discard.
	"touch_save": 26,
	# Phase 4 on touch: off the slab incline's 3.4 m foot platform at a run,
	# CROUCH tapped in the air, and the native rolls on: the momentum kept,
	# little balance lost, the view turned once over.
	"touch_roll": 8,
	# The same roll with REDUCED MOTION on: still a roll, the view not turned over.
	"touch_roll_reduced": 8,
	# Phase 3 on touch: the yard's trapeze. Up its ramp, run off the launch
	# stand, GRAB the hanging bar in the air, pump it with Crouch (pull up
	# through the bottom, let down at the ends), LET GO near the far end and
	# land on the landing stand.
	"touch_trapeze": 8,
	# The title over the live tower at launch: the simulation frozen behind
	# it, no CONTINUE without a save, NEW CLIMB tapped starts the climb.
	"touch_title": 8,
	# Phase 4 on touch: a sprint at deck 4's south edge, the stick let go
	# 2 m short of it: the native teeters (its XCoM past the edge), and the
	# soles' traction cannot stop the body before it goes over.
	"touch_teeter": 26,
	# Upper Stack continuation from Deck 4 checkpoint through S2, C2, S3, C3 to Deck 14 (+154 m).
	"touch_stack_upper": 26,
	"pad_stack_upper": 26,
	"keyboard_stack_upper": 26,
	# The continuous ascent from the game's start at grade through the upper machines to +221 m receiver.
	"touch_stack": 8,
	# The foreman's radio: his first line as the climber steps into S1's cage.
	"touch_radio": 8,
	"pad_stack": 8,
	"keyboard_stack": 8,
	# The fear voice on a ~12 m drop lived through: a yelp, then the cry for
	# the ankle it breaks.
	"fall_voice": 10,
	# Need a mixing audio driver: run under --write-movie (see _audio_mix).
	"audio_mix": 8,
	"audio_fall": 9,
	# Her chart (ui_test_uncanny.gd): a long look at the Matron's drum written
	# into the page in her locker, late; the START list's fast playtest of it;
	# the microphone's consent and its setting; a voice echoed back, late,
	# faint, uncaptioned.
	"touch_her_chart": 11,
	"touch_her_chart_fast": 11,
	"touch_mic_consent": 8,
	"touch_voice_echo": 11,
	# Her hearing: a bark in the player's room wakes her.
	"touch_room_hearing": 11,
	# Renders only, not in CI: one of her pages read at the feet.
	"look_her_chart": 11,
}
var _scenario := ""
# The name run, as the result line prints it: a touch scenario's name with
# _buttons on the end is the same scenario played on the BUTTONS scheme.
var _scenario_name := ""

func begin(main: Node, scenario: String, capture_prefix: String) -> bool:
	var played := scenario.trim_suffix("_buttons")
	if not SCENARIOS.has(played) or (played != scenario and not played.begins_with("touch_")):
		return false
	_main = main
	_scenario = played
	_scenario_name = scenario
	_gestures = played == scenario
	_capture_prefix = capture_prefix
	process_mode = Node.PROCESS_MODE_ALWAYS
	main._settings.touch_scheme = SettingsStore.TOUCH_SCHEME_GESTURES if _gestures else SettingsStore.TOUCH_SCHEME_BUTTONS
	main._apply_settings()
	# The START list's HER CHART (FAST), chosen as a player chooses it: its
	# spawn is the scenario's, its profile the one main.gd maps it to.
	if played == "touch_her_chart_fast":
		main._settings.start_at = SettingsStore.START_NAMES.find(SettingsStore.START_HER_CHART_FAST)
		main.uncanny_profile_override = main.uncanny_profile_for_start(main._settings.start_at)
	if not bool(main._native.configure_initial_spawn(int(SCENARIOS[played]))):
		return false
	# The traversal kernels are authored facing +x (native tests do the same).
	if played in ["touch_climb", "touch_vault", "touch_double_tap_vault", "touch_speed_vault", "touch_precision",
			"touch_wall_run", "touch_hang_drop",
			"touch_hang_climb", "touch_monkey_bars", "touch_swing_launch", "touch_pipe_gallery"]:
		main._yaw = -PI * 0.5
		main._pitch = 0.0
	# HangApproach starts mid-air: the push toward the wall has to be held
	# from the first tick, exactly as the native falsifier holds it.
	if played.begins_with("touch_hang"):
		_stick_push(Vector2(0.0, -1.0))
	# From the static-deck spawn (0, -8) straight down the crawl beam's lane,
	# toward (-2, -15).
	if played == "touch_crouch" or played == "touch_slide":
		main._yaw = atan2(2.0, 7.0)
		main._pitch = 0.0
	_run.call_deferred()
	return true

func _run() -> void:
	await _frames(2)
	# Touch scenarios start the way a thumb does: the first contact switches
	# the interface to touch (a tap in the open look area moves nothing).
	if _scenario.begins_with("touch_") and not _scenario.begins_with("touch_hang"):
		_tap(7, Vector2(_viewport_size().x * 0.7, _viewport_size().y * 0.35))
		await _frames(2)
	# The same lines in the same order on every run of a scenario.
	_main._audio.voice_rng.seed = 0x5C4A9E
	var ok := false
	match _scenario:
		"touch_wall_kick":
			ok = await _touch_wall_kick()
		"touch_ladder":
			ok = await _touch_ladder()
		"touch_route_hint":
			ok = await _touch_route_hint()
		"touch_jump":
			ok = await _touch_jump()
		"touch_move_look":
			ok = await _touch_move_look()
		"touch_gyro_aim":
			ok = await _touch_gyro_aim()
		"touch_climb":
			ok = await _touch_climb(&"climb", 2)
		"touch_vault":
			ok = await _touch_vault()
		"touch_double_tap_vault":
			ok = await _touch_double_tap_vault()
		"touch_speed_vault":
			ok = await _touch_speed_vault()
		"touch_precision":
			ok = await _touch_precision()
		"touch_wall_run":
			ok = await _touch_wall_run()
		"touch_wall_climb":
			ok = await _touch_wall_climb()
		"touch_tuck":
			ok = await _touch_tuck()
		"touch_flow":
			ok = await _touch_flow()
		"touch_springboard":
			ok = await _touch_springboard()
		"touch_quickturn":
			ok = await _touch_quickturn()
		"touch_body":
			ok = await _touch_body()
		"touch_crouch":
			ok = await _touch_crouch()
		"touch_slide":
			ok = await _touch_slide()
		"touch_hang_drop":
			ok = await _touch_hang(false)
		"touch_hang_climb":
			ok = await _touch_hang(true)
		"touch_monkey_bars":
			ok = await _touch_monkey_bars()
		"touch_swing_launch":
			ok = await _touch_swing_launch()
		"touch_zipline":
			ok = await _touch_zipline()
		"touch_zipline_slam":
			ok = await _touch_zipline_slam()
		"touch_pipe_gallery":
			ok = await _touch_pipe_gallery()
		"touch_hook":
			ok = await _touch_hook()
		"touch_hook_rope":
			ok = await _touch_hook_rope()
		"touch_birds":
			ok = await _touch_birds()
		"touch_workers":
			ok = await _touch_workers()
		"touch_works":
			ok = await _touch_works()
		"touch_chute":
			ok = await _touch_chute()
		"touch_arm_break":
			ok = await _touch_arm_break()
		"touch_lethal_feedback":
			ok = await _touch_lethal_feedback()
		"touch_pause":
			ok = await _touch_pause()
		"pad_core":
			ok = await _pad_core()
		"keyboard_core":
			ok = await _keyboard_core()
		"touch_rig":
			ok = await _rig(InputRouter.Device.TOUCH)
		"pad_rig":
			ok = await _rig(InputRouter.Device.GAMEPAD)
		"keyboard_rig":
			ok = await _rig(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_boom":
			ok = await _boom(InputRouter.Device.TOUCH)
		"pad_boom":
			ok = await _boom(InputRouter.Device.GAMEPAD)
		"keyboard_boom":
			ok = await _boom(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_debris":
			ok = await _debris(InputRouter.Device.TOUCH)
		"pad_debris":
			ok = await _debris(InputRouter.Device.GAMEPAD)
		"keyboard_debris":
			ok = await _debris(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_wet":
			ok = await _wet(InputRouter.Device.TOUCH)
		"pad_wet":
			ok = await _wet(InputRouter.Device.GAMEPAD)
		"keyboard_wet":
			ok = await _wet(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_shop":
			ok = await _shop(InputRouter.Device.TOUCH)
		"pad_shop":
			ok = await _shop(InputRouter.Device.GAMEPAD)
		"keyboard_shop":
			ok = await _shop(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_crane":
			ok = await _crane(InputRouter.Device.TOUCH)
		"pad_crane":
			ok = await _crane(InputRouter.Device.GAMEPAD)
		"keyboard_crane":
			ok = await _crane(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_service":
			ok = await _service(InputRouter.Device.TOUCH)
		"pad_service":
			ok = await _service(InputRouter.Device.GAMEPAD)
		"keyboard_service":
			ok = await _service(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_c4":
			ok = await _c4(InputRouter.Device.TOUCH)
		"touch_n":
			ok = await _n(InputRouter.Device.TOUCH)
		"touch_c5":
			ok = await _c5(InputRouter.Device.TOUCH)
		"touch_o":
			ok = await _o(InputRouter.Device.TOUCH)
		"touch_slingshot":
			ok = await _slingshot(InputRouter.Device.TOUCH)
		"touch_swing":
			ok = await _swing(InputRouter.Device.TOUCH)
		"touch_c6":
			ok = await _c6(InputRouter.Device.TOUCH)
		"touch_c7":
			ok = await _c7(InputRouter.Device.TOUCH)
		"touch_summit":
			ok = await _summit(InputRouter.Device.TOUCH)
		"touch_matron":
			ok = await _matron_touch(InputRouter.Device.TOUCH)
		"touch_matron_act":
			ok = await _matron_act_touch(InputRouter.Device.TOUCH)
		"touch_matron_heave":
			ok = await _matron_heave_touch(InputRouter.Device.TOUCH)
		"touch_matron_pipe":
			ok = await _matron_pipe_touch(InputRouter.Device.TOUCH)
		"touch_matron_ride":
			ok = await _matron_ride_touch(InputRouter.Device.TOUCH)
		"touch_matron_line":
			ok = await _matron_line_touch(InputRouter.Device.TOUCH)
		"touch_matron_climb":
			ok = await _matron_climb_touch(InputRouter.Device.TOUCH)
		"touch_bay":
			ok = await _bay_touch(InputRouter.Device.TOUCH)
		"look_matron_locker":
			ok = await _look_matron("locker")
		"look_matron_hunt":
			ok = await _look_matron("hunt")
		"look_matron_catwalk":
			ok = await _look_matron("catwalk")
		"look_matron_gallery":
			ok = await _look_matron_gallery()
		"look_matron_stock":
			ok = await _look_matron("stock")
		"touch_stock":
			ok = await _stock_touch(InputRouter.Device.TOUCH)
		"touch_forklift":
			ok = await _forklift_touch(InputRouter.Device.TOUCH)
		"touch_forklift_strain":
			ok = await _forklift_strain_touch(InputRouter.Device.TOUCH)
		"touch_yard_crane":
			ok = await _yard_crane_touch(InputRouter.Device.TOUCH)
		"touch_crane_magnet":
			ok = await _crane_magnet(InputRouter.Device.TOUCH)
		"pad_crane_magnet":
			ok = await _crane_magnet(InputRouter.Device.GAMEPAD)
		"keyboard_crane_magnet":
			ok = await _crane_magnet(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_crane_spreader":
			ok = await _crane_spreader(InputRouter.Device.TOUCH)
		"look_yard_crane":
			ok = await _look_yard_crane()
		"look_stock_yard":
			ok = await _look_stock_yard()
		"look_finishes":
			ok = await _look_finishes()
		"look_night_lamps":
			ok = await _look_night_lamps()
		"look_tower_stock_deck2":
			ok = await _look_tower_stock("tower_stock_deck2")
		"look_tower_stock_176":
			ok = await _look_tower_stock("tower_stock_176")
		"look_summit":
			ok = await _look_summit(false)
		"look_summit_ground":
			ok = await _look_summit(true)
		"touch_machines":
			ok = await _machines(InputRouter.Device.TOUCH)
		"touch_high":
			ok = await _high(InputRouter.Device.TOUCH)
		"touch_clouds":
			ok = await _clouds()
		"perf_top", "perf_grade":
			ok = await _perf_probe()
		"touch_incline":
			ok = await _incline(InputRouter.Device.TOUCH)
		"touch_wheel":
			ok = await _wheel(InputRouter.Device.TOUCH)
		"touch_tram":
			ok = await _tram(InputRouter.Device.TOUCH)
		"touch_checkpoint":
			ok = await _checkpoint_continuation(InputRouter.Device.TOUCH)
		"touch_save":
			ok = await _save(InputRouter.Device.TOUCH)
		"touch_roll":
			ok = await _roll()
		"touch_roll_reduced":
			ok = await _roll(true)
		"touch_trapeze":
			ok = await _trapeze()
		"touch_title":
			ok = await _title()
		"touch_teeter":
			ok = await _teeter()
		"touch_stack_upper":
			ok = await _stack_upper(InputRouter.Device.TOUCH)
		"pad_stack_upper":
			ok = await _stack_upper(InputRouter.Device.GAMEPAD)
		"keyboard_stack_upper":
			ok = await _stack_upper(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_radio":
			ok = await _touch_radio(InputRouter.Device.TOUCH)
		"touch_stack":
			ok = await _stack(InputRouter.Device.TOUCH)
		"pad_stack":
			ok = await _stack(InputRouter.Device.GAMEPAD)
		"keyboard_stack":
			ok = await _stack(InputRouter.Device.KEYBOARD_MOUSE)
		"fall_voice":
			ok = await _fall_voice()
		"audio_mix":
			ok = await _audio_mix()
		"audio_fall":
			ok = await _audio_fall()
		"touch_her_chart":
			ok = await _touch_her_chart()
		"touch_her_chart_fast":
			ok = await _touch_her_chart_fast()
		"touch_mic_consent":
			ok = await _touch_mic_consent()
		"touch_voice_echo":
			ok = await _touch_voice_echo()
		"touch_room_hearing":
			ok = await _touch_room_hearing()
		"look_her_chart":
			ok = await _look_her_chart()
	if _scenario.begins_with("touch_"):
		var counts: Dictionary = _main._touch.gesture_counts
		_detail += " scheme=%s swipes_up=%d swipes_down=%d flicks=%d" % ["gestures" if _gestures else "buttons",
			int(counts[&"up"]), int(counts[&"down"]), int(counts[&"turn"])]
		# Every swipe and flick made was recognized, and nothing else was: a
		# dropped swipe, or a turn the walk made by itself, fails the run.
		if ok and counts != _injected:
			ok = false
			_detail += " reason=recognized %s, made %s" % [str(counts), str(_injected)]
	print("SCRAPERX_UITEST %s %s %s" % ["PASS" if ok else "FAIL", _scenario_name, _detail])
	get_tree().paused = false
	get_tree().quit(0 if ok else 31)
