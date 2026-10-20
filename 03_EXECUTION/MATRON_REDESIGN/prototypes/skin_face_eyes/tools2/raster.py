# UV-space rasterizer and point-splat depth maps (numpy only).
import numpy as np

def raster_uv(uv, idx, attrs, res):
    """uv: (V,2) in [0,1] image coords; idx (T,3); attrs: dict name->(V,C).
    Returns dict of (res,res,C) arrays + 'mask' (res,res) + 'tri' (res,res) int."""
    H = W = res
    out = {k: np.zeros((H, W, a.shape[1]), np.float32) for k, a in attrs.items()}
    mask = np.zeros((H, W), np.float32); tri = np.full((H, W), -1, np.int32)
    P = uv * [W, H]
    for t, (i0, i1, i2) in enumerate(idx):
        a, b, c = P[i0], P[i1], P[i2]
        x0 = int(np.floor(min(a[0], b[0], c[0]))); x1 = int(np.ceil(max(a[0], b[0], c[0])))
        y0 = int(np.floor(min(a[1], b[1], c[1]))); y1 = int(np.ceil(max(a[1], b[1], c[1])))
        x0 = max(x0, 0); y0 = max(y0, 0); x1 = min(x1, W - 1); y1 = min(y1, H - 1)
        if x1 < x0 or y1 < y0: continue
        xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        d = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if abs(d) < 1e-12: continue
        l0 = ((b[1] - c[1]) * (xs - c[0]) + (c[0] - b[0]) * (ys - c[1])) / d
        l1 = ((c[1] - a[1]) * (xs - c[0]) + (a[0] - c[0]) * (ys - c[1])) / d
        l2 = 1 - l0 - l1
        e = -1e-3
        inside = (l0 >= e) & (l1 >= e) & (l2 >= e)
        if not inside.any(): continue
        yy = ys[inside].astype(int); xx = xs[inside].astype(int)
        w0 = l0[inside][:, None]; w1 = l1[inside][:, None]; w2 = l2[inside][:, None]
        for k, a_ in attrs.items():
            out[k][yy, xx] = w0 * a_[i0] + w1 * a_[i1] + w2 * a_[i2]
        mask[yy, xx] = 1.0; tri[yy, xx] = t
    out['mask'] = mask; out['tri'] = tri
    return out

def surface_samples(pos, nrm, idx, n, seed=1):
    rng = np.random.default_rng(seed)
    a, b, c = pos[idx[:, 0]], pos[idx[:, 1]], pos[idx[:, 2]]
    area = 0.5 * np.linalg.norm(np.cross(b - a, c - a), axis=1)
    t = rng.choice(len(idx), size=n, p=area / area.sum())
    r1 = np.sqrt(rng.random(n)); r2 = rng.random(n)
    w0 = 1 - r1; w1 = r1 * (1 - r2); w2 = r1 * r2
    p = a[t] * w0[:, None] + b[t] * w1[:, None] + c[t] * w2[:, None]
    return p

def fib_dirs(n):
    i = np.arange(n) + 0.5
    phi = np.arccos(1 - 2 * i / n); th = np.pi * (1 + 5 ** 0.5) * i
    return np.stack([np.cos(th) * np.sin(phi), np.cos(phi), np.sin(th) * np.sin(phi)], 1)

def ao_thickness(pos, nrm, idx, occluder_pts, ndir=96, grid=320, bias=0.02):
    """Per-vertex ambient occlusion (cosine-weighted visibility) and thickness
    (material depth seen from behind) from point-splat depth maps."""
    lo = occluder_pts.min(0); hi = occluder_pts.max(0)
    ctr = (lo + hi) / 2; ext = np.linalg.norm(hi - lo) / 2 * 1.02
    dirs = fib_dirs(ndir)
    vis_acc = np.zeros(len(pos)); w_acc = np.zeros(len(pos)); thick = np.full(len(pos), 1e9)
    for d in dirs:
        # camera at +d looking toward -d; depth = distance along -d from a far plane
        up = np.array([0, 1, 0]) if abs(d[1]) < 0.9 else np.array([1, 0, 0])
        ux = np.cross(up, d); ux /= np.linalg.norm(ux); uy = np.cross(d, ux)
        def proj(q):
            r = q - ctr
            px = ((r @ ux) / ext * 0.5 + 0.5) * grid; py = ((r @ uy) / ext * 0.5 + 0.5) * grid
            z = -(r @ d)  # smaller = closer to camera
            return np.clip(px.astype(int), 0, grid - 1), np.clip(py.astype(int), 0, grid - 1), z
        ox, oy, oz = proj(occluder_pts)
        zb = np.full((grid, grid), 1e9); np.minimum.at(zb, (oy, ox), oz)
        vx, vy, vz = proj(pos)
        # a vertex is lit from d if nothing is clearly in front of it
        front = zb[vy, vx]
        cosw = nrm @ d
        lit = (vz - front) < bias
        m = cosw > 0
        vis_acc[m] += lit[m] * cosw[m]; w_acc[m] += cosw[m]
        # thickness: from a camera in front of the vertex, the distance from the vertex
        # (first layer) to the next surface behind it (second layer) = material it sits on
        b = cosw > 0.85
        if b.any():
            z0p = zb[oy, ox]
            sec = oz > z0p + 0.012
            zb2 = np.full((grid, grid), 1e9); np.minimum.at(zb2, (oy[sec], ox[sec]), oz[sec])
            own = np.abs(vz - front) < 0.012
            c = b & own
            tv = zb2[vy[c], vx[c]] - vz[c]
            thick[c] = np.minimum(thick[c], np.maximum(tv, 0.012))
    ao = vis_acc / np.maximum(w_acc, 1e-6)
    return ao.astype(np.float32), thick.astype(np.float32)

def mean_curvature(pos, nrm, idx):
    nb = [set() for _ in range(len(pos))]
    for a, b, c in idx:
        nb[a].update((b, c)); nb[b].update((a, c)); nb[c].update((a, b))
    k = np.zeros(len(pos))
    for i, s in enumerate(nb):
        if not s: continue
        q = pos[list(s)] - pos[i]
        d2 = np.sum(q * q, 1) + 1e-12
        k[i] = np.mean(-2 * (q @ nrm[i]) / d2)
    return k.astype(np.float32)

def ray_thickness(pos, nrm, idx, nrays=6, cone=0.35, eps=0.004, seed=2, chunk=48):
    """Mean first-hit distance of rays shot inward (around -n) from each vertex: local thickness."""
    a = pos[idx[:, 0]]; e1 = pos[idx[:, 1]] - a; e2 = pos[idx[:, 2]] - a
    rng = np.random.default_rng(seed)
    out = np.zeros(len(pos))
    for s in range(0, len(pos), chunk):
        p = pos[s:s + chunk]; n = nrm[s:s + chunk]
        dist_acc = np.zeros(len(p)); cnt = np.zeros(len(p))
        for k in range(nrays):
            j = rng.normal(size=n.shape) * cone
            d = -n + j - n * (np.sum(j * -n, 1, keepdims=True) * 0)  # jittered inward
            d /= np.linalg.norm(d, axis=1, keepdims=True)
            o = p - n * eps
            pv = np.cross(d[:, None, :], e2[None])                  # (R,T,3)
            det = np.einsum('tk,rtk->rt', e1, pv)
            ok = np.abs(det) > 1e-12
            inv = np.where(ok, 1.0 / np.where(ok, det, 1), 0)
            tv = o[:, None, :] - a[None]
            u = np.einsum('rtk,rtk->rt', tv, pv) * inv
            qv = np.cross(tv, e1[None])
            v = np.einsum('rk,rtk->rt', d, qv) * inv
            t = np.einsum('tk,rtk->rt', e2, qv) * inv
            hit = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 1e-4)
            t = np.where(hit, t, np.inf).min(1)
            good = np.isfinite(t)
            dist_acc[good] += t[good]; cnt[good] += 1
        out[s:s + chunk] = np.where(cnt > 0, dist_acc / np.maximum(cnt, 1), 2.0)
    return out.astype(np.float32)

def ray_hits(orig, dirs, pos, idx, chunk=40, far=True):
    """Hit distance along each ray (farthest hit if far=True, else nearest). inf if none."""
    a = pos[idx[:, 0]]; e1 = pos[idx[:, 1]] - a; e2 = pos[idx[:, 2]] - a
    out = np.full(len(orig), np.inf)
    for s in range(0, len(orig), chunk):
        o = orig[s:s + chunk]; d = dirs[s:s + chunk]
        pv = np.cross(d[:, None, :], e2[None])
        det = np.einsum('tk,rtk->rt', e1, pv)
        ok = np.abs(det) > 1e-12
        inv = np.where(ok, 1.0 / np.where(ok, det, 1), 0)
        tv = o[:, None, :] - a[None]
        u = np.einsum('rtk,rtk->rt', tv, pv) * inv
        qv = np.cross(tv, e1[None])
        v = np.einsum('rk,rtk->rt', d, qv) * inv
        t = np.einsum('tk,rtk->rt', e2, qv) * inv
        hit = ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 1e-5)
        if far:
            tt = np.where(hit, t, -np.inf).max(1); tt[~np.isfinite(tt)] = np.inf
        else:
            tt = np.where(hit, t, np.inf).min(1)
        out[s:s + chunk] = tt
    return out
