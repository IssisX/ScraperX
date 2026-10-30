#!/bin/sh
# Rebuilds 03_EXECUTION/TEST_REACH.md from the current checkout (README.md).
set -e
cd "$(dirname "$0")"
python3 cg.py            # call graph of src/, tests/ and the bridge
python3 native.py >/dev/null && python3 gen_native.py && python3 subject.py >/dev/null
python3 gd.py && python3 gd_an.py >/dev/null && python3 gen_gd.py
python3 assemble.py
