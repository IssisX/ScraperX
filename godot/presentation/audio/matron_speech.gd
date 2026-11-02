extends RefCounted
## Her spoken lines (presentation/audio/matron_voice/, listed by matron_voice_lines.gd): which line a
## moment of hers says, and which of its three deliveries. All of 03_EXECUTION/MATRON_REDESIGN/
## HER_LINES_DRAFT.md is kept, with variety and none repeated (the owner, 2026-10-29), and while she holds
## him no sound of hers plays twice running (rule 42): a moment draws its lines from a shuffled round, never
## the line just said; a line draws its deliveries from a shuffled round, never the clip just played, so
## neither a line nor a clip comes twice running, in a hold or out of it.
##
## Choosing only: the audio director plays what is chosen (matron_say), and `log` has one entry for each
## line said, whether or not a device mixes it, for the scripted runs.

const Table := preload("res://presentation/audio/matron_voice_lines.gd")

# One entry per line said: {moment, line, clip, encounter}.
var log: Array = []
var _rng := RandomNumberGenerator.new()
var _bags := {}          # a moment's lines, or a line's clips, not yet drawn this round
var _last_line := 0
var _last_clip := ""
var _streams := {}       # clip file -> AudioStreamOggVorbis


func _init(seed_value: int = 0) -> void:
	if seed_value != 0:
		_rng.seed = seed_value
	else:
		_rng.randomize()


# The line and clip `moment` says now, or {} when it has none. `encounter` is the hold she is in (0:
# none), kept in the log.
func choose(moment: StringName, encounter: int = 0) -> Dictionary:
	var lines: Dictionary = Table.MOMENTS.get(moment, {})
	var numbers: Array = []
	for number in lines:
		if not (lines[number] as Array).is_empty():
			numbers.append(number)
	if numbers.is_empty():
		return {}
	var line: int = _draw(moment, numbers, _last_line)
	var clips: Array = lines[line]
	var clip: String = _draw(line, clips, _last_clip)
	_last_line = line
	_last_clip = clip
	var said := {"moment": moment, "line": line, "clip": clip, "encounter": encounter}
	log.append(said)
	return said


# The stream of a clip file, read once. Ogg pages are decoded as they play.
func stream(clip: String) -> AudioStream:
	if not _streams.has(clip):
		var loaded := AudioStreamOggVorbis.load_from_file(Table.DIR + clip)
		if loaded == null:
			push_error("her voice: cannot read %s%s" % [Table.DIR, clip])
		_streams[clip] = loaded
	return _streams[clip]


# What the line says, as written.
func caption(line: int) -> String:
	return String(Table.CAPTIONS.get(line, ""))


# One of `options` from the key's shuffled round, never `avoid` when there is another. A round that
# would end on `avoid` is started again without it in front.
func _draw(key: Variant, options: Array, avoid: Variant) -> Variant:
	if options.size() == 1:
		return options[0]
	var bag: Array = _bags.get(key, [])
	if bag.is_empty():
		bag = options.duplicate()
		_shuffle(bag)
	var at := 0
	if bag[0] == avoid:
		if bag.size() == 1:
			bag = options.duplicate()
			_shuffle(bag)
			if bag[0] == avoid:
				bag.push_back(bag.pop_front())
		else:
			at = 1
	var pick: Variant = bag[at]
	bag.remove_at(at)
	_bags[key] = bag
	return pick


func _shuffle(items: Array) -> void:
	for i in range(items.size() - 1, 0, -1):
		var j := _rng.randi_range(0, i)
		var held: Variant = items[i]
		items[i] = items[j]
		items[j] = held
