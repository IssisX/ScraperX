extends "res://presentation/audio/sound_bank.gd"
# Prototype of the Matron's own bank (spec: her soundscape). Built on a second
# SoundBank instance after the world's bank, with its own seed, so the world's
# clips and the CI gate on _bank_ready are untouched.

var cost := {}   # name -> msec, for the probe


func build_her() -> void:
	var started := Time.get_ticks_msec()
	_rng.seed = 0x4D4154
	var t := Time.get_ticks_usec()
	var bed := _her_bed(3.4)
	clips[&"matron_bed_loop"] = [_loop(bed, 0.4)]
	_rough(bed, 38.0, 0.6)
	clips[&"matron_bed_rough_loop"] = [_loop(bed, 0.4)]
	_mark(&"bed+rough", t)
	t = Time.get_ticks_usec()
	clips[&"matron_growl_loop"] = [_loop(_her_growl(false), 0.3)]
	_mark(&"matron_growl_loop", t)
	t = Time.get_ticks_usec()
	clips[&"matron_growl_rage_loop"] = [_loop(_her_growl(true), 0.25)]
	_mark(&"matron_growl_rage_loop", t)
	t = Time.get_ticks_usec()
	clips[&"matron_mouth_loop"] = [_loop(_her_mouth(2.0), 0.3)]
	_mark(&"matron_mouth_loop", t)
	for pair in [[&"matron_breath", 3, _her_breath], [&"matron_step", 3, _her_step],
			[&"matron_shriek", 2, _her_shriek], [&"matron_joints", 3, _her_joints],
			[&"matron_croak", 1, _her_croak], [&"matron_cluck", 1, _her_cluck],
			[&"matron_swallow", 1, _her_swallow], [&"matron_sniff", 1, _her_sniff],
			[&"matron_effort", 1, _her_effort], [&"matron_hit", 1, _her_hit],
			[&"matron_laugh", 1, _her_laugh], [&"matron_rip", 2, _her_rip]]:
		t = Time.get_ticks_usec()
		clips[pair[0]] = _variants(pair[1], pair[2])
		_mark(pair[0], t)
	t = Time.get_ticks_usec()
	clips[&"matron_strain_loop"] = [_loop(_her_strain(1.5), 0.25)]
	_mark(&"matron_strain_loop", t)
	build_msec = Time.get_ticks_msec() - started
	# Slice 2 (the fatality's own): timed apart.
	t = Time.get_ticks_usec()
	clips[&"matron_crunch"] = [_wav(_her_crunch())]
	_mark(&"slice2 matron_crunch", t)
	t = Time.get_ticks_usec()
	clips[&"matron_drag_loop"] = [_loop(_her_drag(2.1), 0.3)]
	_mark(&"slice2 matron_drag_loop", t)


func _mark(name: StringName, t0: int) -> void:
	cost[name] = float(Time.get_ticks_usec() - t0) / 1000.0


# --- her voice: the new primitives -------------------------------------------


# A throat: glottal pulses (a smoothstep opening over 60% of the period, a
# sharper closing over 20%, closed for the rest), differentiated as the lips
# radiate it, then tilted by a one-pole low-pass at `tilt_hz` (500 breathy,
# 2000 pressed). f0 glides exponentially f0_a -> f0_b. Per period: f0 x
# (1 +- jitter), amplitude x (1 +- shimmer), and every second period x
# (1 - sub) (period doubling: the growl). `chaos_per_s`: jumps of f0 x
# 0.7..1.3 for 40-80 ms. `breath`: noise 1-4 kHz gated by the open phase.
func _glottal(n: int, f0_a: float, f0_b: float, jitter: float, shimmer: float, sub: float,
		tilt_hz: float, chaos_per_s: float, breath: float) -> PackedFloat32Array:
	var out := PackedFloat32Array()
	out.resize(n)
	var asp := _band_noise_buffer(n, 1000.0, 4000.0) if breath > 0.0 else PackedFloat32Array()
	var a := _alpha(tilt_hz)
	var glide := pow(f0_b / f0_a, 1.0 / float(maxi(n, 1)))
	var f0 := f0_a
	var phase := 0.0
	var k := 0
	var f_mul := 1.0
	var amp := 1.0
	var chaos_left := 0
	var chaos_mul := 1.0
	var g_prev := 0.0
	var lp := 0.0
	for i in n:
		var f := f0 * f_mul * chaos_mul
		phase += f / float(MIX_RATE)
		if phase >= 1.0:
			phase -= 1.0
			k += 1
			f_mul = 1.0 + jitter * _rng.randf_range(-1.0, 1.0)
			amp = (1.0 + shimmer * _rng.randf_range(-1.0, 1.0)) * ((1.0 - sub) if k % 2 == 1 else 1.0)
			if chaos_left <= 0 and chaos_per_s > 0.0 and _rng.randf() < chaos_per_s / f:
				chaos_left = int(_rng.randf_range(0.04, 0.08) * MIX_RATE)
				chaos_mul = _rng.randf_range(0.7, 1.3)
		if chaos_left > 0:
			chaos_left -= 1
			if chaos_left == 0:
				chaos_mul = 1.0
		var g := 0.0
		if phase < 0.6:
			var s := phase / 0.6
			g = s * s * (3.0 - 2.0 * s)
		elif phase < 0.8:
			var s := (phase - 0.6) / 0.2
			g = 1.0 - s * s * (3.0 - 2.0 * s)
		g *= amp
		var d := (g - g_prev) * float(MIX_RATE) / (7.5 * f)
		g_prev = g
		lp += a * (d - lp)
		out[i] = lp + (asp[i] * breath * g if breath > 0.0 else 0.0)
		f0 *= glide
	return out


# A woman's vocal tract, three formants in parallel (Q 5/6/8, gains 1/0.6/0.35),
# scaled by `scale`, their Q by `q_mul`; normalized to a peak of 1.
# BIG (/ʌ/ x 0.85): 650/1190/2360. OPEN: 810/1160/2670. PRESSED: 400/1000/2400.
func _formants(src: PackedFloat32Array, f1: float, f2: float, f3: float, q_mul: float = 1.0) -> PackedFloat32Array:
	var c := PackedFloat32Array()   # per formant: b0, a1, a2, gain
	for spec in [[f1, 5.0, 1.0], [f2, 6.0, 0.6], [f3, 8.0, 0.35]]:
		var w := TAU * minf(float(spec[0]), 0.45 * MIX_RATE) / float(MIX_RATE)
		var alpha := sin(w) / (2.0 * float(spec[1]) * q_mul)
		var a0 := 1.0 + alpha
		c.append_array([alpha / a0, -2.0 * cos(w) / a0, (1.0 - alpha) / a0, float(spec[2])])
	var out := PackedFloat32Array()
	out.resize(src.size())
	var x1 := 0.0
	var x2 := 0.0
	var p1 := 0.0
	var p2 := 0.0
	var q1 := 0.0
	var q2 := 0.0
	var r1 := 0.0
	var r2 := 0.0
	var peak := 0.0
	for i in src.size():
		var x := src[i]
		var dx := x - x2
		var p := c[0] * dx - c[1] * p1 - c[2] * p2
		var q := c[4] * dx - c[5] * q1 - c[6] * q2
		var r := c[8] * dx - c[9] * r1 - c[10] * r2
		x2 = x1
		x1 = x
		p2 = p1
		p1 = p
		q2 = q1
		q1 = q
		r2 = r1
		r1 = r
		var y := p * c[3] + q * c[7] + r * c[11]
		out[i] = y
		peak = maxf(peak, absf(y))
	if peak > 0.0:
		var s := 1.0 / peak
		for i in out.size():
			out[i] *= s
	return out


func _peak(buf: PackedFloat32Array, to: float) -> void:
	var peak := 0.0
	for x in buf:
		peak = maxf(peak, absf(x))
	if peak > 0.0:
		var s := to / peak
		for i in buf.size():
			buf[i] *= s


# Soft clipping (symmetric: an asymmetric curve demodulates the flutter into
# the lows), in place, after a
# normalization to peak 1: the throat pushed past what it can carry.
func _saturate(buf: PackedFloat32Array, drive: float) -> void:
	_peak(buf, 1.0)
	var scale := 1.0 / tanh(drive)
	for i in buf.size():
		buf[i] = tanh(drive * buf[i]) * scale


# Roughness, in place: amplitude flutter, a triangle at `rate_hz` (each cycle
# +-20%), `depth` 0..1, over [from, to) samples (to < 0: the end).
func _rough(buf: PackedFloat32Array, rate_hz: float, depth: float, from: int = 0, to: int = -1) -> void:
	var ph := 0.0
	var r := rate_hz
	for i in range(from, buf.size() if to < 0 else mini(to, buf.size())):
		ph += r / float(MIX_RATE)
		if ph >= 1.0:
			ph -= 1.0
			r = rate_hz * _rng.randf_range(0.8, 1.2)
		buf[i] *= 1.0 - depth * absf(ph * 2.0 - 1.0)


# The input between two corners (the band noise's filter, on a signal).
func _bandpass(input: PackedFloat32Array, low_hz: float, high_hz: float) -> PackedFloat32Array:
	var out := PackedFloat32Array()
	out.resize(input.size())
	var a_hi := _alpha(high_hz)
	var a_lo := _alpha(low_hz)
	var h1 := 0.0
	var h2 := 0.0
	var l1 := 0.0
	var l2 := 0.0
	for i in input.size():
		var x := input[i]
		h1 += a_hi * (x - h1)
		h2 += a_hi * (h1 - h2)
		l1 += a_lo * (x - l1)
		l2 += a_lo * (l1 - l2)
		out[i] = h2 - l2
	return out


# Noise through a band-pass sweeping f_from -> f_to (exponential), damping
# 1/Q (its coefficient updated every 16 samples); tau 0 shapes it as one
# parabolic swell 4u(1-u), else an exponential decay.
func _add_glide(out: PackedFloat32Array, start: float, length: float, gain: float, f_from: float,
		f_to: float, damping: float, tau: float) -> void:
	var first := int(start * MIX_RATE)
	var last := mini(out.size(), first + int(length * MIX_RATE))
	if last <= first:
		return
	var count := last - first
	var step := pow(f_to / f_from, 1.0 / float(count))
	var f := f_from
	var low := 0.0
	var band := 0.0
	var env := gain
	var decay := _decay(tau)
	var norm := 0.6 * damping   # resonance gain is about 1/damping
	var k := 0.0
	for i in range(first, last):
		var j := i - first
		if j % 16 == 0:
			k = 2.0 * sin(PI * f / float(MIX_RATE))
		low += k * band
		var high := _rng.randf_range(-1.0, 1.0) - low - damping * band
		band += k * high
		var u := float(j) / float(count)
		var shape := 4.0 * u * (1.0 - u) if tau <= 0.0 else 1.0
		out[i] += band * norm * env * shape
		env *= decay
		f *= step


# Poisson noise bursts (crackle, mucus, fibers): rate ramps rate_a -> rate_b
# per second over the span; each burst ms_lo..ms_hi long in [low, high] Hz at
# a random level up to `gain`.
func _add_grains(out: PackedFloat32Array, start: float, length: float, rate_a: float, rate_b: float,
		ms_lo: float, ms_hi: float, low_hz: float, high_hz: float, gain: float) -> void:
	var t := start
	while t < start + length:
		var dur := _rng.randf_range(ms_lo, ms_hi) / 1000.0
		_add_band_noise(out, t, dur, gain * _rng.randf_range(0.25, 1.0), dur * 0.35, low_hz, high_hz)
		var rate := lerpf(rate_a, rate_b, (t - start) / length)
		t += -log(maxf(_rng.randf(), 0.0001)) / rate


# A bubble bursting in spit or blood: a sine 500-1500 Hz rising 30% over
# 15-40 ms, decaying with tau 10 ms.
func _add_bubble(out: PackedFloat32Array, start: float, gain: float) -> void:
	var f0 := _rng.randf_range(500.0, 1500.0)
	var rise := int(_rng.randf_range(0.015, 0.04) * MIX_RATE)
	var first := int(start * MIX_RATE)
	var last := mini(out.size(), first + rise + int(0.03 * MIX_RATE))
	var phase := 0.0
	var env := gain
	var decay := _decay(0.01)
	for i in range(first, last):
		var u := minf(1.0, float(i - first) / float(rise))
		phase += TAU * f0 * (1.0 + 0.3 * u) / float(MIX_RATE)
		out[i] += sin(phase) * env
		env *= decay


# Stick-slip: a click (+a, -0.75a, +0.5a, -0.25a) of random level 0.4..1 at
# intervals of 1/rate x 0.7..1.3, the rate ramping rate_a -> rate_b over the
# buffer. Run it through _resonate for a creak, a squeal, a grind.
func _slip(n: int, rate_a: float, rate_b: float) -> PackedFloat32Array:
	var out := PackedFloat32Array()
	out.resize(n)
	var i := 0
	while i < n:
		var a := _rng.randf_range(0.4, 1.0)
		for k in 4:
			if i + k < n:
				out[i + k] += a * (1.0 - 0.25 * float(k)) * (1.0 if k % 2 == 0 else -1.0)
		var rate := lerpf(rate_a, rate_b, float(i) / float(n))
		i += maxi(4, int(float(MIX_RATE) / (rate * _rng.randf_range(0.7, 1.3))))
	return out


# Weight a phone can carry: a thump at f0 with its harmonics 2..8 (gain
# 1/k^0.7), so a speaker that drops everything under 300 Hz still hears it.
func _add_thump(out: PackedFloat32Array, start: float, f0: float, gain: float, tau: float) -> void:
	for k in range(1, 9):
		_add_tone(out, f0 * float(k), start, tau * 6.0, gain / pow(float(k), 0.7), tau)


# --- her clips -----------------------------------------------------------------


# Far: the pressure she puts on the air. Saws at 146.67 and 151 Hz (4.33 Hz
# apart) and 207.33 Hz (a tritone up), each a whole number of cycles in the
# 3.0 s loop, driven (SAT 3), held to 450-2200 Hz, under air 400-1400 Hz
# that swells twice a loop like slow breathing.
func _her_bed(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	var n := out.size()
	var saws := PackedFloat32Array()
	saws.resize(n)
	var pa := _rng.randf()
	var pb := _rng.randf()
	var pc := _rng.randf()
	var ia := (440.0 / 3.0) / float(MIX_RATE)
	var ib := 151.0 / float(MIX_RATE)
	var ic := (622.0 / 3.0) / float(MIX_RATE)
	for i in n:
		pa = fmod(pa + ia, 1.0)
		pb = fmod(pb + ib, 1.0)
		pc = fmod(pc + ic, 1.0)
		saws[i] = (pa * 2.0 - 1.0) * 0.3 + (pb * 2.0 - 1.0) * 0.3 + (pc * 2.0 - 1.0) * 0.22
	_saturate(saws, 3.0)
	var body := _bandpass(_bandpass(saws, 450.0, 2200.0), 450.0, 2200.0)
	var air := _band_noise_buffer(n, 400.0, 1400.0)
	var cycle := int(1.5 * MIX_RATE)
	for i in n:
		var u := float(i % cycle) / float(cycle)
		var swell := 4.0 * u * (1.0 - u)
		out[i] = body[i] * (0.75 + 0.25 * swell) + air[i] * 1.4 * swell * swell
	return out


# Near: a breath cycle with the growl in it. A 1.5 s exhale through her
# throat (GLOT 102 Hz, jitter 6%, shimmer 25%, sub 0.5, tilt 1 kHz -> BIG ->
# SAT 4) and a wet snoring intake. Rage: a 1.0 s exhale (sub 0.7, chaos
# 2/s, SAT 8) and a 0.55 s intake: she breathes faster.
func _her_growl(rage: bool) -> PackedFloat32Array:
	var ex := 1.0 if rage else 1.5
	var inn := 0.55 if rage else 0.75
	var length := ex + inn + (0.25 if rage else 0.35)
	var out := _silence(length)
	var count := int(ex * MIX_RATE)
	var src := _glottal(count, 104.0, 98.0, 0.06, 0.25, 0.7 if rage else 0.5, 1000.0, 2.0 if rage else 0.0, 0.35)
	var voice := _formants(src, 650.0, 1190.0, 2360.0)
	_saturate(voice, 8.0 if rage else 4.0)
	voice = _bandpass(voice, 220.0, 7000.0)
	_peak(voice, 1.0)
	for j in count:
		var v := float(j) / float(count)
		out[j] += voice[j] * minf(1.0, v * 12.0) * (1.0 - 0.35 * v) * minf(1.0, (1.0 - v) * 8.0)
	_add_intake(out, ex + 0.05, inn, 0.8)
	return out


# The intake: noise 0.5-3.5 kHz with a snore in it (a glide 900->1400 Hz
# under a 32 Hz flutter), catching twice for 25 ms.
func _add_intake(out: PackedFloat32Array, start: float, length: float, gain: float) -> void:
	var first := int(start * MIX_RATE)
	var count := int(length * MIX_RATE)
	var air := _band_noise_buffer(count, 500.0, 3500.0)
	var snore := PackedFloat32Array()
	snore.resize(count)
	_add_glide(snore, 0.0, length, 1.0, 900.0, 1400.0, 0.15, 0.0)
	_rough(snore, 32.0, 0.6)
	var c1 := int(_rng.randf_range(0.3, 0.45) * count)
	var c2 := int(_rng.randf_range(0.6, 0.75) * count)
	var half := int(0.0125 * MIX_RATE)
	for j in count:
		var i := first + j
		if i >= out.size():
			break
		var v := float(j) / float(count)
		var env := sin(PI * v)
		if absi(j - c1) < half or absi(j - c2) < half:
			env *= 0.2
		out[i] += (air[j] * 1.6 + snore[j] * 1.2) * env * gain


# Close: her mouth working. Crackle 20-70/s of 0.3-1.5 ms in 2-6 kHz; six
# saliva strings (click pairs 3-8 ms apart); two lip smacks.
func _her_mouth(length: float) -> PackedFloat32Array:
	var out := _silence(length)
	_add_grains(out, 0.0, length, 20.0, 70.0, 0.3, 1.5, 2000.0, 6000.0, 0.5)
	for k in 6:
		var t := _rng.randf_range(0.0, length - 0.05)
		var g := _rng.randf_range(0.4, 0.9)
		_add_band_noise(out, t, 0.002, g, 0.0006, 2500.0, 6500.0)
		_add_band_noise(out, t + _rng.randf_range(0.003, 0.008), 0.002, g * 0.7, 0.0006, 2500.0, 6500.0)
	for k in 2:
		_add_glide(out, _rng.randf_range(0.0, length - 0.1), 0.02, 2.0, 1800.0, 700.0, 0.3, 0.0)
	_peak(out, 0.9)
	return out


# Her breath, one cycle: the wet intake, then a voiced exhale (noise
# 0.35-2.2 kHz under GLOT 95 Hz breathy into BIG, -10 dB), mucus crackle
# over it all.
func _her_breath() -> PackedFloat32Array:
	var length := _rng.randf_range(1.5, 1.9)
	var out := _silence(length)
	var t_in := _rng.randf_range(0.55, 0.75)
	_add_intake(out, 0.0, t_in, 1.0)
	var start := t_in + 0.12
	var count := int((length - start) * MIX_RATE)
	var air := _band_noise_buffer(count, 350.0, 2200.0)
	var voice := _formants(_glottal(count, 95.0, 86.0, 0.08, 0.3, 0.3, 700.0, 0.0, 0.5), 650.0, 1190.0, 2360.0)
	var first := int(start * MIX_RATE)
	for j in count:
		var v := float(j) / float(count)
		var env := minf(1.0, v * 10.0) * pow(1.0 - v, 1.5)
		out[first + j] += (air[j] * 1.5 + voice[j] * 0.3) * env
	_add_grains(out, 0.0, length, 40.0, 40.0, 0.5, 3.0, 2000.0, 5000.0, 0.12)
	return out


# A 220 kg footfall: the slap, a 60 Hz thump with its harmonics, the deck's
# low modes (boom, never bell), the flesh settling after, and one time in
# three the sole peeling off the steel.
func _her_step() -> PackedFloat32Array:
	var out := _silence(0.4)
	_add_band_noise(out, 0.0, 0.006, 1.2, 0.0015, 700.0, 5000.0)
	_add_thump(out, 0.0, 60.0 * _rng.randf_range(0.9, 1.1), 0.15, 0.07)
	for f in [190.0, 430.0, 720.0]:
		_add_mode(out, f * _rng.randf_range(0.92, 1.08), 0.0, 0.25, 0.9, _rng.randf_range(0.05, 0.09), 3.0)
	_add_band_noise(out, 0.08, 0.03, 0.3, 0.008, 300.0, 2000.0)
	_add_band_noise(out, 0.15, 0.03, 0.2, 0.008, 300.0, 2000.0)
	_rough(out, 11.0, 0.25)
	if _rng.randf() < 0.34:
		_add_glide(out, 0.06, 0.025, 1.5, 900.0, 2000.0, 0.3, 0.0)
	return out


# The shriek: a gasp, then GLOT 420->900 Hz (tilt 2 kHz, sub 0.4, chaos
# 3/s) into OPEN, a 70 Hz flutter, SAT 6, breath 2-6 kHz; 30 ms attack, two
# breaks late, the last 30% falling away.
func _her_shriek() -> PackedFloat32Array:
	var length := _rng.randf_range(1.1, 1.4)
	var out := _silence(length)
	_add_glide(out, 0.0, 0.12, 2.0, 1200.0, 2400.0, 0.4, 0.0)
	_add_band_noise(out, 0.0, 0.12, 0.3, 0.06, 1500.0, 5000.0)
	var first := int(0.13 * MIX_RATE)
	var count := out.size() - first
	var voice := _formants(_glottal(count, 420.0, 900.0, 0.03, 0.15, 0.4, 2000.0, 3.0, 0.4), 810.0, 1160.0, 2670.0)
	_rough(voice, 70.0, 0.5)
	_saturate(voice, 6.0)
	var air := _band_noise_buffer(count, 2000.0, 6000.0)
	var b1 := _rng.randf_range(0.55, 0.65)
	var b2 := b1 + _rng.randf_range(0.1, 0.16)
	var attack := 0.03 * MIX_RATE
	for j in count:
		var v := float(j) / float(count)
		var env := minf(1.0, float(j) / attack) * (1.0 if v < 0.7 else (1.0 - v) / 0.3)
		var brk := 0.25 if (absf(v - b1) < 0.02 or absf(v - b2) < 0.015) else 1.0
		out[first + j] += (voice[j] * brk + air[j] * 0.6) * env
	return out


# Joints: 3-8 cavitation pops, the gaps closing 160 -> 40 ms, each a click,
# a 3 kHz tick and a knock 250-450 Hz; gristle grinding between.
func _her_joints() -> PackedFloat32Array:
	var out := _silence(0.7)
	var gap := 0.16
	var at := 0.02
	for k in _rng.randi_range(3, 8):
		if at > 0.62:
			break
		_add_band_noise(out, at, 0.002, 1.0, 0.0007, 2000.0, 6000.0)
		_add_mode(out, 3000.0 * _rng.randf_range(0.85, 1.15), at, 0.03, 0.5, 0.004, 8.0)
		_add_mode(out, _rng.randf_range(250.0, 450.0), at, 0.1, 0.6, 0.02, 4.0)
		at += gap * _rng.randf_range(0.8, 1.2)
		gap = maxf(0.04, gap * 0.7)
	var grist := _resonate(_slip(out.size(), 25.0, 35.0), 1200.0, 2.0)
	for i in out.size():
		out[i] += grist[i] * 0.5 * (1.0 - float(i) / float(out.size()))
	return out


# The croak (a death rattle, Ju-On's): GLOT at 32-40 -> 20-26 Hz, jitter
# 25%, every pulse its own, into 450/1100/2600 Hz at Q x1.3; two swells; a
# wet click to end.
func _her_croak() -> PackedFloat32Array:
	var length := _rng.randf_range(1.0, 1.4)
	var n := int(length * MIX_RATE)
	var out := _bandpass(_formants(_glottal(n, _rng.randf_range(32.0, 40.0), _rng.randf_range(20.0, 26.0), 0.25,
		0.4, 0.3, 1500.0, 0.0, 0.08), 450.0, 1100.0, 2600.0, 1.3), 350.0, 4500.0)
	for i in n:
		var u := float(i) / float(n)
		out[i] *= minf(1.0, 4.0 * u * (1.0 - u) * 2.0) * (0.55 + 0.45 * absf(fmod(u * 2.0, 1.0) * 2.0 - 1.0))
	_peak(out, 0.9)
	_add_band_noise(out, length - 0.08, 0.003, 0.6, 0.001, 2500.0, 6000.0)
	return out


# A cluck of the tongue off the palate: a 2.2 kHz knock (Q 5, 8 ms), a
# click, a 3.5 kHz tick 30 ms after.
func _her_cluck() -> PackedFloat32Array:
	var out := _silence(0.18)
	_add_mode(out, 2200.0, 0.0, 0.06, 1.0, 0.008, 5.0)
	_add_band_noise(out, 0.0, 0.0015, 0.8, 0.0005, 1500.0, 6000.0)
	_add_mode(out, 3500.0, 0.03, 0.03, 0.4, 0.004, 6.0)
	return out


# A swallow: the lips' smack (glide 1800->700 Hz, 20 ms), 3-6 bubbles, a
# gulp (GLOT 110->90 Hz for 0.14 s through 330 Hz Q 3), a throat click.
func _her_swallow() -> PackedFloat32Array:
	var out := _silence(0.6)
	_add_glide(out, 0.0, 0.02, 2.0, 1800.0, 700.0, 0.3, 0.0)
	for k in _rng.randi_range(3, 6):
		_add_bubble(out, _rng.randf_range(0.04, 0.25), _rng.randf_range(0.15, 0.35))
	var count := int(0.14 * MIX_RATE)
	var gulp := _resonate(_glottal(count, 110.0, 90.0, 0.05, 0.2, 0.3, 600.0, 0.0, 0.2), 330.0, 3.0)
	_peak(gulp, 0.6)
	var first := int(0.28 * MIX_RATE)
	for j in count:
		out[first + j] += gulp[j] * sin(PI * float(j) / float(count))
	_add_band_noise(out, 0.47, 0.003, 0.7, 0.001, 1500.0, 5000.0)
	_peak(out, 0.9)
	return out


# A sniff at something she likes: 3-5 pulls of 60-120 ms (noise into 2.8
# kHz Q 3 under a 70 Hz flutter), then a wet snort.
func _her_sniff() -> PackedFloat32Array:
	var out := _silence(0.8)
	var at := 0.0
	for k in _rng.randi_range(3, 5):
		var d := _rng.randf_range(0.06, 0.12)
		var count := int(d * MIX_RATE)
		var pull := _resonate(_band_noise_buffer(count, 1500.0, 5000.0), 2800.0, 3.0)
		_rough(pull, 70.0, 0.5)
		var first := int(at * MIX_RATE)
		for j in count:
			if first + j < out.size():
				out[first + j] += pull[j] * 4.0 * sin(PI * float(j) / float(count))
		at += d + _rng.randf_range(0.02, 0.05)
	_add_band_noise(out, minf(at + 0.03, 0.65), 0.08, 0.6, 0.03, 300.0, 1500.0)
	_add_grains(out, minf(at + 0.03, 0.65), 0.1, 40.0, 40.0, 0.5, 2.0, 2000.0, 5000.0, 0.3)
	return out


# Effort: a pressed throat (GLOT 115->135 Hz, shimmer 35%, tilt 1.5 kHz)
# into a narrow 400/1000/2400 Hz tract, rising over 0.85 s under a 7 Hz
# tremor, bursting into OPEN at SAT 6 for the last 0.35 s.
func _her_effort() -> PackedFloat32Array:
	var length := 1.2
	var n := int(length * MIX_RATE)
	var out := _silence(length)
	var src := _glottal(n, 115.0, 135.0, 0.04, 0.35, 0.3, 1500.0, 0.0, 0.2)
	var split := int(0.85 * MIX_RATE)
	var fade := int(0.03 * MIX_RATE)
	var pressed := _formants(src.slice(0, split + fade), 400.0, 1000.0, 2400.0, 1.4)
	var burst := _formants(src.slice(split), 810.0, 1160.0, 2670.0)
	_saturate(burst, 6.0)
	for i in n:
		var t := float(i) / float(MIX_RATE)
		var w := clampf(float(i - split) / float(fade), 0.0, 1.0)
		var tail := minf(1.0, float(n - i) / (0.15 * MIX_RATE))
		var v := 0.0
		if w < 1.0:
			var trem := 0.8 + 0.2 * absf(fmod(t * 7.0, 1.0) * 2.0 - 1.0)
			v = pressed[i] * t / 0.85 * sqrt(minf(1.0, t / 0.85)) * trem * (1.0 - w)
		if i >= split:
			v += burst[i - split] * w
		out[i] = v * tail
	return out


# A blow lands on her: flesh slapped and the weight behind it, the wind
# knocked out ("HUH": GLOT 140->100 Hz into BIG over breath), a 10 Hz wobble,
# and 0.4 s later a snarl (sub 0.7, chaos 2/s, SAT 8). Louder, never hurt.
func _her_hit() -> PackedFloat32Array:
	var out := _silence(0.85)
	_add_band_noise(out, 0.0, 0.01, 1.0, 0.003, 700.0, 5000.0)
	_add_thump(out, 0.0, 62.0, 0.25, 0.07)
	_add_band_noise(out, 0.01, 0.15, 0.5, 0.06, 400.0, 3000.0)
	var count := int(0.15 * MIX_RATE)
	var huh := _formants(_glottal(count, 140.0, 100.0, 0.05, 0.3, 0.2, 900.0, 0.0, 0.5), 650.0, 1190.0, 2360.0)
	var first := int(0.01 * MIX_RATE)
	for j in count:
		out[first + j] += huh[j] * 0.6 * sin(PI * float(j) / float(count))
	_rough(out, 10.0, 0.3, 0, int(0.3 * MIX_RATE))
	count = int(0.4 * MIX_RATE)
	var snarl := _formants(_glottal(count, 120.0, 112.0, 0.08, 0.35, 0.7, 1200.0, 2.0, 0.3), 650.0, 1190.0, 2360.0)
	_saturate(snarl, 8.0)
	first = int(0.4 * MIX_RATE)
	for j in count:
		if first + j < out.size():
			var v := float(j) / float(count)
			out[first + j] += snarl[j] * 0.7 * minf(1.0, v * 13.0) * minf(1.0, (1.0 - v) * 4.0)
	return out


# Her laugh, wet: GLOT 150->120 Hz in 6 bursts a second on an exhale, into
# BIG, mucus crackle over it.
func _her_laugh() -> PackedFloat32Array:
	var length := 1.3
	var n := int(length * MIX_RATE)
	var out := _silence(length)
	var voice := _formants(_glottal(n, 150.0, 120.0, 0.06, 0.3, 0.4, 1000.0, 0.0, 0.6), 650.0, 1190.0, 2360.0)
	var air := _band_noise_buffer(n, 400.0, 2500.0)
	for i in n:
		var t := float(i) / float(MIX_RATE)
		var p := fmod(t * 6.0, 1.0)
		var gate := smoothstep(0.0, 0.08, p) * (1.0 - smoothstep(0.35, 0.5, p))
		out[i] = (voice[i] * gate * 0.8 + air[i] * 0.8) * pow(1.0 - t / length, 0.7)
	_add_grains(out, 0.0, length, 40.0, 40.0, 0.5, 2.5, 2000.0, 5000.0, 0.15)
	return out


# The arm coming off: the shoulder's pop and weight; fibers parting faster
# and faster (grains 40 -> 600/s over 0.6 s) with pops among them; the
# squelch; bubbles; the green-stick snap and its splinters; the suction pop
# as it comes free; drips on the steel.
func _her_rip() -> PackedFloat32Array:
	var length := 1.5
	var out := _silence(length)
	_add_band_noise(out, 0.0, 0.002, 1.0, 0.0006, 800.0, 6000.0)
	_add_mode(out, 250.0, 0.0, 0.2, 0.9, 0.04, 4.0)
	_add_thump(out, 0.0, 55.0, 0.5, 0.08)
	_add_grains(out, 0.03, 0.6, 40.0, 600.0, 1.0, 4.0, 800.0, 3500.0, 0.55)
	for k in 12:
		_add_band_noise(out, 0.05 + 0.05 * float(k) + _rng.randf_range(0.0, 0.02), 0.003, 1.0, 0.001, 1200.0, 3000.0)
	_add_glide(out, 0.2, 0.25, 3.0, 1500.0, 400.0, 0.25, 0.0)
	for k in _rng.randi_range(15, 25):
		_add_bubble(out, _rng.randf_range(0.2, 1.0), _rng.randf_range(0.1, 0.3))
	var snap := _rng.randf_range(0.5, 0.6)
	_add_band_noise(out, snap, 0.002, 1.0, 0.0006, 1000.0, 7000.0)
	_add_grains(out, snap, 0.06, 650.0, 650.0, 0.5, 2.0, 1200.0, 3000.0, 0.5)
	_add_glide(out, snap + 0.25, 0.04, 3.0, 300.0, 1200.0, 0.3, 0.0)
	for k in _rng.randi_range(10, 20):
		var t := _rng.randf_range(snap + 0.3, length - 0.03)
		_add_mode(out, _rng.randf_range(2200.0, 3600.0), t, 0.03, 0.25, 0.006, 10.0)
		_add_band_noise(out, t, 0.002, 0.15, 0.0008, 1500.0, 5000.0)
	return out


# The pull: a joint creaking under it (stick-slip 30/s into 900 Hz Q 2.5
# and 2.1 kHz Q 4) over a grind. The view pitches it 0.7 -> 1.6 with strain.
func _her_strain(length: float) -> PackedFloat32Array:
	var n := int(length * MIX_RATE)
	var out := _silence(length)
	var sl := _slip(n, 30.0, 30.0)
	var a := _resonate(sl, 900.0, 2.5)
	var b := _resonate(sl, 2100.0, 4.0)
	var gr := _band_noise_buffer(n, 500.0, 2500.0)
	for i in n:
		out[i] = a[i] + 0.6 * b[i] + gr[i] * 0.15
	_peak(out, 0.9)
	return out


# Her knee onto the chest: the weight, the cloth, 3-5 ribs going in 0.25 s,
# the air forced out of him.
func _her_crunch() -> PackedFloat32Array:
	var out := _silence(0.6)
	_add_thump(out, 0.0, 50.0, 0.3, 0.1)
	_add_band_noise(out, 0.0, 0.02, 0.6, 0.01, 300.0, 2000.0)
	var at := 0.03
	for k in _rng.randi_range(3, 5):
		_add_band_noise(out, at, 0.002, 1.0, 0.0006, 1500.0, 6500.0)
		_add_mode(out, _rng.randf_range(500.0, 750.0), at, 0.06, 0.6, 0.015, 4.0)
		at += _rng.randf_range(0.04, 0.07)
	_add_band_noise(out, 0.05, 0.4, 0.35, 0.15, 400.0, 2500.0)
	return out


# A body dragged over plate: friction under stick-slip, skin squeaking,
# cloth rasping, the bumps of the seams.
func _her_drag(length: float) -> PackedFloat32Array:
	var n := int(length * MIX_RATE)
	var out := _silence(length)
	var fr := _band_noise_buffer(n, 250.0, 2000.0)
	var sl := _resonate(_slip(n, 3.0, 8.0), 800.0, 2.0)
	for i in n:
		out[i] = fr[i] * 0.5 + sl[i] * 0.8
	for k in 3:
		_add_glide(out, _rng.randf_range(0.0, length - 0.05), 0.04, 1.5, 900.0, 1800.0, 0.2, 0.0)
	_add_band_noise(out, 0.0, length, 0.05, 0.0, 2000.0, 5000.0)
	for t in [0.3, 1.0, 1.7]:
		_add_thump(out, t, 70.0, 0.4, 0.05)
	return out
