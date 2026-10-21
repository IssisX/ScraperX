extends RefCounted
## Presentation of the native body's orientation; no gameplay state writes.

static func apply(camera: Camera3D, centre: Vector3, eye_offset: Vector3,
		physical: Quaternion, captive_or_recovering: bool) -> void:
	camera.quaternion = physical * camera.quaternion
	if captive_or_recovering:
		camera.position = centre + physical * eye_offset
