import sys, numpy as np
obj, tgt, out = sys.argv[1], sys.argv[2], sys.argv[3]
V=[]; F=[]; G=[]; grp=None
for line in open(obj):
    if line.startswith('v '): V.append([float(x) for x in line.split()[1:4]])
    elif line.startswith('g '): grp=line.split()[1]
    elif line.startswith('f '):
        F.append([int(t.split('/')[0])-1 for t in line.split()[1:]]); G.append(grp)
V=np.array(V)
for line in open(tgt):
    if line[0].isdigit():
        p=line.split(); V[int(p[0])]+= [float(p[1]),float(p[2]),float(p[3])]
body=[f for f,g in zip(F,G) if g=='body']
top=max(V[i,1] for f in body for i in f)
cut=top-3.1
tris=[]
for f in body:
    if all(V[i,1]>cut for i in f):
        for k in range(1,len(f)-1): tris.append((f[0],f[k],f[k+1]))
T=np.array(tris); used=np.unique(T); remap=-np.ones(len(V),int); remap[used]=np.arange(len(used))
P=(V[used]-[0,top-1.2,0])*0.1  # metres, origin near eye level
N=np.zeros_like(P); TT=remap[T]
a,b,c=P[TT[:,0]],P[TT[:,1]],P[TT[:,2]]; fn=np.cross(b-a,c-a)
for k in range(3): np.add.at(N,TT[:,k],fn)
N/=np.linalg.norm(N,axis=1,keepdims=True)+1e-12
with open(out,'w') as o:
    for p in P: o.write('v %.5f %.5f %.5f\n'%tuple(p))
    for n in N: o.write('vn %.5f %.5f %.5f\n'%tuple(n))
    for t in TT: o.write('f %d//%d %d//%d %d//%d\n'%(t[0]+1,t[0]+1,t[1]+1,t[1]+1,t[2]+1,t[2]+1))
print(len(TT),'tris',len(P),'verts')
