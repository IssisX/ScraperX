"""Partition the preserved facade oracle into native finite-body ownership.

Read only the original pre-extraction numeric record, never current KitView
output. All 69 original boxes retain every channel and material. Only body
ownership and per-batch triangle indices change; the shared local origin stays
(0, -11, 0). The unaffected original records remain compared in full.
"""
import copy
import gzip
import json
from pathlib import Path

root = Path(__file__).resolve().parent
original = json.loads(gzip.decompress((root / "kit-normal-numeric.json.gz").read_bytes()))
facade = next(body[1] for body in original if body[0] == "KitBody1600")
# Original material-batch order: galvanized, rust, yellow, hazard, steel.
materials = dict(zip("GRYHS", facade))
counts = dict(zip("GRYHS", [6, 7, 15, 4, 37]))
assert len(facade) == len(materials)
for name, batch in materials.items():
    assert len(batch[1][0][0][0][1]) == counts[name] * 24


def boxes(batch, start, count):
    result = copy.deepcopy(batch)
    source = batch[1][0][0]
    channels = result[1][0][0]
    for channel, values in enumerate(source):
        if values is None:
            continue
        stride = 36 if channel == 12 else (96 if channel == 2 else 24)
        assert len(values[1]) == len(source[0][1]) // 24 * stride
        channels[channel][1] = copy.deepcopy(values[1][start * stride:(start + count) * stride])
        if channel == 12:
            channels[channel][1] = [index - start * 24 for index in channels[channel][1]]
    return result


# Explicit native ownership; ranges are original ordered material-box indices.
partition = [
    (1600, [("G", 0, 1), ("R", 0, 3), ("Y", 0, 6), ("H", 0, 1)]),
    (2560, [("S", 0, 1), ("H", 1, 1)]),
    (2561, [("G", 1, 1), ("R", 3, 1), ("S", 1, 2)]),
    (2562, [("G", 2, 4), ("S", 3, 3)]),
    (2563, [("Y", 6, 1), ("H", 2, 1), ("R", 4, 1)]),
    (2564, [("R", 5, 1), ("H", 3, 1)]),
    (2565, [("Y", 7, 2), ("R", 6, 1)]),
    (2566, [("Y", 9, 6), ("S", 6, 31)]),
]
coverage = {name: [0] * count for name, count in counts.items()}
records = []
for entity, slices in partition:
    batches = []
    for name, start, count in slices:
        assert start >= 0 and count > 0 and start + count <= counts[name]
        for index in range(start, start + count):
            coverage[name][index] += 1
        batches.append(boxes(materials[name], start, count))
    records.append([f"KitBody{entity}", batches])
assert all(uses == 1 for material in coverage.values() for uses in material)
assert sum(len(material) for material in coverage.values()) == 69
assert len(records) == 8
data = (json.dumps(records, separators=(",", ":")) + "\n").encode()
(root / "kit-causal-facade-numeric.json.gz").write_bytes(gzip.compress(data, mtime=0))
print("Derived facade oracle: all 69 original boxes exactly once across 8 bodies;", len(data), "bytes")
