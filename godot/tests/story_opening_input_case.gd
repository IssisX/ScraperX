extends "res://tests/mounted_winch_crossing_input_case.gd"

# Exercises shipping Main, touch routing, native contacts and packaged audio.
# Receiver stages test narrative integration, not the continuous ascent route.
const STEP := 1.0 / 90.0
var phase := ""
var done := false
var initial_position := Vector3.ZERO
var initial_serial := 0
var initial_plays := 0

func step(count: int = 1) -> void:
	for i in count:
		main._process(STEP)

func begin(shipping_main: Node) -> Dictionary:
	main = shipping_main
	main.set_process(false)
	main._router.capture_mouse = false
	main._router.gameplay_active = true
	step(2)
	var state: Dictionary = main._story_guidance.sample(main._ctx, int(main._native.get_tick_index()))
	check(not main._ci_mode and main._uitest_scenario.is_empty(), "shipping story path is enabled")
	check(state.intro_visible and state.objective_id == &"freight_one", "Receiving has a live opening and first destination")
	check(main._story_presenter._objective_text.text == state.objective, "shipping presenter shows native-derived direction")
	check(main._story_presenter.mouse_filter == Control.MOUSE_FILTER_IGNORE, "opening does not capture touch")
	var radio: Dictionary = main._story_radio.readback()
	check(int(radio.scheduled_plays) == 1 and radio.missing_keys.is_empty(), "packaged introduction is scheduled once with no missing recording")
	initial_serial = int(state.transmission_id)
	initial_plays = int(radio.scheduled_plays)
	initial_position = main._native.get_player_position()
	stick(Vector2.RIGHT)
	phase = "moving"
	return result()

func advance() -> Dictionary:
	match phase:
		"moving":
			step(70)
			stick(Vector2.ZERO)
			var position: Vector3 = main._native.get_player_position()
			check(Vector2(position.x - initial_position.x, position.z - initial_position.z).length() > .25, "touch stick moves the actual body during the opening")
			check(int(main._story_radio.readback().scheduled_plays) == initial_plays, "opening movement does not replay its radio cue")
			check(main._native.debug_restart_at(Vector3(-30, 11.95, -140)), "staged real crane-dock receiver")
			phase = "dock"
		"dock":
			step(100)
			var state: Dictionary = main._story_guidance.sample(main._ctx, int(main._native.get_tick_index()))
			check(bool(main._ctx.checkpoint_footing_valid) and int(main._ctx.support_entity) == 1451, "native solved actual dock footing")
			check(state.objective_id == &"crown" and state.transmission_key == &"freight_one_iona_crown", "real arrival advances purpose and selects its recording")
			check(int(state.transmission_id) > initial_serial, "retry and arrival do not reuse consumed voice serial")
			check(main._story_presenter._objective_text.text == state.objective, "actual arrival reaches shipping HUD")
			var radio: Dictionary = main._story_radio.readback()
			check(int(radio.scheduled_plays) > initial_plays and radio.manifest_error.is_empty(), "actual arrival schedules its packaged voice")
			main._open_pause()
			radio = main._story_radio.readback()
			check(main._paused and not bool(radio.playing) and radio.stream == null, "pause stops recorded radio immediately")
			# A separate live observer proves the alternative receiver from native
			# contacts without granting progress from its staged altitude.
			check(main._native.debug_restart_at(Vector3(20, 11.95, -119)), "staged real net receiver")
			phase = "net"
		"net":
			main._native.set_move_input(0.0, 0.0)
			for i in 100:
				main._native.advance_frame(STEP)
			main._ctx = main._read_context()
			var observer: RefCounted = load("res://presentation/story_guidance.gd").new()
			var state: Dictionary = observer.sample(main._ctx, int(main._native.get_tick_index()))
			check(bool(main._ctx.checkpoint_footing_valid) and int(main._ctx.support_entity) == 2952, "native solved actual alternate-receiver footing")
			check(state.objective_id == &"crown", "alternate receiver counts without mandatory truck delivery")
			state.transmission_id = 1000000
			main._story_radio.present(state, STEP)
			check(bool(main._story_radio.readback().playing), "actual recorded arrival starts before teardown")
			main._audio.quiesce()
			var radio: Dictionary = main._story_radio.readback()
			check(bool(radio.quiesced) and not bool(radio.playing) and radio.stream == null, "audio teardown reaches nested radio owner")
			done = true
			phase = "complete"
	return result()

func result() -> Dictionary:
	return {"checks": checks, "failures": failures, "done": done, "phase": phase,
		"touch": true, "actual_native": true, "receiver_stages_only": true,
		"tick": main._native.get_tick_index() if main != null else -1}
