extends SceneTree

# Native ordinary input and production AudioDirector.update; no viewport,
# force/velocity writes, manual foot receipts or full game scene setup.
var native: Object
var audio: Node
var failures: Array[String] = []
var records: Array[Dictionary] = []
var phase := "start"
var step := 1.0 / 90.0

func _initialize() -> void: _run.call_deferred()
func _require(value: bool, why: String) -> void:
    if not value:
        failures.append(phase + ": " + why)
        push_error(phase + ": " + why)
func _wood() -> bool:
    var owner := int(native.get_support_entity_id())
    return bool(native.is_player_grounded()) and owner >= 2880 and owner <= 2891
func _feed() -> void:
    var count := int(audio.jumps)
    audio.update(step,native.get_player_position(),native.get_player_linear_velocity(),bool(native.is_player_grounded()),int(native.get_support_entity_id()),int(native.get_traversal_state()),bool(native.is_parachute_deployed()),int(native.get_death_count()),bool(native.is_player_crouched()),native.get_support_point_linear_velocity(),native.get_landing_state())
    if int(audio.jumps) != count:
        var state: Dictionary = native.get_landing_state()
        var velocity: Vector3 = native.get_player_linear_velocity()
        _require(not bool(native.is_player_grounded()) and velocity.y > 0.0,"Jump cue follows true actual airborne positive rise")
        records.append({"phase":phase,"tick":int(native.get_tick_index()),"grounded":bool(native.is_player_grounded()),"velocity_y":velocity.y,"foot_start":int(state["foot_push_start_count"]),"foot_active":bool(state["foot_push_active"]),"stop":int(state["foot_push_stop_reason"]),"work_j":float(state["foot_push_command_work_bound_j"]),"jumps":int(audio.jumps)})
        print("WOOD_JUMP_AUDIO cue=%s" % records.back())
func _tick(count: int = 1) -> void:
    for i in count:
        native.advance_frame(step)
        var state: Dictionary = native.get_landing_state()
        _require((native.get_player_position() as Vector3).is_finite() and float(state["player_gravity_factor"]) == 1.0 and int(native.get_death_count()) == 0,"finite actual state/gravity1/deaths0")
        _feed()
func _steer(x: float, extra: Vector2 = Vector2.ZERO) -> void:
    var at: Vector3 = native.get_player_position()
    var velocity: Vector3 = native.get_player_linear_velocity()
    var stick := (1.8 * (Vector2(x,-128.2)-Vector2(at.x,at.z))-.28*Vector2(velocity.x,velocity.z)+extra).limit_length(1.0)
    native.set_move_input(stick.x,stick.y)
func _walk(x: float, timber: bool) -> void:
    var effort := Vector2.ZERO
    for i in 1350:
        var at: Vector3 = native.get_player_position()
        var velocity: Vector3 = native.get_player_linear_velocity()
        var offset := Vector2(x-at.x,-128.2-at.z)
        var supported := _wood() if timber else bool(native.is_player_grounded()) and int(native.get_support_entity_id()) == 1935
        if supported and absf(offset.x) < .08 and absf(offset.y) < .03 and Vector2(velocity.x,velocity.z).length() < .12:
            native.set_move_input(0.0,0.0)
            return
        if offset.length() > .75: effort = Vector2.ZERO
        else: effort = (effort + .4 * offset * step).limit_length(.35)
        _steer(x,effort)
        _tick()
    _require(false,"ordinary walking reaches real supported target")
func _reset_audio() -> void:
    var state: Dictionary = native.get_landing_state()
    audio.reset_landing_feedback(int(state["landing_count"]),int(native.get_death_count()),state)
    _feed()
func _run() -> void:
    _require(ClassDB.class_exists("ScraperXSimulation"),"real native extension")
    native = ClassDB.instantiate("ScraperXSimulation")
    step = float(native.get_fixed_step_seconds())
    audio = (load("res://presentation/audio/audio_director.gd") as GDScript).new()
    root.add_child(audio)
    await process_frame
    phase = "actual_steel_approach"
    _require(bool(native.debug_restart_at(Vector3(-8.8,407.9,-128.2))),"sole initial actual steel stage")
    native.set_facing(1.0,0.0)
    native.set_move_input(0.0,0.0)
    _reset_audio()
    _tick(360)
    _require(bool(native.is_player_grounded()) and int(native.get_support_entity_id()) == 1935,"real steel start")
    _walk(-6.8,true)
    _tick(360)
    _require(_wood() and int(native.get_landing_state()["plank_broken_joint_mask"]) == 0,"actual intact loaded wood")
    phase = "actual_finite_wood_jump"
    var before := int(audio.jumps)
    var start := int(native.get_landing_state()["foot_push_start_count"])
    _require(bool(native.request_jump()),"ordinary supported Jump")
    _tick()
    var airborne := false
    var landed := false
    var spam := 0
    var peak := float((native.get_player_linear_velocity() as Vector3).y)
    for i in 540:
        var state: Dictionary = native.get_landing_state()
        if bool(state["foot_push_active"]):
            native.request_jump()
            spam += 1
        _steer(-6.8)
        _tick()
        state = native.get_landing_state()
        _require(int(state["foot_push_start_count"]) == start + 1,"active Jump spam never refreshes native receipt")
        peak = maxf(peak,float((native.get_player_linear_velocity() as Vector3).y))
        airborne = airborne or not bool(native.is_player_grounded()) and int(native.get_support_entity_id()) == 0
        if airborne and _wood():
            landed = true
            break
    _require(airborne and landed and peak > .8 and peak < 2.5 and spam > 1,"real sub-threshold finite wood takeoff and landing")
    _require(int(audio.jumps) == before + 1,"one actual wood takeoff cue")
    for repeat in 30: _feed()
    _require(int(audio.jumps) == before + 1,"same snapshot no repeated Jump cue")
    phase = "actual_wood_landing_settle"
    native.set_move_input(0.0,0.0)
    var quiet := 0
    for i in 360:
        _tick()
        var relative: Vector3 = native.get_player_linear_velocity() - native.get_support_point_linear_velocity()
        _require(int(native.get_landing_state()["plank_broken_joint_mask"]) == 0,"real landing remains intact")
        quiet = quiet + 1 if _wood() and absf(relative.y) < .1 and Vector2(relative.x,relative.z).length() < .1 else 0
        if quiet == 30: break
    _require(quiet == 30,"quiet actual loaded wood before next input")
    phase = "actual_drop_cancel_quiet"
    before = int(audio.jumps)
    native.request_jump()
    _tick()
    _require(bool(native.get_landing_state()["foot_push_active"]),"real second foot command begins")
    native.request_release()
    _tick(13)
    _require(not bool(native.get_landing_state()["foot_push_active"]) and int(audio.jumps) == before,"Drop before takeoff cancels pending cue")
    _walk(-6.8,true)
    _tick(90)
    phase = "actual_checkpoint_restart_quiet"
    before = int(audio.jumps)
    native.request_jump()
    _tick()
    _require(bool(native.get_landing_state()["foot_push_active"]),"real foot active before restart")
    _require(bool(native.restart_checkpoint()),"actual checkpoint restart accepted")
    _reset_audio()
    _tick(90)
    _require(not bool(native.get_landing_state()["foot_push_active"]) and int(audio.jumps) == before,"restart clears pending takeoff without cue")
    phase = "unchanged_actual_steel_jump"
    _walk(-8.8,false)
    _tick(90)
    before = int(audio.jumps)
    start = int(native.get_landing_state()["foot_push_start_count"])
    native.request_jump()
    _tick()
    _require(not bool(native.is_player_grounded()) and (native.get_player_linear_velocity() as Vector3).y > 5.34 and not bool(native.get_landing_state()["foot_push_active"]) and int(native.get_landing_state()["foot_push_start_count"]) == start,"unchanged real steel5.5takeoff")
    _require(int(audio.jumps) == before + 1,"old normal steel Jump cue unchanged")
    for repeat in 30: _feed()
    _require(int(audio.jumps) == before + 1,"repeated actual steel snapshot no spam")
    var receipt := {"status":"PASS" if failures.is_empty() else "FAIL","failures":failures,"records":records,"wood_peak_vy_mps":peak,"wood_spam_inputs":spam,"driver":AudioServer.get_driver_name(),"scope":"one real normal-world initial steel stage; ordinary native wood Jump/Drop/checkpoint restart/steel Jump inputs; production AudioDirector.update; no viewport, full UI, audible-output or haptic-device claim"}
    var file := FileAccess.open("res://jump-receipt.json",FileAccess.WRITE)
    file.store_string(JSON.stringify(receipt,"  "))
    file.close()
    native = null
    audio.queue_free()
    await process_frame
    print("WOOD_JUMP_AUDIO %s" % receipt["status"])
    quit(0 if failures.is_empty() else 4)
