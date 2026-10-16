extends SceneTree

const Preparation := preload("res://presentation/ui/startup_preparation.gd")

# Exclusive test double: its counters are inspected only after panel join.
# It models readiness and exact tick acceptance, never production physics.
class NativeDouble extends RefCounted:
	var ready_after := 3
	var advances := 0
	var fixed_reads := 0
	var bad_clock := false
	var main_thread_work := false
	var delay_usec := 0
	var accepted_steps := 1
	func get_fixed_step_seconds() -> float:
		fixed_reads += 1
		return 1.0 / 90.0
	func advance_frame(dt: float) -> int:
		main_thread_work = main_thread_work or Thread.is_main_thread()
		bad_clock = bad_clock or absf(dt - 1.0 / 90.0) > 0.00000001
		if delay_usec > 0:
			OS.delay_usec(delay_usec)
		advances += 1
		return accepted_steps
	func is_initial_world_settled() -> bool:
		main_thread_work = main_thread_work or Thread.is_main_thread()
		return advances >= ready_after

class FailedLaunch extends Preparation:
	func _start_worker(_work: Callable) -> Error:
		return ERR_CANT_CREATE

var failed := false

func check(ok: bool, message: String) -> void:
	if not ok:
		failed = true
		push_error("STARTUP_PREPARATION FAIL " + message)

func _initialize() -> void:
	_run.call_deferred()

func _observe(panel: Preparation) -> Dictionary:
	var result := {"prepared": 0, "failed": 0, "cancel": 0, "retry": 0, "native": null, "steps": -1}
	panel.prepared.connect(func(native: Object, steps: int) -> void:
		result.prepared += 1
		result.native = native
		result.steps = steps
		check(not panel._thread.is_started(), "ownership handed back only after join")
	)
	panel.failed.connect(func(message: String) -> void:
		result.failed += 1
		check(not message.is_empty(), "failure is visible and explanatory")
	)
	panel.cancel_requested.connect(func() -> void: result.cancel += 1)
	panel.retry_requested.connect(func() -> void: result.retry += 1)
	return result

func _finish(panel: Preparation) -> void:
	for attempt in 500:
		if panel._finished:
			return
		await create_timer(0.001).timeout
	check(false, "bounded test worker finishes without blocking UI polling")

func _run() -> void:
	var panel := Preparation.new()
	root.add_child(panel)
	var result := _observe(panel)
	var native := NativeDouble.new()
	check(panel.start(native) == OK, "launch actual thread")
	check(panel.start(native) == ERR_ALREADY_IN_USE, "single-use prevents a second worker")
	await _finish(panel)
	check(result.prepared == 1 and result.failed == 0 and result.native == native and result.steps == 3,
		"success requires actual readiness and returns the exact owned native")
	check(native.advances == 3 and native.fixed_reads == 1 and not native.bad_clock and not native.main_thread_work,
		"worker alone runs exact neutral fixed ticks; clock cached once before launch")
	check(panel._progress.value == 100.0 and panel._native == null, "success completes progress and releases panel reference")
	await process_frame
	check(result.prepared == 1, "success emits once")
	panel.free()

	panel = FailedLaunch.new()
	root.add_child(panel)
	result = _observe(panel)
	native = NativeDouble.new()
	check(panel.start(native) == ERR_CANT_CREATE, "thread launch error propagates")
	check(result.failed == 1 and result.prepared == 0 and native.advances == 0 and panel._native == null,
		"failed launch releases native and never reports readiness")
	check(panel._retry.visible, "launch failure presents retry")
	panel._retry.pressed.emit()
	check(result.retry == 1, "retry is an explicit request for a fresh attempt")
	panel.free()

	panel = Preparation.new()
	root.add_child(panel)
	result = _observe(panel)
	native = NativeDouble.new()
	native.ready_after = Preparation.MAX_STEPS + 1
	check(panel.start(native) == OK, "launch never-ready test")
	await _finish(panel)
	check(result.failed == 1 and result.prepared == 0 and native.advances == 900,
		"readiness false reaches exact hard bound and cannot become early success")
	check(panel._progress.value <= 95.0 and panel._retry.visible, "timeout never claims complete progress and offers retry")
	panel.free()

	panel = Preparation.new()
	root.add_child(panel)
	result = _observe(panel)
	native = NativeDouble.new()
	native.accepted_steps = 0
	check(panel.start(native) == OK, "launch rejected-step test")
	await _finish(panel)
	check(result.failed == 1 and result.prepared == 0 and native.advances == 1,
		"a rejected fixed tick fails instead of fabricating progress")
	panel.free()

	panel = Preparation.new()
	root.add_child(panel)
	result = _observe(panel)
	native = NativeDouble.new()
	panel.notification(Node.NOTIFICATION_APPLICATION_PAUSED)
	check(panel.start(native) == OK, "launch while application paused")
	await create_timer(0.03).timeout
	check(panel.progress_steps() == 0 and result.prepared == 0, "paused worker makes no ticks or handoff")
	panel.notification(Node.NOTIFICATION_APPLICATION_RESUMED)
	await _finish(panel)
	check(result.prepared == 1 and native.advances == 3, "resume continues actual preparation")
	panel.free()

	panel = Preparation.new()
	root.add_child(panel)
	result = _observe(panel)
	native = NativeDouble.new()
	panel.notification(Node.NOTIFICATION_APPLICATION_PAUSED)
	check(panel.start(native) == OK, "launch cancellable paused worker")
	panel._cancel.pressed.emit()
	panel._cancel.pressed.emit()
	await _finish(panel)
	check(result.cancel == 1 and result.prepared == 0 and result.failed == 0 and native.advances == 0,
		"cancel emits once, wakes paused worker and cannot fabricate success")
	panel.free()

	panel = Preparation.new()
	root.add_child(panel)
	result = _observe(panel)
	native = NativeDouble.new()
	native.ready_after = 901
	native.delay_usec = 1000
	check(panel.start(native) == OK, "launch active teardown test")
	await create_timer(0.005).timeout
	panel.free()
	# Destruction joins before returning, so accessing the retained test double
	# now is safe and demonstrates that no worker outlives its panel.
	check(native.advances < 900 and result.prepared == 0 and result.failed == 0,
		"teardown cancels and joins active work before native or panel destruction")
	print("STARTUP_PREPARATION failures=" + str(int(failed)))
	quit(1 if failed else 0)
