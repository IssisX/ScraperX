"""Private, bounded native falsifiers. Failed convergence is retained as evidence."""
from pathlib import Path
import hashlib
import itertools
import json
import subprocess
import time

ROOT = Path(__file__).resolve().parent
EXE = ROOT / "rope-ladder-probe"
CASES = {
    "still": ("unloaded", 0, 0, 0, 0),
    "breeze": ("unloaded", 1, 0, 0, 0),
    "reverse": ("unloaded", -1, 0, 0, 0),
    "free_sway": ("unloaded", 0, 0, 0, .025),
    "central_catch": ("rider", 1, 0, 2.5, 0),
    "left_catch": ("rider", 1, -.25, 2.5, 0),
    "right_catch": ("rider", 1, .25, 2.5, 0),
}
results = {}
for name, args in CASES.items():
    pair = {}
    for hz in (90, 360):
        command = ["proot-distro", "login", "ubuntu", "--shared-tmp", "--", str(EXE), repr(1 / hz), *map(str, args), "64"]
        start = time.monotonic()
        done = subprocess.run(command, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        log = ROOT / f"case-{name}-{hz}.log"
        log.write_text(done.stdout + "\nSTDERR\n" + done.stderr)
        line = next((line for line in done.stdout.splitlines() if line.startswith("RESULT ")), None)
        if line is None:
            raise RuntimeError(f"No result for {name}/{hz}: exit {done.returncode}; see {log}")
        result = json.loads(line[7:])
        result.update(exit_code=done.returncode, wall_seconds=time.monotonic() - start, command=command, log=log.name)
        pair[str(hz)] = result
        print(f"{name}/{hz}: exit={done.returncode} stretch={result['max_stretch']:.6g}m residual={result['max_residual_j']:.6g}J", flush=True)
    coarse, fine = pair["90"], pair["360"]
    delta = {field: abs(coarse[field] - fine[field]) for field in (
        "tail_z", "tail_y", "max_stretch", "max_rung_speed", "max_hand_error", "max_hand_force", "max_tension", "max_residual_j", "post_startup_residual_j", "climb_rise", "departure_speed", "loaded_root_reaction", "unloaded_root_reaction")}
    failures = []
    for hz, result in pair.items():
        if result["exit_code"] or result["max_lambda"] > 1e-5:
            failures.append(f"{hz}: constraint sign/force/finite-state check")
        if result["max_stretch"] > .001:
            failures.append(f"{hz}: geometric extension exceeds chosen 1mm constraint tolerance")
        if result["release_velocity_jump"] != 0:
            failures.append(f"{hz}: release resets velocity")
        if result["max_power"] > 3000:
            failures.append(f"{hz}: measured positive motor work exceeds proposed 3kW source")
    # 0.5m/s speed margin protecting catch clearance -> one tenth = 0.05m/s.
    if delta["max_rung_speed"] > .05:
        failures.append("peak rung speed does not converge within 0.05m/s")
    if delta["max_hand_error"] > .04:
        failures.append("hand reach deviation does not converge within 0.04m")
    if delta["tail_z"] > .04 or delta["tail_y"] > .04:
        failures.append("tail endpoint does not converge within 0.04m")
    results[name] = dict(runs=pair, differences=delta, failures=failures,
                         evidence_class="UNRESOLVED" if failures else "MEASURED",
                         note="Native feasibility metrics; mechanical work ledger is not a complete rope-material or hand-muscle energy model.")
hashes = {path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in (ROOT / "rope_ladder_probe.cpp", EXE, Path(__file__))}
report = dict(profile="MACRO-TRAVERSAL-STRICT", topology="60 lightweight rigid rungs,120 tension-only side-strand limits", status="FEASIBILITY_ONLY", cases=results, hashes=hashes,
              boundaries=["No repository/gameplay integration", "Stock hard DistanceConstraint has no speculative taut activation", "Shipping upright capsule rotational constraint retained", "No independent rope midpoint inertia, rope contact or strand damage", "No complete microscopic motor storage/dissipation ledger", "No Godot render, route placement, Android or device proof"])
(ROOT / "native-cases.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps({"case_count": len(results), "failed_cases": {name: result["failures"] for name, result in results.items() if result["failures"]}, "hashes": hashes}, indent=2))
