extends RefCounted
# One gait for the whole body: how often the feet strike at a speed over the
# ground, and how long each foot is down. The footsteps heard, the head's
# bob, the arms' pump and the legs drawn all follow it, so they keep one
# rhythm. From measured human gait: a walk at 1.4 m/s strikes about 1.8
# times a second (a 0.78 m step), a run at 5.5 m/s about 2.9 (1.9 m), flow's
# 9 m/s about 3.5 (2.6 m). Running faster is mostly longer steps, not quicker
# ones; a fixed step length (0.69 m before) made a run strike 8 times a
# second and a flow 13, a tap dance.
#
# Each table is speed (m/s) against its value, linear between the points and
# held past the ends.

# Footfalls a second.
const CADENCE := [Vector2(0.0, 1.4), Vector2(1.4, 1.8), Vector2(3.0, 2.6), Vector2(5.5, 2.9),
	Vector2(9.0, 3.5)]
# How long each foot is on the ground per footfall, seconds: a walk's long
# stance, a run's short contact.
const CONTACT := [Vector2(0.0, 0.7), Vector2(1.4, 0.65), Vector2(3.0, 0.3), Vector2(5.5, 0.2),
	Vector2(9.0, 0.12)]


static func cadence(speed: float) -> float:
	return _lookup(CADENCE, speed)


static func contact_seconds(speed: float) -> float:
	return _lookup(CONTACT, speed)


static func _lookup(table: Array, speed: float) -> float:
	var first: Vector2 = table[0]
	if speed <= first.x:
		return first.y
	for i in range(1, table.size()):
		var b: Vector2 = table[i]
		if speed <= b.x:
			var a: Vector2 = table[i - 1]
			return lerpf(a.y, b.y, (speed - a.x) / (b.x - a.x))
	return (table[table.size() - 1] as Vector2).y
