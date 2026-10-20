extends RefCounted
## How often and how late the tower answers the player (her chart,
## 03_EXECUTION/UNCANNY/HER_CHART.md): rare, and never right away. NORMAL is
## the game's. FAST is the owner's playtest start, HER CHART (FAST) on the
## development START list: the same rules on a short clock, so a page and an
## echo come within minutes instead of the better part of an hour. Nothing
## here is simulation state.

const NORMAL := &"normal"
const FAST := &"fast"

var name := NORMAL
# A look counts in her chart once the eyes have rested on one thing this long
# in all (seconds of play).
var look_min_s := 20.0
# Nothing is written about a moment until it is this old: late, never instant.
var late_s := 240.0
# The first page waits for this much play; each page after it waits this long
# after the one before.
var first_page_s := 720.0
var page_gap_s := 1500.0
# Samples (two a second, standing still near a drop) before the chart says
# how the player stands at an edge.
var edge_samples_min := 40
# An echo plays a clip at least this old, at least this long after the last
# echo, in a stretch at least this quiet.
var echo_min_age_s := 900.0
var echo_gap_s := 1200.0
var quiet_s := 20.0
# Clips are deleted at this age, whatever else.
var clip_max_age_s := 7.0 * 86400.0
# Where this profile keeps its chart and clips (user://): the playtest's
# never mixes with the game's.
var storage_dir := "user://her_chart"


func configure(profile: StringName) -> void:
	if profile != FAST:
		return
	name = FAST
	look_min_s = 4.0
	late_s = 20.0
	first_page_s = 75.0
	page_gap_s = 120.0
	edge_samples_min = 8
	echo_min_age_s = 60.0
	echo_gap_s = 90.0
	quiet_s = 6.0
	clip_max_age_s = 86400.0
	storage_dir = "user://her_chart_fast"
