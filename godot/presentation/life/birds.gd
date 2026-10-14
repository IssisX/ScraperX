extends Node3D
## Birds on the structure: presentation only, no consequence in the world.
##
## Pigeons stand in small flocks on real surfaces near the player (each bird's
## footing is the native's first surface below it). They peck and shuffle,
## and flush when the player comes close, runs at them, or lands hard nearby:
## up and away, round, and down onto another real surface. Higher up, gulls
## circle the tower at the player's height, mostly gliding.
##
## Every bird is one instance of one MultiMesh (one draw call); the wings
## beat in the vertex shader from per-instance phase and amplitude.

const PERCHED := 0
const FLUSHED := 1
const RETURNING := 2
const LEAVING := 3

const FLOCKS := 3
const FLOCK_MAX := 8
const GULLS := 9
const CAPACITY := FLOCKS * FLOCK_MAX + GULLS

# How near the player may come before a flock rises, walking and running.
const FLUSH_NEAR_M := 4.5
const FLUSH_RUN_M := 9.0
const FLUSH_RUN_SPEED := 4.5
# A landing at least this fast (m/s, downward) within FLUSH_THUD_M flushes too.
const FLUSH_THUD_SPEED := 6.0
const FLUSH_THUD_M := 14.0
# Roosts are looked for this far from the player, and only this far above or
# below the player's feet.
const ROOST_NEAR_M := 9.0
const ROOST_FAR_M := 28.0
const ROOST_RISE_M := 14.0
# A flock farther than this from the player is let go and placed again.
const FORGET_M := 60.0
# Gulls appear above this height.
const GULL_FLOOR_M := 60.0
# Native probes allowed per frame (a ray each).
const PROBES_PER_FRAME := 10

const PIGEON_COLORS := [Color("3c3f47"), Color("33363d"), Color("4a4d55"), Color("2b2d33")]
const GULL_COLOR := Color("b9b8b2")

var _native: Object
var _audio: Object
var _rng := RandomNumberGenerator.new()
var _multimesh: MultiMesh
var _instance: MultiMeshInstance3D

# Per bird: [position, velocity, state, flock, phase, flap, yaw, timer, target, scale].
var _pos := PackedVector3Array()
var _vel := PackedVector3Array()
var _state := PackedInt32Array()
var _flock_of := PackedInt32Array()
var _phase := PackedFloat32Array()
var _flap := PackedFloat32Array()
var _yaw := PackedFloat32Array()
var _timer := PackedFloat32Array()
var _target := PackedVector3Array()
var _active := PackedByteArray()
# Per flock: its centre (where it stands or will), whether placed, its size.
var _flock_centre := PackedVector3Array()
var _flock_live := PackedByteArray()
var _gull_centre := Vector3.ZERO
var _gull_live := false
var _probes_left := 0
var _last_vy := 0.0
var flushes := 0
var landings := 0


func setup(native: Object, audio: Object) -> void:
	_native = native
	_audio = audio
	_rng.seed = 0xB12D5
	_pos.resize(CAPACITY)
	_vel.resize(CAPACITY)
	_state.resize(CAPACITY)
	_flock_of.resize(CAPACITY)
	_phase.resize(CAPACITY)
	_flap.resize(CAPACITY)
	_yaw.resize(CAPACITY)
	_timer.resize(CAPACITY)
	_target.resize(CAPACITY)
	_active.resize(CAPACITY)
	_flock_centre.resize(FLOCKS)
	_flock_live.resize(FLOCKS)
	for i in CAPACITY:
		_flock_of[i] = i / FLOCK_MAX if i < FLOCKS * FLOCK_MAX else -1
		_phase[i] = _rng.randf() * TAU
	_multimesh = MultiMesh.new()
	_multimesh.transform_format = MultiMesh.TRANSFORM_3D
	_multimesh.use_colors = true
	_multimesh.use_custom_data = true
	_multimesh.mesh = _bird_mesh()
	_multimesh.instance_count = CAPACITY
	_multimesh.visible_instance_count = CAPACITY
	_instance = MultiMeshInstance3D.new()
	_instance.name = "Birds"
	_instance.multimesh = _multimesh
	_instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	# The flock moves round the player; its bounds are the space it may use.
	_instance.custom_aabb = AABB(Vector3(-200, -60, -200), Vector3(400, 140, 400))
	var material := ShaderMaterial.new()
	material.shader = _bird_shader()
	_instance.material_override = material
	add_child(_instance)
	for i in CAPACITY:
		_multimesh.set_instance_transform(i, Transform3D(Basis().scaled(Vector3.ZERO), Vector3.ZERO))
		_multimesh.set_instance_color(i, GULL_COLOR if _flock_of[i] < 0 else
			PIGEON_COLORS[i % PIGEON_COLORS.size()])


func perched_count() -> int:
	var count := 0
	for i in CAPACITY:
		if _active[i] == 1 and _state[i] == PERCHED:
			count += 1
	return count


func flying_count() -> int:
	var count := 0
	for i in CAPACITY:
		if _active[i] == 1 and _state[i] != PERCHED:
			count += 1
	return count


# The nearest standing flock's centre to `from`, or null.
func nearest_perched_flock(from: Vector3) -> Variant:
	var best: Variant = null
	var best_d := INF
	for f in FLOCKS:
		if _flock_live[f] == 0 or not _flock_perched(f):
			continue
		var d := _flock_centre[f].distance_to(from)
		if d < best_d:
			best_d = d
			best = _flock_centre[f]
	return best


# Highest a bird of a flock that was standing at `centre` is above it now.
func highest_above(centre: Vector3) -> float:
	var best := -INF
	for i in FLOCKS * FLOCK_MAX:
		if _active[i] == 1 and _flock_centre[_flock_of[i]].distance_to(centre) < 0.01:
			best = maxf(best, _pos[i].y - centre.y)
	return best


func update(delta: float, player: Vector3, velocity: Vector3, grounded: bool) -> void:
	if _native == null or delta <= 0.0:
		return
	_probes_left = PROBES_PER_FRAME
	var thud := grounded and _last_vy < -FLUSH_THUD_SPEED
	_last_vy = velocity.y if not grounded else 0.0
	var ground_speed := Vector2(velocity.x, velocity.z).length()
	for f in FLOCKS:
		if _flock_live[f] == 0:
			_place_flock(f, player)
		elif _flock_centre[f].distance_to(player) > FORGET_M and _flock_settled(f):
			_retire_flock(f)
		elif _flock_perched(f):
			var d := _flock_centre[f].distance_to(player)
			if d < FLUSH_NEAR_M or (d < FLUSH_RUN_M and ground_speed > FLUSH_RUN_SPEED) or \
					(thud and d < FLUSH_THUD_M):
				_flush(f, player)
	_update_gulls(player)
	for i in CAPACITY:
		if _active[i] == 0:
			continue
		match _state[i]:
			PERCHED:
				_perch(i, delta)
			FLUSHED:
				_fly_off(i, delta, player)
			RETURNING:
				_glide_in(i, delta)
			LEAVING:
				_leave(i, delta, player)
		_write(i)


# --- flocks -------------------------------------------------------------------


func _place_flock(f: int, player: Vector3) -> void:
	if _probes_left < FLOCK_MAX + 1:
		return
	var angle := _rng.randf() * TAU
	var reach := _rng.randf_range(ROOST_NEAR_M, ROOST_FAR_M)
	var spot: Variant = _surface(player + Vector3(cos(angle) * reach, 0.0, sin(angle) * reach), player.y)
	if spot == null:
		return
	_flock_centre[f] = spot
	_flock_live[f] = 1
	var size := _rng.randi_range(4, FLOCK_MAX)
	for k in FLOCK_MAX:
		var i := f * FLOCK_MAX + k
		_active[i] = 0
		if k >= size:
			continue
		var foot: Variant = _surface(spot + Vector3(_rng.randf_range(-1.1, 1.1), 0.0, _rng.randf_range(-1.1, 1.1)),
			spot.y)
		if foot == null or absf((foot as Vector3).y - spot.y) > 0.6:
			continue
		_active[i] = 1
		_pos[i] = foot
		_vel[i] = Vector3.ZERO
		_state[i] = PERCHED
		_yaw[i] = _rng.randf() * TAU
		_timer[i] = _rng.randf_range(0.2, 2.0)
		_flap[i] = 0.0
	var placed := false
	for k in FLOCK_MAX:
		placed = placed or _active[f * FLOCK_MAX + k] == 1
	if not placed:
		_flock_live[f] = 0


func _retire_flock(f: int) -> void:
	_flock_live[f] = 0
	for k in FLOCK_MAX:
		_active[f * FLOCK_MAX + k] = 0
		_write(f * FLOCK_MAX + k)


func _flock_perched(f: int) -> bool:
	var any := false
	for k in FLOCK_MAX:
		var i := f * FLOCK_MAX + k
		if _active[i] == 1:
			if _state[i] != PERCHED:
				return false
			any = true
	return any


func _flock_settled(f: int) -> bool:
	for k in FLOCK_MAX:
		var i := f * FLOCK_MAX + k
		if _active[i] == 1 and _state[i] == FLUSHED:
			return false
	return true


func _flush(f: int, player: Vector3) -> void:
	flushes += 1
	if _audio != null:
		_audio.flutter(_flock_centre[f])
	var away := _flock_centre[f] - player
	away.y = 0.0
	away = away.normalized() if away.length_squared() > 0.01 else Vector3.FORWARD
	for k in FLOCK_MAX:
		var i := f * FLOCK_MAX + k
		if _active[i] == 0:
			continue
		var spread := away.rotated(Vector3.UP, _rng.randf_range(-0.7, 0.7))
		_state[i] = FLUSHED
		# A start staggered over a quarter second: the nearest bird goes first.
		_timer[i] = _rng.randf_range(0.0, 0.25)
		_vel[i] = spread * _rng.randf_range(3.5, 5.5) + Vector3.UP * _rng.randf_range(3.0, 4.5)
		_target[i] = Vector3.ZERO


# Up and away, climbing to 8-12 m over where it stood, then looking for
# somewhere else to stand. No footing found: it flies off for good.
func _fly_off(i: int, delta: float, player: Vector3) -> void:
	if _timer[i] > 0.0:
		_timer[i] -= delta
		_flap[i] = 0.0
		return
	var f := _flock_of[i]
	_flap[i] = 1.0
	_phase[i] += delta * 30.0
	var climb := _flock_centre[f].y + 10.0 - _pos[i].y
	_vel[i].y = lerpf(_vel[i].y, clampf(climb, -1.0, 4.0), 1.0 - exp(-2.0 * delta))
	var flat := Vector3(_vel[i].x, 0.0, _vel[i].z)
	if flat.length() < 7.0:
		flat = flat.normalized() * minf(7.0, flat.length() + 6.0 * delta)
	# A slow turn, so the flock wheels instead of flying straight off.
	flat = flat.rotated(Vector3.UP, delta * 0.6 * (1.0 if f % 2 == 0 else -1.0))
	_vel[i] = Vector3(flat.x, _vel[i].y, flat.z)
	_pos[i] += _vel[i] * delta
	_face(i, delta)
	if climb < 1.5 and _pos[i].distance_to(_flock_centre[f]) > 12.0:
		_choose_landing(i, player)


func _choose_landing(i: int, player: Vector3) -> void:
	var f := _flock_of[i]
	# The first bird to look finds the flock's new place; the rest follow it.
	if _flock_centre_is_old(f):
		if _probes_left < 2:
			return
		var ahead := Vector3(_vel[i].x, 0.0, _vel[i].z).normalized()
		var guess := _pos[i] + ahead * _rng.randf_range(10.0, 22.0)
		var spot: Variant = _surface(guess, player.y)
		if spot == null or (spot as Vector3).distance_to(player) < ROOST_NEAR_M:
			for k in FLOCK_MAX:
				var j := f * FLOCK_MAX + k
				if _active[j] == 1 and _state[j] == FLUSHED:
					_state[j] = LEAVING
			return
		_flock_centre[f] = spot
		_flock_live[f] = 2
	if _probes_left < 1:
		return
	var foot: Variant = _surface(_flock_centre[f] + Vector3(_rng.randf_range(-1.1, 1.1), 0.0,
		_rng.randf_range(-1.1, 1.1)), _flock_centre[f].y)
	if foot == null or absf((foot as Vector3).y - _flock_centre[f].y) > 0.6:
		foot = _flock_centre[f]
	_target[i] = foot
	_state[i] = RETURNING


# A flock's centre is still where it stood until its first bird finds a new
# place (live 2 means it has one).
func _flock_centre_is_old(f: int) -> bool:
	return _flock_live[f] == 1


func _glide_in(i: int, delta: float) -> void:
	var to := _target[i] + Vector3.UP * 0.02 - _pos[i]
	var d := to.length()
	var speed := clampf(d * 1.2, 1.2, 7.0)
	_vel[i] = _vel[i].lerp(to.normalized() * speed, 1.0 - exp(-3.0 * delta))
	_pos[i] += _vel[i] * delta
	# Gliding most of the way; a flurry of beats to brake at the end.
	_flap[i] = 1.0 if d < 2.5 else 0.15
	_phase[i] += delta * (34.0 if d < 2.5 else 6.0)
	_face(i, delta)
	if d < 0.12:
		_pos[i] = _target[i]
		_vel[i] = Vector3.ZERO
		_state[i] = PERCHED
		_flap[i] = 0.0
		_timer[i] = _rng.randf_range(0.5, 2.0)
		landings += 1
		var f := _flock_of[i]
		if _flock_live[f] == 2 and _flock_perched(f):
			_flock_live[f] = 1


func _leave(i: int, delta: float, player: Vector3) -> void:
	_flap[i] = 0.8
	_phase[i] += delta * 24.0
	_vel[i].y = lerpf(_vel[i].y, 2.0, 1.0 - exp(-1.0 * delta))
	_pos[i] += _vel[i] * delta
	_face(i, delta)
	if _pos[i].distance_to(player) > FORGET_M:
		_active[i] = 0
		var f := _flock_of[i]
		var any := false
		for k in FLOCK_MAX:
			any = any or _active[f * FLOCK_MAX + k] == 1
		if not any:
			_flock_live[f] = 0


# Standing: a peck, a shuffle, a turn, now and then a short hop.
func _perch(i: int, delta: float) -> void:
	_timer[i] -= delta
	_flap[i] = maxf(0.0, _flap[i] - delta * 4.0)
	_phase[i] += delta * 30.0
	if _timer[i] <= 0.0:
		_timer[i] = _rng.randf_range(0.4, 2.4)
		var roll := _rng.randf()
		if roll < 0.35:
			_yaw[i] += _rng.randf_range(-1.2, 1.2)
		elif roll < 0.45 and _probes_left > 0:
			var step := Vector3(sin(_yaw[i]), 0.0, cos(_yaw[i])) * _rng.randf_range(0.15, 0.4)
			var foot: Variant = _surface(_pos[i] + step, _pos[i].y)
			if foot != null and absf((foot as Vector3).y - _pos[i].y) < 0.15:
				_pos[i] = foot
				_flap[i] = 0.6


func _face(i: int, delta: float) -> void:
	var flat := Vector2(_vel[i].x, _vel[i].z)
	if flat.length_squared() > 0.04:
		var want := atan2(flat.x, flat.y)
		_yaw[i] = lerp_angle(_yaw[i], want, 1.0 - exp(-8.0 * delta))


# --- gulls ---------------------------------------------------------------------


func _update_gulls(player: Vector3) -> void:
	var first := FLOCKS * FLOCK_MAX
	var want := player.y > GULL_FLOOR_M
	if want and (not _gull_live or _gull_centre.distance_to(player) > 90.0):
		_gull_centre = player + Vector3(_rng.randf_range(-30.0, 30.0), 0.0, _rng.randf_range(-30.0, 30.0))
		_gull_live = true
		for k in GULLS:
			var i := first + k
			_active[i] = 1
			_state[i] = LEAVING   # never perches; driven below
			_timer[i] = _rng.randf_range(25.0, 60.0)        # orbit radius
			_target[i] = Vector3(_rng.randf() * TAU, _rng.randf_range(-10.0, 25.0), _rng.randf_range(0.12, 0.25))
	elif not want and _gull_live:
		_gull_live = false
		for k in GULLS:
			_active[first + k] = 0
			_write(first + k)
	if not _gull_live:
		return
	var delta := get_process_delta_time()
	for k in GULLS:
		var i := first + k
		var orbit := _target[i]
		orbit.x += orbit.z * delta * (1.0 if k % 3 else -1.0)
		_target[i] = orbit
		var radius := _timer[i]
		var at := _gull_centre + Vector3(cos(orbit.x) * radius, orbit.y + sin(orbit.x * 2.0) * 3.0,
			sin(orbit.x) * radius)
		_vel[i] = (at - _pos[i]) / maxf(delta, 1.0e-3) if _pos[i] != Vector3.ZERO else Vector3.ZERO
		_pos[i] = at
		# Mostly gliding: a few beats on each lap.
		var beating := fmod(orbit.x, TAU) < 0.6
		_flap[i] = 0.9 if beating else 0.08
		_phase[i] += delta * (10.0 if beating else 2.0)
		_yaw[i] = atan2(_vel[i].x, _vel[i].z) if _vel[i].length_squared() > 0.01 else _yaw[i]
		_write(i)


# --- drawing --------------------------------------------------------------------


func _write(i: int) -> void:
	if _active[i] == 0:
		_multimesh.set_instance_transform(i, Transform3D(Basis().scaled(Vector3.ZERO), Vector3.ZERO))
		return
	var gull := _flock_of[i] < 0
	var scale := 2.2 if gull else 1.0
	var basis := Basis(Vector3.UP, _yaw[i])
	if _state[i] == PERCHED:
		# A peck: the body tips nose-down now and then.
		basis = basis * Basis(Vector3.RIGHT, maxf(0.0, sin(_phase[i] * 0.21)) * 0.5)
	else:
		var climb := atan2(_vel[i].y, Vector2(_vel[i].x, _vel[i].z).length() + 0.01)
		basis = basis * Basis(Vector3.RIGHT, -clampf(climb, -0.7, 0.7))
		if gull:
			basis = basis * Basis(Vector3.BACK, 0.35 * (1.0 if _flock_of[i] % 2 == 0 else -1.0))
	_multimesh.set_instance_transform(i, Transform3D(basis.scaled(Vector3.ONE * scale), _pos[i]))
	# Wings folded on the ground; spread and beating in the air.
	var spread := 0.0 if _state[i] == PERCHED and _flap[i] < 0.05 else 1.0
	_multimesh.set_instance_custom_data(i, Color(_phase[i], _flap[i], spread, 0.0))


func _surface(at: Vector3, near_y: float) -> Variant:
	if _probes_left <= 0:
		return null
	_probes_left -= 1
	var from := Vector3(at.x, near_y + ROOST_RISE_M, at.z)
	var y: float = _native.get_surface_below(from, ROOST_RISE_M * 2.0)
	if is_nan(y):
		return null
	return Vector3(at.x, y, at.z)


# A pigeon about 0.3 m long, along +z: a body, a head, a fan of tail, and two
# wings whose vertices the shader lifts by their distance from the body.
func _bird_mesh() -> ArrayMesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var body_top := Vector3(0.0, 0.11, 0.0)
	var nose := Vector3(0.0, 0.1, 0.13)
	var tail := Vector3(0.0, 0.085, -0.13)
	var belly := Vector3(0.0, 0.04, 0.0)
	var left := Vector3(-0.05, 0.08, 0.0)
	var right := Vector3(0.05, 0.08, 0.0)
	var head := Vector3(0.0, 0.15, 0.1)
	for tri in [[nose, left, body_top], [nose, body_top, right], [nose, belly, left], [nose, right, belly],
			[tail, body_top, left], [tail, right, body_top], [tail, left, belly], [tail, belly, right],
			[head, nose + Vector3(-0.025, 0.0, 0.0), nose + Vector3(0.025, 0.0, 0.0)],
			[head, body_top, nose + Vector3(-0.025, 0.0, 0.0)], [head, nose + Vector3(0.025, 0.0, 0.0), body_top],
			[tail, tail + Vector3(-0.04, 0.0, -0.06), tail + Vector3(0.04, 0.0, -0.06)]]:
		for v in tri:
			st.add_vertex(v)
	for side in [-1.0, 1.0]:
		var root_front := Vector3(0.045 * side, 0.1, 0.05)
		var root_back := Vector3(0.045 * side, 0.1, -0.04)
		var tip := Vector3(0.3 * side, 0.1, -0.03)
		var mid_front := Vector3(0.17 * side, 0.1, 0.04)
		for tri in [[root_front, mid_front, root_back], [mid_front, tip, root_back]]:
			for v in tri:
				st.add_vertex(v)
	st.generate_normals()
	return st.commit()


func _bird_shader() -> Shader:
	var shader := Shader.new()
	shader.code = """
shader_type spatial;
render_mode cull_disabled;

void vertex() {
	float out_from_body = max(abs(VERTEX.x) - 0.045, 0.0);
	float phase = INSTANCE_CUSTOM.r;
	float flap = INSTANCE_CUSTOM.g;
	float spread = INSTANCE_CUSTOM.b;
	// Folded: the wing laid along the body. Spread: out, beating about its root.
	float fold = 1.0 - spread;
	VERTEX.x = sign(VERTEX.x) * (0.045 + out_from_body * mix(1.0, 0.15, fold)) * step(0.001, abs(VERTEX.x))
		+ VERTEX.x * (1.0 - step(0.001, abs(VERTEX.x)));
	VERTEX.z -= out_from_body * fold * 0.6;
	VERTEX.y += out_from_body * (sin(phase) * 1.1 * flap + 0.2 * spread);
	// Darker flight feathers toward the tips.
	COLOR.rgb *= 1.0 - 0.45 * clamp(out_from_body / 0.25, 0.0, 1.0);
}

void fragment() {
	ALBEDO = COLOR.rgb;
	ROUGHNESS = 0.9;
	SPECULAR = 0.2;
}
"""
	return shader
