# Stage 1: geometry, per-vertex AO/curvature/thickness, UV raster of position/normal/tangent.
# Output: work/mesh.npz, work/raster_<res>.npz
import sys, os, numpy as np
HERE = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, HERE)
from geom import build, normals_tangents, ORIGIN, SCALE
from raster import raster_uv, surface_samples, ao_thickness, mean_curvature, ray_thickness
WORK = os.path.join(HERE, '..', 'work2'); os.makedirs(WORK, exist_ok=True)
RES = int(sys.argv[1]) if len(sys.argv) > 1 else 2048

g = build()
pos = g['pos_dm']; uv = g['uv']; idx = g['idx']; rv = g['rv']
N, T, w = normals_tangents(pos, uv, idx)
# smooth normals across UV seams: average by original vertex id
Nw = np.zeros((len(g['P']), 3)); np.add.at(Nw, rv, N)
Nw /= np.linalg.norm(Nw, axis=1, keepdims=True) + 1e-12
N = Nw[rv].astype(np.float32)
T = T - N * np.sum(N * T, 1, keepdims=True); T /= np.linalg.norm(T, axis=1, keepdims=True) + 1e-12

# welded topology for AO / curvature / thickness
uniq, inv = np.unique(rv, return_inverse=True)
widx = inv[idx]
wpos = g['P'][uniq]; wn = Nw[uniq]
curv = mean_curvature(wpos, wn, widx)
# occluders: the head plus the eyeballs (approximated by helper-eye vertices filled as spheres)
pts = surface_samples(wpos, wn, widx, 500000)
eyes = []
for grp in ('helper-l-eye', 'helper-r-eye'):
    vs = sorted(set(v for f, gg in zip(g['F'], g['G']) if gg == grp for v, _ in f))
    q = g['P'][vs]; c = q.mean(0); r = np.linalg.norm(q - c, axis=1).max()
    eyes.append((c, r))
    # the real eyeball we render: 12.2 mm, set back behind the aged lids (see stage3)
    d = np.random.default_rng(3).normal(size=(20000, 3)); d /= np.linalg.norm(d, axis=1, keepdims=True)
    pts = np.concatenate([pts, c + np.array([0, 0, -0.006]) + d * 0.122])
ao, _ = ao_thickness(wpos, wn, widx, pts, ndir=128, grid=360, bias=0.03)
thick = ray_thickness(wpos, wn, widx)
print('ao range', ao.min(), ao.max(), 'thick pct', np.percentile(thick, [1, 10, 50, 90]))
print('curv pct', np.percentile(curv, [1, 10, 50, 90, 99]))

np.savez(os.path.join(WORK, 'mesh.npz'), pos_dm=pos, uv=uv, idx=idx, N=N, T=T, w=w, rv=rv, isl=g['isl'], uv_old=g['uv_old'],
         ao=ao[inv], curv=curv[inv], thick=thick[inv], eyes=np.array([[*c, r] for c, r in eyes]), P=g['P'],
         islands=np.array([[*i['bb_min'], *i['bb_max'], *i['at'], i['scale']] for i in g['info']['islands']]))
Bv = np.cross(N, T) * w[:, None]
r = raster_uv(uv, idx, {'P': pos.astype(np.float32), 'N': N, 'T': T, 'B': Bv.astype(np.float32),
                        'isl': g['isl'][:, None].astype(np.float32), 'ouv': g['uv_old'].astype(np.float32),
                        'ao': ao[inv][:, None], 'thick': thick[inv][:, None], 'curv': curv[inv][:, None]}, RES)
np.savez(os.path.join(WORK, f'raster_{RES}.npz'), **r)
print('covered', r['mask'].mean())
