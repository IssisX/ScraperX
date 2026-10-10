extends SceneTree
var failures: Array[String] = []
var audio: Node
func _initialize() -> void: _run.call_deferred()
func _require(ok: bool, why: String) -> void:
    if not ok:
        failures.append(why)
        push_error(why)
func _state(count: int, active: bool, stop: int, load: float) -> Dictionary:
    return {"landing_count":0,"landing_normal_speed_mps":0.0,"foot_push_start_count":count,"foot_push_active":active,"foot_push_stop_reason":stop,"foot_push_support_entity_id":2886,"foot_push_command_work_bound_j":20.0,"foot_push_peak_load_n":load}
func _update(state: Dictionary, grounded: bool, velocity: Vector3) -> void:
    audio.update(1.0/90.0,Vector3(-6.8,407.9,-128.2),velocity,grounded,2886 if grounded else 0,0,false,0,false,Vector3.ZERO,state)
func _run() -> void:
    # Explicit synthetic receipt control checks; no native event or body claim.
    audio = (load("res://presentation/audio/audio_director.gd") as GDScript).new()
    root.add_child(audio)
    await process_frame
    audio.reset_landing_feedback(0,0,_state(0,false,1,0.0))
    _update(_state(0,false,1,0.0),true,Vector3.ZERO)
    _update(_state(1,true,0,0.0),true,Vector3.ZERO)
    _update(_state(1,true,0,0.0),false,Vector3(0,3.0,0))
    _require(int(audio.jumps)==1,"ordinary Jump emits once despite pending zero-load receipt")
    _update(_state(1,true,0,100.0),false,Vector3(0,1.0,0))
    _require(int(audio.jumps)==1,"ordinary cue consumes pending wood receipt before later load")
    for reason in range(1,9):
        audio.reset_landing_feedback(0,0,_state(0,false,1,0.0))
        _require(not audio._consume_foot_jump(1.0/90.0,_state(1,true,0,100.0),Vector3.ZERO,true,0,0,false),"source grounded arm stays quiet")
        _require(not audio._consume_foot_jump(1.0/90.0,_state(1,false,reason,100.0),Vector3(0,.5,0),false,0,0,false),"invalid/cancelled stop stays quiet reason%d" % reason)
        _require(not audio._consume_foot_jump(1.0/90.0,_state(1,false,10,100.0),Vector3(0,.5,0),false,0,0,false),"consumed invalid stop never replays reason%d" % reason)
    var receipt := {"status":"PASS" if failures.is_empty() else "FAIL","failures":failures,"ordinary_overlap_jump_cues":int(audio.jumps),"invalid_stop_cases":8,"scope":"synthetic source-level duplicate/cancel/invalid-stop checks only; no native physical or audible output claim"}
    var file := FileAccess.open("res://guard-receipt.json",FileAccess.WRITE)
    file.store_string(JSON.stringify(receipt,"  "))
    file.close()
    audio.queue_free()
    await process_frame
    print("WOOD_JUMP_AUDIO_FINAL_GUARDS %s" % receipt["status"])
    quit(0 if failures.is_empty() else 4)
