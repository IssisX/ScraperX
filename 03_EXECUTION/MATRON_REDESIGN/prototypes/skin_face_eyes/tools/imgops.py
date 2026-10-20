# numpy-only image helpers: box/gauss blur, dilation, noise in 3D.
import numpy as np

def box1(a, r, axis):
    if r <= 0: return a
    pad = [(0, 0)] * a.ndim; pad[axis] = (r + 1, r)
    c = np.cumsum(np.pad(a, pad, mode='edge'), axis=axis, dtype=np.float64)
    n = a.shape[axis]
    hi = np.take(c, np.arange(2 * r + 1, 2 * r + 1 + n), axis=axis)
    lo = np.take(c, np.arange(0, n), axis=axis)
    return ((hi - lo) / (2 * r + 1)).astype(np.float32)

def gauss(a, sigma):
    """Three box passes ~ gaussian. Works on HxW or HxWxC."""
    if sigma < 0.5: return a.astype(np.float32)
    r = max(1, int(round(np.sqrt(12 * sigma * sigma / 3 + 1) / 2)))
    out = a.astype(np.float32)
    for _ in range(3):
        out = box1(out, r, 0); out = box1(out, r, 1)
    return out

def dilate(img, mask, iters=16):
    """Push colors outward from covered texels into empty ones (UV padding)."""
    img = img.astype(np.float32).copy(); m = mask.astype(np.float32).copy()
    sq = img.ndim == 2
    if sq: img = img[..., None]
    for _ in range(iters):
        acc = np.zeros_like(img); w = np.zeros_like(m)
        for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1), (1, -1), (-1, 1)):
            acc += np.roll(np.roll(img * m[..., None], dy, 0), dx, 1)
            w += np.roll(np.roll(m, dy, 0), dx, 1)
        grow = (m == 0) & (w > 0)
        img[grow] = acc[grow] / w[grow][:, None]
        m[grow] = 1.0
    return img[..., 0] if sq else img

def _hash3(ix, iy, iz, seed=0):
    h = (ix.astype(np.int64) * 73856093) ^ (iy.astype(np.int64) * 19349663) ^ (iz.astype(np.int64) * 83492791) ^ (seed * 2654435761)
    h = (h ^ (h >> 13)) * 1274126177
    h = h ^ (h >> 16)
    return (h & 0xFFFFFF).astype(np.float32) / float(0xFFFFFF)

def vnoise(p, seed=0):
    """Smooth value noise in 3D, p: (N,3) -> (N,) in [0,1]."""
    f = np.floor(p); t = p - f; t = t * t * (3 - 2 * t)
    i = f.astype(np.int64)
    out = 0
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                w = (t[:, 0] if dx else 1 - t[:, 0]) * (t[:, 1] if dy else 1 - t[:, 1]) * (t[:, 2] if dz else 1 - t[:, 2])
                out = out + w * _hash3(i[:, 0] + dx, i[:, 1] + dy, i[:, 2] + dz, seed)
    return out

def fbm(p, octaves=4, seed=0, lac=2.03, gain=0.5):
    s = 0; a = 1; norm = 0
    for o in range(octaves):
        s = s + a * vnoise(p * (lac ** o), seed + o * 17); norm += a; a *= gain
    return s / norm

def worley(p, seed=0, chunk=400000):
    """F1 and F2 cell distances (in cell units) of 3D Worley noise, plus the cell id hash."""
    n = len(p); F1 = np.empty(n, np.float32); F2 = np.empty(n, np.float32); ID = np.empty(n, np.float32)
    for s in range(0, n, chunk):
        q = p[s:s + chunk]; base = np.floor(q).astype(np.int64)
        d1 = np.full(len(q), 9.0); d2 = np.full(len(q), 9.0); idv = np.zeros(len(q))
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                for dz in (-1, 0, 1):
                    c = base + [dx, dy, dz]
                    j = np.stack([_hash3(c[:, 0], c[:, 1], c[:, 2], seed + k) for k in range(3)], 1)
                    d = np.linalg.norm(c + j - q, axis=1)
                    hid = _hash3(c[:, 0], c[:, 1], c[:, 2], seed + 7)
                    closer = d < d1
                    d2 = np.where(closer, d1, np.minimum(d2, d))
                    idv = np.where(closer, hid, idv)
                    d1 = np.where(closer, d, d1)
        F1[s:s + chunk] = d1; F2[s:s + chunk] = d2; ID[s:s + chunk] = idv
    return F1, F2, ID

def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0, 1); return t * t * (3 - 2 * t)

def srgb_to_lin(c): c = np.asarray(c, np.float32); return np.where(c <= 0.04045, c / 12.92, ((c + 0.055) / 1.055) ** 2.4)
def lin_to_srgb(c): c = np.clip(c, 0, 1); return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)

# sRGB(D65) <-> CIE Lab
_M = np.array([[0.4124, 0.3576, 0.1805], [0.2126, 0.7152, 0.0722], [0.0193, 0.1192, 0.9505]], np.float32)
_Mi = np.linalg.inv(_M).astype(np.float32)
_W = np.array([0.95047, 1.0, 1.08883], np.float32)
def rgb_to_lab(rgb):
    xyz = srgb_to_lin(rgb) @ _M.T / _W
    f = np.where(xyz > 0.008856, np.cbrt(xyz), 7.787 * xyz + 16 / 116)
    return np.stack([116 * f[..., 1] - 16, 500 * (f[..., 0] - f[..., 1]), 200 * (f[..., 1] - f[..., 2])], -1)
def lab_to_rgb(lab):
    fy = (lab[..., 0] + 16) / 116; fx = fy + lab[..., 1] / 500; fz = fy - lab[..., 2] / 200
    f = np.stack([fx, fy, fz], -1)
    xyz = np.where(f > 0.2069, f ** 3, (f - 16 / 116) / 7.787) * _W
    return lin_to_srgb(xyz @ _Mi.T)
