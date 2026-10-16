extends RefCounted

# Stable APK signing writes build_info.json with the final manifest code and source.
# Editor and preexport runs must never claim the not-yet-selected Android code.
static func read_identity(path: String = "res://build_info.json") -> Dictionary:
	if not FileAccess.file_exists(path):
		return {}
	var parser := JSON.new()
	if parser.parse(FileAccess.get_file_as_string(path)) != OK or not parser.data is Dictionary:
		return {}
	var info: Dictionary = parser.data
	var source: Variant = info.get("source_sha")
	if not source is String or source.length() != 40:
		return {}
	for character in source:
		if not character in "0123456789abcdef":
			return {}
	var code: Variant = info.get("version_code")
	if code == null:
		return {"source_sha": source}
	if not (code is int or code is float) or not is_finite(float(code)):
		return {}
	if float(code) != floorf(float(code)) or code < 1 or code > 2100000000:
		return {}
	return {"version_code": int(code), "source_sha": source}


static func label(path: String = "res://build_info.json") -> String:
	var info := read_identity(path)
	if info.is_empty():
		return "LOCAL DEVELOPMENT"
	if not info.has("version_code"):
		return "DEVELOPMENT  /  SOURCE " + str(info["source_sha"]).left(12)
	return "BUILD %d  /  SOURCE %s" % [int(info["version_code"]), str(info["source_sha"]).left(12)]
