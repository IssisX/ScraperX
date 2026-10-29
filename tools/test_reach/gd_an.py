import pickle,json,collections,re,os,glob
from common import R,S
funcs,edges,nat,fam,extends,scen,arms,bound,gdfiles=pickle.load(open(S+'/gd.pkl','rb'))
d=json.load(open(S+'/cg.json'))
defs={};cedges=collections.defaultdict(set);over=[];names={}
for f,v in d.items():
    for u,x in v['defs'].items(): defs[u]=x
    for a,b,l in v['edges']: cedges[a].add(b)
    over+=v['over']; names.update({k:tuple(x) for k,x in v['names'].items()})
for o,b in over: cedges[b].add(o)
srcdef={u:x for u,x in defs.items() if x[2].startswith('src/sim')}
isbridge={u for u,x in defs.items() if x[2]=='src/bridge/scraperx_simulation.cpp'}
hub=[u for u,x in defs.items() if x[0]=='advance_frame' and x[2].startswith('src/sim/simulation')]
def closure(sd,cut=set()):
    seen=set(sd);st=list(sd)
    while st:
        x=st.pop()
        if x in cut: continue
        for y in cedges.get(x,()):
            if y not in seen: seen.add(y);st.append(y)
    return seen
# bridge method name -> keys
bkey=collections.defaultdict(list)
for u,x in defs.items():
    if x[2]=='src/bridge/scraperx_simulation.cpp' and x[1]=='ScraperXSimulation': bkey[x[0]].append(u)
def native_keys(gdname):
    fn=bound.get(gdname,gdname)
    return bkey.get(fn,[])
istest=lambda f:os.path.basename(f).startswith('ui_test_')
CB={'_ready','_process','_physics_process','_input','_unhandled_input','_notification','_draw','_enter_tree','_exit_tree','_unhandled_key_input','_init'}
game=[k for k in funcs if not istest(k[0]) and not k[0].startswith('godot/tests/') and not k[0].startswith('godot/tools')]
coreseed=[k for k in game if k[1] in CB]
def gbfs(sd,allow=lambda k:True):
    seen=set(sd);st=list(sd)
    while st:
        x=st.pop()
        for y in edges.get(x,()):
            if y not in seen and allow(y): seen.add(y);st.append(y)
    return seen
core=gbfs(coreseed,lambda k:not istest(k[0]))
corenat={n for k in core for n in nat.get(k,())}
print('core funcs',len(core),'of',len(game),'core natives',len(corenat))
out={}
for s,fns in arms.items():
    seeds=[k for fn in fns for k in byn(fn)] if False else None
byname=collections.defaultdict(list)
for k in funcs: byname[k[1]].append(k)
res={}
for s,fns in arms.items():
    seeds=[k for fn in fns for k in byname.get(fn,[]) if istest(k[0])]
    clos=gbfs(seeds)
    tf=[k for k in clos if istest(k[0])]
    D=set()
    for k in tf:
        for y in edges.get(k,()):
            if not istest(y[0]): D.add(y)
    spec=gbfs(D,lambda k:not istest(k[0]))-core
    specD=D-core
    # natives: from test funcs and from specific game funcs
    nD={n for k in tf for n in nat.get(k,())}
    nS={n for k in spec for n in nat.get(k,())}
    nat_s=(nD|nS)-corenat
    nat_all=nD|nS
    res[s]=dict(seeds=seeds,tf=tf,D=D,spec=spec,nat_s=nat_s,nat_all=nat_all,nD=nD)
# sim reach of natives
def simreach(natset):
    keys=[k for n in natset for k in native_keys(n)]
    return closure(keys,set(hub))
for s,r in res.items():
    r['sim']={u for u in simreach(r['nat_s']) if u in srcdef}
    r['simD']={defs_u for n in r['nD']-corenat for k in native_keys(n) for defs_u in cedges.get(k,()) if defs_u in srcdef}
pickle.dump((res,core,corenat,game,arms,scen),open(S+'/gd_res.pkl','wb'))
import statistics
print(statistics.mean(len(r['nat_s']) for r in res.values()),statistics.mean(len(r['sim']) for r in res.values()))
print(sorted(((len(r['spec']),s) for s,r in res.items()))[:8],sorted(((len(r['spec']),s) for s,r in res.items()))[-5:])
