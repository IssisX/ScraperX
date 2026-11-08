#!/usr/bin/env python3
"""Write godot/presentation/life/matron/hair_seat.bin from a probe recording.

    python3 tools/matron_probe/seat_hair.py <recording> [--out PATH]

Keeps the seated vertex positions the game loaded (the recording's hair part)
and gives each ribbon one set of skin weights for both of its edges, taken
along the one solver chain that carries most of that ribbon. Along the chain a
vertex blends the two bones nearest its place on it (the head above the
chain's root), so a ribbon bends with the chain and never spans two chains.

File (little endian): u32 magic 0x32534548 ("HES2"), u32 n, n x 3 f32
positions, n x 4 i32 skin bind indices, n x 4 f32 weights.
"""
import argparse
import os
import struct

import numpy as np

from score import Hair, Run, inverse

MAGIC = 0x32534548
OUT = os.path.join(os.path.dirname(__file__), "..", "..", "godot", "presentation", "life", "matron", "hair_seat.bin")


def chain_polylines(run, part):
    """Rest polyline of each chain in the hair part's bind space: bone origins plus the last tip."""
    bind_of = {int(b): i for i, b in enumerate(part.binds)}
    rest = inverse(part.bind_poses)            # (binds, 3, 4): bone rest in skeleton space
    lines, binds = [], []
    for ch in run.chains:
        idx = [int(b) for b in ch["idx"]]
        pts = [rest[bind_of[b]][:, 3] for b in idx]
        tip_dir = pts[-1] - pts[-2]
        seg = float(ch["seg"]) if np.isscalar(ch["seg"]) else float(ch["seg"][-1])
        pts.append(pts[-1] + tip_dir / max(np.linalg.norm(tip_dir), 1e-9) * seg)
        lines.append(np.array(pts))
        binds.append([bind_of[b] for b in idx])
    return lines, binds


def place(line, q):
    """Parameter t along the polyline (0 = root, k = joint k) of its point nearest q, and the distance."""
    best = (np.inf, 0.0)
    for k in range(len(line) - 1):
        a, b = line[k], line[k + 1]
        d = b - a
        u = float(np.clip(np.dot(q - a, d) / max(np.dot(d, d), 1e-12), 0.0, 1.0))
        dist = np.linalg.norm(a + u * d - q)
        if k == 0 and np.dot(q - a, d) < 0:
            u = np.dot(q - a, d) / max(np.dot(d, d), 1e-12)   # above the root: negative
        if dist < best[0]:
            best = (dist, k + u)
    return best[1]


def near(line, q):
    """Distance from q to the polyline."""
    best = np.inf
    for k in range(len(line) - 1):
        a, d = line[k], line[k + 1] - line[k]
        u = float(np.clip(np.dot(q - a, d) / max(np.dot(d, d), 1e-12), 0.0, 1.0))
        best = min(best, float(np.linalg.norm(a + u * d - q)))
    return best


ROOT_BLEND = (-0.5, 1.5)   # chain parameter over which a ribbon hands over from the head to its chain


def weights_at(t, chain_binds, head_bind):
    """Skin weights at chain parameter t (0 = root, k = joint k). Bone k is solid in the middle of its
    segment and shares its joints half and half with its neighbor; near the root the head hands the
    ribbon over smoothly. Both edges of a ribbon get the same weights, so they move as one."""
    nb = len(chain_binds)
    u = float(np.clip(t - 0.5, 0.0, nb - 1.0))
    s = min(int(np.floor(u)), nb - 2)
    f = u - s
    ws = {chain_binds[s]: 1.0 - f}
    ws[chain_binds[s + 1]] = ws.get(chain_binds[s + 1], 0.0) + f
    lo, hi = ROOT_BLEND
    x = float(np.clip((t - lo) / (hi - lo), 0.0, 1.0))
    h = 1.0 - x * x * (3.0 - 2.0 * x)
    out = [(head_bind, h)] if h > 0 else []
    out += [(b, w * (1.0 - h)) for b, w in ws.items() if w * (1.0 - h) > 0]
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("recording")
    ap.add_argument("--out", default=OUT)
    a = ap.parse_args()
    run = Run(a.recording)
    part = run.parts["hair"]
    hair = Hair(run)
    n = len(part.v)
    head_bind = int(np.nonzero(part.binds == run.bone_index["head"])[0][0])
    lines, cbinds = chain_polylines(run, part)
    bi = np.zeros((n, 4), np.int32)
    bw = np.zeros((n, 4), np.float32)
    bi[:, 0] = part.bind_index[np.arange(n), np.argmax(part.w, axis=1)]
    bw[:, 0] = 1.0
    # each vertex's strand (connected ribbon) via the pairing
    strand_of = np.full(n, -1)
    strand_of[hair.a] = hair.strand
    strand_of[hair.b] = hair.strand
    # unpaired vertices join the strand of a triangle neighbor
    for _ in range(4):
        for tri in part.tris:
            s = strand_of[tri].max()
            if s >= 0:
                strand_of[tri[strand_of[tri] < 0]] = s
    for s in range(hair.nstrands):
        verts = np.nonzero(strand_of == s)[0]
        ks = np.nonzero(hair.strand == s)[0]
        mids = 0.5 * (part.v[hair.a[ks]] + part.v[hair.b[ks]])
        if hair.rigid[ks].all():
            bi[verts, 0] = head_bind
            continue                      # rigid on the head
        # the chain whose rest line runs nearest the ribbon, for the whole ribbon
        c = int(np.argmin([np.mean([near(line, m) for m in mids]) for line in lines]))
        ts = np.array([place(lines[c], m) for m in mids])
        ts = np.maximum.accumulate(ts) if ts[-1] >= ts[0] else np.minimum.accumulate(ts)
        for k, t in zip(ks, ts):
            ws = weights_at(t, cbinds[c], head_bind)
            for v in (hair.a[k], hair.b[k]):
                bi[v], bw[v] = 0, 0.0
                for j, (b, w) in enumerate(ws[:4]):
                    bi[v, j], bw[v, j] = b, w
        # unpaired: nearest middle point's weights
        paired = set(hair.a[ks]) | set(hair.b[ks])
        for v in verts:
            if v in paired:
                continue
            k = ks[int(np.argmin(np.linalg.norm(mids - part.v[v], axis=1)))]
            bi[v], bw[v] = bi[hair.a[k]], bw[hair.a[k]]
    with open(a.out, "wb") as fh:
        fh.write(struct.pack("<II", MAGIC, n))
        fh.write(part.v.astype("<f4").tobytes())
        fh.write(bi.astype("<i4").tobytes())
        fh.write(bw.astype("<f4").tobytes())
    print("wrote %s: %d vertices, %d strands" % (a.out, n, hair.nstrands))


if __name__ == "__main__":
    main()
