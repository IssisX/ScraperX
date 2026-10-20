extends Node3D
## Live fabrication's objects as the player sees them (src/sim/fabricated.hpp,
## 03_EXECUTION/LIVE_FABRICATION/DESIGN.md): each one a real body the native
## simulates, drawn from the model its delivery carried (a GLB, loaded at run
## time with GLTFDocument, as the climber's body and the Matron are) and
## moved every frame to its body's pose. An object whose model is not to hand
## (a save loaded without it) is drawn as a plain crate of its own hulls, the
## native's triangles, so what is seen is still what collides. Nothing here
## moves anything; nothing here checks a delivery (the bridge does, before
## the native takes it).

const CRATE_COLOR := Color("6d675b")

var _native: Object
var _models := {}        # glb_sha512 (hex) -> the model's bytes, to hand
var _nodes := {}         # entity -> Node3D at the body's pose
var _crate: StandardMaterial3D
## For tests: what each entity is drawn with ("model" or "crate").
var drawn_as := {}


func setup(native: Object) -> void:
	name = "FabricatedView"
	_native = native
	_crate = StandardMaterial3D.new()
	_crate.albedo_color = CRATE_COLOR
	_crate.roughness = 0.9


## A delivery the native took (its entity) and the model it came with, which
## the bridge has checked against the hash its signed hull file names: drawn
## from now on, and kept by that hash for the object should it be loaded
## again. False when the model cannot be read (it is drawn as a crate).
func show_model(entity: int, model: PackedByteArray) -> bool:
	var index := _index_of(entity)
	if index < 0:
		return false
	var object: Dictionary = _native.get_fabricated_object(index)
	_models[String(object["glb_sha512"])] = model
	_forget(entity)
	return _build(entity, object, index)


## A model to hand for any object that names it by its hash (a cache).
func keep_model(glb_sha512: String, model: PackedByteArray) -> void:
	_models[glb_sha512] = model


## Every object the native holds drawn, and each where its body is.
func follow() -> void:
	if _native == null:
		return
	var count := int(_native.get_fabricated_count())
	for index in count:
		var object: Dictionary = _native.get_fabricated_object(index)
		var entity := int(object["entity"])
		if not _nodes.has(entity):
			_build(entity, object, index)
		var node: Node3D = _nodes[entity]
		var body := int(object["body"])
		node.visible = bool(_native.is_kit_body_enabled(body))
		if node.visible:
			node.transform = _native.get_kit_body_transform(body)


func node_for(entity: int) -> Node3D:
	return _nodes.get(entity) as Node3D


func _index_of(entity: int) -> int:
	for index in int(_native.get_fabricated_count()):
		if int(_native.get_fabricated_object(index)["entity"]) == entity:
			return index
	return -1


func _forget(entity: int) -> void:
	if _nodes.has(entity):
		var old: Node3D = _nodes[entity]
		_nodes.erase(entity)
		old.queue_free()


func _build(entity: int, object: Dictionary, index: int) -> bool:
	var node := Node3D.new()
	node.name = "Fabricated%d" % entity
	add_child(node)
	_nodes[entity] = node
	var model: PackedByteArray = _models.get(String(object["glb_sha512"]), PackedByteArray())
	if not model.is_empty():
		var doc := GLTFDocument.new()
		var state := GLTFState.new()
		var err := doc.append_from_buffer(model, "", state)
		var scene: Node3D = doc.generate_scene(state) as Node3D if err == OK else null
		if scene != null:
			node.add_child(scene)
			drawn_as[entity] = "model"
			node.transform = _native.get_kit_body_transform(int(object["body"]))
			return true
		push_warning("fabricated object %d: its model cannot be read (%s); drawn as a crate" % [entity, error_string(err)])
	# A plain crate of its hulls: the native's own triangles, in its frame
	# (Jolt winds a face counter-clockwise seen from outside; Godot's front
	# faces wind clockwise).
	var tool := SurfaceTool.new()
	tool.begin(Mesh.PRIMITIVE_TRIANGLES)
	var corners: PackedVector3Array = _native.get_fabricated_hull_triangles(index)
	for t in range(0, corners.size() - 2, 3):
		tool.add_vertex(corners[t])
		tool.add_vertex(corners[t + 2])
		tool.add_vertex(corners[t + 1])
	tool.generate_normals()
	var crate := MeshInstance3D.new()
	crate.mesh = tool.commit()
	crate.material_override = _crate
	node.add_child(crate)
	drawn_as[entity] = "crate"
	node.transform = _native.get_kit_body_transform(int(object["body"]))
	return model.is_empty()
