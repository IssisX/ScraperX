# Pre-integrated skin LUT (Penner 2011) from d'Eon & Luebke's six-Gaussian skin profile.
import sys, numpy as np
from PIL import Image
var=np.array([0.0064,0.0484,0.187,0.567,1.99,7.41])  # mm^2
w=np.array([[0.233,0.455,0.649],[0.100,0.336,0.344],[0.118,0.198,0.0],[0.113,0.007,0.007],[0.358,0.004,0.0],[0.078,0.0,0.0]])
def R(d):  # d in mm -> rgb
    g=np.exp(-d[...,None]**2/(2*var))/(2*np.pi*var)
    return g@w
W,H=128,64
xs=np.linspace(-np.pi,np.pi,512)
img=np.zeros((H,W,3))
for j in range(H):
    curv=(j+0.5)/H              # 0..1 -> radius
    r=1.0/(curv*0.9+0.02)*1.0   # mm-ish scale; small curvature = big radius
    d=np.abs(2*r*np.sin(xs/2))
    prof=R(d)
    norm=prof.sum(0)
    for i in range(W):
        ndl=(i+0.5)/W*2-1
        th=np.arccos(np.clip(ndl,-1,1))
        lit=np.clip(np.cos(th+xs),0,None)[:,None]
        img[H-1-j,i]=(lit*prof).sum(0)/norm
img=np.clip(img,0,1)**(1/2.2)
Image.fromarray((img*255).astype(np.uint8)).save(sys.argv[1])
print('ok')
