#!/usr/bin/env python3
"""Persistent limits and prerequisite gates for local mechanics experiments.

This controls child executions, not model judgment or OS permissions. Keep one
state file per unresolved mechanism across sessions; do not reset by renaming it.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import time


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def save(path, state):
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(json.dumps(state, indent=2) + "\n")
    temporary.replace(path)


def run(args):
    path = args.state.resolve()
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.with_suffix(path.suffix + ".lock").open("a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        if args.action == "start":
            if path.exists():
                raise ValueError("state exists; use status or evidence-based reassess")
            if not 0 < args.budget <= 1200:
                raise ValueError("budget must be positive and at most1200seconds")
            save(path, {"objective": args.objective, "budget_seconds": args.budget,
                        "started": time.time(), "failures": 0, "trials": [],
                        "reassessments": [], "pending": False})
            return 0
        state = json.loads(path.read_text())
        if args.action == "status":
            print(json.dumps(state, indent=2))
            return 0
        if args.action == "reassess":
            if not args.cause.strip() or not args.change.strip() or not args.evidence.is_file() or not args.evidence.stat().st_size:
                raise ValueError("reassessment needs actual evidence, supported cause and next change")
            state["reassessments"].append({"at": time.time(), "cause": args.cause,
                "change": args.change, "evidence": str(args.evidence.resolve()),
                "evidence_sha256": digest(args.evidence), "previous_failures": state["failures"],
                "previous_started": state["started"], "previous_pending": state["pending"]})
            state.update(started=time.time(), failures=0, pending=False)
            save(path, state)
            return 0
        if args.action == "reuse":
            receipt = json.loads(args.receipt.read_text())
            sources = receipt.get("sources", {})
            if receipt.get("exit") != 0 or not sources or any(
                    not Path(p).is_file() or digest(p) != h for p, h in sources.items()):
                raise ValueError("reuse requires a passing receipt with matching actual sources/archives")
            state["trials"].append({"stage": args.stage, "exit": 0, "sources": sources,
                "reused_receipt": str(args.receipt.resolve()), "receipt_sha256": digest(args.receipt)})
            save(path, state)
            return 0
        remaining = state["budget_seconds"] - (time.time() - state["started"])
        if state["pending"] or state["failures"] >= 2 or remaining <= 0:
            raise ValueError("STOP: failed/interrupted/time budget reached; record causal reassessment")
        if not args.command:
            raise ValueError("child command required after --")
        for name in args.requires:
            matches = [t for t in state["trials"] if t["stage"] == name]
            if not matches or matches[-1].get("exit") != 0:
                raise ValueError("STOP: no passing prerequisite: " + name)
            prerequisite = matches[-1]
            if any(not Path(p).is_file() or digest(p) != h for p, h in prerequisite["sources"].items()):
                raise ValueError("STOP: prerequisite source changed: " + name)
        sources = {str(p.resolve()): digest(p) for p in args.source}
        if not sources:
            raise ValueError("record at least one actual source/archive")
        if any(t.get("command") == args.command and t["sources"] == sources and
               t.get("started", 0) >= state["started"] for t in state["trials"]):
            raise ValueError("STOP: matching experiment already executed; reuse result or reassess new evidence")
        trial = {"stage": args.stage, "command": args.command, "sources": sources,
                 "started": time.time()}
        state["trials"].append(trial)
        state["pending"] = True
        save(path, state)
        log = path.parent / (path.stem + ".trial-" + str(len(state["trials"])) + ".log")
        trial["log"] = str(log)
        child = None
        try:
            with log.open("w") as output:
                child = subprocess.Popen(args.command, stdout=output, stderr=subprocess.STDOUT,
                                         start_new_session=True)
                result = child.wait(timeout=remaining)
        except subprocess.TimeoutExpired:
            # Kill only the new trial process group, including normal launcher
            # descendants; never match unrelated builds by process name.
            os.killpg(child.pid, signal.SIGKILL)
            child.wait()
            result = 124
        except OSError as error:
            log.write_text(str(error) + "\n")
            result = 127
        except BaseException:
            if child is not None:
                try:
                    os.killpg(child.pid, signal.SIGKILL)
                except ProcessLookupError:
                    pass
                child.wait()
            raise
        trial.update(exit=result, finished=time.time())
        state["pending"] = False
        if result:
            state["failures"] += 1
        save(path, state)
        print(json.dumps(trial), flush=True)
        return result if result >= 0 else 128 - result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--state", type=Path, required=True)
    sub = parser.add_subparsers(dest="action", required=True)
    start = sub.add_parser("start")
    start.add_argument("--objective", required=True)
    start.add_argument("--budget", type=float, default=600)
    sub.add_parser("status")
    reuse = sub.add_parser("reuse")
    reuse.add_argument("--stage", required=True)
    reuse.add_argument("--receipt", type=Path, required=True)
    reassess = sub.add_parser("reassess")
    reassess.add_argument("--evidence", type=Path, required=True)
    reassess.add_argument("--cause", required=True)
    reassess.add_argument("--change", required=True)
    execute = sub.add_parser("run")
    execute.add_argument("--stage", required=True)
    execute.add_argument("--source", type=Path, action="append", default=[])
    execute.add_argument("--requires", action="append", default=[])
    execute.add_argument("command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if getattr(args, "command", None) and args.command[0] == "--":
        args.command = args.command[1:]
    try:
        return run(args)
    except (ValueError, OSError) as error:
        parser.exit(75, str(error) + "\n")


if __name__ == "__main__":
    raise SystemExit(main())
