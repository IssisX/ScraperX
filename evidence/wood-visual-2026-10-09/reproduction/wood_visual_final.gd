extends SceneTree

# This adapter selects presentation only. All bodies, geometry, transforms,
# player contact and simulation stepping remain owned by the native world.
class NativeSelection:
    extends RefCounted
    var source: Object
    var indices: Array[int] = []
    func _init(value: Object) -> void:
        source = value
        for body in int(source.get_kit_body_count()):
            var entity := int(source.get_kit_body_entity_id(body))
            if entity == 1935 or entity == 1936 or entity >= 2880 and entity <= 2891:
                indices.append(body)
    func get_kit_body_count() -> int: return indices.size()
    func get_kit_body_entity_id(i: int) -> int: return int(source.get_kit_body_entity_id(indices[i]))
    func get_kit_body_transform(i: int) -> Transform3D: return source.get_kit_body_transform(indices[i])
    func get_kit_body_render_transform(i: int) -> Transform3D: return source.get_kit_body_render_transform(indices[i])
    func get_kit_body_parts(i: int) -> PackedFloat32Array: return source.get_kit_body_parts(indices[i])
    func get_kit_body_part_mesh(i: int, part: int) -> PackedFloat32Array: return source.get_kit_body_part_mesh(indices[i], part)
    func get_kit_body_material_key(i: int) -> int: return int(source.get_kit_body_material_key(indices[i]))
    func is_kit_body_dynamic(i: int) -> bool: return bool(source.is_kit_body_dynamic(indices[i]))
    func is_kit_body_enabled(i: int) -> bool: return bool(source.is_kit_body_enabled(indices[i]))
    func get_landing_state() -> Dictionary: return source.get_landing_state()
    func get_cargo_net_indices() -> PackedInt32Array: return PackedInt32Array()
    func get_kit_cable_count() -> int: return 0

var native: Object
var selection: NativeSelection
var view: Node3D
var records: Array[Dictionary] = []
var failures: Array[String] = []
var camera: Camera3D
var open_interface: Dictionary = {}

func _initialize() -> void:
    _run.call_deferred()

func _capture(label: String) -> void:
    root.disable_3d = false
    view.render_view()
    var segments: Array[Dictionary] = []
    for index in selection.indices.size():
        var entity := selection.get_kit_body_entity_id(index)
        if entity < 2880 or entity > 2891:
            continue
        var pose := selection.get_kit_body_transform(index)
        var rendered := selection.get_kit_body_render_transform(index)
        var node := view.get_node("KitBody%d" % entity) as Node3D
        if not node.transform.is_equal_approx(rendered):
            failures.append("presentation transform mismatch %d" % entity)
        if not node.visible or not rendered.origin.is_finite():
            failures.append("hidden or non-finite segment %d" % entity)
        var meshes := node.find_children("*", "MeshInstance3D", true, false)
        if meshes.size() != 1:
            failures.append("expected one native timber mesh %d" % entity)
        else:
            var mesh := (meshes[0] as MeshInstance3D).mesh
            var material := mesh.surface_get_material(0) as ShaderMaterial
            if material == null:
                failures.append("native wood shader material missing %d" % entity)
            var bounds := mesh.get_aabb()
            if bounds.size.distance_to(Vector3(0.2,0.038,0.19)) > 0.00002:
                failures.append("native box bounds changed %d actual=%s" % [entity,bounds.size])
        var mesh_node := meshes[0] as MeshInstance3D
        var arrays: Array = mesh_node.mesh.surface_get_arrays(0)
        var digest := HashingContext.new()
        digest.start(HashingContext.HASH_SHA256)
        digest.update(var_to_bytes([arrays[Mesh.ARRAY_VERTEX], arrays[Mesh.ARRAY_NORMAL], arrays[Mesh.ARRAY_INDEX]]))
        var geometry_hash := digest.finish().hex_encode()
        var material_mask = null
        var shader_material := mesh_node.mesh.surface_get_material(0) as ShaderMaterial
        for uniform in shader_material.shader.get_shader_uniform_list():
            if String(uniform["name"]) == "broken_joint_mask":
                material_mask = shader_material.get_shader_parameter("broken_joint_mask")
                if int(material_mask) != (int(native.get_landing_state()["plank_broken_joint_mask"]) & 0x7ff):
                    failures.append("actual native fracture mask binding mismatch %d" % entity)
        segments.append({"entity":entity,"geometry_sha256":geometry_hash,"shader_broken_mask":material_mask,"physical_origin":[pose.origin.x,pose.origin.y,pose.origin.z],
            "render_origin":[rendered.origin.x,rendered.origin.y,rendered.origin.z],
            "basis_y":[pose.basis.y.x,pose.basis.y.y,pose.basis.y.z]})
    await RenderingServer.frame_post_draw
    await RenderingServer.frame_post_draw
    var image := root.get_texture().get_image()
    var image_path := "res://%s.png" % label
    var save_error := image.save_png(image_path)
    if save_error != OK:
        failures.append("screenshot save error %d" % save_error)
    root.disable_3d = true
    records.append({"open_interface":open_interface,"pose":label,"segments":segments,"player":str(native.get_player_position()),
        "grounded":bool(native.is_player_grounded()),"support":int(native.get_support_entity_id()),
        "fracture_mask":int(native.get_landing_state()["plank_broken_joint_mask"]),
        "peak_strength_ratio":float(native.get_landing_state()["plank_peak_strength_ratio"]),
        "tick":int(native.get_tick_index()),"screenshot":image_path})
    print("SCRAPERX_PLANK_VISUAL pose=%s support=%d grounded=%s segments=%d" % [
        label,int(native.get_support_entity_id()),str(native.is_player_grounded()),segments.size()])

func _run() -> void:
    if DisplayServer.get_name() == "headless" or not ClassDB.class_exists("ScraperXSimulation"):
        push_error("Actual renderer and latest native extension required")
        quit(2)
        return
    root.size = Vector2i(648,432)
    root.disable_3d = true
    native = ClassDB.instantiate("ScraperXSimulation")
    selection = NativeSelection.new(native)
    if selection.indices.size() != 14:
        push_error("Expected actual normal-world steel1935/1936 and all12 plank segments; got %d" % selection.indices.size())
        quit(3)
        return
    var scene := Node3D.new()
    root.add_child(scene)
    view = (load("res://presentation/kit_view.gd") as GDScript).new()
    scene.add_child(view)
    var palette: Array[Material] = []
    var colours := ["70737a","635345","4a3420","6d7377","dfb743","cc9c39","575556","eee6d7","8f5036"]
    for colour in colours:
        var material := StandardMaterial3D.new()
        material.albedo_color = Color(colour)
        material.roughness = 0.8
        palette.append(material)
    view.setup(selection,palette,palette[0],StandardMaterial3D.new(),func(_a,_b,_c,_d,_e,_f): pass)
    var environment := WorldEnvironment.new()
    environment.environment = Environment.new()
    environment.environment.background_mode = Environment.BG_COLOR
    environment.environment.background_color = Color("101922")
    environment.environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
    environment.environment.ambient_light_color = Color("c1ceda")
    environment.environment.ambient_light_energy = 0.75
    environment.environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
    scene.add_child(environment)
    var light := DirectionalLight3D.new()
    light.rotation_degrees = Vector3(-40,-25,0)
    light.light_energy = 1.5
    scene.add_child(light)
    camera = Camera3D.new()
    camera.position = Vector3(-6.96,407.26,-127.86)
    camera.fov = 48.0
    camera.near = 0.05
    camera.far = 30.0
    scene.add_child(camera)
    camera.look_at(Vector3(-6.8,406.981,-128.2))
    camera.current = true
    var dt := float(native.get_fixed_step_seconds())
    if not bool(native.debug_restart_at(Vector3(-8.8,407.9,-128.2))):
        failures.append("actual steel approach staging rejected")
    native.set_move_input(0.0,0.0)
    for tick in 360:
        native.advance_frame(dt)
    await process_frame
    await _capture("wood_close_unloaded")
    camera.position = Vector3(-7.72,407.22,-127.86)
    camera.look_at(Vector3(-7.93,406.982,-128.2))
    await _capture("wood_supported_end_close")
    camera.position = Vector3(-8.9,408.85,-127.1)
    camera.look_at(Vector3(-6.7,406.98,-128.2))
    await _capture("wood_steel_approach")
    # Explicit capture-only existing3m impact scenario. Native gravity/contact
    # alone supplies the load: no break hook or manual force/velocity is used.
    if not bool(native.debug_restart_at(Vector3(-6.8,410.9,-128.2))):
        failures.append("existing3m fall staging rejected")
    var early_captured := false
    for tick in 180:
        native.advance_frame(dt)
        var mask := int(native.get_landing_state()["plank_broken_joint_mask"])
        if mask != 0 and not early_captured:
            # Pause only stepping for the still: this is actual gravity-driven
            # separation, not a forced fracture or relocated fragment.
            var seam := 0
            while seam < 11 and (mask & (1 << seam)) == 0:
                seam += 1
            var first_body := int(native.get_kit_body_index(2880 + seam))
            var second_body := int(native.get_kit_body_index(2881 + seam))
            var first_pose: Transform3D = native.get_kit_body_transform(first_body)
            var second_pose: Transform3D = native.get_kit_body_transform(second_body)
            var first_end: Vector3 = first_pose * Vector3(0.1,0,0)
            var second_end: Vector3 = second_pose * Vector3(-0.1,0,0)
            var gap := first_end.distance_to(second_end)
            if gap < 0.03:
                continue
            if gap > 0.6:
                failures.append("first actual broken interface opened beyond close-view extent")
                break
            open_interface = {"seam":seam,"endpoint_gap_m":gap,"first_entity":2880+seam,"second_entity":2881+seam,"mask":mask}
            var focus := (first_end + second_end) * 0.5
            camera.position = focus + Vector3(0.25,0.22,0.40)
            camera.look_at(focus)
            await _capture("wood_actual_open_interface_close")
            early_captured = true
    if not early_captured:
        failures.append("no actual open broken interface captured")
    var broken := int(native.get_landing_state()["plank_broken_joint_mask"])
    if broken == 0:
        failures.append("actual native gravity impact did not fracture existing timber")
    camera.position = Vector3(-6.8,408.25,-125.2)
    camera.look_at(Vector3(-6.8,404.65,-128.2))
    # Existing state retained; this single-frame pass inspects open endgrain.
    var receipt := {"renderer":DisplayServer.get_name(),"selected_bodies":selection.indices.size(),
        "source":"normal native world + actual kit_view, presentation selection only",
        "poses":records,"failures":failures,"scope":"filtered648x432 Compatibility material inspection; actual normal-world geometry/transform and gravity-impact fracture; explicit diagnostic stages/camera; no earned route, phone or final-art claim"}
    var file := FileAccess.open("res://visual-receipt.json",FileAccess.WRITE)
    file.store_string(JSON.stringify(receipt,"  "))
    file.close()
    print("SCRAPERX_PLANK_VISUAL %s checks=actual_segments_native_transform_material" % ("PASS" if failures.is_empty() else "FAIL"))
    native = null
    quit(0 if failures.is_empty() else 4)
