"""Exact exported relocation audit; run after the documented Godot solid export.

python3 tests/twin_geometry_test.py <pre-relocation commit>
The baseline is explicit so a future relocation cannot silently reuse this one.
"""
from pathlib import Path
import re
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
base = subprocess.check_output(
    ["git", "show", f"{sys.argv[1]}:src/sim/world_solids.inc"], cwd=root, text=True
)
current = (root / "src/sim/world_solids.inc").read_text()
pattern = re.compile(r"    \{([^}]+)\},  // (\w+)\n")

def boxes(text):
    return [(m[2], [float(v.strip().removesuffix("F")) for v in m[1].split(",")])
            for m in pattern.finditer(text) if len(m[1].split(",")) == 10]

old, new = boxes(base), boxes(current)
assert len(old) == len(new), "relocation must not add or remove exported boxes"
moved = {"TowerMass", "FacePier", "FaceBand", "FaceDuct", "TimberCladding"}
count = 0
for (part0, before), (part, after) in zip(old, new):
    assert part == part0, "export order changed"
    expected = before.copy()
    if part in moved:
        expected[2] += 66.0
        count += 1
    assert all(abs(a - b) < 0.00011 for a, b in zip(expected, after)), (part, expected, after)
assert count == 44, f"expected every shaft collision part, saw {count}"
# Every other byte (including hull vertices and A's upper continuation) stays.
def without_moved(text):
    return pattern.sub(lambda m: "" if m[2] in moved else m[0], text)
assert without_moved(base) == without_moved(current), "unrelated exported geometry changed"

def bounds(n):
    x, y, z, qx, qy, qz, qw, hx, hy, hz = n
    # Conservative world AABB of each rotated box, including distant dressing.
    r = [[1-2*(qy*qy+qz*qz), 2*(qx*qy-qz*qw), 2*(qx*qz+qy*qw)],
         [2*(qx*qy+qz*qw), 1-2*(qx*qx+qz*qz), 2*(qy*qz-qx*qw)],
         [2*(qx*qz-qy*qw), 2*(qy*qz+qx*qw), 1-2*(qx*qx+qy*qy)]]
    half = [sum(abs(v)*h for v,h in zip(row, (hx,hy,hz))) for row in r]
    return ([p-h for p,h in zip((x,y,z),half)], [p+h for p,h in zip((x,y,z),half)])

mass = next(n for part,n in new if part == "TowerMass")
assert mass[:3] == [-30.0, 800.0, -264.0] and mass[-3:] == [46.0, 800.0, 40.0]
lo, hi = bounds(mass)
for part,n in new:
    if part in moved:
        continue
    a,b = bounds(n)
    assert not all(min(u,v) - max(l,k) > 0.001 for l,u,k,v in zip(lo,hi,a,b)), \
        f"relocated shaft overlaps unrelated exported {part}"
# Broad-phase hull audit too. Mountain terrain already intersects the old
# shaft's AABB; moving toward the yard must introduce no new candidates.
raw = current.split("constexpr float kWorldSolidHullPoints[] = {")[1].split("};")[0]
values = [float(v) for v in re.findall(r"(-?\d+\.\d+)F", raw)]
points = list(zip(values[::3], values[1::3], values[2::3]))
old_lo, old_hi = bounds(next(n for part,n in old if part == "TowerMass"))
def overlaps(a, b, c, d):
    return all(min(u,v) - max(l,k) > 0.001 for l,u,k,v in zip(a,b,c,d))
terrain_candidates = 0
for first, size, part in re.findall(r"\{(\d+), (\d+)\},  // (\w+)",
                                    current.split("constexpr WorldSolidHull kWorldSolidHulls[] = {")[1]):
    hull = points[int(first):int(first)+int(size)]
    a = [min(p[i] for p in hull) for i in range(3)]
    b = [max(p[i] for p in hull) for i in range(3)]
    if overlaps(lo, hi, a, b):
        assert overlaps(old_lo, old_hi, a, b) and part == "MountainPeak", \
            f"new hull overlap candidate: {part}"
        terrain_candidates += 1
assert terrain_candidates == 4, "recheck the shaft's existing terrain embedding"
nominal_gap = -176.0 - hi[2]
facade_front = max(bounds(n)[1][2] for part,n in new if part in moved)
frame_north = min(bounds(n)[0][2] for part,n in new if part == "OuterEdgeBeam")
clear_gap = frame_north - facade_front
assert 30.48 <= clear_gap <= 60.96
print(f"PASS twin export: {count} boxes moved +66m; all other boxes/hulls unchanged; "
      f"mass clear of unrelated boxes; no new hull candidates; nominal gap={nominal_gap:.2f}m; facade gap={clear_gap:.2f}m")
