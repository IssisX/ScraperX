"""Per-blend-shape absolute normals (own code). Godot 4.7 gl_compatibility blends
normals as absolute values even in RELATIVE mode, so each shape needs its own
full normal set: the vertex normals of (base + delta), welded across UV seams.
usage: python3 -I bake_bs_normals.py DATA_DIR"""
import json, os, sys
import numpy as np
D = sys.argv[1]
for lod in ("lod0", "lod1"):
    d = os.path.join(D, lod)
    meta = json.load(open(os.path.join(d, "meta.json")))
    P = np.fromfile(os.path.join(d, "pos.f32"), "<f4").reshape(-1, 3).astype(np.float64)
    N0 = np.fromfile(os.path.join(d, "nrm.f32"), "<f4").reshape(-1, 3)
    T = np.fromfile(os.path.join(d, "idx.i32"), "<i4").reshape(-1, 3)
    key = np.round(P / 1e-5).astype(np.int64)
    _, inv = np.unique(key, axis=0, return_inverse=True)
    inv = inv.reshape(-1)
    def normals(Q):
        a, b, c = Q[T[:, 0]], Q[T[:, 2]], Q[T[:, 1]]  # Godot winding was flipped
        fn = np.cross(b - a, c - a)
        acc = np.zeros((inv.max() + 1, 3))
        for k in range(3):
            np.add.at(acc, inv[T[:, k]], fn)
        n = acc[inv]
        return n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    nb = normals(P)
    print(lod, "base normal agreement (mean dot with stored):", float((nb * N0).sum(1).mean()))
    for s in meta["shapes"]:
        dl = np.fromfile(os.path.join(d, "bs_%s.f32" % s), "<f4").reshape(-1, 3)
        n = normals(P + dl)
        # keep the stored base normal wherever the shape does not move the skin
        moved = np.linalg.norm(dl, axis=1) > 1e-6
        n[~moved] = N0[~moved]
        n.astype("<f4").tofile(os.path.join(d, "bsn_%s.f32" % s))
    print(lod, "done", len(meta["shapes"]), "shapes")
