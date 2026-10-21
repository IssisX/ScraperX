extends Node3D
## Blood: pooled arterial sprays that pulse with a heartbeat and decay, drips,
## one-shot bursts, and splat/pool/smear quads laid on the surfaces they hit.
## Pools are fixed-size (Android): the oldest splat is recycled, sprays and
## drips are a handful of emitters re-pointed, never spawned per event.

const Kit := preload("res://presentation/life/gore/gore_kit.gd")

const SPRAYS := 4
const DRIPS := 4
const BURSTS := 2
var splat_cap := 72
const HEART_HZ := 1.75
const SPRAY_SECONDS := 7.0
const LOD_FAR_M := 28.0

## Ground height to splat on when nothing physical is hit (the deck the
## player is on); set each frame by the owner.
var ground_y := 0.0
var camera_position := Vector3.ZERO
var splats_laid := 0
var pulses := 0

class Spray:
	var particles: GPUParticles3D
	var process: ParticleProcessMaterial
	var follow: Node3D
	var dir_local := Vector3.UP
	var strength := 1.0
	var age := 0.0
	var duration := 0.0
	var last_beat := -1
	var active := false

var _sprays: Array[Spray] = []
var _drips: Array[Spray] = []
var _bursts: Array[GPUParticles3D] = []
var _burst_next := 0
var _splats: Array[MeshInstance3D] = []
var _splat_age: Array[float] = []
var _splat_next := 0
var _rng := RandomNumberGenerator.new()


func _ready() -> void:
	_rng.seed = 0xB100D
	for i in SPRAYS:
		_sprays.append(_make_emitter(220, 1.1, false))
	for i in DRIPS:
		_drips.append(_make_emitter(40, 1.6, true))
	for i in BURSTS:
		var b := _make_emitter(420, 1.4, false).particles
		b.one_shot = true
		b.explosiveness = 0.92
		b.emitting = false
		_bursts.append(b)
	for i in splat_cap:
		var m := MeshInstance3D.new()
		var q := QuadMesh.new()
		q.size = Vector2.ONE
		m.mesh = q
		m.material_override = Kit.splat_material(_rng.randf() * 100.0, 0.0)
		m.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		m.visible = false
		add_child(m)
		_splats.append(m)
		_splat_age.append(0.0)


func _make_emitter(amount: int, lifetime: float, drip: bool) -> Spray:
	var s := Spray.new()
	var p := GPUParticles3D.new()
	p.amount = amount
	p.lifetime = lifetime
	p.local_coords = false
	p.emitting = false
	p.fixed_fps = 0
	p.visibility_aabb = AABB(Vector3(-6, -12, -6), Vector3(12, 14, 12))
	var pm := ParticleProcessMaterial.new()
	pm.direction = Vector3.UP
	pm.spread = 6.0 if drip else 11.0
	pm.initial_velocity_min = 0.0 if drip else 3.2
	pm.initial_velocity_max = 0.4 if drip else 5.2
	pm.gravity = Vector3(0, -9.8, 0)
	pm.damping_min = 0.0
	pm.damping_max = 0.6
	pm.scale_min = 0.5
	pm.scale_max = 1.3
	var curve := Curve.new()
	curve.add_point(Vector2(0.0, 0.7))
	curve.add_point(Vector2(0.2, 1.0))
	curve.add_point(Vector2(1.0, 0.35))
	var ct := CurveTexture.new()
	ct.curve = curve
	pm.scale_curve = ct
	var ramp := Gradient.new()
	ramp.set_color(0, Kit.BLOOD_BRIGHT)
	ramp.set_color(1, Kit.BLOOD * 0.8)
	var gt := GradientTexture1D.new()
	gt.gradient = ramp
	pm.color_ramp = gt
	pm.particle_flag_align_y = true
	pm.emission_shape = ParticleProcessMaterial.EMISSION_SHAPE_SPHERE
	pm.emission_sphere_radius = 0.012 if not drip else 0.03
	p.process_material = pm
	var quad := QuadMesh.new()
	quad.size = Vector2(0.011, 0.034) if not drip else Vector2(0.009, 0.022)
	quad.material = Kit.material("blood_particle")
	p.draw_pass_1 = quad
	p.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(p)
	s.particles = p
	s.process = pm
	return s


## A cut artery on `follow`, spraying along its local `dir_local`, at
## `strength` (1 = an arm; legs more), pulsing for SPRAY_SECONDS and
## fading out.
func spray(follow: Node3D, dir_local: Vector3, strength: float = 1.0) -> void:
	var s := _free(_sprays)
	s.follow = follow
	s.dir_local = dir_local.normalized()
	s.strength = strength
	s.age = 0.0
	s.duration = SPRAY_SECONDS * clampf(strength, 0.6, 1.6)
	s.last_beat = -1
	s.active = true
	s.particles.emitting = true
	s.particles.restart()


## Slow drips off `follow` for `seconds`.
func drip(follow: Node3D, dir_local: Vector3, seconds: float) -> void:
	var s := _free(_drips)
	s.follow = follow
	s.dir_local = dir_local.normalized()
	s.strength = 1.0
	s.age = 0.0
	s.duration = seconds
	s.active = true
	s.particles.emitting = true


## One explosive burst (an impact): `dir` is the general throw (up off a
## surface), `force` scales speed and the splat ring.
func burst(at: Vector3, dir: Vector3, force: float) -> void:
	var b := _bursts[_burst_next]
	_burst_next = (_burst_next + 1) % _bursts.size()
	b.global_position = at
	var pm: ParticleProcessMaterial = b.process_material
	pm.direction = dir.normalized()
	pm.spread = 70.0
	pm.initial_velocity_min = 1.5 * force
	pm.initial_velocity_max = 4.5 * force
	b.amount_ratio = 1.0
	b.restart()
	b.emitting = true


func _free(pool: Array[Spray]) -> Spray:
	var oldest := pool[0]
	for s in pool:
		if not s.active:
			return s
		if s.age > oldest.age:
			oldest = s
	return oldest


## Lay a splat at `at` on a surface with `normal`. smear > 0 stretches it
## along `along` (a drag); size in metres.
func splat(at: Vector3, normal: Vector3, size: float, smear: float = 0.0, along: Vector3 = Vector3.ZERO) -> MeshInstance3D:
	var m := _splats[_splat_next]
	_splat_age[_splat_next] = 0.0
	_splat_next = (_splat_next + 1) % _splats.size()
	var n := normal.normalized() if normal.length_squared() > 1.0e-6 else Vector3.UP
	var x := along - n * along.dot(n)
	if x.length_squared() < 1.0e-6:
		x = n.cross(Vector3.RIGHT if absf(n.dot(Vector3.RIGHT)) < 0.9 else Vector3.FORWARD)
	x = x.normalized()
	var y := n.cross(x).normalized()
	# QuadMesh faces +z: z = the surface normal.
	var basis := Basis(x * size * (1.0 + 1.6 * smear), y * size, n)
	m.global_transform = Transform3D(basis, at + n * (0.004 + 0.0004 * float(_splat_next % 8)))
	var mat: ShaderMaterial = m.material_override
	mat.set_shader_parameter("seed", _rng.randf() * 100.0)
	mat.set_shader_parameter("smear", smear)
	mat.set_shader_parameter("age", 0.0)
	mat.set_shader_parameter("alpha_scale", 1.0)
	m.visible = true
	splats_laid += 1
	return m


## Where a drop thrown from `from` at `v` lands: a physics hit if the world
## has colliders, else the ground plane. Returns [position, normal] or [].
func land(from: Vector3, v: Vector3) -> Array:
	var space := get_world_3d().direct_space_state if is_inside_tree() else null
	var p := from
	var vel := v
	var dt := 0.04
	for i in 60:
		var next := p + vel * dt
		vel.y -= 9.8 * dt
		if space != null:
			var q := PhysicsRayQueryParameters3D.create(p, next)
			var hit := space.intersect_ray(q)
			if not hit.is_empty():
				return [hit["position"], hit["normal"]]
		if next.y <= ground_y:
			var t := (p.y - ground_y) / maxf(p.y - next.y, 1.0e-5)
			return [p.lerp(next, t) + Vector3(0, ground_y - p.lerp(next, t).y, 0), Vector3.UP]
		p = next
	return []


func _process(delta: float) -> void:
	for s in _sprays:
		_step_spray(s, delta, false)
	for s in _drips:
		_step_spray(s, delta, true)
	for i in _splats.size():
		if _splats[i].visible:
			_splat_age[i] += delta
			if int(_splat_age[i] * 2.0) != int((_splat_age[i] - delta) * 2.0):
				(_splats[i].material_override as ShaderMaterial).set_shader_parameter("age", _splat_age[i])


func _step_spray(s: Spray, delta: float, drip: bool) -> void:
	if not s.active:
		return
	s.age += delta
	if s.age >= s.duration or s.follow == null or not is_instance_valid(s.follow) or not s.follow.is_inside_tree():
		s.active = false
		s.particles.emitting = false
		return
	var xf := s.follow.global_transform
	var dir := (xf.basis * s.dir_local).normalized()
	s.particles.global_position = xf.origin
	s.process.direction = dir
	var far := xf.origin.distance_to(camera_position) > LOD_FAR_M
	var fade := clampf(1.0 - s.age / s.duration, 0.0, 1.0)
	if drip:
		s.particles.amount_ratio = 0.0 if far else clampf(fade * 2.0, 0.0, 1.0)
		return
	# The heartbeat: a sharp systolic jet then a weak run between, the jets
	# weakening as the pressure falls.
	var phase := fmod(s.age * HEART_HZ, 1.0)
	var jet := pow(maxf(0.0, sin(phase * PI / 0.3)), 2.0) if phase < 0.3 else 0.12
	var pressure := fade * fade
	s.particles.amount_ratio = 0.0 if far else clampf((0.15 + 0.85 * jet) * pressure, 0.0, 1.0)
	var speed := (2.0 + 4.8 * jet) * pressure * s.strength
	s.process.initial_velocity_min = speed * 0.7
	s.process.initial_velocity_max = speed * 1.15
	var beat := int(s.age * HEART_HZ)
	if beat != s.last_beat and phase > 0.12:
		s.last_beat = beat
		pulses += 1
		if pressure > 0.05:
			var hit := land(xf.origin, dir * speed)
			if not hit.is_empty():
				splat(hit[0], hit[1], 0.18 + 0.35 * pressure * s.strength, 0.25, dir)
