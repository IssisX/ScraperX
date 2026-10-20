import sys, numpy as np
from PIL import Image, ImageDraw
obj, tgt, out = sys.argv[1], sys.argv[2], sys.argv[3]
V=[]; F=[]; G=[]; grp=None
for line in open(obj):
    if line.startswith('v '): V.append([float(x) for x in line.split()[1:4]])
    elif line.startswith('g '): grp=line.split()[1]
    elif line.startswith('f '):
        F.append([int(t.split('/')[0])-1 for t in line.split()[1:]]); G.append(grp)
V=np.array(V)
if tgt != '-':
    for line in open(tgt):
        if line[0].isdigit():
            p=line.split(); V[int(p[0])]+= [float(p[1]),float(p[2]),float(p[3])]
keep=('body','helper-l-eye','helper-r-eye')
tris=[]
for f,g in zip(F,G):
    if g not in keep: continue
    if not all(V[i,1]>5.6 for i in f): continue
    for k in range(1,len(f)-1): tris.append((f[0],f[k],f[k+1]))
T=np.array(tris)
def render(yaw, fn):
    c,s=np.cos(yaw),np.sin(yaw)
    R=np.array([[c,0,s],[0,1,0],[-s,0,c]])
    P=(V-[0,6.6,0])@R.T
    a,b,cc=P[T[:,0]],P[T[:,1]],P[T[:,2]]
    n=np.cross(b-a,cc-a); n/=np.linalg.norm(n,axis=1,keepdims=True)+1e-9
    L=np.array([-0.5,0.4,0.75]); L/=np.linalg.norm(L)
    sh=np.clip(n@L,0,1)*0.85+0.12
    depth=(a[:,2]+b[:,2]+cc[:,2])/3
    order=np.argsort(depth)
    W=600; sc=W/3.2
    img=Image.new('RGB',(W,W),(20,20,22)); d=ImageDraw.Draw(img)
    for i in order:
        if n[i,2]<=0: continue
        pts=[(W/2+p[0]*sc, W/2-p[1]*sc) for p in (a[i],b[i],cc[i])]
        v=sh[i]; col=(int(225*v),int(190*v),int(170*v))
        d.polygon(pts,fill=col)
    img.save(fn)
render(0.0, out+'_front.png'); render(0.9, out+'_three.png')
print(len(T),'tris')
