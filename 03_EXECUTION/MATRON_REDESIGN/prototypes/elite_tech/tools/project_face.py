"""Project a frontal face photo onto the head's UV albedo (own code, numpy+Pillow).

The route to a photographic face: a frontal, flat-lit portrait (made with an
Apache-2.0 model such as FLUX.1-schnell or Qwen-Image) is fitted to the head by
a similarity transform between landmarks (eye centres, nose tip, mouth centre),
then baked into the head's UV space. Each texel takes the photo where the skin
faces the camera (weight = N.z^2, z-buffer visible), and keeps the base albedo
elsewhere, with a feathered seam.

usage:
  python3 -I project_face.py DATA_DIR selftest OUT_DIR
      round trip: render the base albedo front-on in software, project that
      render back, report the texel error (proves the bake lands correctly)
  python3 -I project_face.py DATA_DIR photo.png lx ly rx ry mx my OUT_DIR
      lx,ly / rx,ry = the photo's eye centres in pixels, mx,my = mouth centre
"""
import json
import os
import sys

import numpy as np
from PIL import Image

D = sys.argv[1]
lod = os.path.join(D, "lod1")
P = np.fromfile(os.path.join(lod, "pos.f32"), "<f4").reshape(-1, 3).astype(np.float64)
N = np.fromfile(os.path.join(lod, "nrm.f32"), "<f4").reshape(-1, 3).astype(np.float64)
UV = np.fromfile(os.path.join(lod, "uv.f32"), "<f4").reshape(-1, 2).astype(np.float64)
T = np.fromfile(os.path.join(lod, "idx.i32"), "<i4").reshape(-1, 3)
eyes = json.load(open(os.path.join(D, "eyes.json")))
EL = np.array(eyes["l"]["centre"])
ER = np.array(eyes["r"]["centre"])
# mouth centre: the most forward vertex near the midline at lip height
cand = np.where((np.abs(P[:, 0]) < 0.004) & (P[:, 1] > 0.59) & (P[:, 1] < 0.625))[0]
MC = P[cand[np.argmax(P[cand, 2])]]


def raster(tri2d, size, attrs, zvals=None, zbuf=None):
    """Rasterize triangles (n,3,2 in pixels). attrs: (n,3,k) interpolated.
    Returns image (size,size,k), coverage mask, and z buffer if zvals given."""
    H = W = size
    out = np.zeros((H, W, attrs.shape[2]))
    mask = np.zeros((H, W), bool)
    zb = np.full((H, W), -1e9) if zvals is not None else None
    for t in range(len(tri2d)):
        p = tri2d[t]
        x0, y0 = np.floor(p.min(0)).astype(int)
        x1, y1 = np.ceil(p.max(0)).astype(int)
        x0, y0 = max(x0, 0), max(y0, 0)
        x1, y1 = min(x1, W - 1), min(y1, H - 1)
        if x1 < x0 or y1 < y0:
            continue
        xs, ys = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        a, b, c = p
        den = (b[1] - c[1]) * (a[0] - c[0]) + (c[0] - b[0]) * (a[1] - c[1])
        if abs(den) < 1e-12:
            continue
        w0 = ((b[1] - c[1]) * (xs - c[0]) + (c[0] - b[0]) * (ys - c[1])) / den
        w1 = ((c[1] - a[1]) * (xs - c[0]) + (a[0] - c[0]) * (ys - c[1])) / den
        w2 = 1 - w0 - w1
        ins = (w0 >= -1e-6) & (w1 >= -1e-6) & (w2 >= -1e-6)
        if not ins.any():
            continue
        val = w0[..., None] * attrs[t, 0] + w1[..., None] * attrs[t, 1] + w2[..., None] * attrs[t, 2]
        sl = (slice(y0, y1 + 1), slice(x0, x1 + 1))
        if zvals is not None:
            z = w0 * zvals[t, 0] + w1 * zvals[t, 1] + w2 * zvals[t, 2]
            ins &= z > zb[sl]
            zb[sl] = np.where(ins, z, zb[sl])
        out[sl] = np.where(ins[..., None], val, out[sl])
        mask[sl] |= ins
    return out, mask, zb


def front_camera(size, margin=0.15):
    """orthographic front camera framing the face: world (x,y) -> pixels"""
    cx, cy = 0.0, 0.645
    half = 0.13
    s = size / (2 * half)
    return lambda q: np.stack([(q[..., 0] - cx) * s + size / 2, (cy - q[..., 1]) * s + size / 2], -1)


def bake(photo, to_photo_px, base_alb, tex_size=1024):
    """photo: HxWx3 float; to_photo_px maps world points (...,3) to photo pixels."""
    ph, pw = photo.shape[:2]
    # visibility: z buffer in the photo's camera
    tri_w = P[T]
    tri_px = to_photo_px(tri_w)
    _, _, zb = raster(tri_px, max(ph, pw), np.zeros((len(T), 3, 1)), zvals=tri_w[..., 2])
    # bake in UV space: interpolate world position and normal per texel
    tri_uv = UV[T] * tex_size
    attrs = np.concatenate([P[T], N[T]], -1)
    pn, mask, _ = raster(tri_uv, tex_size, attrs)
    pos, nrm = pn[..., :3], pn[..., 3:]
    nrm /= np.maximum(np.linalg.norm(nrm, axis=-1, keepdims=True), 1e-9)
    px = to_photo_px(pos)
    xi = np.clip(px[..., 0].astype(int), 0, pw - 1)
    yi = np.clip(px[..., 1].astype(int), 0, ph - 1)
    vis = pos[..., 2] >= zb[yi, xi] - 0.004
    w = np.clip(nrm[..., 2], 0, 1) ** 2 * vis * mask
    w = np.clip((w - 0.15) / 0.5, 0, 1)
    # bilinear sample of the photo
    fx, fy = np.clip(px[..., 0] - 0.5, 0, pw - 1.001), np.clip(px[..., 1] - 0.5, 0, ph - 1.001)
    x0, y0 = fx.astype(int), fy.astype(int)
    ax, ay = (fx - x0)[..., None], (fy - y0)[..., None]
    smp = (photo[y0, x0] * (1 - ax) * (1 - ay) + photo[y0, x0 + 1] * ax * (1 - ay)
           + photo[y0 + 1, x0] * (1 - ax) * ay + photo[y0 + 1, x0 + 1] * ax * ay)
    base = np.asarray(Image.fromarray((base_alb * 255).astype(np.uint8)).resize((tex_size, tex_size), Image.LANCZOS)) / 255.0
    out = base * (1 - w[..., None]) + smp * w[..., None]
    return out, w


base_full = np.asarray(Image.open(os.path.join(D, "tex", "head_albedo.png")).convert("RGB")) / 255.0
if sys.argv[2] == "selftest":
    OUT = sys.argv[3]
    os.makedirs(OUT, exist_ok=True)
    size = 512
    cam = front_camera(size)
    # a UV checker as the "skin", rendered front-on in software
    tex = 1024
    yy, xx = np.mgrid[0:tex, 0:tex]
    checker = (((xx // 32) + (yy // 32)) % 2).astype(float)
    chk = np.stack([0.25 + 0.6 * checker, 0.3 + 0.3 * (xx / tex), 0.3 + 0.3 * (yy / tex)], -1)
    tri_px = cam(P[T])
    img, m, zb = raster(tri_px, size, UV[T], zvals=P[T][..., 2])
    ui = np.clip((img[..., 0] * tex).astype(int), 0, tex - 1)
    vi = np.clip((img[..., 1] * tex).astype(int), 0, tex - 1)
    photo = np.where(m[..., None], chk[vi, ui], 0.05)
    Image.fromarray((photo * 255).astype(np.uint8)).save(os.path.join(OUT, "selftest_render.png"))
    baked, w = bake(photo, cam, chk, tex)
    err = np.abs(baked - chk).sum(-1)
    sel = w > 0.99
    print("texels fully from the photo:", int(sel.sum()), "mean abs error there:", float(err[sel].mean()),
          "share with error > 0.1:", float((err[sel] > 0.1).mean()))
    Image.fromarray((baked * 255).astype(np.uint8)).save(os.path.join(OUT, "selftest_baked.png"))
    Image.fromarray((w * 255).astype(np.uint8)).save(os.path.join(OUT, "selftest_weight.png"))
else:
    photo = np.asarray(Image.open(sys.argv[2]).convert("RGB")) / 255.0
    lx, ly, rx, ry, mx, my = map(float, sys.argv[3:9])
    OUT = sys.argv[9]
    # similarity transform from the mesh's front (x,y) landmarks to photo pixels
    src = np.array([[EL[0], -EL[1]], [ER[0], -ER[1]], [MC[0], -MC[1]]])  # world y up -> image y down
    dst = np.array([[lx, ly], [rx, ry], [mx, my]])
    A = []
    b = []
    for (x, y), (u, v) in zip(src, dst):
        A.append([x, -y, 1, 0]); b.append(u)
        A.append([y, x, 0, 1]); b.append(v)
    a_, b_, tx, ty = np.linalg.lstsq(np.array(A), np.array(b), rcond=None)[0]
    def to_px(q):
        x, y = q[..., 0], -q[..., 1]
        return np.stack([a_ * x - b_ * y + tx, b_ * x + a_ * y + ty], -1)
    baked, w = bake(photo, to_px, base_full, 2048)
    Image.fromarray((baked * 255).astype(np.uint8)).save(os.path.join(OUT, "head_albedo_projected.png"))
    Image.fromarray((w * 255).astype(np.uint8)).save(os.path.join(OUT, "projection_weight.png"))
    print("baked", OUT)
