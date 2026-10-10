extends SceneTree

# Actual native receipts and production presentation consumer only. Main is
# not added to the tree, so its full-world setup/input/render paths do not run.
var failures: Array[String] = []
var records: Array[Dictionary] = []
var audio: Node
var main: Node3D
var native: Object

func _initialize() -> void:
    _run.call_deferred()

func _require(condition: bool, message: String) -> void:
    if not condition:
        failures.append(message)
        push_error(message)

func _consume(dispatch: bool = true) -> Dictionary:
    var state: Dictionary = native.get_landing_state()
    var tick := int(native.get_tick_index())
    var cue: Dictionary = main._consume_plank_fracture(state, 0, tick)
    if not cue.is_empty():
        var at: Vector3 = cue["at"]
        var expected := Vector3.ZERO
        var count := 0
        for joint in 11:
            if int(cue["new_bits"]) & (1 << joint):
                var a: Transform3D = native.get_kit_body_transform(int(native.get_kit_body_index(2880 + joint)))
                var b: Transform3D = native.get_kit_body_transform(int(native.get_kit_body_index(2881 + joint)))
                expected += ((a * Vector3(.1,.019,0)) + (b * Vector3(-.1,.019,0))) * .5
                count += 1
        _require(count > 0 and at.distance_to(expected / maxf(count,1)) < .00002,"cue follows actual adjacent material endpoints")
        _require(at.is_finite() and float(cue["strength"]) >= .35 and float(cue["strength"]) <= .85,"cue finite and restrained")
        records.append({"tick":tick,"mask":int(state["plank_broken_joint_mask"]),"serial":int(state["plank_fracture_count"]),"new_bits":int(cue["new_bits"]),"strength":float(cue["strength"]),"at":[at.x,at.y,at.z],"effects_muted":AudioServer.is_bus_mute(AudioServer.get_bus_index(&"Effects"))})
        if dispatch:
            # Same production call as main._update_feedback; no fake receipt.
            audio.timber_fracture(at, float(cue["strength"]))
    return cue

func _world(muted: bool) -> void:
    native = ClassDB.instantiate("ScraperXSimulation")
    main._native = native
    main._fb_deaths = 0
    audio.set_volumes(1.0,0.0 if muted else 1.0,1.0,1.0)
    _require(AudioServer.is_bus_mute(AudioServer.get_bus_index(&"Effects")) == muted,"existing Effects bus mute applied")
    _require(bool(native.debug_restart_at(Vector3(-8.8,407.9,-128.2))),"real steel start accepted")
    native.set_move_input(0.0,0.0)
    main._reset_plank_feedback(native.get_landing_state(),int(native.get_tick_index()))
    var before := int(audio.timber_fracture_cues)
    var dt := float(native.get_fixed_step_seconds())
    for tick in 360:
        native.advance_frame(dt)
        _require(_consume().is_empty(),"elastic own-weight world stays quiet")
    _require(int(audio.timber_fracture_cues) == before,"elastic state no snap receipt")
    _require(bool(native.debug_restart_at(Vector3(-6.8,410.9,-128.2))),"existing3m fall staging accepted")
    main._reset_plank_feedback(native.get_landing_state(),int(native.get_tick_index()))
    var record_begin := records.size()
    for tick in 180:
        native.advance_frame(dt)
        _consume()
    var broken: Dictionary = native.get_landing_state()
    _require(int(broken["plank_broken_joint_mask"]) != 0,"actual gravity/contact fracture occurred")
    _require(records.size() - record_begin == 1,"one cue for observed native fracture batch")
    _require(int(audio.timber_fracture_cues) == before + 1,"actual AudioDirector counts one valid native cue under Dummy/mute")
    for repeat in 30:
        _require(_consume().is_empty(),"unchanged damaged snapshot cannot replay")
    # Startup into real already-damaged state is quiet, including unknown baseline.
    var startup: Node3D = (load("res://presentation/main.gd") as GDScript).new()
    startup._native = native
    startup._fb_deaths = 0
    _require(startup._consume_plank_fracture(broken,0,int(native.get_tick_index())).is_empty(),"startup damaged state quiet")
    startup.free()
    # Existing restart is physics authority. The actual post-restart receipt is
    # reset by the production helper used in main._after_restart.
    _require(bool(native.debug_restart_at(Vector3(-8.8,407.9,-128.2))),"explicit supported restart accepted")
    main._reset_plank_feedback(native.get_landing_state(),int(native.get_tick_index()))
    for tick in 30:
        native.advance_frame(dt)
        _require(_consume().is_empty(),"post-restart snapshot quiet")
    audio.reset_landing_feedback(0,0,native.get_landing_state())
    audio.set_volumes(1.0,1.0,1.0,1.0)
    for tick in 30:
        native.advance_frame(dt)
        _require(_consume().is_empty(),"enabling Effects cannot replay consumed fracture")
    _require(int(audio.timber_fracture_cues) == before + 1,"restart/unmute add no receipt")
    print("SCRAPERX_TIMBER_FEEDBACK mode=%s cues=1 broken_mask=%d quiet_elastic_restart_repeat_unmute=true" % ["muted" if muted else "normal",int(broken["plank_broken_joint_mask"])])
    if muted:
        _rollback_receipt_case()
    main._native = null
    native = null

func _rollback_receipt_case() -> void:
    # Explicit source-level receipt sequencing; these dictionaries are NOT
    # alleged native fracture events. Native endpoint lookup remains real.
    var rest := {"plank_fracture_count":0,"plank_broken_joint_mask":0,"plank_discarded_strain_energy_j":0.0}
    main._reset_plank_feedback(rest,100)
    var first := {"plank_fracture_count":1,"plank_broken_joint_mask":1,"plank_discarded_strain_energy_j":20.0}
    _require(not main._consume_plank_fracture(first,0,110).is_empty(),"source receipt first timeline emits")
    _require(main._consume_plank_fracture(rest,0,10).is_empty(),"source receipt rollback stays quiet")
    var future := {"plank_fracture_count":1,"plank_broken_joint_mask":2,"plank_discarded_strain_energy_j":20.0}
    _require(not main._consume_plank_fracture(future,0,20).is_empty(),"new low-tick timeline not suppressed by old cooldown")
    var cascade := {"plank_fracture_count":2,"plank_broken_joint_mask":6,"plank_discarded_strain_energy_j":40.0}
    _require(main._consume_plank_fracture(cascade,0,24).is_empty(),"source cascade within9native ticks coalesced")
    _require(main._consume_plank_fracture(cascade,0,35).is_empty(),"source consumed cascade not replayed later")
    print("SCRAPERX_TIMBER_FEEDBACK source_receipt_rollback_and_cascade=PASS")

func _pool_guard_case() -> void:
    # Dummy does not mix. Disable only AudioDirector's Dummy early return to
    # inspect real pooled-player control states; never claim speaker output.
    audio._silent = false
    var index := int(audio._next_voice_3d)
    var before := int(audio.timber_fracture_cues)
    audio.set_volumes(1.0,0.0,1.0,1.0)
    audio.timber_fracture(Vector3(-6.8,407,-128.2),.5)
    _require(int(audio._next_voice_3d) == index,"muted Effects prevents queued positional voice")
    audio.set_volumes(0.0,1.0,1.0,1.0)
    audio.timber_fracture(Vector3(-6.8,407,-128.2),.5)
    _require(int(audio._next_voice_3d) == index,"muted Master prevents queued positional voice")
    audio.set_volumes(1.0,1.0,1.0,1.0)
    for voice in audio._voices_3d:
        _require(not voice.playing,"unmuting does not reveal old muted snap")
    audio.timber_fracture(Vector3(-6.8,407,-128.2),.5)
    var voice: AudioStreamPlayer3D = audio._voices_3d[index]
    _require(voice.playing and voice.bus == &"Effects","unmuted valid snap starts existing Effects voice")
    audio.reset_landing_feedback(0,0,{})
    _require(not voice.playing,"explicit restart stops existing timber voice")
    _require(int(audio.timber_fracture_cues) == before + 3,"counter counts valid muted and unmuted diagnostics")
    for player in audio._voices_3d:
        player.stop()
        player.stream = null
    audio._silent = true
    print("SCRAPERX_TIMBER_FEEDBACK source_pool_mute_restart=PASS audible_output_unobserved=true")

func _run() -> void:
    _require(ClassDB.class_exists("ScraperXSimulation"),"actual native extension present")
    main = (load("res://presentation/main.gd") as GDScript).new()
    audio = (load("res://presentation/audio/audio_director.gd") as GDScript).new()
    root.add_child(audio)
    await process_frame
    var clip := load("res://assets/audio/timber/timber_snap_400638_preview.wav") as AudioStreamWAV
    _require(clip != null and absf(clip.get_length()-1.1) < .001,"actual imported1.1s snap asset")
    _world(false)
    _world(true)
    _pool_guard_case()
    var receipt := {"status":"PASS" if failures.is_empty() else "FAIL","failures":failures,"records":records,"native_audio_receipts":2,"total_audio_receipts_including3source_pool_checks":int(audio.timber_fracture_cues),"driver":AudioServer.get_driver_name(),"scope":"actual native gravity fracture + production Main consumer and AudioDirector cue/mute receipts; no full-world UI, audible speaker, haptic device or phone mix claim"}
    var file := FileAccess.open("res://feedback-receipt.json",FileAccess.WRITE)
    file.store_string(JSON.stringify(receipt,"  "))
    file.close()
    main.free()
    audio.queue_free()
    await process_frame
    print("SCRAPERX_TIMBER_FEEDBACK %s" % receipt["status"])
    quit(0 if failures.is_empty() else 4)
