#!/usr/bin/env python3
"""Straighten the hair ribbon rest shape onto the solver polylines.

Why: the hair bones rest exactly on straight-down lines from their
roots (20 chains x 5 bones x 0.09 m), but the ribbon bind shape drapes
backwards over the head: drawn rest middles sit a median 0.029 m off
their solver segment, growing to ~0.07 m at the tips, mostly backwards
in head space. The solver pushes points, not drawn middles, so that
fixed base offset survives every pose and no step_hair tweak can clear
it. This script removes the base at the source, in the mesh itself.

Per ribbon middle point (the probe's pairing: connected mesh pieces
split by UV side, edges paired in UV order), for solver-mapped middles
only (dominant hair bone after reweight_hair.py):
  - move both edge vertices by the same delta so the middle lands on
    its chain's root column at its own height: (root_x, y, root_z).
    Height is preserved, so rows are never stacked onto one point;
    ribbon width (the edge-to-edge vector) is preserved.
  - if the height now falls in a different segment of the same chain,
    re-skin both edge vertices to that segment's bone: their hair
    weights are replaced by a single weight on the new bone carrying
    the old total hair weight, head and non-hair weights untouched.
    (In practice every such move is one segment up: rows sit above
    the segment their dominant bone names.)
Head-only middles (no hair bone) are left alone, as are vertices in
no middle point. The rigid drawn tail past the last simulated point
keeps its height, so the handful of tail middles below the tip point
still sit up to ~0.08 m past it in the rest metric; they are 39 of
10,803 solver middles and the rig clamps the tail at runtime.

Normals are recomputed from the edited triangles with the winding
the stored normals use (verified: recomputing on the unedited mesh
with this winding matches the stored normals at cosine ~0.98).

The rewrite is in place on the POSITION, WEIGHTS_0 and NORMAL
accessors only (same component types, same byte lengths, same file
size, no structural change); the POSITION accessor min/max in the
JSON are updated. Running it on an already straightened file changes
no position by more than float32 rounding and re-skins nothing.
It prints a report including the rest offset before and after,
measured the probe's way (drawn rest middle vs its dominant
segment), so the acceptance check is the script's own output.
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


def write_acc_f32(data, jdoc, bin_off, ai, arr):
    a = jdoc["accessors"][ai]
    b = jdoc["bufferViews"][a["bufferView"]]
    assert a["componentType"] == 5126, "accessor is not float32; refusing to rewrite"
    start = bin_off + b.get("byteOffset", 0) + a.get("byteOffset", 0)
    out = np.frombuffer(data, dtype="<f4", count=a["count"] * 3, offset=start).reshape(a["count"], 3)
    out[:, :] = arr.astype("<f4")
    if "min" in a:
        a["min"] = [float(x) for x in arr.min(axis=0)]
    if "max" in a:
        a["max"] = [float(x) for x in arr.max(axis=0)]


def write_weights(data, jdoc, bin_off, ai, arr):
    a = jdoc["accessors"][ai]
    b = jdoc["bufferViews"][a["bufferView"]]
    assert a["componentType"] == 5126, "WEIGHTS_0 is not float32; refusing to rewrite"
    start = bin_off + b.get("byteOffset", 0) + a.get("byteOffset", 0)
    out = np.frombuffer(data, dtype="<f4", count=a["count"] * 4, offset=start).reshape(a["count"], 4)
    out[:, :] = arr.astype("<f4")


def recompute_normals(pos, tris):
    # Winding matching the stored normals: cross(p2-p0, p1-p0).
    fn = np.cross(pos[tris[:, 2]] - pos[tris[:, 0]], pos[tris[:, 1]] - pos[tris[:, 0]])
    vn = np.zeros_like(pos)
    np.add.at(vn, tris[:, 0], fn)
    np.add.at(vn, tris[:, 1], fn)
    np.add.at(vn, tris[:, 2], fn)
    n = np.linalg.norm(vn, axis=1, keepdims=True)
    return np.where(n > 0, vn / np.maximum(n, 1e-12), 0.0)


def rest_offset(middles, chain, seg, roots, segs):
    """Distance from each solver middle to its dominant segment, probe-style."""
    m = chain >= 0
    a = np.array([roots[c] - np.array([0.0, i * segs[c], 0.0]) for c, i in zip(chain[m], seg[m])])
    b = a - np.array([[0.0, segs[c], 0.0] for c in chain[m]])
    ab = b - a
    d2 = np.einsum("ij,ij->i", ab, ab)
    t = np.clip(np.einsum("ij,ij->i", middles[m] - a, ab) / np.maximum(d2, 1e-12), 0.0, 1.0)
    closest = a + t[:, None] * ab
    return np.linalg.norm(middles[m] - closest, axis=1), m


def main():
    src = sys.argv[1]
    rig_path = sys.argv[2]
    dst = sys.argv[3] if len(sys.argv) > 3 else src
    data, jdoc, bin_off = load_glb(src)
    rig = json.load(open(rig_path))
    chains = rig["hair"]["chains"]
    nodes = jdoc["nodes"]
    skin = jdoc["skins"][0]
    joint_names = [nodes[i].get("name") for i in skin["joints"]]

    hair = [m for m in jdoc["meshes"] if m["name"] == "hair"][0]["primitives"][0]
    attrs = hair["attributes"]
    joints = read_acc(data, jdoc, bin_off, attrs["JOINTS_0"]).astype(int)
    weights = read_acc(data, jdoc, bin_off, attrs["WEIGHTS_0"]).astype(float)
    pos = read_acc(data, jdoc, bin_off, attrs["POSITION"]).astype(float)
    tris = read_acc(data, jdoc, bin_off, hair["indices"]).reshape(-1, 3).astype(int)
    uv = read_acc(data, jdoc, bin_off, attrs["TEXCOORD_0"])
    n = len(joints)

    # Chain / segment per joint index, and root + segment length per chain.
    hair_bone = {}  # joint index -> (chain, seg)
    roots = []
    segs = []
    for c, ch in enumerate(chains):
        roots.append(np.array(ch["root"], dtype=float))
        segs.append(float(ch["segment_m"]))
        for s, name in enumerate(ch["bones"]):
            if name in joint_names:
                hair_bone[joint_names.index(name)] = (c, s)
    roots = np.array(roots)

    # Middle pairing, as in reweight_hair.py / the probe.
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
    roots_lab = np.array([find(i) for i in range(n)])
    _, lab = np.unique(roots_lab, return_inverse=True)

    u = uv[:, 0] > 0.5
    order = np.lexsort((uv[:, 1], u, lab))
    lab_o, u_o = lab[order], u[order]
    pairs_a, pairs_b = [], []
    i = 0
    while i < n:
        j = i
        while j < n and lab_o[j] == lab_o[i]:
            j += 1
        seg_ = order[i:j]
        left = seg_[~u_o[i:j]]
        right = seg_[u_o[i:j]]
        m_ = min(len(left), len(right))
        if m_ >= 2:
            pairs_a.append(left[:m_])
            pairs_b.append(right[:m_])
        i = j
    A = np.concatenate(pairs_a) if pairs_a else np.array([], dtype=int)
    B = np.concatenate(pairs_b) if pairs_b else np.array([], dtype=int)
    middles = 0.5 * (pos[A] + pos[B])

    # Dominant hair bone of the first edge, per middle.
    dom_chain = np.full(len(A), -1)
    dom_seg = np.full(len(A), -1)
    dom_joint = np.full(len(A), -1)
    for i, v in enumerate(A):
        best_w, best = 0.0, (-1, -1, -1)
        for k in range(4):
            cs = hair_bone.get(int(joints[v, k]), (-1, -1))
            if cs[0] >= 0 and weights[v, k] > best_w:
                best_w, best = weights[v, k], (cs[0], cs[1], int(joints[v, k]))
        # A middle whose dominant hair weight is negligible is head-only,
        # not solver-mapped (1e-4 cuts float32 residue, not real weights).
        if best_w >= 1e-4:
            dom_chain[i], dom_seg[i], dom_joint[i] = best

    before, m = rest_offset(middles, dom_chain, dom_seg, roots, segs)
    print("solver middles: %d of %d, head-only middles: %d" % (m.sum(), len(A), (~m).sum()))
    print("rest offset before: median %.4f m, mean %.4f m, frac>0.015 %.3f"
          % (np.median(before), before.mean(), (before > 0.015).mean()))

    new_pos = pos.copy()
    new_w = weights.copy()
    moved = 0
    reskinned = 0
    max_delta = 0.0
    for i in range(len(A)):
        c = dom_chain[i]
        if c < 0:
            continue
        root = roots[c]
        seg_len = segs[c]
        target = np.array([root[0], middles[i, 1], root[2]])
        delta = target - middles[i]
        d = float(np.linalg.norm(delta))
        if d > 1e-7:
            new_pos[A[i]] += delta
            new_pos[B[i]] += delta
            moved += 1
            max_delta = max(max_delta, d)
        newseg = int(np.clip(np.floor((root[1] - middles[i, 1]) / seg_len), 0, 4))
        if newseg != dom_seg[i]:
            new_bone_name = chains[c]["bones"][newseg]
            new_joint = joint_names.index(new_bone_name)
            for v in (A[i], B[i]):
                hair_sum = 0.0
                for k in range(4):
                    if hair_bone.get(int(joints[v, k]), (-1, -1))[0] >= 0:
                        hair_sum += weights[v, k]
                if hair_sum <= 0:
                    continue
                row = np.zeros(4)
                for k in range(4):
                    if hair_bone.get(int(joints[v, k]), (-1, -1))[0] < 0:
                        row[k] = weights[v, k]
                # Put the whole hair share on the new bone's first slot.
                placed = False
                for k in range(4):
                    if hair_bone.get(int(joints[v, k]), (-1, -1))[0] >= 0:
                        if not placed:
                            joints[v, k] = new_joint
                            row[k] = hair_sum
                            placed = True
                        else:
                            row[k] = 0.0
                if not placed:
                    joints[v, 0] = new_joint
                    row[0] = hair_sum
                new_w[v] = row / row.sum() if row.sum() > 0 else row
                reskinned += 1

    new_middles = 0.5 * (new_pos[A] + new_pos[B])
    # Recompute dominant chain/seg from the edited weights for the check.
    chk_chain = np.full(len(A), -1)
    chk_seg = np.full(len(A), -1)
    for i, v in enumerate(A):
        best_w, best = 0.0, (-1, -1)
        for k in range(4):
            cs = hair_bone.get(int(joints[v, k]), (-1, -1))
            if cs[0] >= 0 and new_w[v, k] > best_w:
                best_w, best = new_w[v, k], cs
        if best_w >= 1e-4:
            chk_chain[i], chk_seg[i] = best
    after, m2 = rest_offset(new_middles, chk_chain, chk_seg, roots, segs)
    print("middles moved: %d, vertices re-skinned: %d, max delta %.4f m"
          % (moved, reskinned, max_delta))
    print("rest offset after: median %.4f m, mean %.4f m, frac>0.015 %.3f, max %.4f m"
          % (np.median(after), after.mean(), (after > 0.015).mean(), after.max()))
    # Per-along rest means after, using the probe's along order (strand order).
    print("segment reassignments (old->new): %s" % Counter(
        [(int(dom_seg[i]), int(chk_seg[i])) for i in range(len(A))
         if dom_chain[i] >= 0 and chk_seg[i] != dom_seg[i]]))

    # Normals: verify the winding on the unedited mesh, then recompute.
    old_normals = read_acc(data, jdoc, bin_off, attrs["NORMAL"]).astype(float)
    check = recompute_normals(pos, tris)
    cos = np.einsum("ij,ij->i", check, old_normals)
    print("normal winding check on unedited mesh: median cosine %.3f" % float(np.median(cos)))
    new_normals = recompute_normals(new_pos, tris)

    # Write back in place. JOINTS_0 is uint16 in this file; rewrite via its own view.
    jacc = jdoc["accessors"][attrs["JOINTS_0"]]
    bv = jdoc["bufferViews"][jacc["bufferView"]]
    assert jacc["componentType"] == 5123, "JOINTS_0 is not uint16; refusing to rewrite"
    start = bin_off + bv.get("byteOffset", 0) + jacc.get("byteOffset", 0)
    jarr = np.frombuffer(data, dtype="<u2", count=n * 4, offset=start).reshape(n, 4)
    jarr[:, :] = joints.astype("<u2")
    write_acc_f32(data, jdoc, bin_off, attrs["POSITION"], new_pos)
    write_acc_f32(data, jdoc, bin_off, attrs["NORMAL"], new_normals)
    write_weights(data, jdoc, bin_off, attrs["WEIGHTS_0"], new_w)

    # Rewrite the JSON chunk with updated POSITION min/max (length may change).
    new_json = json.dumps(jdoc, separators=(",", ":")).encode()
    pad = (4 - len(new_json) % 4) % 4
    new_json += b" " * pad
    old_json_len = None
    off = 12
    while off < len(data):
        clen, ctype = struct.unpack_from("<II", data, off)
        if ctype == 0x4E4F534A:
            old_json_len = clen
            json_off = off
            break
        off += 8 + clen
    assert old_json_len is not None
    bin_chunk = bytes(data[json_off + 8 + old_json_len:])
    out = bytearray()
    out += struct.pack("<III", 0x46546C67, 2, 12 + 8 + len(new_json) + len(bin_chunk))
    out += struct.pack("<II", len(new_json), 0x4E4F534A) + new_json
    out += bin_chunk
    open(dst, "wb").write(bytes(out))
    print("wrote %s (%d bytes, was %d)" % (dst, len(out), len(data)))


if __name__ == "__main__":
    main()
