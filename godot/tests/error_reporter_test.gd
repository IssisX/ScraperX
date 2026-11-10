extends SceneTree
## J-031: the Sentry error reporter stays inert off the phone, and a forced-on
## instance turns a Logger error into a valid Sentry envelope, within its
## bounds. The send is injected, so nothing touches the network.
##   godot --headless --path godot --script res://tests/error_reporter_test.gd

const Reporter := preload("res://presentation/error_reporter.gd")
var failures := 0
var sent: Array[String] = []


func check(condition: bool, description: String) -> void:
	if not condition:
		failures += 1
		print("ERROR_REPORTER FAIL ", description)


func capture(body: String) -> void:
	sent.append(body)


func log_error(r, message: String, file: String, line: int, type: int,
		backtraces: Array[ScriptBacktrace] = []) -> void:
	r.logger._log_error("do_thing", file, line, "cond", message, false, type, backtraces)


# Envelope = header line, item header line, payload of exactly `length` bytes, "\n".
func parse_envelope(body: String) -> Dictionary:
	var lines := body.split("\n")
	check(body.ends_with("\n") and lines.size() == 4 and lines[3] == "", "envelope has three lines and a final newline")
	if lines.size() < 3:
		return {}
	var header = JSON.parse_string(lines[0])
	var item = JSON.parse_string(lines[1])
	var event = JSON.parse_string(lines[2])
	check(header is Dictionary and item is Dictionary and event is Dictionary, "every line is JSON")
	if not (header is Dictionary and item is Dictionary and event is Dictionary):
		return {}
	check(header["dsn"] == Reporter.DSN, "header dsn")
	check(header["event_id"] == event["event_id"], "header event_id matches event")
	check(str(header["sent_at"]).ends_with("Z"), "sent_at is UTC")
	check(item["type"] == "event" and item["content_type"] == "application/json", "item header")
	check(int(item["length"]) == lines[2].to_utf8_buffer().size(), "item length is payload bytes")
	return event


func _initialize() -> void:
	# 1. Off the phone the autoload does nothing.
	check(not Reporter.should_activate(), "should_activate false headless on desktop")
	var autoload := root.get_node_or_null("ErrorReporter")
	if autoload:
		check(not autoload.active and autoload.logger == null, "autoload inert")
		check(autoload.get_child_count() == 0, "autoload made no HTTPRequest")
	var idle = Reporter.new()
	check(not idle.active and idle.logger == null and idle.sink == null, "fresh instance inert")
	idle.free()

	# 2. Forced on with an injected sender and no OS logger registration.
	var r = Reporter.new()
	r.activate(capture, false)
	root.add_child(r)
	check(r.get_child_count() == 0, "injected sender makes no HTTPRequest")
	log_error(r, "boom", "res://presentation/x.gd", 12, Logger.ERROR_TYPE_SCRIPT)
	log_error(r, "boom", "res://presentation/x.gd", 12, Logger.ERROR_TYPE_SCRIPT)  # duplicate
	log_error(r, "careful", "res://presentation/x.gd", 13, Logger.ERROR_TYPE_WARNING)  # warning
	log_error(r, "pushed", "res://presentation/y.gd", 7, Logger.ERROR_TYPE_ERROR,
			Engine.capture_script_backtraces())
	check(r.sink.accepted == 2, "dedupe and warnings skipped (accepted %d)" % r.sink.accepted)

	r._process(0.0)
	r._process(0.0)  # one in flight: no second send before request_finished
	check(sent.size() == 1, "one request in flight (sent %d)" % sent.size())
	var event := parse_envelope(sent[0]) if sent.size() > 0 else {}
	if not event.is_empty():
		check(str(event["event_id"]).length() == 32 and str(event["event_id"]) == str(event["event_id"]).to_lower(), "event_id 32 lowercase hex")
		check(event["platform"] == "other" and event["level"] == "error" and event["environment"] == "phone", "platform level environment")
		check(event["server_name"] == "" and not event.has("user"), "no PII")
		check(str(event["release"]) == "scraperx@0.1.0", "release from config/version (%s)" % event["release"])
		check(str(event["timestamp"]).ends_with("Z"), "timestamp RFC 3339 UTC")
		check(event["logentry"]["formatted"] == "boom", "message")
		var ex: Dictionary = event["exception"]["values"][0]
		check(ex["type"] == "script_error" and ex["value"] == "boom", "exception type and value")
		var frame: Dictionary = ex["stacktrace"]["frames"][-1]
		check(frame["filename"] == "res://presentation/x.gd" and int(frame["lineno"]) == 12 and frame["function"] == "do_thing", "fallback frame from file/line/function")

	r.request_finished()
	r._process(0.0)
	check(sent.size() == 2, "next event after request_finished")
	if sent.size() == 2:
		var pushed := parse_envelope(sent[1])
		if not pushed.is_empty():
			var frames: Array = pushed["exception"]["values"][0]["stacktrace"]["frames"]
			check(frames.size() >= 1 and pushed["exception"]["values"][0]["type"] == "error", "push_error event has frames")
			if Engine.capture_script_backtraces().size() > 0:
				check(str(frames[-1]["filename"]).ends_with("error_reporter_test.gd"), "backtrace frames oldest first, test file last")

	# 3. At most MAX_EVENTS per session, whatever arrives.
	for i in 60:
		log_error(r, "flood %d" % i, "res://presentation/z.gd", i, Logger.ERROR_TYPE_SCRIPT)
	check(r.sink.accepted == Reporter.MAX_EVENTS, "capped at %d (accepted %d)" % [Reporter.MAX_EVENTS, r.sink.accepted])
	check(r.sink.queue.size() <= Reporter.MAX_QUEUE, "queue bounded")
	for i in 100:
		r.request_finished()
		r._process(0.0)
	check(sent.size() == Reporter.MAX_EVENTS, "sent exactly %d (sent %d)" % [Reporter.MAX_EVENTS, sent.size()])

	r.queue_free()
	print("ERROR_REPORTER %s" % ("PASS" if failures == 0 else "FAIL"))
	quit(1 if failures else 0)
