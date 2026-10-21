extends RefCounted

# Rendering only: isolate the exact terminal and freeze voice/pose. The local
# identity source uses a bundled fictional portrait, never a player's files.
const Record := preload("res://presentation/story_handover_record.gd")
class IdentityFixture extends RefCounted:
	var photo: Texture2D
	func photo_texture() -> Texture2D:
		return photo
	func face_landmarks() -> PackedVector2Array:
		return PackedVector2Array([Vector2(.391,.387), Vector2(.574,.354), Vector2(.504,.578)])
class SilentVoice extends Node:
	func speech_level_for(_speaker: StringName) -> float:
		return 0.0
	func playback_seconds_for(_speaker: StringName) -> float:
		return 0.0

func run(tree: SceneTree) -> Dictionary:
	var record := Record.new()
	var identity := IdentityFixture.new()
	var voice := SilentVoice.new()
	tree.root.add_child(record)
	tree.root.add_child(voice)
	record.identity_store = identity
	record._near = true
	record._build_display()
	var images: Array[PackedByteArray] = []
	for i in 3:
		identity.photo = load("res://assets/story/iona_dispatch_portrait.png") if i == 1 else null
		record.invalidate_identity()
		record.present_record({}, voice, i + 1)
		await RenderingServer.frame_post_draw
		await RenderingServer.frame_post_draw
		images.append(record._display.get_texture().get_image().get_data())
	var failures: Array[String] = []
	if images[0].is_empty() or images[0] == images[1]:
		failures.append("new identity must change actual outer screen pixels without gaze or speech changes")
	if images[1] == images[2] or images[0] != images[2]:
		failures.append("removing identity must immediately restore exact default screen pixels")
	if not record._human.last_error.is_empty():
		failures.append("actual portrait fitting failed: " + record._human.last_error)
	var result := {"checks": 3, "failures": failures, "rendered": true, "gameplay": false,
		"photo_changes_screen": images[0] != images[1], "removal_exact_default": images[0] == images[2]}
	record.queue_free()
	voice.queue_free()
	await tree.process_frame
	return result
