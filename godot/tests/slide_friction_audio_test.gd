extends SceneTree

const Friction := preload("res://presentation/audio/slide_friction.gd")
const Bank := preload("res://presentation/audio/sound_bank.gd")
var checks := 0
var failures := 0
var voice: AudioStreamPlayer
var friction: Friction
var tick := 0
var deaths := 0


func _initialize() -> void:
	_run.call_deferred()


func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("SLIDE_FRICTION_AUDIO FAIL " + why)


func sample(state: Dictionary, grounded: bool = true, support: int = 51, audible: bool = true) -> void:
	tick += 1
	friction.update(voice, state, grounded, support, tick, deaths, audible)


func _run() -> void:
	var bank := Bank.new()
	bank.build()
	var generic: AudioStreamWAV = bank.clips[&"slide_friction_loop"][0]
	var metal: AudioStreamWAV = bank.clips[&"slide_metal_loop"][0]
	check(generic.loop_mode == AudioStreamWAV.LOOP_FORWARD and metal.loop_mode == AudioStreamWAV.LOOP_FORWARD,
		"real fixed bank buffers loop without allocating a voice per frame")
	check(generic.data != metal.data and generic.get_length() > 1.0 and metal.get_length() > 1.0,
		"steel and generic contact have distinct complete friction buffers")
	voice = AudioStreamPlayer.new()
	root.add_child(voice)
	var children := root.get_child_count()
	friction = Friction.new()
	friction.configure(generic, metal)
	var state := {"player_sliding": false, "slide_speed_mps": 0.0,
		"slide_count": 0, "support_sound_surface": 2}
	sample(state)
	check(not friction.active and not voice.playing, "initial supported stance is silent")
	state.merge({"player_sliding": true, "slide_speed_mps": 8.0, "slide_count": 1}, true)
	sample(state)
	check(friction.active and voice.playing and voice.stream == metal,
		"real selected steel stream starts through the actual audio player")
	check(friction.strength == 1.0 and friction.surface == 2, "native8mps reaches bounded full feedback")
	var starts := friction.starts
	friction.update(voice, state, true, 51, tick, deaths, true)
	check(friction.starts == starts and voice.playing, "duplicate native tick cannot restart the loop")
	var fast_db := voice.volume_db
	var fast_pitch := voice.pitch_scale
	state.slide_speed_mps = 3.0
	sample(state)
	check(voice.volume_db < fast_db and voice.pitch_scale < fast_pitch and friction.starts == starts,
		"lower actual slip quiets and lowers the existing continuous voice")
	state.slide_speed_mps = 1000000.0
	sample(state)
	check(voice.volume_db == fast_db and voice.pitch_scale == fast_pitch,
		"large finite native speed cannot exceed mix and pitch bounds")
	state.support_sound_surface = 1
	sample(state)
	check(voice.stream == generic and friction.surface == 0 and friction.starts == starts + 1,
		"new current concrete contact replaces steel without overlapping players")
	state.support_sound_surface = "2"
	sample(state)
	check(voice.stream == generic, "malformed optional material cannot manufacture steel")
	state.slide_speed_mps = 0.0
	sample(state)
	check(not friction.active and not voice.playing,
		"zero support-relative slip is silent even if the world support is moving")
	state.slide_speed_mps = 8.0
	sample(state)
	check(friction.active, "current native sliding can resume after actual movement")
	friction.update(voice, state, false, 51, tick, deaths, true)
	check(not friction.active and not voice.playing, "lost contact stops immediately even on a duplicate tick")
	sample(state, false)
	check(not voice.playing, "airborne stale slide fields have no release tail")
	sample(state, true, 0)
	check(not voice.playing, "a ray or missing support identity cannot start friction")
	sample(state)
	check(voice.playing, "actual supported current slide resumes")
	friction.update(voice, state, true, 51, tick, deaths, false)
	check(not voice.playing and not friction.active, "mute stops live playback without waiting for a new physics tick")
	sample(state, true, 51, false)
	check(not voice.playing, "muted native activity cannot queue historical sound")
	sample(state)
	check(voice.playing, "unmute hears only the still-current slide")
	deaths += 1
	sample(state)
	check(not voice.playing, "death establishes a quiet native receipt baseline")
	sample(state)
	check(voice.playing, "subsequent valid live activity can play after baseline")
	tick = 0
	sample(state)
	check(not voice.playing, "native tick rewind cuts the previous playback")
	sample(state)
	check(voice.playing, "forward native samples resume after rewind baseline")
	state.slide_count = 0
	sample(state)
	check(not voice.playing, "restored lower slide generation cuts historical activity")
	sample(state)
	check(not voice.playing, "an active flag without any actual slide generation stays silent")
	state.slide_count = 1
	for invalid in [{}, {"player_sliding": "true", "slide_speed_mps": 8.0, "slide_count": 1},
			{"player_sliding": true, "slide_speed_mps": NAN, "slide_count": 1},
			{"player_sliding": true, "slide_speed_mps": INF, "slide_count": 1},
			{"player_sliding": true, "slide_speed_mps": -1.0, "slide_count": 1},
			{"player_sliding": true, "slide_speed_mps": "8", "slide_count": 1},
			{"player_sliding": true, "slide_speed_mps": 8.0, "slide_count": -1}]:
		sample(invalid)
		check(not voice.playing and not friction.active, "missing or invalid native slide data stops playback")
		sample(state)
		check(not voice.playing, "first recovered sample remains a quiet baseline")
		sample(state)
		check(voice.playing, "valid subsequent native sample recovers without a stale cue")
	friction.reset(voice)
	check(not voice.playing and voice.stream == null and not friction.active,
		"explicit pause/restart/teardown clears the voice and its receipt history")
	check(root.get_child_count() == children, "all transitions reuse exactly one fixed player")
	friction.configure(null, null)
	friction = null
	voice.queue_free()
	bank.clips.clear()
	for _i in 3:
		await process_frame
	print("SLIDE_FRICTION_AUDIO %s checks=%d failures=%d native_contacts_exercised=0 heard_audio=0" % [
		"PASS" if failures == 0 else "FAIL", checks, failures])
	quit(1 if failures else 0)
