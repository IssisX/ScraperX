extends SceneTree

const Presenter := preload("res://presentation/story_presenter.gd")
var checks := 0
var failures := 0


func _initialize() -> void:
	_run.call_deferred()


func check(ok: bool, message: String) -> void:
	checks += 1
	if not ok:
		failures += 1
		push_error("STORY_PRESENTER FAIL " + message)


func transparent(node: Node) -> bool:
	if node is Control and (node.mouse_filter != Control.MOUSE_FILTER_IGNORE or node.focus_mode != Control.FOCUS_NONE):
		return false
	for child in node.get_children():
		if not transparent(child):
			return false
	return true


func state() -> Dictionary:
	return {"intro_title": "KELLERWORKS", "intro_subtitle": "Reach the upper service galleries.",
		"intro_visible": true, "chapter": "WORKING APRON", "objective_id": &"approach",
		"objective": "Find the freight platform", "detail": "Follow the marked loading lane.",
		"transmission_id": 7, "speaker": "CONTROL", "transmission": "The service route starts above the apron."}


func _run() -> void:
	var presenter := Presenter.new()
	root.add_child(presenter)
	presenter.present({}, 1.0)
	check(not presenter._intro.visible and not presenter._objective.visible and not presenter._radio.visible, "neutral initial state hides every group")
	var supplied := state()
	presenter.present(supplied, 0.1)
	check(presenter._intro.visible and presenter._intro_alpha > 0.0 and presenter._intro_alpha < 1.0, "supplied intro fades in")
	check(presenter._objective_text.text == supplied.objective and presenter._detail.text == supplied.detail and presenter._chapter.text == supplied.chapter, "display uses supplied objective facts")
	check(presenter._transmission.text == supplied.transmission and presenter._speaker.text == supplied.speaker, "display uses supplied radio facts")
	check(presenter._objective_id == &"approach", "objective identifier is observed without progression")
	check(transparent(presenter), "every descendant ignores input and focus")
	var node_count := presenter.get_child_count()
	presenter.present(supplied, 0.3)
	var accent: float = presenter._radio_accent
	check(accent < 1.0, "duplicate transmission ID decays instead of restarting")
	presenter.present(supplied, 0.3)
	check(presenter._radio_accent < accent and presenter._radio.visible, "radio lifetime stays with supplied state after accent finishes")
	supplied.objective = "Reach the receiving landing"
	supplied.objective_id = &"landing"
	presenter.present(supplied, 0.0)
	check(presenter._objective_text.text == supplied.objective and presenter._objective_id == &"landing", "objective changes immediately even with zero delta")
	supplied.transmission_id = 8
	supplied.transmission = "Take the upper service walkway."
	presenter.present(supplied, 0.0)
	check(presenter._radio_accent == 1.0 and presenter._transmission.text == supplied.transmission, "new supplied ID earns one accent")
	supplied.intro_visible = false
	presenter.present(supplied, 0.1)
	check(presenter._intro.visible and presenter._intro_alpha < 1.0, "retained copy permits visual fade out")
	presenter.present(supplied, 1.0)
	check(not presenter._intro.visible and presenter._objective.visible, "intro fade never changes objective")
	presenter.present({}, 0.0)
	check(not presenter._intro.visible and not presenter._objective.visible and not presenter._radio.visible, "neutral cancellation hides immediately with zero delta")
	check(presenter._transmission.text.is_empty() and presenter._objective_text.text.is_empty(), "neutral cancellation clears stale copy")
	presenter.present(supplied, 0.0)
	check(presenter._radio_accent == 0.0, "cancelled same ID can display supplied copy without replaying accent")
	presenter.present(supplied, NAN)
	check(is_finite(presenter._intro_alpha) and is_finite(presenter._radio_accent), "invalid delta cannot poison visual state")
	presenter.present({"objective": 41, "transmission_id": "bad", "transmission": "bad", "intro_title": []}, -1.0)
	check(not presenter._objective.visible and not presenter._radio.visible and not presenter._intro.visible, "invalid boundary values stay neutral")
	for dimensions in [Vector2(2160, 1856), Vector2(2400, 1080), Vector2(1920, 1080), Vector2(1080, 2400), Vector2(960, 540)]:
		var safe := Rect2(Vector2(24, 32), dimensions - Vector2(48, 64))
		presenter.present(state(), 1.0)
		presenter._layout(safe)
		for group in [presenter._intro, presenter._objective, presenter._radio]:
			check(safe.encloses(Rect2(group.position, group.size)), "story group fits safe bounds " + str(dimensions))
			check(group.position.y + group.size.y < safe.position.y + safe.size.y * 0.55, "story group clears lower thumb decks " + str(dimensions))
			for child in (group.content.get_children() if group is Presenter.CachedStoryText else group.get_children()):
				check(child.size.x > 0 and child.size.y > 0 and child.get_theme_font_size("font_size") >= 10, "readable positive label bounds " + str(dimensions))
	check(presenter.get_child_count() == node_count, "repeated samples allocate no additional groups")
	presenter.present(state(), 1.0)
	for group in [presenter._intro, presenter._objective]:
		group._viewport.render_target_update_mode = SubViewport.UPDATE_DISABLED
	presenter.present(state(), 0.0)
	check(presenter._intro._viewport.render_target_update_mode == SubViewport.UPDATE_DISABLED and presenter._objective._viewport.render_target_update_mode == SubViewport.UPDATE_DISABLED, "unchanged copy and fade opacity reuse text rasters")
	var replacement := state()
	replacement.objective = "Take another physical route"
	presenter.present(replacement, 0.0)
	check(presenter._objective._viewport.render_target_update_mode == SubViewport.UPDATE_ONCE and presenter._intro._viewport.render_target_update_mode == SubViewport.UPDATE_DISABLED, "changed objective redraws only its text cache")
	check(presenter._intro._viewport.gui_disable_input and presenter._objective._viewport.gui_disable_input, "cached text viewports cannot capture touch")

	presenter.free()
	print("STORY_PRESENTER %s checks=%d failures=%d supplied_state=1 input_transparent=1 layouts=5 world_physics=0" % ["PASS" if failures == 0 else "FAIL", checks, failures])
	quit(0 if failures == 0 else 1)
