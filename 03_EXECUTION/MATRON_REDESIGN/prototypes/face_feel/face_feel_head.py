import sys, numpy as np
obj, tgt = sys.argv[1], sys.argv[2]
V=[]; F=[]; grp=None; G=[]
for line in open(obj):
    if line.startswith('v '): V.append([float(x) for x in line.split()[1:4]])
    elif line.startswith('g '): grp=line.split()[1]
    elif line.startswith('f '):
        idx=[int(t.split('/')[0])-1 for t in line.split()[1:]]
        F.append(idx); G.append(grp)
V=np.array(V)
for line in open(tgt):
    if line[0].isdigit():
        p=line.split(); V[int(p[0])]+= [float(p[1]),float(p[2]),float(p[3])]
body=[f for f,g in zip(F,G) if g=='body']
bv=np.unique(np.concatenate([np.array(f) for f in body]))
print('verts', len(V), 'body faces', len(body), 'body verts', len(bv))
ys=V[bv,1]; print('body y range', ys.min(), ys.max())
top=ys.max(); height=top-ys.min()
for cut in [0.80,0.84,0.87]:
    yc=ys.min()+height*cut
    hf=[f for f in body if all(V[i,1]>yc for i in f)]
    hv=np.unique(np.concatenate([np.array(f) for f in hf]))
    tris=sum(len(f)-2 for f in hf)
    print(f'cut {cut:.2f} y>{yc:.2f}: faces {len(hf)} tris {tris} verts {len(hv)}')
groups=sorted(set(G)); print([g for g in groups if 'helper' not in g and 'joint' not in g])
print([g for g in groups if 'teeth' in g or 'tongue' in g or 'eye' in g][:20])
