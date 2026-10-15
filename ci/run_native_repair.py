#!/usr/bin/env python3
"""Build only named native owners; this never exports or certifies an APK."""
import json
from pathlib import Path
import re
import subprocess


OWNERS = {
    "suspended_ladder_route": "scraperx_suspended_ladder_tests",
    "swing_stair_route_2": "scraperx_swing_stair_route_tests",
    "split_inspection_route": "scraperx_split_inspection_route_tests",
}


def main():
    root = Path(__file__).resolve().parent.parent
    request = json.loads((root / "ci/native-repair-request.json").read_text())
    checks = request["checks"]
    if not checks or len(set(checks)) != len(checks) or any(c not in OWNERS for c in checks):
        raise ValueError("request must name distinct supported native checks")
    build = root / "build/native-repair"
    evidence = root / "artifacts/native-repair"
    evidence.mkdir(parents=True, exist_ok=True)
    receipt = {"source_commit": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
               "scope": "selected native diagnosis only; no APK/full proof", "request": request, "commands": []}
    commands = [
        ["cmake", "-S", str(root), "-B", str(build), "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release",
         "-DSCRAPERX_BUILD_BRIDGE=OFF", "-DSCRAPERX_WITH_JOLT=ON"],
        ["cmake", "--build", str(build), "--parallel", "2", "--target"] + sorted({OWNERS[c] for c in checks}),
        ["ctest", "--test-dir", str(build), "--parallel", "2", "--output-on-failure", "--no-tests=error",
         "--output-junit", str(evidence / "native-tests.xml"), "-R",
         "^scraperx_sim\\.(" + "|".join(re.escape(c) for c in checks) + ")$"],
    ]
    for index, command in enumerate(commands):
        # Stream live output and retain it. The command's actual exit controls
        # this workflow, independently of what appears in its text output.
        with (evidence / (str(index) + ".log")).open("w") as log:
            child = subprocess.Popen(command, cwd=root, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
            for line in child.stdout:
                print(line, end="", flush=True)
                log.write(line)
            result = child.wait()
        receipt["commands"].append({"argv": command, "exit": result})
        (evidence / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
        if result:
            return result
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
