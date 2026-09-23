extends RefCounted
# Every sound the presentation plays, synthesised once at startup from
# seeded noise, filters and decaying partials -- no audio files ship. The
# seed is fixed, so the bank is identical on every run and every device.
#
# Not here, on purpose: the human fear voice that Governing Law 8 / GDD 8.4
# require for large falls. Screams, gasps and panicked swearing cannot be
# synthesised credibly; they need recorded performances.

const MIX_RATE := 22050
const STEP_VARIANTS := 4

var clips := {}   # StringName -> Array[AudioStreamWAV] (variants)
var build_msec := 0

var _rng := RandomNumberGenerator.new()


func build() -> void:
	var started := Time.get_ticks_msec()
	_rng.seed = 0x5C4A9E
	clips[&"step_concrete"] = _variants(STEP_VARIANTS, _step_concrete)
	clips[&"step_metal"] = _variants(STEP_VARIANTS, _step_metal)
	clips[&"step_earth"] = _variants(STEP_VARIANTS, _step_earth)
	clips[&"jump"] = _variants(2, _jump)
	clips[&"land_soft"] = _variants(2, _land.bind(0.0))
	clips[&"land_hard"] = _variants(2, _land.bind(1.0))
	clips[&"grab"] = _variants(3, _grab)
	clips[&"scrape"] = _variants(2, _scrape)
	clips[&"chute"] = [_wav(_chute())]
	clips[&"impact_lethal"] = [_wav(_impact_lethal())]
	clips[&"ui_tap"] = [_wav(_blip(2600.0, 0.03))]
	clips[&"ui_back"] = [_wav(_blip(1500.0, 0.04))]
	clips[&"wind_loop"] = [_loop(_wind(4.5), 0.5)]
	clips[&"rush_loop"] = [_loop(_rush(2.4), 0.4)]
	clips[&"hum_loop"] = [_loop_exact(_hum(2.0))]
	build_msec = Time.get_ticks_msec() - started


func pick(name: StringName) -> AudioStreamWAV:
	var list: Array = clips.get(name, [])
	if list.is_empty():
		return null
	return list[_rng.randi_range(0, list.size() - 1)]


# --- clips -------------------------------------------------------------------


# Boot on concrete: a dull heel thump under a short gritty scuff.
func _step_concrete() -> PackedFloat32Array:
	var out := _silence(0.11)
	var lp := _rng.randf_range(1000.0, 1500.0)
	_add_noise(out, 0.0, 0.11, 0.55, 0.018, lp)
	_add_tone(out, _rng.randf_range(62.0, 78.0), 0.0, 0.08, 0.7, 0.022)
	return out


# Boot on steel plate: the thump plus a faint inharmonic ring.
func _step_metal() -> PackedFloat32Array:
	var out := _silence(0.22)
	_add_noise(out, 0.0, 0.06, 0.45, 0.012, 2600.0)
	_add_tone(out, _rng.randf_range(80.0, 96.0), 0.0, 0.07, 0.55, 0.02)
	var base := _rng.randf_range(380.0, 460.0)
	for ratio in [1.0, 2.71, 5.18]:
		_add_tone(out, base * ratio * _rng.randf_range(0.97, 1.03), 0.0, 0.22, 0.07 / ratio, 0.09)
	return out


# Meadow: softer, darker, a little crackle of grit and stalks.
func _step_earth() -> PackedFloat32Array:
	var out := _silence(0.14)
	_add_noise(out, 0.0, 0.14, 0.5, 0.03, 650.0)
	for i in 6:
		var at := _rng.randf_range(0.0, 0.06)
		_add_noise(out, at, 0.004, 0.25, 0.002, 3200.0)
	return out


# Push-off: a cloth whoosh that swells and falls.
func _jump() -> PackedFloat32Array:
	var out := _silence(0.2)
	var n := out.size()
	var lp := 0.0
	var cutoff := _alpha(_rng.randf_range(1400.0, 1900.0))
	for i in n:
		var t := float(i) / float(n)
		lp += cutoff * (_rng.randf_range(-1.0, 1.0) - lp)
		out[i] += lp * sin(PI * t) * 0.6
	_add_tone(out, 70.0, 0.0, 0.05, 0.35, 0.015)
	return out


# Landing: weight arriving through the legs; `hard` deepens and lengthens it.
func _land(hard: float) -> PackedFloat32Array:
	var length := lerpf(0.18, 0.3, hard)
	var out := _silence(length)
	_add_tone(out, lerpf(62.0, 44.0, hard), 0.0, length, 0.9, lerpf(0.035, 0.07, hard))
	_add_noise(out, 0.0, length, lerpf(0.4, 0.65, hard), lerpf(0.03, 0.05, hard), lerpf(900.0, 700.0, hard))
	return out


# Palms slapping onto a steel lip.
func _grab() -> PackedFloat32Array:
	var out := _silence(0.16)
	_add_noise(out, 0.0, 0.01, 0.8, 0.003, 5000.0)
	var base := _rng.randf_range(820.0, 980.0)
	for ratio in [1.0, 2.43, 4.1]:
		_add_tone(out, base * ratio, 0.0, 0.16, 0.12 / ratio, 0.05)
	return out


# Boots and body dragging over an edge.
func _scrape() -> PackedFloat32Array:
	var out := _silence(0.36)
	var n := out.size()
	var lp := 0.0
	var hp := 0.0
	var a_lp := _alpha(2600.0)
	var a_hp := _alpha(700.0)
	var grain := 0.0
	for i in n:
		var t := float(i) / float(n)
		var x := _rng.randf_range(-1.0, 1.0)
		lp += a_lp * (x - lp)
		hp += a_hp * (lp - hp)
		if i % 90 == 0:
			grain = _rng.randf_range(0.4, 1.0)
		out[i] += (lp - hp) * grain * sin(PI * t) * 0.9
	return out


# Canopy snapping open: a sharp crack then heavy flapping cloth.
func _chute() -> PackedFloat32Array:
	var out := _silence(0.7)
	_add_noise(out, 0.0, 0.02, 0.9, 0.006, 4000.0)
	var n := out.size()
	var lp := 0.0
	var a := _alpha(900.0)
	for i in n:
		var t := float(i) / float(MIX_RATE)
		lp += a * (_rng.randf_range(-1.0, 1.0) - lp)
		var flap := 0.5 + 0.5 * sin(TAU * 11.0 * t)
		out[i] += lp * flap * exp(-t * 3.5) * 0.9
	return out


func _impact_lethal() -> PackedFloat32Array:
	var out := _silence(0.5)
	_add_tone(out, 36.0, 0.0, 0.5, 1.0, 0.12)
	_add_noise(out, 0.0, 0.5, 0.8, 0.08, 600.0)
	_add_noise(out, 0.01, 0.2, 0.4, 0.03, 2500.0)
	return out


func _blip(freq: float, length: float) -> PackedFloat32Array:
	var out := _silence(length)
	_add_tone(out, freq, 0.0, length, 0.35, length * 0.3)
	_add_noise(out, 0.0, 0.004, 0.2, 0.001, 6000.0)
	return out


# Mountain wind: dark noise with slow gusts.
func _wind(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var n := out.size()
	var lp1 := 0.0
	var lp2 := 0.0
	var a := _alpha(420.0)
	var gust := 0.6
	var gust_target := 0.6
	for i in n:
		if i % 2205 == 0:
			gust_target = _rng.randf_range(0.35, 1.0)
		gust += 0.0004 * (gust_target - gust)
		lp1 += a * (_rng.randf_range(-1.0, 1.0) - lp1)
		lp2 += a * (lp1 - lp2)
		out[i] = lp2 * 3.2 * gust
	return out


# Air tearing past a falling body: brighter, steadier.
func _rush(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var n := out.size()
	var lp := 0.0
	var hp := 0.0
	var a_lp := _alpha(3800.0)
	var a_hp := _alpha(500.0)
	for i in n:
		lp += a_lp * (_rng.randf_range(-1.0, 1.0) - lp)
		hp += a_hp * (lp - hp)
		out[i] = (lp - hp) * 0.8
	return out


# Motor and gearbox: mains fundamental, harmonics and a little grit. Whole
# cycles only, so the loop point is seamless without a crossfade.
func _hum(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	for pair in [[50.0, 0.35], [100.0, 0.22], [150.0, 0.12], [300.0, 0.05]]:
		_add_tone(out, pair[0], 0.0, length, pair[1], 0.0)
	var lp := 0.0
	var a := _alpha(1200.0)
	for i in out.size():
		lp += a * (_rng.randf_range(-1.0, 1.0) - lp)
		out[i] += lp * 0.08
	return out


# --- primitives --------------------------------------------------------------


func _variants(count: int, maker: Callable) -> Array:
	var list := []
	for i in count:
		list.append(_wav(maker.call()))
	return list


func _silence(seconds: float) -> PackedFloat32Array:
	var out := PackedFloat32Array()
	out.resize(int(seconds * MIX_RATE))
	return out


func _alpha(cutoff_hz: float) -> float:
	return 1.0 - exp(-TAU * cutoff_hz / float(MIX_RATE))


# Low-passed noise from `start` for `length` seconds, exponential decay `tau`.
func _add_noise(out: PackedFloat32Array, start: float, length: float, gain: float, tau: float,
		cutoff_hz: float) -> void:
	var first := int(start * MIX_RATE)
	var last := mini(out.size(), first + int(length * MIX_RATE))
	var a := _alpha(cutoff_hz)
	var lp := 0.0
	for i in range(first, last):
		var t := float(i - first) / float(MIX_RATE)
		lp += a * (_rng.randf_range(-1.0, 1.0) - lp)
		out[i] += lp * gain * exp(-t / tau) * 2.0


# A sine partial; tau 0 holds it at constant level.
func _add_tone(out: PackedFloat32Array, freq: float, start: float, length: float, gain: float,
		tau: float) -> void:
	var first := int(start * MIX_RATE)
	var last := mini(out.size(), first + int(length * MIX_RATE))
	var step := TAU * freq / float(MIX_RATE)
	for i in range(first, last):
		var k := float(i - first)
		var env := 1.0 if tau <= 0.0 else exp(-k / (tau * float(MIX_RATE)))
		out[i] += sin(step * k) * gain * env


func _wav(samples: PackedFloat32Array, loop_end: int = -1) -> AudioStreamWAV:
	var peak := 0.0
	for x in samples:
		peak = maxf(peak, absf(x))
	var scale := 0.9 / peak if peak > 0.9 else 1.0
	# A one-shot cut off mid-ring clicks (observed: grab ended at 0.73 of full
	# scale); the last 8 ms ramp it to silence. Loops are left whole.
	if loop_end <= 0:
		var ramp := mini(int(0.008 * MIX_RATE), samples.size() / 4)
		for k in ramp:
			samples[samples.size() - 1 - k] *= float(k) / float(ramp)
	var data := PackedByteArray()
	data.resize(samples.size() * 2)
	for i in samples.size():
		data.encode_s16(i * 2, int(clampf(samples[i] * scale, -1.0, 1.0) * 32767.0))
	var wav := AudioStreamWAV.new()
	wav.format = AudioStreamWAV.FORMAT_16_BITS
	wav.mix_rate = MIX_RATE
	wav.stereo = false
	wav.data = data
	if loop_end > 0:
		wav.loop_mode = AudioStreamWAV.LOOP_FORWARD
		wav.loop_begin = 0
		wav.loop_end = loop_end
	return wav


# Seamless loop: the tail is crossfaded into the head, then cut off.
func _loop(samples: PackedFloat32Array, fade_seconds: float) -> AudioStreamWAV:
	var fade := int(fade_seconds * MIX_RATE)
	var body := samples.size() - fade
	var out := samples.slice(0, body)
	for i in fade:
		var w := float(i) / float(fade)
		out[i] = out[i] * w + samples[body + i] * (1.0 - w)
	return _wav(out, out.size())


func _loop_exact(samples: PackedFloat32Array) -> AudioStreamWAV:
	return _wav(samples, samples.size())
