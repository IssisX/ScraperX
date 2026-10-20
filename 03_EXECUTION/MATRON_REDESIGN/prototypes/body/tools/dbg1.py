import sys, os, json, numpy as np
sys.argv=['x', sys.argv[1], sys.argv[2]]
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)),'build_matron_body.py')).read().replace('if __name__ == "__main__":\n    main()',''))
V0, VT, faces = load_obj(os.path.join(DL, "3dobjs/base.obj"))
for nm in ['matron','average','stick']:
    p=PRESETS[nm]
    terms=macro_terms(p)
    V=morph(V0,p)
    tot=0
    for rel,k in terms:
        idx,d=target(rel)
        mag=np.abs(d).sum()*k
        print(nm, rel.split('/')[-1], round(k,3), len(idx), round(float(mag),1))
    print(nm, 'mean |V-V0|', np.abs(V-V0).mean())
