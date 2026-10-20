import sys, os, json, numpy as np
sys.argv=['x', sys.argv[1], sys.argv[2]]
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)),'build_matron_body.py')).read().replace('if __name__ == "__main__":\n    main()',''))
V0, VT, faces = load_obj(os.path.join(DL, "3dobjs/base.obj"))
tris=tri_faces(faces)
T=np.array([t[0] for t in tris if t[2]=='body'])
bv=np.unique(T.ravel())
for nm in ['matron','stick','average']:
  for h in [0.5,0.6,0.65,0.7,0.75]:
    p=dict(PRESETS[nm]); p['height']=h
    V=morph(V0,p)*0.1
    hh=V[bv,1].max()-V[bv,1].min()
    vol=volume(V,T)
    print(nm,h,'natural h',round(hh,3),'mass',round(vol*1010,1),'bmi',round(vol*1010/hh**2,1))
