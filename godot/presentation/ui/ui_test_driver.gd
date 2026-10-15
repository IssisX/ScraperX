extends "res://presentation/ui/test_scenarios/ui_test_harness.gd"

# Scripted proof that every control path reaches the native authority. Each
# scenario injects events through the real viewport input pipeline -- the
# same _input the devices feed -- and asserts a NATIVE state change (tick,
# traversal, velocity, station state), never a presentation variable alone.
#
# One scenario per process, because the native spawn is fixed at tick 0:
#   godot --headless --fixed-fps 60 --path godot -- --uitest=<scenario>
# Prints "SCRAPERX_UITEST PASS|FAIL <scenario> ..." and exits 0 or 31. With
# --capture=<prefix> under a real renderer, each scenario also saves the
# screen at its most telling moment as <prefix>_<pose>.png.


const LaunchSuite := preload("res://presentation/ui/test_scenarios/launch_scenarios.gd")
const OpeningSuite := preload("res://presentation/ui/test_scenarios/opening_scenarios.gd")
const FramesSuite := preload("res://presentation/ui/test_scenarios/frames_scenarios.gd")
const CampaignSuite := preload("res://presentation/ui/test_scenarios/campaign_scenarios.gd")
const CrownSuite := preload("res://presentation/ui/test_scenarios/crown_scenarios.gd")
const HaulSuite := preload("res://presentation/ui/test_scenarios/haul_scenarios.gd")
const LadderSuite := preload("res://presentation/ui/test_scenarios/ladder_scenarios.gd")
const ControlsSuite := preload("res://presentation/ui/test_scenarios/controls_scenarios.gd")
const AudioSuite := preload("res://presentation/ui/test_scenarios/audio_scenarios.gd")

const SCENARIOS := {
	"ground_foundation": 8,
	"touch_cargo_net": 8,
	"touch_campaign_to_121": 8,
	"touch_campaign_to_143": 8,
	"touch_campaign_to_165": 8,
	"touch_campaign_to_198": 8,
	"touch_campaign_to_231": 8,
	"touch_campaign_to_253": 8,
	"touch_campaign_to_286": 8,
	"touch_campaign_to_308": 8,
	"touch_service_lift": 8,
	"touch_balance_lift": 8,
	"touch_crown_gondola": 8,
	"touch_crown_swing": 8,
	"touch_traction_tram": 8,
	"touch_barrel_helix": 8,
	"touch_cascade_mast": 8,
	"touch_pitman_lift": 8,
	"touch_gravity_reclaim": 8,
	"touch_slab_haul_cart": 8,
	"touch_taper_inspection_route": 8,
	"touch_causal_facade": 8,
	"touch_north_grip_diagnostic": 8,
	"touch_single_hand_catch": 8,
	"touch_single_hand_catch_right": 8,
	"touch_single_hand_catch_action": 8,
	"touch_suspended_ladder": 8,
	"keyboard_slingshot": 8,
	"pad_slingshot": 8,
	"touch_slingshot": 8,
	"touch_slingshot_landing": 8,
	"pipe_bridge": 8,
	"touch_pipe_bridge": 8,
	"keyboard_pipe_bridge": 8,
	"touch_facade": 8,
	"touch_stair": 8,
	"touch_upper": 8,
	"touch_teeter": 8,
	"touch_ballast_mantle_feedback": 8,
	"touch_braced_bay": 8,
	"touch_west_brace_bay": 8,
	"touch_north_frame": 8,
	"touch_jump": 8,
	"touch_move_look": 8,
	"touch_gyro_aim": 8,
	"touch_climb": 4,
	"touch_vault": 3,
	"touch_double_tap_vault": 3,
	"touch_crouch": 0,
	"touch_hang_drop": 5,
	"touch_hang_climb": 5,
	"touch_chute": 11,
	"touch_lethal_feedback": 11,
	"touch_sump": 17,
	"touch_pendant": 18,
	"touch_carry": 22,
	"touch_water_screw": 25,
	"touch_water_lift": 26,
	"touch_pause": 8,
	"pad_core": 8,
	"pad_pendant": 14,
	"keyboard_core": 8,
	# AS-006 Stage A, rigged and ridden on each device from the stair's top deck.
	"touch_rig": 24,
	"pad_rig": 24,
	"keyboard_rig": 24,
	# Needs a mixing audio driver: run under --write-movie (see _audio_mix).
	"audio_mix": 8,
}

var _launch_suite: LaunchSuite
var _opening_suite: OpeningSuite
var _frames_suite: FramesSuite
var _campaign_suite: CampaignSuite
var _crown_suite: CrownSuite
var _haul_suite: HaulSuite
var _ladder_suite: LadderSuite
var _controls_suite: ControlsSuite
var _audio_suite: AudioSuite


func begin(main: Node, scenario: String, capture_prefix: String) -> bool:
	if not SCENARIOS.has(scenario):
		return false
	_main = main
	_scenario = scenario
	_capture_prefix = capture_prefix
	_create_suites()
	process_mode = Node.PROCESS_MODE_ALWAYS
	# The removed opening survives only as an explicitly selected regression.
	if scenario in ["pipe_bridge", "touch_pipe_bridge", "keyboard_pipe_bridge", "touch_facade", "touch_stair", "touch_upper", "touch_teeter", "touch_braced_bay", "touch_north_frame"]:
		if not bool(main._native.configure_pipe_bridge_fixture()):
			return false
	if scenario not in ["ground_foundation", "touch_suspended_ladder", "touch_cargo_net", "touch_campaign_to_121", "touch_campaign_to_143", "touch_campaign_to_165", "touch_campaign_to_198", "touch_campaign_to_231", "touch_campaign_to_253", "touch_campaign_to_286", "touch_campaign_to_308", "touch_service_lift", "touch_balance_lift", "touch_crown_gondola", "touch_crown_swing", "touch_traction_tram", "touch_barrel_helix", "touch_cascade_mast", "touch_pitman_lift", "touch_gravity_reclaim", "touch_slab_haul_cart", "touch_taper_inspection_route", "touch_causal_facade", "touch_north_grip_diagnostic", "touch_single_hand_catch", "touch_single_hand_catch_right", "touch_single_hand_catch_action", "keyboard_slingshot", "pad_slingshot", "touch_slingshot", "touch_slingshot_landing", "pipe_bridge", "touch_pipe_bridge", "keyboard_pipe_bridge", "touch_facade", "touch_stair", "touch_upper", "touch_teeter", "touch_braced_bay", "touch_north_frame", "touch_ballast_mantle_feedback", "touch_west_brace_bay"] and not bool(main._native.configure_regression_spawn(int(SCENARIOS[scenario]))):
		return false
	# The traversal kernels are authored facing +x (native tests do the same).
	if scenario in ["touch_climb", "touch_vault", "touch_double_tap_vault", "touch_hang_drop",
			"touch_hang_climb"]:
		main._yaw = -PI * 0.5
		main._pitch = 0.0
	# HangApproach starts mid-air: the push toward the wall has to be held
	# from the first tick, exactly as the native falsifier holds it.
	if scenario.begins_with("touch_hang"):
		_stick_push(Vector2(0.0, -1.0))
	# From the static-deck spawn (0, -8) straight down the crawl beam's lane,
	# toward (-2, -15).
	if scenario == "touch_crouch":
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
	var ok := false
	match _scenario:
		"ground_foundation":
			ok = await _controls_suite._ground_foundation()
		"touch_suspended_ladder":
			ok = await _ladder_suite._touch_suspended_ladder()
		"touch_cargo_net":
			ok = await _ladder_suite._touch_cargo_net()
		"touch_campaign_to_121":
			ok = await _campaign_suite._touch_campaign_to_121()
		"touch_campaign_to_143":
			ok = await _campaign_suite._touch_campaign_to_143()
		"touch_campaign_to_165":
			ok = await _campaign_suite._touch_campaign_to_165()
		"touch_campaign_to_198":
			ok = await _campaign_suite._touch_campaign_to_198()
		"touch_campaign_to_231":
			ok = await _campaign_suite._touch_campaign_to_231()
		"touch_campaign_to_253":
			ok = await _campaign_suite._touch_campaign_to_253()
		"touch_campaign_to_286":
			ok = await _campaign_suite._touch_campaign_to_286()
		"touch_campaign_to_308":
			ok = await _campaign_suite._touch_campaign_to_308()
		"touch_service_lift":
			ok = await _campaign_suite._touch_service_lift(true)
		"touch_balance_lift":
			ok = await _campaign_suite._touch_balance_lift(true)
		"touch_crown_gondola":
			ok = await _crown_suite._touch_crown_gondola(true)
		"touch_crown_swing":
			ok = await _crown_suite._touch_crown_swing()
		"touch_traction_tram":
			ok = await _campaign_suite._touch_traction_tram(true)
		"touch_barrel_helix":
			ok = await _campaign_suite._touch_barrel_helix(true)
		"touch_cascade_mast":
			ok = await _campaign_suite._touch_cascade_mast(true)
		"touch_pitman_lift":
			ok = await _campaign_suite._touch_pitman_lift(true)
		"touch_gravity_reclaim":
			ok = await _haul_suite._touch_gravity_reclaim()
		"touch_slab_haul_cart":
			ok = await _haul_suite._touch_slab_haul_cart()
		"touch_taper_inspection_route":
			ok = await _haul_suite._touch_taper_inspection_route()
		"touch_north_grip_diagnostic":
			ok = await _frames_suite._touch_north_grip_diagnostic()
		"touch_single_hand_catch":
			ok = await _crown_suite._touch_single_hand_catch()
		"touch_single_hand_catch_right":
			ok = await _crown_suite._touch_single_hand_catch(true)
		"touch_single_hand_catch_action":
			ok = await _crown_suite._touch_single_hand_catch(false, true)
		"keyboard_slingshot":
			ok = await _launch_suite._slingshot(InputRouter.Device.KEYBOARD_MOUSE)
		"pad_slingshot":
			ok = await _launch_suite._slingshot(InputRouter.Device.GAMEPAD)
		"touch_slingshot":
			ok = await _launch_suite._slingshot(InputRouter.Device.TOUCH)
		"touch_slingshot_landing":
			ok = await _launch_suite._slingshot(InputRouter.Device.TOUCH, true)
		"pipe_bridge":
			ok = await _opening_suite._pipe_bridge(InputRouter.Device.GAMEPAD)
		"touch_pipe_bridge":
			ok = await _opening_suite._pipe_bridge(InputRouter.Device.TOUCH)
		"keyboard_pipe_bridge":
			ok = await _opening_suite._pipe_bridge(InputRouter.Device.KEYBOARD_MOUSE)
		"touch_facade", "touch_causal_facade":
			ok = await _opening_suite._touch_facade()
		"touch_stair":
			ok = await _opening_suite._touch_stair()
		"touch_upper":
			ok = await _opening_suite._touch_upper()
		"touch_teeter":
			ok = await _opening_suite._touch_teeter()
		"touch_ballast_mantle_feedback":
			ok = await _opening_suite._touch_ballast_mantle_feedback()
		"touch_braced_bay":
			ok = await _opening_suite._touch_braced_bay()
		"touch_west_brace_bay":
			ok = await _frames_suite._touch_west_brace_bay()
		"touch_north_frame":
			ok = await _frames_suite._touch_north_service_frame()
		"touch_jump":
			ok = await _controls_suite._touch_jump()
		"touch_move_look":
			ok = await _controls_suite._touch_move_look()
		"touch_gyro_aim":
			ok = await _controls_suite._touch_gyro_aim()
		"touch_climb":
			ok = await _controls_suite._touch_climb(&"climb", 2)
		"touch_vault":
			ok = await _controls_suite._touch_vault()
		"touch_double_tap_vault":
			ok = await _controls_suite._touch_double_tap_vault()
		"touch_crouch":
			ok = await _controls_suite._touch_crouch()
		"touch_hang_drop":
			ok = await _controls_suite._touch_hang(false)
		"touch_hang_climb":
			ok = await _controls_suite._touch_hang(true)
		"touch_chute":
			ok = await _controls_suite._touch_chute()
		"touch_lethal_feedback":
			ok = await _controls_suite._touch_lethal_feedback()
		"touch_sump":
			ok = await _controls_suite._touch_sump()
		"touch_pendant":
			ok = await _controls_suite._touch_pendant()
		"touch_carry":
			ok = await _controls_suite._touch_carry()
		"touch_water_screw":
			ok = await _controls_suite._touch_water_screw()
		"touch_water_lift":
			ok = await _controls_suite._touch_water_lift()
		"touch_pause":
			ok = await _controls_suite._touch_pause()
		"pad_core":
			ok = await _controls_suite._pad_core()
		"pad_pendant":
			ok = await _controls_suite._pad_pendant()
		"keyboard_core":
			ok = await _controls_suite._keyboard_core()
		"touch_rig":
			ok = await _controls_suite._rig(InputRouter.Device.TOUCH)
		"pad_rig":
			ok = await _controls_suite._rig(InputRouter.Device.GAMEPAD)
		"keyboard_rig":
			ok = await _controls_suite._rig(InputRouter.Device.KEYBOARD_MOUSE)
		"audio_mix":
			ok = await _audio_suite._audio_mix()
	print("SCRAPERX_UITEST %s %s %s" % ["PASS" if ok else "FAIL", _scenario, _detail])
	get_tree().paused = false
	get_tree().quit(0 if ok else 31)


func _create_suites() -> void:
	_launch_suite = LaunchSuite.new(self)
	_crown_suite = CrownSuite.new(self)
	_ladder_suite = LadderSuite.new(self, _crown_suite)
	_opening_suite = OpeningSuite.new(self, _ladder_suite)
	_frames_suite = FramesSuite.new(self, _opening_suite)
	_campaign_suite = CampaignSuite.new(self, _crown_suite, _frames_suite, _ladder_suite)
	_haul_suite = HaulSuite.new(self)
	_controls_suite = ControlsSuite.new(self)
	_audio_suite = AudioSuite.new(self)


# The second-hand bridge proof uses this assertion without starting a scenario.
func _single_hand_controls_honest(posture_label: String) -> bool:
	return CrownSuite.new(self)._single_hand_controls_honest(posture_label)
