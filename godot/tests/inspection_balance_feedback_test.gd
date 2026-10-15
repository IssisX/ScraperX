extends SceneTree

const Feedback := preload("res://presentation/inspection_balance_feedback.gd")

class NativeSample extends RefCounted:
	var hatch := Transform3D(Basis.IDENTITY, Vector3(-19.35, 473.2, -134.3))
	var player := hatch.origin
	func get_kit_body_index(entity: int) -> int:
		return entity if entity in [1979, 1997, 1902] else -1
	func get_player_position() -> Vector3:
		return player
	func get_kit_body_transform(entity: int) -> Transform3D:
		if entity == 1979:
			return hatch
		if entity == 1997:
			return Transform3D(Basis.IDENTITY, Vector3(-19.35, 472.6, -133.5))
		return Transform3D(Basis.IDENTITY, Vector3(-20.05, 473.31, -134.45))

var _failures := 0

func _initialize() -> void:
	call_deferred("_run")

func _check(value: bool, label: String) -> void:
	if not value:
		_failures += 1
		push_error(label)

func _run() -> void:
	_check(absf(Feedback._angular_speed(Quaternion.IDENTITY,
		Quaternion(Vector3.FORWARD, 0.00001), 0.02) - 0.0005) < 0.000001,
		"slow real span rotation remains observable below acos rounding threshold")
	var native := NativeSample.new()
	var view := Feedback.new()
	root.add_child(view)
	var events: Array[StringName] = []
	view.feedback.connect(func(kind: StringName, _strength: float): events.append(kind))
	for tick in 120:
		view.update_from_native(native, 1.0 / 90.0)
	_check(events.is_empty(), "still unloaded hatch produces no feedback")
	_check(view._stop_contact(native, native.hatch).is_finite(), "real rear rib touches rear stop")
	native.hatch.basis = Basis(Vector3.RIGHT, 0.08)
	_check(not view._stop_contact(native, native.hatch).is_finite(), "midstroke has no stop contact")
	native.hatch.basis = Basis(Vector3.RIGHT, deg_to_rad(10.0))
	_check(view._stop_contact(native, native.hatch).is_finite(), "front rib reaches real lower stop")
	# Reset observer to the unloaded pose before exercising measured motion.
	native.hatch.basis = Basis.IDENTITY
	view._sampled = false
	view.update_from_native(native, 0.02)
	for tick in 35:
		native.hatch.basis = Basis(Vector3.RIGHT, deg_to_rad(10.0) * (tick + 1) / 35.0)
		view.update_from_native(native, 0.02, true)
	view.update_from_native(native, 0.02, true)
	_check(events.has(&"bearing") and events.has(&"stop"), "real sampled motion and stop classify finite cues")
	var settled_count := events.size()
	for tick in 120:
		view.update_from_native(native, 1.0 / 90.0)
	_check(events.size() == settled_count, "settled hatch does not retrigger")
	native.player += Vector3(100.0, 0.0, 0.0)
	for tick in 80:
		native.hatch.basis = Basis(Vector3.RIGHT, sin(float(tick)) * 0.1)
		view.update_from_native(native, 0.02)
	_check(events.size() == settled_count, "distant mechanism produces no feedback")
	view.set_volumes(0.0, 1.0)
	_check(view._gain == 0.0, "settings mute is honored")
	view.free()
	print("inspection balance feedback: %s" % ("PASS" if _failures == 0 else "FAIL"))
	quit(0 if _failures == 0 else 1)
