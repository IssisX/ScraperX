# Head geometry from the CC0 MakeHuman hm08 base: aged, cut, UV islands repacked,
# normals + tangents computed here (so the normal-map bake and the renderer share one frame).
import sys, json, numpy as np
sys.path.insert(0, '/tmp/claude-0/-home-user-ScraperX/290f45ef-c41d-5b08-903d-c4b10970a44f/scratchpad/overhaul/skin_face_eyes/tools')
from mhload import load_obj, aged, target

CUT_Y = 5.0          # dm; keeps the neck and the top of the chest (the burn climbs from the pocket)
ORIGIN = np.array([0.0, 7.284, 0.60])   # dm; eyes at y=0, head centre near the origin
SCALE = 0.1          # dm -> m

def build(extra_targets=None):
    V, VT, F, G = load_obj()
    P = aged(V, extra_targets, F, G)
    faces = [f for f, g in zip(F, G) if g == 'body' and all(P[v, 1] > CUT_Y for v, _ in f)]
    # islands by shared UV indices
    par = {}
    def find(a):
        while par.setdefault(a, a) != a:
            par[a] = par[par[a]]; a = par[a]
        return a
    for f in faces:
        for _, t in f[1:]: par[find(t)] = find(f[0][1])
    roots = {}
    isl_of_face = []
    for f in faces:
        r = find(f[0][1]); roots.setdefault(r, len(roots)); isl_of_face.append(roots[r])
    nisl = len(roots)
    uvimg = np.stack([VT[:, 0], 1.0 - VT[:, 1]], 1)
    bb = []
    for k in range(nisl):
        ts = [t for f, i in zip(faces, isl_of_face) if i == k for _, t in f]
        q = uvimg[ts]; bb.append((q.min(0), q.max(0)))
    # uniform-density shelf packing, tallest first
    order = sorted(range(nisl), key=lambda k: -(bb[k][1][1] - bb[k][0][1]))
    pad = 6 / 2048
    k0 = order[0]
    s_head = (1 - 2 * pad) / (bb[k0][1][1] - bb[k0][0][1])
    x0 = pad + (bb[k0][1][0] - bb[k0][0][0]) * s_head + pad
    scale_of = {k0: s_head}
    for s in np.arange(s_head, 0.3, -0.01):
        place = {k0: (pad, pad)}; x = x0; y = pad; shelf_h = 0; ok = True
        for k in order[1:]:
            w = (bb[k][1][0] - bb[k][0][0]) * s; h = (bb[k][1][1] - bb[k][0][1]) * s
            if x + w + pad > 1:
                x = x0; y += shelf_h + pad; shelf_h = 0
            if x + w + pad > 1 or y + h + pad > 1: ok = False; break
            place[k] = (x, y); x += w + pad; shelf_h = max(shelf_h, h)
        if ok: break
    for k in order[1:]: scale_of[k] = s
    # new per-corner uv
    tris = []; tri_uvi = []
    for f, k in zip(faces, isl_of_face):
        for j in range(1, len(f) - 1):
            tris.append((f[0], f[j], f[j + 1])); tri_uvi.append(k)
    # unique (v, t) pairs -> render vertices
    key = {}; rv = []; rt = []; risl = []
    idx = np.zeros((len(tris), 3), np.int64)
    for i, (tri, k) in enumerate(zip(tris, tri_uvi)):
        for c, (v, t) in enumerate(tri):
            kk = (v, t)
            if kk not in key:
                key[kk] = len(rv); rv.append(v); rt.append(t); risl.append(k)
            idx[i, c] = key[kk]
    rv = np.array(rv); rt = np.array(rt); risl = np.array(risl)
    uv_old = uvimg[rt]
    uv = np.zeros_like(uv_old)
    for k in range(nisl):
        m = risl == k
        uv[m] = (uv_old[m] - bb[k][0]) * scale_of[k] + place[k]
    pos_dm = P[rv]
    # Godot draws clockwise-wound triangles as front faces; MakeHuman is CCW -> flip winding
    idx = idx[:, [0, 2, 1]]
    info = dict(islands=[dict(bb_min=bb[k][0].tolist(), bb_max=bb[k][1].tolist(), at=place[k], scale=float(scale_of[k])) for k in range(nisl)])
    return dict(P=P, V=V, F=F, G=G, rv=rv, uv=uv.astype(np.float32), uv_old=uv_old, isl=risl, idx=idx, pos_dm=pos_dm, info=info)

def normals_tangents(pos, uv, idx):
    """Area-weighted normals; per-vertex tangents from UV gradients (image v down = Godot UV)."""
    a, b, c = pos[idx[:, 0]], pos[idx[:, 1]], pos[idx[:, 2]]
    # Godot front face is clockwise in its convention; geometric normal from original CCW order:
    fn = np.cross(c - a, b - a)
    N = np.zeros_like(pos)
    for k in range(3): np.add.at(N, idx[:, k], fn)
    N /= np.linalg.norm(N, axis=1, keepdims=True) + 1e-12
    ta, tb, tc = uv[idx[:, 0]], uv[idx[:, 1]], uv[idx[:, 2]]
    e1 = b - a; e2 = c - a; d1 = tb - ta; d2 = tc - ta
    r = d1[:, 0] * d2[:, 1] - d2[:, 0] * d1[:, 1]
    r = np.where(np.abs(r) < 1e-12, 1e-12, r)
    sdir = (e1 * d2[:, 1:2] - e2 * d1[:, 1:2]) / r[:, None]   # dP/du
    tdir = (e2 * d1[:, 0:1] - e1 * d2[:, 0:1]) / r[:, None]   # dP/dv
    # weight by area so tiny slivers don't dominate
    area = np.linalg.norm(fn, axis=1, keepdims=True)
    sd = sdir / (np.linalg.norm(sdir, axis=1, keepdims=True) + 1e-12) * area
    td = tdir / (np.linalg.norm(tdir, axis=1, keepdims=True) + 1e-12) * area
    T = np.zeros_like(pos); B = np.zeros_like(pos)
    for k in range(3): np.add.at(T, idx[:, k], sd); np.add.at(B, idx[:, k], td)
    T = T - N * np.sum(N * T, 1, keepdims=True)
    T /= np.linalg.norm(T, axis=1, keepdims=True) + 1e-12
    w = np.where(np.sum(np.cross(N, T) * B, 1) < 0, -1.0, 1.0)
    return N.astype(np.float32), T.astype(np.float32), w.astype(np.float32)

if __name__ == '__main__':
    g = build()
    print(json.dumps(g['info'], indent=0)[:1500])
    print('render verts', len(g['rv']), 'tris', len(g['idx']))
