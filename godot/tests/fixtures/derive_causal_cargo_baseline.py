"""Derive only the intentional cargo changes from the preserved pre-extraction oracle.

This does not read current KitView output. Original box channels supply topology,
UVs, normals, tangents, materials and signs; explicit section geometry supplies
only changed corners/ownership. All other original bodies remain compared in full.
"""
import copy
import gzip
import json
from pathlib import Path

root = Path(__file__).resolve().parent
original = json.loads(gzip.decompress((root / "kit-normal-numeric.json.gz").read_bytes()))
cargo = next(body[1] for body in original if body[0] == "KitBody1952")
# Original material-batch order: deck, hazard, columns, concrete, steel, rails.
deck, hazard, columns, concrete, steel, rails, sign = cargo


def boxes(batch, start, count):
    result = copy.deepcopy(batch)
    source = batch[1][0][0]
    channels = result[1][0][0]
    for channel, values in enumerate(source):
        if values is None:
            continue
        stride = 36 if channel == 12 else (96 if channel == 2 else 24)
        channels[channel][1] = copy.deepcopy(values[1][start * stride:(start + count) * stride])
        if channel == 12:
            channels[channel][1] = [index - start * 24 for index in channels[channel][1]]
    return result


def append_box(batch, low, high):
    # Axis-aligned native boxes use the original BoxMesh corner ordering.
    template = boxes(steel, 0, 1)
    channels = template[1][0][0]
    vertices = channels[0][1]
    old_low = [min(v[axis] for v in vertices) for axis in range(3)]
    old_high = [max(v[axis] for v in vertices) for axis in range(3)]
    channels[0][1] = [[low[axis] if abs(v[axis] - old_low[axis]) < 0.0001 else high[axis]
                       for axis in range(3)] for v in vertices]
    target = batch[1][0][0]
    vertex_offset = len(target[0][1])
    for channel, values in enumerate(channels):
        if values is None:
            continue
        target[channel][1].extend([index + vertex_offset for index in values[1]]
                                 if channel == 12 else values[1])


frame_columns = copy.deepcopy(columns)
for vertex in frame_columns[1][0][0][0][1]:
    if abs(vertex[1]) < 0.0001:
        vertex[1] = 0.30
frame_steel = copy.deepcopy(steel)
append_box(frame_steel, [17.4, 0.34, -118.20], [22.6, 0.50, -117.98])
for column in range(9):
    for y in [0.45, 10.75]:
        x = 18.0 + 0.5 * column
        append_box(frame_steel, [x - 0.08, y - 0.075, -118.04],
                               [x + 0.08, y + 0.075, -117.94])
frame = ["KitBody2952", [deck, boxes(hazard, 0, 1), frame_columns, frame_steel, rails, sign]]
entry = boxes(hazard, 1, 1)
for vertex in entry[1][0][0][0][1]:
    for axis, origin in enumerate([20.0, 0.04, -116.7]):
        vertex[axis] -= origin
records = [["KitBody2954", [entry]], frame, ["KitBody1952", [concrete]]]
data = (json.dumps(records, separators=(",", ":")) + "\n").encode()
(root / "kit-causal-cargo-numeric.json.gz").write_bytes(gzip.compress(data, mtime=0))
print("Derived scoped cargo oracle from original channels:", len(data), "bytes")
