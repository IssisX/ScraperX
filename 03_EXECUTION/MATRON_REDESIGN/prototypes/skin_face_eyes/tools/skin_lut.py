# Pre-integrated skin diffusion LUT (Penner & Borshukov, GPU Pro 2, 2011), using the
# six-Gaussian skin diffusion profile of d'Eon & Luebke (GPU Gems 3, ch. 14, 2007).
# x: N.L in [-1,1]; y (row 0 = top): curvature row c in [0,1] -> sphere radius r = 10/(4*c) mm (c=1: 2.5 mm).
# Stored sqrt-encoded (precision near the terminator); the shader squares it.
import sys, numpy as np
from PIL import Image
var = np.array([0.0064, 0.0484, 0.187, 0.567, 1.99, 7.41])        # mm^2
w = np.array([[0.233, 0.455, 0.649], [0.100, 0.336, 0.344], [0.118, 0.198, 0.0],
              [0.113, 0.007, 0.007], [0.358, 0.004, 0.0], [0.078, 0.0, 0.0]])
w = w / w.sum(0)                                                    # each channel integrates to 1
def R(d):
    g = np.exp(-d[..., None] ** 2 / (2 * var)) / (2 * np.pi * var)
    return g @ w
W, H = 256, 64
xs = np.linspace(-np.pi, np.pi, 2048)
img = np.zeros((H, W, 3))
for j in range(H):
    c = (H - 1 - j + 0.5) / H                    # top row = high curvature
    r = 10.0 / (4.0 * max(c, 0.002))             # mm
    d = np.abs(2 * r * np.sin(xs / 2))
    prof = R(d)
    norm = prof.sum(0)
    for i in range(W):
        ndl = (i + 0.5) / W * 2 - 1
        th = np.arccos(np.clip(ndl, -1, 1))
        lit = np.clip(np.cos(th + xs), 0, None)[:, None]
        img[j, i] = (lit * prof).sum(0) / norm
img = np.clip(img, 0, 1)
Image.fromarray((np.sqrt(img) * 255 + 0.5).astype(np.uint8)).save(sys.argv[1])
print('lut ok; flat row at ndl=0.5:', img[-1, int(0.75 * W)].round(3), 'curved row at ndl=0:', img[0, W // 2].round(3))
