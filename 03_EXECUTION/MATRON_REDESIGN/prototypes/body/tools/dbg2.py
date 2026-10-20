import sys, os, json, numpy as np
sys.argv=['x', sys.argv[1], sys.argv[2]]
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)),'build_matron_body.py')).read().replace('if __name__ == "__main__":\n    main()',''))
V0, VT, faces = load_obj(os.path.join(DL, "3dobjs/base.obj"))
tris=tri_faces(faces)
T=np.array([t[0] for t in tris if t[2]=='body'])
be=boundary_edges(T)
L=loops(be)
print('boundary loops', len(L), [len(l) for l in L][:20])
for l in L:
    print(len(l), V0[l].mean(0).round(2))
V=V0*0.1
print('vol', volume(V,T), 'shifted', volume(V+np.array([0,5,3.0]),T))
