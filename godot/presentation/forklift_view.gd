extends Node3D

# Native receipts include steering, suspension and spin on the render clock.
# CylinderMesh's Y axis is the wheel axle; no local motion is synthesized.
var _wheels: Array[MeshInstance3D] = []

func render_state(state: Dictionary) -> void:
	visible = bool(state.get("available", false))
	if not visible:
		return
	var wheels: Array = state.get("wheels", [])
	while _wheels.size() < wheels.size():
		var wheel := MeshInstance3D.new()
		var mesh := CylinderMesh.new()
		mesh.radial_segments = 16
		mesh.rings = 1
		wheel.mesh = mesh
		var rubber := StandardMaterial3D.new()
		rubber.albedo_color = Color("292b2c")
		rubber.roughness = 0.92
		wheel.material_override = rubber
		add_child(wheel)
		_wheels.append(wheel)
	for i in _wheels.size():
		var wheel := _wheels[i]
		wheel.visible = i < wheels.size()
		if not wheel.visible:
			continue
		var receipt: Dictionary = wheels[i]
		var mesh := wheel.mesh as CylinderMesh
		var radius := float(receipt["radius_m"])
		var width := float(receipt["width_m"])
		if mesh.top_radius != radius:
			mesh.top_radius = radius
			mesh.bottom_radius = radius
		if mesh.height != width:
			mesh.height = width
		wheel.global_transform = receipt["transform"]
