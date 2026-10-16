extends RefCounted

# Stable APK signing writes build_info.json with the final manifest code and source.
# Editor and preexport runs must never claim the not-yet-selected Android code.
static func label(path: String = "res://build_info.json") -> String:
	if not FileAccess.file_exists(path):
		return "LOCAL DEVELOPMENT"
	var parser := JSON.new()
	if parser.parse(FileAccess.get_file_as_string(path)) != OK or not parser.data is Dictionary:
		return "LOCAL DEVELOPMENT"
	var info: Dictionary = parser.data
	var source: Variant = info.get("source_sha")
	if not source is String or source.length() != 40:
		return "LOCAL DEVELOPMENT"
	for character in source:
		if not character in "0123456789abcdef":
			return "LOCAL DEVELOPMENT"
	var code: Variant = info.get("version_code")
	if code == null:
		return "DEVELOPMENT  /  SOURCE " + source.left(12)
	if not (code is int or code is float) or not is_finite(float(code)):
		return "LOCAL DEVELOPMENT"
	if float(code) != floorf(float(code)) or code < 1 or code > 2100000000:
		return "LOCAL DEVELOPMENT"
	return "BUILD %d  /  SOURCE %s" % [int(code), source.left(12)]
