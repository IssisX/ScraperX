# Climbing handoff baseline

Base: f7e792843deca85827ab48a4034e8634a83cb5fb, ChatGPT.

Exact-source GitHub full run38969060848 fails AS-026 receiver arrest and AS-024 loaded cargo pause. The receiver reaches1.76349m on its left hand at tick1550, beyond the existing1.75m envelope; the next tick releases both hands. AS-024 fails lane20 after lane19 passes; the combined assertion does not identify the first changed field.

The same published native source and test inputs pass Linux ARM64 against the source-matched startup archive: suspended ladder exit0 in52.15s; cargo route lanes19/20/21 exit0 in114.29s. Commands, outputs and archive/source hashes are retained in `/data/data/com.termux/files/usr/tmp/scraperx-climbing-handoff-20261017/baseline-result.json`. This ARM result cannot stand in for failing x86 behavior.

This change adds diagnostic output and enables only the two existing checks in the existing focused native CI runner. No physical law, test input, assertion, threshold or deadline changes. The next repair must follow the first invalid x86 transition; no shared cause is assumed. Existing catch-net and inspection failures stay deferred.
