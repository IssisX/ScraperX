extends Node

# J-031: script errors and push_error() reports from real play on the phone go
# to Sentry (project "scraperx"), so Cory's crashes-in-the-hand reach us without
# him copying logs. Errors only: no warnings, no breadcrumbs, no PII (no user,
# no device name, server_name empty).
#
# Inert everywhere but an exported Android game with a real display: CI, the
# godot-touch scenarios, headless runs and the editor never add the logger,
# never make an HTTPRequest and never send (the free plan is 5,000 errors a
# month and CI runs hundreds of scenarios with known errors).
#
# Bounds per session: dedupe by message+file+line, at most MAX_EVENTS events,
# a queue of at most MAX_QUEUE (the newest is dropped when full), one request
# in flight, TIMEOUT_S per request, no retries. Godot calls the Logger from any
# thread, so the callback only queues under a Mutex; the main thread drains it
# in _process with an HTTPRequest node, never blocking a frame.
#
# Format: develop.sentry.dev/sdk/foundations/envelopes/ (envelope) and
# .../envelopes/event-payloads/ (event, logentry, stacktrace) plus
# develop.sentry.dev/sdk/telemetry/errors/#wire-format (exception).
# Native crashes (signals) are not caught here: that needs the official SDK
# and a Gradle export, a later protected job.

const DSN := "https://c3c9ac33eb98efdac8832ff34c0555e0@o4512379465891840.ingest.us.sentry.io/4512379490664448"
const ENVELOPE_URL := "https://o4512379465891840.ingest.us.sentry.io/api/4512379490664448/envelope/"
const AUTH_HEADER := "X-Sentry-Auth: Sentry sentry_version=7, sentry_key=c3c9ac33eb98efdac8832ff34c0555e0, sentry_client=scraperx-godot/1.0"
const SDK_NAME := "scraperx.godot"
const SDK_VERSION := "1.0"
const MAX_EVENTS := 30
const MAX_QUEUE := 30
const MAX_SEEN := 512
const TIMEOUT_S := 10.0
const ERROR_TYPE_NAMES := ["error", "warning", "script_error", "shader_error"]


# Shared between the Logger (any thread) and the node (main thread). Every
# field is touched only while holding mutex.
class Sink extends RefCounted:
	var mutex := Mutex.new()
	var queue: Array[Dictionary] = []
	var seen := {}
	var accepted := 0
	var dropped := 0

	func offer(record: Dictionary) -> void:
		mutex.lock()
		var key := "%s|%s|%d" % [record["message"], record["file"], record["line"]]
		if accepted >= MAX_EVENTS or seen.has(key) or seen.size() >= MAX_SEEN:
			mutex.unlock()
			return
		seen[key] = true
		if queue.size() >= MAX_QUEUE:
			dropped += 1
		else:
			queue.append(record)
			accepted += 1
		mutex.unlock()

	func take() -> Dictionary:
		mutex.lock()
		var record: Dictionary = queue.pop_front() if not queue.is_empty() else {}
		mutex.unlock()
		return record


class ReporterLogger extends Logger:
	var sink: Sink

	func _init(p_sink: Sink) -> void:
		sink = p_sink

	func _log_message(_message: String, _error: bool) -> void:
		pass

	func _log_error(function: String, file: String, line: int, code: String,
			rationale: String, _editor_notify: bool, error_type: int,
			script_backtraces: Array[ScriptBacktrace]) -> void:
		if error_type == ERROR_TYPE_WARNING:
			return
		# Plain values only: nothing from this thread outlives the call.
		var frames: Array[Dictionary] = []
		for bt in script_backtraces:
			if bt == null or bt.is_empty():
				continue
			# ScriptBacktrace frame 0 is the most recent; Sentry wants oldest first.
			for i in range(bt.get_frame_count() - 1, -1, -1):
				frames.append({"function": bt.get_frame_function(i),
						"filename": bt.get_frame_file(i), "lineno": bt.get_frame_line(i),
						"in_app": true})
			break
		sink.offer({
			"message": rationale if not rationale.is_empty() else code,
			"code": code, "function": function, "file": file, "line": line,
			"error_type": error_type, "frames": frames,
			"unix_time": Time.get_unix_time_from_system(),
		})


var active := false
var sink: Sink
var logger: ReporterLogger
var _send: Callable
var _http: HTTPRequest
var _busy := false
var _registered := false


static func should_activate() -> bool:
	if not OS.has_feature("android") or OS.has_feature("editor") or Engine.is_editor_hint():
		return false
	if DisplayServer.get_name() == "headless":
		return false
	for arg in OS.get_cmdline_args() + OS.get_cmdline_user_args():
		if arg.begins_with("--uitest") or arg == "--headless":
			return false
	return true


func _init() -> void:
	process_mode = Node.PROCESS_MODE_ALWAYS
	if should_activate():
		activate(Callable(), true)


# sender(body: String) -> void replaces the network send (tests). A sender
# must call request_finished() when it is done, as the HTTPRequest does.
# register_logger false keeps a forced-on test instance off the OS logger list.
func activate(sender: Callable, register_logger: bool) -> void:
	if active:
		return
	active = true
	sink = Sink.new()
	logger = ReporterLogger.new(sink)
	_send = sender if sender.is_valid() else _http_send
	if register_logger:
		OS.add_logger(logger)
		_registered = true


func _ready() -> void:
	if active and not _http and _send == _http_send:
		_http = HTTPRequest.new()
		_http.timeout = TIMEOUT_S
		_http.request_completed.connect(func(_r, _c, _h, _b): request_finished())
		add_child(_http)
	set_process(active)


func _exit_tree() -> void:
	if _registered:
		OS.remove_logger(logger)
		_registered = false


func _process(_delta: float) -> void:
	if _busy:
		return
	var record := sink.take()
	if record.is_empty():
		return
	_busy = true
	_send.call(build_envelope(record))


func request_finished() -> void:
	_busy = false


func _http_send(body: String) -> void:
	if not _http:
		_busy = false
		return
	var headers := PackedStringArray([AUTH_HEADER, "Content-Type: application/x-sentry-envelope"])
	if _http.request(ENVELOPE_URL, headers, HTTPClient.METHOD_POST, body) != OK:
		_busy = false


# RFC 3339 in UTC with milliseconds (a float through JSON.stringify can lose digits).
static func rfc3339_utc(unix: float) -> String:
	var whole := int(unix)
	return "%s.%03dZ" % [Time.get_datetime_string_from_unix_time(whole), int((unix - whole) * 1000.0)]


static func release_name() -> String:
	var version := str(ProjectSettings.get_setting("application/config/version", ""))
	return "scraperx@" + (version if not version.is_empty() else "unknown")


static func new_event_id() -> String:
	var b := Crypto.new().generate_random_bytes(16)
	b[6] = (b[6] & 0x0f) | 0x40  # uuid4 version
	b[8] = (b[8] & 0x3f) | 0x80  # RFC 4122 variant
	return b.hex_encode()


static func build_event(record: Dictionary, event_id: String) -> Dictionary:
	var frames: Array = record["frames"]
	if frames.is_empty():
		frames = [{"function": record["function"], "filename": record["file"],
				"lineno": record["line"], "in_app": true}]
	var type_index: int = record["error_type"]
	var type_name: String = ERROR_TYPE_NAMES[type_index] if type_index >= 0 and type_index < ERROR_TYPE_NAMES.size() else "error"
	return {
		"event_id": event_id,
		"timestamp": rfc3339_utc(record["unix_time"]),
		"platform": "other",
		"level": "error",
		"logger": "godot",
		"release": release_name(),
		"environment": "phone",
		"server_name": "",
		"sdk": {"name": SDK_NAME, "version": SDK_VERSION},
		"logentry": {"formatted": record["message"]},
		"exception": {"values": [{
			"type": type_name,
			"value": record["message"],
			"mechanism": {"type": "godot.logger", "handled": true},
			"stacktrace": {"frames": frames},
		}]},
		"tags": {"godot_error_type": type_name, "godot_version": Engine.get_version_info()["string"]},
		"extra": {"code": record["code"]},
		"contexts": {"os": {"name": OS.get_name(), "version": OS.get_version()}},
	}


static func build_envelope(record: Dictionary) -> String:
	var event_id := new_event_id()
	var payload := JSON.stringify(build_event(record, event_id))
	var header := JSON.stringify({"event_id": event_id, "dsn": DSN,
			"sent_at": rfc3339_utc(Time.get_unix_time_from_system())})
	var item := JSON.stringify({"type": "event", "content_type": "application/json",
			"length": payload.to_utf8_buffer().size()})
	return header + "\n" + item + "\n" + payload + "\n"
