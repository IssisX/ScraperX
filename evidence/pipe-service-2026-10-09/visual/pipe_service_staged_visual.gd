extends SceneTree
# Explicit capture-only staging in actual production main/normal native world.
# This is a surface/sightline inspection, never an earned route or phone test.
var main: Node
var records: Array[Dictionary] = []
var failures: Array[String] = []
func _initialize() -> void:
    root.disable_3d = true
    _run.call_deferred()
func _pose(label: String, centre: Vector3, owner: int, yaw: float, pitch: float, crouch: bool = false) -> void:
    root.disable_3d = true
    if not bool(main._native.debug_restart_at(centre)):
        failures.append("visual staging rejected " + label)
        return
    main._native.set_move_input(0.0,0.0)
    main._native.set_crouch_input(crouch)
    var dt := float(main._native.get_fixed_step_seconds())
    for tick in 40:
        main._native.advance_frame(dt)
        main._render_snapshot(dt)
    var actual_owner := int(main._native.get_support_entity_id())
    if actual_owner != owner or not bool(main._native.is_player_grounded()):
        failures.append("stage did not settle on expected real footing %s owner%d" % [label,actual_owner])
        return
    if bool(main._native.is_player_crouched()) != crouch:
        failures.append("actual crouch state differs " + label)
    main._yaw = yaw
    main._pitch = pitch
    main._render_snapshot()
    main._ctx = main._read_context()
    main._arms.update_arms(main._arms_state({"move":Vector2.ZERO,"pendant":Vector2.ZERO}),main._camera.global_transform,0.0)
    var native_agreement := true
    var native_parts := 0
    var render_triangles := 0
    var render_surfaces := 0
    for entity in [1939]:
        var index := int(main._native.get_kit_body_index(entity))
        var body := main._kit_view.get_node("KitBody%d" % entity) as Node3D
        native_agreement = native_agreement and body != null and body.transform.is_equal_approx(main._native.get_kit_body_render_transform(index))
        native_parts += int(main._native.get_kit_body_parts(index).size()) / main._kit_view.KIT_PART_FLOATS
        for child in body.get_children():
            if child is MeshInstance3D:
                for surface in child.mesh.get_surface_count():
                    var arrays: Array = child.mesh.surface_get_arrays(surface)
                    var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
                    var indices: PackedInt32Array = arrays[Mesh.ARRAY_INDEX]
                    render_triangles += (indices.size() if not indices.is_empty() else vertices.size()) / 3
                    render_surfaces += 1
    if not native_agreement: failures.append("native render mismatch " + label)
    root.disable_3d = false
    await RenderingServer.frame_post_draw
    await RenderingServer.frame_post_draw
    var path := "res://%s.png" % label
    if root.get_texture().get_image().save_png(path) != OK:
        failures.append("screenshot save failed " + label)
    root.disable_3d = true
    records.append({"label":label,"explicit_visual_staging":true,"player":str(main._native.get_player_position()),
        "support":actual_owner,"grounded":bool(main._native.is_player_grounded()),"crouched":bool(main._native.is_player_crouched()),"camera":str(main._camera.global_transform),
        "native_render_agreement":native_agreement,"native_parts":native_parts,"render_triangles":render_triangles,"render_surfaces":render_surfaces,"image":path,"viewport":str(root.get_visible_rect().size),
        "fov":main._camera.fov,"shadow_quality":main._settings.shadow_quality})
    print("PIPE_SERVICE_FULLWORLD_CAPTURE %s support%d native_render_agreement=%s" % [label,actual_owner,native_agreement])
func _run() -> void:
    root.size = Vector2i(648,557)
    root.content_scale_size = root.size
    main = load("res://main.tscn").instantiate()
    root.add_child(main)
    main.set_process(false)
    root.disable_3d = true
    main._router.capture_mouse = false
    Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
    # Existing quality preset only for bounded software capture. Production defaults unchanged.
    main._settings.apply_quality(0)
    main._settings.render_scale = 1.0
    main._apply_settings()
    root.disable_3d = true
    print("PIPE_SERVICE_FULLWORLD_CAPTURE_READY actualmain normalworld size=%s" % root.get_visible_rect().size)
    await _pose("staged_service_entry451_pov",Vector3(-20.5,451.9,-130.0),1939,-PI/2.0,-0.08)
    await _pose("staged_pipe_approach451_pov",Vector3(-17.0,451.9,-129.375),1939,-PI/2.0,-0.20)
    await _pose("staged_service_window451_pov",Vector3(-17.6,451.9,-130.65),1939,-PI/2.0+0.20,-0.06,true)
    var file := FileAccess.open("res://pipe-service-visual-receipt.json",FileAccess.WRITE)
    file.store_string(JSON.stringify({"scope":"explicit staged visual inspection; normal production main/native world; capture-only low quality preset; no earned route/phone/performance proof","captures":records,"failures":failures},"  "))
    file.close()
    print("PIPE_SERVICE_FULLWORLD_VISUAL %s" % ("PASS" if failures.is_empty() else "FAIL"))
    quit(0 if failures.is_empty() else 3)
