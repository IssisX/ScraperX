extends RefCounted
# Every sound the presentation plays, synthesised once at startup from
# seeded noise, filters and decaying partials -- no audio files ship. The
# seed is fixed, so the bank is identical on every run and every device.
#
# Voiced for a phone speaker first. Those reproduce little under ~300 Hz, so
# every clip carries its identity in the 300 Hz - 6 kHz band -- the grit of a
# boot, the ring of steel, the hiss of air -- and keeps its low thump only
# as weight for headphones. (Measured: the first bank put its energy in
# 50-90 Hz thumps and hum; above 300 Hz a walk peaked at -27.6 dBFS and the
# ambience sat at -48.6 dBFS, which on a phone is next to silence.)
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
	clips[&"rustle"] = _variants(2, _rustle)
	clips[&"chute"] = [_wav(_chute())]
	clips[&"impact_lethal"] = [_wav(_impact_lethal())]
	clips[&"ui_tap"] = [_wav(_blip(2600.0, 0.03))]
	clips[&"ui_back"] = [_wav(_blip(1500.0, 0.04))]
	clips[&"clang"] = _variants(3, _clang)
	clips[&"creak"] = _variants(2, _creak)
	clips[&"bird"] = _variants(3, _bird)
	clips[&"wind_loop"] = [_loop(_wind(4.5), 0.5)]
	clips[&"rush_loop"] = [_loop(_rush(2.4), 0.4)]
	clips[&"hum_loop"] = [_loop(_hum(2.0), 0.3)]
	clips[&"drone_loop"] = [_loop(_drone(3.6), 0.6)]
	clips[&"hiss_loop"] = [_loop(_hiss(1.6), 0.3)]
	clips[&"rattle_loop"] = [_loop(_rattle(1.4), 0.2)]
	clips[&"motor_loop"] = [_loop(_motor(1.8), 0.3)]
	build_msec = Time.get_ticks_msec() - started


func pick(name: StringName) -> AudioStreamWAV:
	var list: Array = clips.get(name, [])
	if list.is_empty():
		return null
	return list[_rng.randi_range(0, list.size() - 1)]


# --- clips -------------------------------------------------------------------


# Boot on concrete: a heel click, a short knock, gritty scuff, a little weight.
func _step_concrete() -> PackedFloat32Array:
	var out := _silence(0.15)
	_add_band_noise(out, 0.0, 0.02, 0.9, 0.004, 1500.0, 6000.0)
	_add_tone(out, _rng.randf_range(330.0, 420.0), 0.0, 0.05, 0.22, 0.008)
	_add_band_noise(out, 0.006, 0.13, 0.75, 0.035, _rng.randf_range(500.0, 700.0),
		_rng.randf_range(2600.0, 3600.0))
	_add_tone(out, _rng.randf_range(62.0, 78.0), 0.0, 0.08, 0.35, 0.02)
	return out


# Boot on steel plate: the click and a bright inharmonic ring off the plate.
func _step_metal() -> PackedFloat32Array:
	var out := _silence(0.26)
	_add_band_noise(out, 0.0, 0.03, 0.8, 0.005, 1200.0, 7000.0)
	_add_tone(out, _rng.randf_range(80.0, 96.0), 0.0, 0.07, 0.3, 0.02)
	var base := _rng.randf_range(380.0, 460.0)
	for pair in [[1.0, 0.22], [2.71, 0.16], [5.18, 0.1], [8.4, 0.05]]:
		_add_tone(out, base * pair[0] * _rng.randf_range(0.97, 1.03), 0.0, 0.26, pair[1],
			0.11 / sqrt(pair[0]))
	return out


# Meadow: a soft crunch of grit and stalks.
func _step_earth() -> PackedFloat32Array:
	var out := _silence(0.16)
	_add_band_noise(out, 0.0, 0.16, 0.7, 0.04, 300.0, 2200.0)
	for i in 9:
		var at := _rng.randf_range(0.0, 0.08)
		_add_band_noise(out, at, 0.006, 0.4, 0.002, 2500.0, 7000.0)
	_add_tone(out, 58.0, 0.0, 0.06, 0.25, 0.02)
	return out


# Push-off: a cloth whoosh that swells and falls, and the shoe leaving.
func _jump() -> PackedFloat32Array:
	var out := _silence(0.24)
	var n := out.size()
	var band := _band_noise_buffer(n, _rng.randf_range(500.0, 700.0),
		_rng.randf_range(2200.0, 3000.0))
	for i in n:
		var t := float(i) / float(n)
		out[i] += band[i] * sin(PI * t) * 1.3
	_add_band_noise(out, 0.0, 0.03, 0.5, 0.008, 800.0, 4000.0)
	_add_tone(out, 70.0, 0.0, 0.05, 0.25, 0.015)
	return out


# Landing: weight through the legs, the soles slapping; `hard` is heavier.
func _land(hard: float) -> PackedFloat32Array:
	var length := lerpf(0.2, 0.34, hard)
	var out := _silence(length)
	_add_band_noise(out, 0.0, 0.03, lerpf(0.8, 1.0, hard), 0.006, 900.0, 6000.0)
	_add_tone(out, lerpf(260.0, 190.0, hard), 0.0, 0.08, lerpf(0.3, 0.4, hard), 0.012)
	_add_band_noise(out, 0.0, length, lerpf(0.6, 0.85, hard), lerpf(0.04, 0.07, hard), 250.0,
		lerpf(2500.0, 1800.0, hard))
	_add_tone(out, lerpf(62.0, 44.0, hard), 0.0, length, 0.5, lerpf(0.035, 0.07, hard))
	return out


# Palms slapping onto a steel lip.
func _grab() -> PackedFloat32Array:
	var out := _silence(0.18)
	_add_band_noise(out, 0.0, 0.012, 1.0, 0.003, 1500.0, 8000.0)
	var base := _rng.randf_range(820.0, 980.0)
	for ratio in [1.0, 2.43, 4.1]:
		_add_tone(out, base * ratio, 0.0, 0.18, 0.16 / ratio, 0.05)
	return out


# Boots and body dragging over an edge.
func _scrape() -> PackedFloat32Array:
	var out := _silence(0.36)
	var n := out.size()
	var band := _band_noise_buffer(n, 700.0, 2600.0)
	var grain := 0.0
	for i in n:
		var t := float(i) / float(n)
		if i % 90 == 0:
			grain = _rng.randf_range(0.4, 1.0)
		out[i] += band[i] * grain * sin(PI * t) * 1.6
	return out


# Clothing and kit shifting as the body folds down or straightens.
func _rustle() -> PackedFloat32Array:
	var out := _silence(0.2)
	var n := out.size()
	var band := _band_noise_buffer(n, 900.0, 4200.0)
	var grain := 0.0
	for i in n:
		var t := float(i) / float(n)
		if i % 60 == 0:
			grain = _rng.randf_range(0.2, 1.0)
		out[i] += band[i] * grain * pow(sin(PI * t), 1.5) * 1.2
	return out


# Canopy snapping open: a sharp crack then heavy flapping cloth.
func _chute() -> PackedFloat32Array:
	var out := _silence(0.7)
	_add_band_noise(out, 0.0, 0.02, 1.0, 0.006, 1500.0, 8000.0)
	var n := out.size()
	var band := _band_noise_buffer(n, 250.0, 2200.0)
	for i in n:
		var t := float(i) / float(MIX_RATE)
		var flap := 0.5 + 0.5 * sin(TAU * 11.0 * t)
		out[i] += band[i] * flap * exp(-t * 3.5) * 1.6
	return out


func _impact_lethal() -> PackedFloat32Array:
	var out := _silence(0.5)
	_add_tone(out, 36.0, 0.0, 0.5, 0.8, 0.12)
	_add_band_noise(out, 0.0, 0.5, 1.0, 0.08, 150.0, 2500.0)
	_add_band_noise(out, 0.01, 0.2, 0.6, 0.03, 1500.0, 6000.0)
	return out


func _blip(freq: float, length: float) -> PackedFloat32Array:
	var out := _silence(length)
	_add_tone(out, freq, 0.0, length, 0.35, length * 0.3)
	_add_band_noise(out, 0.0, 0.004, 0.2, 0.001, 3000.0, 9000.0)
	return out


# A heavy steel mass striking steel: the ballast landing on the tipper, the
# return basin, the scoop. Inharmonic partials of a thick plate, long ring.
func _clang() -> PackedFloat32Array:
	var out := _silence(0.9)
	_add_band_noise(out, 0.0, 0.02, 1.0, 0.004, 800.0, 8000.0)
	var base := _rng.randf_range(190.0, 260.0)
	for pair in [[1.0, 0.45, 0.35], [2.76, 0.35, 0.28], [5.40, 0.22, 0.2], [8.93, 0.12, 0.12],
			[13.3, 0.06, 0.08]]:
		_add_tone(out, base * pair[0] * _rng.randf_range(0.98, 1.02), 0.0, 0.9, pair[1], pair[2])
	_add_tone(out, base * 0.5, 0.0, 0.3, 0.3, 0.06)
	return out


# A loaded hinge turning: stick-slip pulses through a wooden-steel body.
func _creak() -> PackedFloat32Array:
	var out := _silence(0.5)
	var n := out.size()
	var rate := _rng.randf_range(38.0, 60.0)
	var body := _rng.randf_range(620.0, 900.0)
	var phase := 0.0
	var ring := 0.0
	var ring_v := 0.0
	var w := TAU * body / float(MIX_RATE)
	for i in n:
		var t := float(i) / float(n)
		phase += (rate * (0.8 + 0.4 * t)) / float(MIX_RATE)
		var pulse := 0.0
		if phase >= 1.0:
			phase -= 1.0
			pulse = _rng.randf_range(0.6, 1.0)
		# A damped resonator struck by each slip.
		ring_v += pulse * 0.3 - w * w * ring - 0.02 * ring_v
		ring += ring_v
		out[i] += ring * sin(PI * t) * 0.9
	return out


# A small bird in the meadow conifers: two to four quick falling whistles.
func _bird() -> PackedFloat32Array:
	var out := _silence(0.7)
	var at := 0.0
	var notes := _rng.randi_range(2, 4)
	var top := _rng.randf_range(3800.0, 5200.0)
	for k in notes:
		var length := _rng.randf_range(0.05, 0.09)
		var first := int(at * MIX_RATE)
		var last := mini(out.size(), first + int(length * MIX_RATE))
		var phase := 0.0
		for i in range(first, last):
			var u := float(i - first) / float(last - first)
			var freq := lerpf(top, top * 0.62, u) * (1.0 + 0.03 * sin(TAU * 38.0 * u))
			phase += TAU * freq / float(MIX_RATE)
			out[i] += sin(phase) * sin(PI * u) * 0.5
		at += length + _rng.randf_range(0.04, 0.09)
		top *= _rng.randf_range(0.9, 1.08)
	return out


# Mountain wind: broad air with slow gusts, and a thin whistle that wanders
# with them.
func _wind(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var n := out.size()
	var air := _band_noise_buffer(n, 160.0, 1400.0)
	var whistle := _resonant_noise_buffer(n, 780.0, 0.25, 14.0)
	var gust := 0.6
	var gust_target := 0.6
	for i in n:
		if i % 2205 == 0:
			gust_target = _rng.randf_range(0.35, 1.0)
		gust += 0.0004 * (gust_target - gust)
		out[i] = (air[i] * 2.2 + whistle[i] * 0.5 * gust) * gust
	return out


# Air tearing past a falling body: brighter, steadier.
func _rush(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var band := _band_noise_buffer(out.size(), 500.0, 3800.0)
	for i in out.size():
		out[i] = band[i] * 1.4
	return out


# Motor and gearbox at the plant: mains hum for weight, the motor's slot
# whine and bearing noise so it carries on a small speaker.
func _hum(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	for pair in [[50.0, 0.3], [100.0, 0.2], [150.0, 0.1], [300.0, 0.06], [600.0, 0.08],
			[1200.0, 0.05], [1800.0, 0.025]]:
		_add_tone(out, pair[0], 0.0, length, pair[1], 0.0)
	var bearing := _band_noise_buffer(out.size(), 1800.0, 4500.0)
	for i in out.size():
		out[i] += bearing[i] * 0.25
	return out


# The yard itself, heard from anywhere in it: a far-off industrial bed of
# rumble, distant machinery tones beating slowly against each other.
func _drone(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var n := out.size()
	var bed := _band_noise_buffer(n, 90.0, 900.0)
	var swell := 0.7
	var swell_target := 0.7
	for i in n:
		if i % 4410 == 0:
			swell_target = _rng.randf_range(0.55, 1.0)
		swell += 0.00025 * (swell_target - swell)
		out[i] = bed[i] * 1.8 * swell
	# Tones beating slowly: each partial's level swings at its own rate.
	for pair in [[220.0, 0.05, 0.56], [331.0, 0.04, 0.28], [587.0, 0.02, 0.83]]:
		var tone := _silence(length)
		_add_tone(tone, pair[0], 0.0, length, pair[1], 0.0)
		var swing := _silence(length)
		_add_tone(swing, pair[2], 0.0, length, 0.4, 0.0)
		for i in n:
			out[i] += tone[i] * (0.6 + swing[i])
	return out


# Steam venting through the plant's orifice.
func _hiss(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var band := _band_noise_buffer(out.size(), 1800.0, 8500.0)
	for i in out.size():
		out[i] = band[i] * 2.2 * (0.9 + 0.1 * sin(TAU * 7.0 * float(i) / float(MIX_RATE)))
	return out


# Hoist chain running over its sprocket: links clicking, each one ringing.
func _rattle(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var at := 0.0
	while at < length - 0.02:
		_add_band_noise(out, at, 0.012, _rng.randf_range(0.4, 0.8), 0.002, 1800.0, 6500.0)
		_add_tone(out, _rng.randf_range(1400.0, 2600.0), at, 0.03, 0.12, 0.008)
		at += _rng.randf_range(0.022, 0.045)
	return out


# The lift's drive: a geared motor under load.
func _motor(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	for k in range(1, 6):
		_add_tone(out, 110.0 * float(k), 0.0, length, 0.18 / float(k), 0.0)
	var grind := _band_noise_buffer(out.size(), 400.0, 1600.0)
	for i in out.size():
		out[i] += grind[i] * 0.5
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
	var env := gain * 2.0
	var decay := _decay(tau)
	for i in range(first, last):
		lp += a * (_rng.randf_range(-1.0, 1.0) - lp)
		out[i] += lp * env
		env *= decay


# Noise between two corner frequencies (a low-pass minus a lower low-pass,
# each two poles), decaying with `tau`.
func _add_band_noise(out: PackedFloat32Array, start: float, length: float, gain: float,
		tau: float, low_hz: float, high_hz: float) -> void:
	var first := int(start * MIX_RATE)
	var last := mini(out.size(), first + int(length * MIX_RATE))
	if last <= first:
		return
	var band := _band_noise_buffer(last - first, low_hz, high_hz)
	var env := gain * 2.0
	var decay := _decay(tau)
	for i in range(first, last):
		out[i] += band[i - first] * env
		env *= decay


func _band_noise_buffer(count: int, low_hz: float, high_hz: float) -> PackedFloat32Array:
	var out := PackedFloat32Array()
	out.resize(count)
	var a_hi := _alpha(high_hz)
	var a_lo := _alpha(low_hz)
	var h1 := 0.0
	var h2 := 0.0
	var l1 := 0.0
	var l2 := 0.0
	for i in count:
		var x := _rng.randf_range(-1.0, 1.0)
		h1 += a_hi * (x - h1)
		h2 += a_hi * (h1 - h2)
		l1 += a_lo * (x - l1)
		l2 += a_lo * (l1 - l2)
		out[i] = h2 - l2
	return out


# Noise through a narrow resonance whose centre drifts by +/- `wander` of
# itself at `drift_hz` cycles over the buffer's own length.
func _resonant_noise_buffer(count: int, center_hz: float, wander: float,
		drift_cycles: float) -> PackedFloat32Array:
	var out := PackedFloat32Array()
	out.resize(count)
	var low := 0.0
	var band := 0.0
	var q := 0.08
	for i in count:
		var u := float(i) / float(count)
		var f := center_hz * (1.0 + wander * sin(TAU * drift_cycles * u))
		var k := 2.0 * sin(PI * f / float(MIX_RATE))
		var x := _rng.randf_range(-1.0, 1.0)
		low += k * band
		var high := x - low - q * band
		band += k * high
		out[i] = band * 0.2
	return out


# A sine partial; tau 0 holds it at constant level. The sine runs as the
# two-term recurrence s[n+1] = 2 cos(w) s[n] - s[n-1] and the envelope as a
# running product: no sin or exp per sample, which is most of the bank's
# build time in GDScript.
func _add_tone(out: PackedFloat32Array, freq: float, start: float, length: float, gain: float,
		tau: float) -> void:
	var first := int(start * MIX_RATE)
	var last := mini(out.size(), first + int(length * MIX_RATE))
	var step := TAU * freq / float(MIX_RATE)
	var coupling := 2.0 * cos(step)
	var previous := -sin(step)
	var current := 0.0
	var env := gain
	var decay := _decay(tau)
	for i in range(first, last):
		out[i] += current * env
		var next := coupling * current - previous
		previous = current
		current = next
		env *= decay


# Per-sample factor of an exponential decay with time constant `tau`
# seconds; tau 0 means no decay.
func _decay(tau: float) -> float:
	return 1.0 if tau <= 0.0 else exp(-1.0 / (tau * float(MIX_RATE)))


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
