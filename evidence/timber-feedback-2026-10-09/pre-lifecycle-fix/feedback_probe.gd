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
    main._native = null
    native = null

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
    var receipt := {"status":"PASS" if failures.is_empty() else "FAIL","failures":failures,"records":records,"audio_receipts":int(audio.timber_fracture_cues),"driver":AudioServer.get_driver_name(),"scope":"actual native gravity fracture + production Main consumer and AudioDirector cue/mute receipts; no full-world UI, audible speaker, haptic device or phone mix claim"}
    var file := FileAccess.open("res://feedback-receipt.json",FileAccess.WRITE)
    file.store_string(JSON.stringify(receipt,"  "))
    file.close()
    main.free()
    audio.queue_free()
    await process_frame
    print("SCRAPERX_TIMBER_FEEDBACK %s" % receipt["status"])
    quit(0 if failures.is_empty() else 4)
