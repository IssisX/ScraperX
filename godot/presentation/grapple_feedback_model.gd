extends RefCounted
# Read-only native receipts to presentation. No capture/contact inference.
const NATIVE_HZ := 90.0
var _tick := -1
var _deaths := -1
var _launch_j := 0.0
var _reel_j := 0.0
var _length := 0.0
var _attached := false
var _danger := false
var _warning_tick := -100000
var _targets := {"equipped": false, "power": 0.0, "winding": 0.0, "load": 0.0, "danger": 0.0}

func reset() -> void:
	_tick = -1
	_deaths = -1
	_attached = false
	_danger = false
	_warning_tick = -100000
	_targets = {"equipped": false, "power": 0.0, "winding": 0.0, "load": 0.0, "danger": 0.0}

func sample(tick: int, deaths: int, gun: Dictionary) -> Dictionary:
	var out := _targets.duplicate()
	out.merge({"fresh": false, "reset": false, "launch": false, "failed": false, "warning": false, "wound_m": 0.0})
	if tick <= _tick and deaths == _deaths:
		return out
	var launch_j := float(gun.get("total_launch_work_j", 0.0))
	var reel_j := float(gun.get("reel_work_j", 0.0))
	var length := float(gun.get("paid_out_length", 0.0))
	var tension := float(gun.get("tension_n", 0.0))
	if not is_finite(launch_j) or not is_finite(reel_j) or not is_finite(length) or not is_finite(tension) \
			or launch_j < 0.0 or reel_j < 0.0 or length < 0.0 or tension < 0.0:
		reset()
		return {"equipped": false, "fresh": false, "reset": true, "launch": false, "failed": false,
			"warning": false, "wound_m": 0.0, "power": 0.0, "winding": 0.0, "load": 0.0, "danger": 0.0}
	var equipped := bool(gun.get("equipped", false))
	var attached := bool(gun.get("line_attached", false))
	var state := int(gun.get("state", 0))
	var seed := _tick < 0 or deaths != _deaths or launch_j < _launch_j or reel_j < _reel_j
	var seconds := float(tick - _tick) / NATIVE_HZ
	var active := equipped and attached and state == 2
	var power := 0.0
	var winding := 0.0
	var wound := 0.0
	if not seed and seconds > 0.0:
		out["launch"] = equipped and launch_j > _launch_j
		# Line-specific failure only: rejected aim/recovery is not a snapped line.
		out["failed"] = equipped and _attached and not attached and state == 4 \
			and str(gun.get("last_failure", "")) in ["LINE SNAGGED", "LINE BROKEN"]
		if active and _attached:
			power = clampf((reel_j - _reel_j) / seconds / 750.0, 0.0, 1.0)
			# Slack take-up can cost zero command work. Its actual decreasing
			# native line length still earns quiet winding, never loaded power.
			wound = clampf(_length - length, 0.0, seconds)
			winding = clampf(wound / seconds, 0.0, 1.0)
	var load := clampf(tension / 1500.0, 0.0, 1.0) if active else 0.0
	var danger := clampf(tension / 3000.0, 0.0, 1.0) if equipped and attached else 0.0
	var next_danger := danger >= (0.7 if _danger else 0.8)
	out["warning"] = not seed and next_danger and not _danger and tick - _warning_tick >= 45
	if out["warning"]:
		_warning_tick = tick
	_danger = next_danger
	_tick = tick
	_deaths = deaths
	_launch_j = launch_j
	_reel_j = reel_j
	_length = length
	_attached = attached
	_targets = {"equipped": equipped, "power": power, "winding": winding, "load": load, "danger": danger}
	out.merge(_targets, true)
	out["fresh"] = true
	out["reset"] = seed
	out["wound_m"] = wound
	return out
