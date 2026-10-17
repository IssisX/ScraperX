extends Node3D
## Loose stock as the player sees it (src/sim/loose_stock.hpp): every piece is
## a kit body, drawn and moved by the kit's own drawing (main_machines.gd);
## this gives the pieces that are painted their paint -- the containers'
## corrugated colours and the locking bars on their doors, the drums'
## colours, rusted beams among the mill-scaled -- in place of the kit's
## palette. Nothing here moves anything.

# LooseStock::Kind (loose_stock.hpp).
const KIND_DRUM := 3
const KIND_CONTAINER := 5
const KIND_STEEL_BEAM := 6
const KIND_GAS_BOTTLE := 14
const KIND_CABLE_REEL := 15
const KIND_TOOLBOX := 16
# A container's half size along its own x, y, z, its doors at +x.
const CONTAINER_HALF := Vector3(6.095, 1.295, 1.22)


func setup(main: Node, native: Object, kit_root: Node3D) -> void:
	name = "StockView"
	var ribbed: Texture2D = main._corrugation_texture()
	var container_paint: Array[Material] = []
	for colour in [Color("7b3422"), Color("34506b"), Color("42603f"), Color("8d8f88"), Color("9a5a26")]:
		var material: StandardMaterial3D = main._material(colour, 0.35, 0.78)
		material.normal_enabled = true
		material.normal_texture = ribbed
		material.uv1_triplanar = true
		material.uv1_triplanar_sharpness = 4.0
		material.uv1_scale = Vector3(3.4, 3.4, 3.4)
		container_paint.append(material)
	var bar_material: Material = main._material(Color("2b2a28"), 0.6, 0.55)
	var bar_mesh := BoxMesh.new()
	bar_mesh.size = Vector3(0.05, CONTAINER_HALF.y * 2.0 * 0.9, 0.05)
	bar_mesh.material = bar_material
	# Weathered like the rest of the steel: the bump and the streaks of rust.
	var drum_paint: Array[Material] = []
	for colour in [Color("2c4a73"), Color("a9851f"), Color("7c2a1e")]:
		drum_paint.append(main._material(colour, 0.35, 0.72, Color.BLACK, 1.0, main._bump_steel))
	var beam_paint: Array[Material] = [
		main._material(Color("1d1a17"), 0.72, 0.6, Color.BLACK, 1.0, main._bump_steel),
		main._material(Color("3b1f13"), 0.28, 0.95, Color.BLACK, 1.0, main._bump_steel)]
	# Gas bottles in their gases' colours (oxygen black, acetylene maroon,
	# argon blue), their caps left steel; the reels' wound cable black
	# rubber; the toolboxes red, blue or yellow enamel.
	var bottle_paint: Array[Material] = []
	for colour in [Color("1c1c1e"), Color("6b1e24"), Color("2d5f8a")]:
		bottle_paint.append(main._material(colour, 0.3, 0.55, Color.BLACK, 1.0, main._bump_steel))
	var cable_paint: Material = main._material(Color("141414"), 0.0, 0.82)
	var toolbox_paint: Array[Material] = []
	for colour in [Color("a3231c"), Color("1f4f8f"), Color("c9a227")]:
		toolbox_paint.append(main._material(colour, 0.45, 0.45, Color.BLACK, 1.0, main._bump_steel))
	var stock: PackedInt32Array = native.get_loose_stock()
	for i in range(0, stock.size() - 2, 3):
		var entity := int(native.get_kit_body_entity_id(stock[i]))
		var body := kit_root.get_node_or_null("KitBody%d" % entity) as Node3D
		if body == null:
			continue
		var kind := stock[i + 1]
		var variant := stock[i + 2]
		match kind:
			KIND_CONTAINER:
				_paint(body, container_paint[variant % container_paint.size()])
				# The door end's four locking bars.
				for bar_z in [-0.85, -0.35, 0.35, 0.85]:
					var bar := MeshInstance3D.new()
					bar.name = "ContainerBar"
					bar.mesh = bar_mesh
					bar.position = Vector3(CONTAINER_HALF.x + 0.025, 0.0, bar_z)
					bar.visibility_range_end = 160.0
					body.add_child(bar)
			KIND_DRUM:
				_paint(body, drum_paint[variant % drum_paint.size()])
			KIND_STEEL_BEAM:
				_paint(body, beam_paint[variant % beam_paint.size()])
			# A moving body is drawn one mesh per material, in the order its
			# parts first use them (main_machines.gd): the bottle's body, then
			# its steel cap; the reel's timber flanges, then its cable; the
			# toolbox's box, then its steel handle.
			KIND_GAS_BOTTLE:
				_paint_mesh(body, 0, bottle_paint[variant % bottle_paint.size()])
			KIND_CABLE_REEL:
				_paint_mesh(body, 1, cable_paint)
			KIND_TOOLBOX:
				_paint_mesh(body, 0, toolbox_paint[variant % toolbox_paint.size()])


# The index-th mesh of the body (see above) only.
func _paint_mesh(body: Node3D, index: int, material: Material) -> void:
	var meshes := body.get_children().filter(func(child: Node) -> bool: return child is MeshInstance3D)
	if index < meshes.size():
		(meshes[index] as MeshInstance3D).material_override = material


func _paint(body: Node3D, material: Material) -> void:
	for child in body.get_children():
		if child is MeshInstance3D:
			(child as MeshInstance3D).material_override = material
