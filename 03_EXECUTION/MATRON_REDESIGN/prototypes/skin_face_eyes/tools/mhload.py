# Minimal reader for MakeHuman hm08 base.obj + .target files (CC0 data).
# Written from the file formats, no MakeHuman code used.
import numpy as np
SP = '/tmp/claude-0/-home-user-ScraperX/290f45ef-c41d-5b08-903d-c4b10970a44f/scratchpad'
MH = SP + '/mh'
def load_obj(path=MH + '/base.obj'):
    V = []; VT = []; F = []; G = []; g = None
    for line in open(path):
        if line.startswith('v '): V.append([float(x) for x in line.split()[1:4]])
        elif line.startswith('vt '): VT.append([float(x) for x in line.split()[1:3]])
        elif line.startswith('g '): g = line.split()[1]
        elif line.startswith('f '):
            F.append([tuple(int(a) - 1 for a in t.split('/')[:2]) for t in line.split()[1:]]); G.append(g)
    return np.array(V), np.array(VT), F, G
TD = '/tmp/claude-0/-home-user-ScraperX/290f45ef-c41d-5b08-903d-c4b10970a44f/scratchpad/overhaul/skin_face_eyes/dl/mh_targets'
def target(n, name, tdir=TD):
    d = np.zeros((n, 3))
    for line in open(f'{tdir}/{name}.target'):
        if line.startswith('#') or not line.strip(): continue
        p = line.split(); d[int(p[0])] = [float(x) for x in p[1:4]]
    return d
AGE = {'head-age-incr': 1.0, 'l-cheek-volume-decr': 1.0, 'r-cheek-volume-decr': 0.9, 'l-eye-bag-incr': 0.8, 'r-eye-bag-incr': 1.0,
       'l-eye-push1-in': 0.8, 'r-eye-push1-in': 0.7, 'mouth-angles-down': 0.6, 'nose-hump-incr': 0.7, 'mouth-laugh-lines-out': 0.8,
       'l-cheek-bones-incr': 0.6, 'r-cheek-bones-incr': 0.6, 'mouth-upperlip-volume-decr': 0.8, 'mouth-lowerlip-volume-decr': 0.6}
# The macro shape the first panel missed: caucasian-female-old (MakeHuman's old-woman body/face),
# with average weight so she reads as a real old woman, not a skull.
MACRO = {'caucasian-female-old': 1.0, 'universal-female-old-minmuscle-averageweight': 1.0}
AGE_SCALE = {'l-cheek-volume-decr': 0.35, 'r-cheek-volume-decr': 0.35, 'l-eye-bag-incr': 0.7, 'r-eye-bag-incr': 0.7}
SAG = {'l-cheek-trans-down': 0.5, 'r-cheek-trans-down': 0.5}
EYE_MID = np.array([0.0, 7.2841, 1.2454])
def aged(V, extra=None, F=None, G=None):
    P = V.copy()
    for k, w in MACRO.items(): P = P + target(len(V), k) * w
    for k, w in AGE.items(): P = P + target(len(V), k) * w * AGE_SCALE.get(k, 1.0)
    for k, w in SAG.items(): P = P + target(len(V), k) * w
    if extra:
        for k, w in extra.items(): P = P + target(len(V), k) * w
    if F is not None:
        eyev = [i for f, g in zip(F, G) if g in ('helper-l-eye', 'helper-r-eye') for i, _ in f]
        P = P + (EYE_MID - P[eyev].mean(0))   # keep the eyes where every landmark below expects them
    return P
