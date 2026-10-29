import pickle,os,collections,re,glob,json
from common import R,S
res,core,corenat,game,arms,scen=pickle.load(open(S+'/gd_res.pkl','rb'))
funcs,edges,nat,fam,extends,scen2,arms2,bound,gdfiles=pickle.load(open(S+'/gd.pkl','rb'))
d=json.load(open(S+'/cg.json'))
defs={};cedges=collections.defaultdict(set);over=[]
for f,v in d.items():
    for u,x in v['defs'].items(): defs[u]=x
    for a,b,l in v['edges']: cedges[a].add(b)
    over+=v['over']
for o,b in over: cedges[b].add(o)
srcdef={u:x for u,x in defs.items() if x[2].startswith('src/sim')}
hub=set(u for u,x in defs.items() if x[0]=='advance_frame' and x[2].startswith('src/sim/simulation'))
bkey=collections.defaultdict(list)
for u,x in defs.items():
    if x[2]=='src/bridge/scraperx_simulation.cpp' and x[1]=='ScraperXSimulation': bkey[x[0]].append(u)
def closure(sd):
    seen=set(sd);st=list(sd)
    while st:
        x=st.pop()
        if x in hub: continue
        for y in cedges.get(x,()):
            if y not in seen: seen.add(y);st.append(y)
    return seen
ffield=collections.defaultdict(set)
for _f,_v in d.items():
    for cur,fl in _v.get('fields',[]): ffield[cur].add(fl)
bkey=collections.defaultdict(list)
for u,x in defs.items():
    if x[2]=='src/bridge/scraperx_simulation.cpp' and x[1]=='ScraperXSimulation': bkey[x[0]].append(u)
def _full(sd):
    seen=set(sd);st=list(sd)
    while st:
        x=st.pop()
        for y in cedges.get(x,()):
            if y not in seen: seen.add(y);st.append(y)
    return seen
_allnat={n for v in nat.values() for n in v}
tickset=_full(set(hub)|{k for nme in _allnat for k in bkey.get(bound.get(nme,nme),[])})
tickfiles={defs[u][2] for u in tickset if u in srcdef}

ffield=collections.defaultdict(set)
for _f,_v in d.items():
    for cur,fl in _v.get('fields',[]): ffield[cur].add(fl)
S_=list(res.keys())
n=len(S_)
istest=lambda f:os.path.basename(f).startswith('ui_test_')
bn=lambda f:f.replace('godot/presentation/','')
# per-scenario: natives called straight from scenario code or its direct game calls (all, including per-frame ones)
simd={}
for s in S_:
    r=res[s]
    keys=[k for nme in r['nD'] for k in bkey.get(bound.get(nme,nme),[])]
    simd[s]={u for u in closure(keys) if u in srcdef}
    r['simd']=simd[s]
    r['fs']=set().union(*[ffield[u] for u in closure(keys) if u in ffield] or [set()])
fcn=collections.Counter(f for s in S_ for f in res[s]['fs'])
common={f for f,c in fcn.items() if c>0.35*n}
for s in S_: res[s]['obs']=res[s]['fs']-common
def sizeof(s): return len(res[s]['D'])+len(res[s]['nD'])
order=sorted(S_,key=lambda s:(sizeof(s),s))
rank={s:i for i,s in enumerate(order)}
def show(xs,cap=6):
    xs=sorted(xs,key=lambda s:rank[s])
    t=', '.join(f'`{x}`' for x in xs[:cap])
    return t+(f' +{len(xs)-cap} more' if len(xs)>cap else '')
# device/families
def device(s):
    if s.startswith('touch_'): return 'touch (gestures; `_buttons` too)'
    if s.startswith('pad_'): return 'gamepad events'
    if s.startswith('keyboard_'): return 'keyboard/mouse events'
    return 'none injected (renders/audio/perf)' if s.startswith(('look_','perf_','audio_','fall_')) else 'touch'
rows=['| Scenario | Spawn | Driver | Input | Game functions it calls straight from the scenario (file:function) | Bridge methods it calls (native) | `src/sim` files those run (no per-tick step) |','|---|---|---|---|---|---|---|']
fcount=collections.Counter(f for s in S_ for f in {defs[u][2] for u in simd[s]})
for s in order:
    r=res[s]
    gd=sorted({os.path.basename(k[0])[:-3]+':'+k[1] for k in r['D']})
    gds=', '.join(gd[:5])+(f' +{len(gd)-5}' if len(gd)>5 else '')
    nn=sorted(r['nD']); nns=', '.join(nn[:5])+(f' +{len(nn)-5}' if len(nn)>5 else '')
    sf=sorted(f.replace('src/sim/','') for f in {defs[u][2] for u in simd[s]} if fcount[f]<=0.35*n and not f.endswith('.hpp'))
    sfs=', '.join(sf[:8])+(f' +{len(sf)-8}' if len(sf)>8 else '')
    rows.append(f'| `{s}` | {scen[s]} | `{",".join(arms[s])}` | {device(s)} | {gds or "-"} | {nns or "-"} | {sfs or "-"} |')
scen_table='\n'.join(rows)
# ---- GD files inverted
pres=sorted(f for f in gdfiles if f.startswith('godot/presentation/') and not istest(f))
gdi=collections.defaultdict(lambda:collections.defaultdict(set))
for s in S_:
    for k in res[s]['D']: gdi[k[0]][k[1]].add(s)
    for k in res[s]['spec']: gdi[k[0]].setdefault(k[1],set()).add(s)
# headless tests
heads=sorted(f for f in gdfiles if f.startswith('godot/tests/'))
hp={}
for h in heads:
    t=open(R+'/'+h,encoding='utf-8').read()
    pre=sorted({'godot/'+p for p in re.findall(r'(?:preload|load)\("res://([^"]+\.gd)"\)',t)})
    hp[h]=pre
rows=['| File | Lines | Functions run by a scenario | Per-frame core | Scenarios that call into it (narrowest first) | Headless tests | Verdict |','|---|---|---|---|---|---|---|']
unreach=[]
for f in pres:
    nl=sum(1 for _ in open(R+'/'+f,encoding='utf-8'))
    fk=[k for k in funcs if k[0]==f]
    nc=sum(1 for k in fk if k in core)
    ss=set()
    for fn,sset in gdi.get(f,{}).items(): ss|=sset
    anyreach=nc or ss or any(f in v for v in hp.values())
    hs=[os.path.basename(h) for h,v in hp.items() if f in v]
    # reached by calls from other game funcs at all?
    allr=bool(nc) or bool(ss)
    # called from any function at all (even non-core)?
    called=any(k in {y for e in edges.values() for y in e} for k in fk)
    if not allr and not hs:
        v='no scenario or test reaches it' if not called else 'only called from code no scenario reaches'
        unreach.append(f)
    elif len(ss)>=0.7*n: v='core'
    elif ss: v='scenario-specific' 
    else: v='per-frame core only (every scenario)'
    rows.append(f'| `{bn(f)}` | {nl} | {len(fk)} | {nc}/{len(fk)} | {show(ss) if ss else "-"} | {", ".join(hs) or "-"} | {v} |')
gd_table='\n'.join(rows)
# ---- inverted src/sim for scenarios
rows=['| `src/sim` file | **N**: scenarios whose bridge calls run it | **O**: scenarios that read a bridge value this file writes (it runs in their ticks) | N/O counts |','|---|---|---|---|']
allsim=sorted(os.path.relpath(p,R) for p in glob.glob(R+'/src/sim/**/*.cpp',recursive=True))
sim_unreach=[]
for f in allsim:
    ss=[s for s in S_ if f in {defs[u][2] for u in simd[s]}]
    fu=[u for u,x in srcdef.items() if x[2]==f]
    ob=[s for s in S_ if any(ffield.get(u,set())&res[s]['obs'] for u in fu)]
    if not ss and not ob and not any(x[2]==f for u,x in srcdef.items() if u in tickset): sim_unreach.append(f)
    a=("every scenario that reads state (%d of %d)"%(len(ss),n)) if len(ss)>=0.8*n else (show(ss) if ss else '-')
    b=show(ob) if ob and len(ob)<0.8*n else (('most scenarios (%d)'%len(ob)) if ob else '-')
    rows.append(f'| `{f}` | {a if (ss or not ob) else "-"} | {b if ob else ("tick only: every scenario that advances the world runs it; none reads a value it writes" if f in tickfiles else "-")} | {len(ss)}/{len(ob)} |')
sim_table='\n'.join(rows)
# ---- GD function tables for big files
bigrows=[]
for f in pres:
    nl=sum(1 for _ in open(R+'/'+f,encoding='utf-8'))
    if nl<600: continue
    rr=['| Lines | Function | Scenarios that call it (narrowest first) | Per-frame core |','|---|---|---|---|']
    for k in sorted([k for k in funcs if k[0]==f],key=lambda k:funcs[k][0]):
        a,b,_=funcs[k]
        if b-a<25: continue
        ss=gdi.get(f,{}).get(k[1],set())
        rr.append(f'| {a}-{b} | `{k[1]}` | {show(ss) if ss else "-"} | {"yes" if k in core else ("no" if not ss else "no")} |')
    bigrows.append(f'### `{bn(f)}` ({nl} lines)\n\n'+'\n'.join(rr)+'\n')
gd_fn='\n'.join(bigrows)
# headless table
rows=['| Test | Loads | Lines |','|---|---|---|']
for h in heads:
    rows.append(f'| `{h.replace("godot/","")}` | {", ".join("`"+bn(p)+"`" for p in hp[h]) or "-"} | {sum(1 for _ in open(R+"/"+h))} |')
head_table='\n'.join(rows)
pickle.dump(dict(scen_table=scen_table,gd_table=gd_table,sim_table=sim_table,gd_fn=gd_fn,head_table=head_table,unreach=unreach,sim_unreach=sim_unreach,n=n,order=order),open(S+'/gd_out.pkl','wb'))
print(len(scen_table),len(gd_table),len(sim_table),len(gd_fn),unreach,sim_unreach)
