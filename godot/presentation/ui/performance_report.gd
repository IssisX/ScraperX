extends RefCounted

# A bounded foreground frame window. Recording allocates no collections or text;
# sorting and formatting happen only when the player requests a pause/report.
# Render counters describe the previous completed render frame. Native timing
# is measured advance_frame WALL time plus optional process CPU observations.
# Neither observation isolates physics or measures GPU cost.
const CAPACITY := 512
var _frame_ms := PackedFloat64Array()
var _native_ms := PackedFloat64Array()
var _native_process_cpu_ms := PackedFloat64Array()
var _draw_calls := PackedInt64Array()
var _primitives := PackedInt64Array()
var _count := 0
var _next := 0
var _paused := false
var _skip_next := false
var _last_record_usec := -1
var _steps := 0
var _height := 0.0
var _pitch := 0.0

func _init() -> void:
	_frame_ms.resize(CAPACITY)
	_native_ms.resize(CAPACITY)
	_native_process_cpu_ms.resize(CAPACITY)
	_draw_calls.resize(CAPACITY)
	_primitives.resize(CAPACITY)

func skip_next_frame() -> void:
	_skip_next = true
	_last_record_usec = -1

func set_paused(paused: bool) -> void:
	if _paused and not paused:
		skip_next_frame()
	_paused = paused
	if paused:
		_last_record_usec = -1

# delta_seconds remains caller-compatible simulation context, never wall timing.
# Tests may supply a monotonic timestamp; production always reads the real clock.
func record_frame(delta_seconds: float, native_usec: int, steps: int,
		draw_calls: int, primitives: int, height: float, pitch_degrees: float,
		timestamp_usec: int = -1, native_process_cpu_usec: int = -1) -> void:
	if _paused:
		return
	var now_usec := Time.get_ticks_usec() if timestamp_usec == -1 else timestamp_usec
	var previous_usec := _last_record_usec
	_last_record_usec = now_usec if now_usec >= 0 else -1
	if _skip_next:
		_skip_next = false
		return
	# The first call establishes the interval boundary. A bad/reversed clock
	# rebases without admitting a fabricated interval or poisoning later samples.
	if previous_usec < 0 or now_usec <= previous_usec:
		return
	if not is_finite(delta_seconds) or delta_seconds < 0.0 or native_usec < 0 \
			or steps < 0 or draw_calls < 0 or primitives < 0 \
			or not is_finite(height) or not is_finite(pitch_degrees):
		return
	_frame_ms[_next] = float(now_usec - previous_usec) / 1000.0
	_native_ms[_next] = float(native_usec) / 1000.0
	_native_process_cpu_ms[_next] = float(native_process_cpu_usec) / 1000.0 if native_process_cpu_usec >= 0 else -1.0
	_draw_calls[_next] = draw_calls
	_primitives[_next] = primitives
	_next = (_next + 1) % CAPACITY
	_count = mini(_count + 1, CAPACITY)
	_steps = steps
	_height = height
	_pitch = pitch_degrees

# Nearest-rank quantiles, including zero-step renders and genuine long stalls.
func summary() -> Dictionary:
	var frames := _frame_ms.slice(0, _count)
	var native := _native_ms.slice(0, _count)
	var process_cpu := PackedFloat64Array()
	var elapsed_ms := 0.0
	var max_draws := 0
	var max_primitives := 0
	for i in _count:
		elapsed_ms += frames[i]
		if _native_process_cpu_ms[i] >= 0.0:
			process_cpu.append(_native_process_cpu_ms[i])
		max_draws = maxi(max_draws, _draw_calls[i])
		max_primitives = maxi(max_primitives, _primitives[i])
	frames.sort()
	native.sort()
	process_cpu.sort()
	var last := (_next + CAPACITY - 1) % CAPACITY
	return {
		"sample_count": _count, "elapsed_seconds": elapsed_ms / 1000.0,
		"fps": float(_count) * 1000.0 / elapsed_ms if elapsed_ms > 0.0 else 0.0,
		"frame_ms_p50": _rank(frames, 0.50), "frame_ms_p95": _rank(frames, 0.95),
		"frame_ms_max": _rank(frames, 1.0),
		"native_wall_ms_p50": _rank(native, 0.50), "native_wall_ms_p95": _rank(native, 0.95),
		"native_wall_ms_max": _rank(native, 1.0),
		"native_process_cpu_samples": process_cpu.size(),
		"native_process_cpu_ms_p50": _rank(process_cpu, 0.50),
		"native_process_cpu_ms_p95": _rank(process_cpu, 0.95),
		"native_process_cpu_ms_max": _rank(process_cpu, 1.0),
		"draw_calls_last": _draw_calls[last] if _count > 0 else 0,
		"draw_calls_max": max_draws,
		"primitives_last": _primitives[last] if _count > 0 else 0,
		"primitives_max": max_primitives,
		"steps_last": _steps, "height": _height, "pitch_degrees": _pitch,
	}

func _rank(values: PackedFloat64Array, fraction: float) -> float:
	if values.is_empty():
		return 0.0
	return values[maxi(0, int(ceil(float(values.size()) * fraction)) - 1)]

func copy_text(build_identity: Dictionary) -> String:
	var s := summary()
	# Whitelist and cap identity fields so a malformed caller cannot make a huge report.
	var source := str(build_identity.get("source_sha", "unknown")).left(80).replace("\n", " ")
	var version := str(build_identity.get("version_code", "development")).left(40).replace("\n", " ")
	var cpu_line := "Native advance Process CPU: Unavailable\n"
	if s.native_process_cpu_samples > 0:
		cpu_line = "Native advance Process CPU ms p50 / p95 / max: %.3f / %.3f / %.3f (%d samples)\n" % [s.native_process_cpu_ms_p50, s.native_process_cpu_ms_p95, s.native_process_cpu_ms_max, s.native_process_cpu_samples]
	return ("ScraperX performance report\nBuild %s / source %s\n" % [version, source]
		+ "Recent window: %d frames / %.3f s / %.1f FPS\n" % [s.sample_count, s.elapsed_seconds, s.fps]
		+ "Frame WALL ms p50 / p95 / max: %.3f / %.3f / %.3f\n" % [s.frame_ms_p50, s.frame_ms_p95, s.frame_ms_max]
		+ "Native advance WALL ms p50 / p95 / max: %.3f / %.3f / %.3f\n" % [s.native_wall_ms_p50, s.native_wall_ms_p95, s.native_wall_ms_max]
		+ cpu_line
		+ "Draw calls last / max: %d / %d; primitives last / max: %d / %d\n" % [s.draw_calls_last, s.draw_calls_max, s.primitives_last, s.primitives_max]
		+ "Render counters: previous completed render frame.\n"
		+ "Recent height: %.2f m; pitch: %.1f degrees; native steps: %d\n" % [s.height, s.pitch_degrees, s.steps_last]
		+ "Explicit pause intervals excluded. Process CPU covers all app threads during the native call and can exceed WALL. Neither timing isolates physics or measures GPU cost.")
