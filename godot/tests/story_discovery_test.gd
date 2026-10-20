extends SceneTree

const Discovery := preload("res://presentation/story_discovery.gd")
var checks := 0
var failures: Array[String] = []

func _initialize() -> void:
	_run.call_deferred()

func check(ok: bool, why: String) -> void:
	checks += 1
	if not ok:
		failures.append(why)
		push_error("STORY_DISCOVERY " + why)

func _run() -> void:
	var observer := Discovery.new()
	root.add_child(observer)
	var at := Vector3(-101.5, 6.8, -95)
	var camera := Transform3D(Basis.IDENTITY, at + Vector3(0, .62, 0))
	camera = camera.looking_at(Discovery.CARD_POSITION, Vector3.UP)
	var context := {"position": at, "grounded": true, "checkpoint_footing_valid": true, "support_entity": 1440}
	check(observer._print.text.contains("ROWAN \"RO\" VALE") and observer._print.no_depth_test == false, "physical card identifies Ro and respects scene depth")
	observer.observe(camera, context, 0)
	var quiet := true
	for i in 100:
		quiet = observer.observe(camera, context, 0).is_empty() and quiet
	check(quiet, "render frames without native time cannot earn reading")
	quiet = true
	for tick in range(1, 81):
		quiet = observer.observe(camera, context, tick).is_empty() and quiet
	check(quiet, "a partial glance cannot trigger the exchange")
	check(observer.observe(camera, context, 81) == &"vale_emergency_contact", "continuous deliberate attention discovers the printed contact")
	check(observer.observe(camera, context, 82).is_empty(), "a read card triggers once")
	check(observer.observe(camera, context, 0).is_empty(), "native timeline restart begins fresh attention")
	context.checkpoint_footing_valid = false
	quiet = true
	for tick in range(1, 100):
		quiet = observer.observe(camera, context, tick).is_empty() and quiet
	check(quiet, "grounded without firm footing cannot read")
	context.checkpoint_footing_valid = true
	context.support_entity = 51
	quiet = true
	for tick in range(100, 200):
		quiet = observer.observe(camera, context, tick).is_empty() and quiet
	check(quiet, "another support at the same height is not the gallery")
	context.support_entity = 1440
	var away := Transform3D(Basis.IDENTITY, camera.origin)
	quiet = true
	for tick in range(200, 300):
		quiet = observer.observe(away, context, tick).is_empty() and quiet
	check(quiet, "walking past without looking does not discover the card")
	check(observer.observe(camera, context, 500).is_empty(), "an unobserved time gap does not count as reading")
	check(observer.observe(camera, {}, 501).is_empty() and not observer.visible, "missing native observation hides optional presentation")
	print("STORY_DISCOVERY %s checks=%d failures=%d supplied_native_facts_only=1" % ["PASS" if failures.is_empty() else "FAIL", checks, failures.size()])
	observer.queue_free()
	await process_frame
	quit(0 if failures.is_empty() else 1)
