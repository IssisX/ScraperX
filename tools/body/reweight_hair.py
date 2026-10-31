#!/usr/bin/env python3
"""Re-weight the hair mesh in matron.glb so each ribbon follows one chain.

Why: a hair ribbon's vertices are skinned across neighboring chains. Two
chains that drape on opposite sides of her head pull their shared ribbon
vertices to a blend between them, and that blend cuts straight through
her head even when every solver point clears her body volumes. The probe
scores each ribbon's middle point (the average of its two edge vertices),
so a middle point is only on one chain when both its edges are.

Rule, per ribbon middle point (the probe's pairing: connected mesh pieces
split by UV side, edges paired in UV order): the target chain is the
dominant hair chain of the first edge, else of the second edge. Both edge
vertices keep only weights to that chain, to the head, and to non-hair
bones; other-chain hair weights are dropped and the rest renormalized.
Vertices in no middle point keep only their own dominant chain the same
way. Vertices with no hair weight, and vertices whose kept weights sum to
zero, are left unchanged.

The rewrite is in place on the WEIGHTS_0 accessor only (float32, same
byte length, same file size, no structural change). Running it on an
already re-weighted file changes no chain assignment and no weight by
more than float32 rounding (a second run reports 0 vertices changed
under the usual 1e-5 tolerance). It prints a report: vertices changed,
middle points left cross-chain, and the largest weight-sum deviation
after renormalization.
"""

import json
import struct
import sys
from collections import Counter

import numpy as np


def load_glb(path):
    data = bytearray(open(path, "rb").read())
    off = 12
    jdoc = None
    bin_off = None
    while off < len(data):
        clen, ctype = struct.unpack_from("<II", data, off)
        if ctype == 0x4E4F534A:
            jdoc = json.loads(bytes(data[off + 8:off + 8 + clen]))
        elif ctype == 0x004E4942:
            bin_off = off + 8
        off += 8 + clen
    assert jdoc is not None and bin_off is not None, "not a glb with JSON + BIN chunks"
    return data, jdoc, bin_off


def read_acc(data, jdoc, bin_off, ai):
    a = jdoc["accessors"][ai]
    b = jdoc["bufferViews"][a["bufferView"]]
    start = bin_off + b.get("byteOffset", 0) + a.get("byteOffset", 0)
    fmt = {5120: "b", 5121: "B", 5122: "h", 5123: "H", 5125: "I", 5126: "f"}[a["componentType"]]
    cnt = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[a["type"]]
    return np.frombuffer(data, dtype="<" + fmt, count=a["count"] * cnt, offset=start).reshape(a["count"], cnt)


def main():
    src = sys.argv[1]
    dst = sys.argv[2] if len(sys.argv) > 2 else src
    data, jdoc, bin_off = load_glb(src)
    acc = jdoc["accessors"]
    bv = jdoc["bufferViews"]
    nodes = jdoc["nodes"]
    skin = jdoc["skins"][0]
    joint_names = [nodes[i].get("name") for i in skin["joints"]]

    hair = [m for m in jdoc["meshes"] if m["name"] == "hair"][0]["primitives"][0]
    attrs = hair["attributes"]
    joints = read_acc(data, jdoc, bin_off, attrs["JOINTS_0"]).astype(int)
    weights = read_acc(data, jdoc, bin_off, attrs["WEIGHTS_0"]).astype(float)
    tris = read_acc(data, jdoc, bin_off, hair["indices"]).reshape(-1, 3)
    uv = read_acc(data, jdoc, bin_off, attrs["TEXCOORD_0"])
    n = len(joints)

    hair_bone = {}
    for c in range(20):
        for s in range(5):
            name = "hair%d_%d" % (c, s)
            if name in joint_names:
                hair_bone[joint_names.index(name)] = c

    # Connected mesh pieces (strands), as in the probe's pairing.
    parent = np.arange(n)

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for t in tris:
        r = find(int(t[0]))
        for v in t[1:]:
            rv = find(int(v))
            if rv != r:
                parent[rv] = r
    roots = np.array([find(i) for i in range(n)])
    _, lab = np.unique(roots, return_inverse=True)

    dom_chain = np.full(n, -1)
    for i in range(n):
        best_w, best_c = 0.0, -1
        for k in range(4):
            ch = hair_bone.get(int(joints[i, k]), -1)
            if ch >= 0 and weights[i, k] > best_w:
                best_w, best_c = weights[i, k], ch
        dom_chain[i] = best_c

    u = uv[:, 0] > 0.5
    order = np.lexsort((uv[:, 1], u, lab))
    lab_o, u_o = lab[order], u[order]
    pairs_a, pairs_b = [], []
    i = 0
    while i < n:
        j = i
        while j < n and lab_o[j] == lab_o[i]:
            j += 1
        seg = order[i:j]
        left = seg[~u_o[i:j]]
        right = seg[u_o[i:j]]
        m_ = min(len(left), len(right))
        if m_ >= 2:
            pairs_a.append(left[:m_])
            pairs_b.append(right[:m_])
        i = j
    A = np.concatenate(pairs_a) if pairs_a else np.array([], dtype=int)
    B = np.concatenate(pairs_b) if pairs_b else np.array([], dtype=int)

    # Target chain per vertex: the middle point's chain for paired edges,
    # else the vertex's own dominant chain.
    target = dom_chain.copy()
    paired = np.zeros(n, dtype=bool)
    for a, b in zip(A, B):
        ch = dom_chain[a] if dom_chain[a] >= 0 else dom_chain[b]
        target[a] = ch
        target[b] = ch
        paired[a] = True
        paired[b] = True

    new_w = weights.copy()
    changed = 0
    zero_kept = 0
    for i in range(n):
        ch = target[i]
        if ch < 0:
            continue
        keep = np.zeros(4)
        for k in range(4):
            c = hair_bone.get(int(joints[i, k]), -1)
            if c < 0 or c == ch:
                keep[k] = weights[i, k]
        ssum = keep.sum()
        if ssum <= 0:
            zero_kept += 1
            continue
        new_w[i] = keep / ssum
        if not np.allclose(new_w[i], weights[i]):
            changed += 1

    # Residual cross-chain middle points after the rewrite.
    def chain_of(i, wrow):
        best_w, best_c = 0.0, -1
        for k in range(4):
            c = hair_bone.get(int(joints[i, k]), -1)
            if c >= 0 and wrow[k] > best_w:
                best_w, best_c = wrow[k], c
        return best_c

    cross = 0
    for a, b in zip(A, B):
        ca, cb = chain_of(a, new_w[a]), chain_of(b, new_w[b])
        if ca >= 0 and cb >= 0 and ca != cb:
            cross += 1

    # Write back in place into the WEIGHTS_0 buffer view.
    wacc = acc[attrs["WEIGHTS_0"]]
    assert wacc["componentType"] == 5126, "WEIGHTS_0 is not float32; refusing to rewrite"
    wview = bv[wacc["bufferView"]]
    start = bin_off + wview.get("byteOffset", 0) + wacc.get("byteOffset", 0)
    arr = np.frombuffer(data, dtype="<f4", count=n * 4, offset=start).reshape(n, 4)
    arr[:, :] = new_w.astype("<f4")
    open(dst, "wb").write(bytes(data))

    print("hair vertices: %d, middle points: %d" % (n, len(A)))
    print("vertices changed: %d" % changed)
    print("vertices with zero kept weight (left unchanged): %d" % zero_kept)
    print("middle points still cross-chain: %d" % cross)
    print("max weight-sum deviation: %.3g" % float(np.abs(new_w.sum(axis=1) - 1.0).max()))


if __name__ == "__main__":
    main()
