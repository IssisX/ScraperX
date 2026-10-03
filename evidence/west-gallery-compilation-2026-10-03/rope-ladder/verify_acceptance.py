"""Rejects a feasibility result that fails declared physics gates."""
from pathlib import Path
import json
import sys

report = json.loads((Path(__file__).resolve().parent / "native-cases.json").read_text())
failed = {name: case["failures"] for name, case in report["cases"].items() if case["failures"]}
print(json.dumps({"status": "REJECTED" if failed else "FEASIBILITY_ONLY", "failed_cases": failed}, indent=2))
sys.exit(1 if failed else 0)
