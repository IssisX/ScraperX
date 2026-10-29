import pickle,collections,os,re
from common import R,S
groups,res,direct,defs,names,cutres,core,hubs=pickle.load(open(S+'/native.pkl','rb'))
import json
_d=json.load(open(S+'/cg.json'))
ffield=collections.defaultdict(set)
for _f,_v in _d.items():
    for cur,fl in _v.get('fields',[]): ffield[cur].add(fl)
del _d
ALL='(all: scraperx_sim.athletic_traversal)'
G=list(res.keys())
srcdef={u:x for u,x in defs.items() if x[2].startswith('src/')}
def sfiles(seen): return {defs[u][2] for u in seen if u in srcdef}
size={g:sum(1 for u in cutres[g] if u in srcdef) for g in G}
order=sorted([g for g in G if g!=ALL],key=lambda g:(size[g],g))
rank={g:i for i,g in enumerate(order)}
def show(gs,cap=8):
    gs=sorted(gs,key=lambda g:rank.get(g,-1))
    s=', '.join(f'`{g}`' for g in gs[:cap])
    return s+(f' +{len(gs)-cap} more' if len(gs)>cap else '')
real=[g for g in G if g!=ALL]
n=len(real)
gread={g:set().union(*[ffield[u] for u in res[g][1] if u in defs and defs[u][2].startswith('tests/')] or [set()]) for g in G}
fc=collections.Counter(f for g in real for f in gread[g])
common={f for f,c in fc.items() if c>0.35*n}
gobs={g:gread[g]-common for g in G}
def fsrc(u): return ffield.get(u,set())-common
def tiers(u):
    dr=[g for g in real if u in direct[g]]
    t1=[g for g in real if u in cutres[g]]
    ob=[g for g in real if u in res[g][1] and (fsrc(u)&gobs[g])]
    return dr,t1,ob
def pick(u_list):
    dr=set();t1=set();ob=set()
    for u in u_list:
        a,b,c=tiers(u); dr|=set(a);t1|=set(b);ob|=set(c)
    return [('D',dr),('T1+O',t1&ob),('O',ob),('T1',t1)]
def fmt(tl,per=4):
    seen=set();parts=[]
    for lab,gs in tl:
        gs=[g for g in gs if g not in seen]
        if not gs: continue
        seen|=set(gs)
        if len(gs)>=0.8*n and lab in ('T1','O'): parts.append(f'**{lab}** ~all ({len(gs)})'); continue
        parts.append(f'**{lab}** '+show(gs,per))
    return ' · '.join(parts) if parts else '-'
cutf={g:sfiles(cutres[g]) for g in G}
allf={g:sfiles(res[g][1]) for g in G}
dirf={g:{defs[u][2] for u in direct[g]} for g in G}
ctest=re.search(r'foreach\(slice ([^)]*)\)',open(R+'/CMakeLists.txt').read()).group(1).split()
out=[]
# ---- group table
out.append('| Group (`SCRAPERX_ONLY=`) | ctest name | Runs | Src functions reached without the per-tick step | Files only this kind of group reaches |')
out.append('|---|---|---|---|---|')
fcount=collections.Counter(f for g in real for f in cutf[g])
for g in order+[ALL]:
    fns=res[g][0]
    ct='scraperx_sim.'+g if g in ctest else ('scraperx_sim.athletic_traversal' if g==ALL else '-')
    if g in ('shared_survival (own exe)','parkour_flow (own exe)'):
        ct='scraperx_sim.'+g.split(' ')[0]
    spec=sorted(f.replace('src/sim/','').replace('src/bridge/','bridge/') for f in cutf[g] if fcount[f]<=n*0.3 and not f.endswith('.hpp'))
    spec_s=', '.join(spec[:14])+(f' +{len(spec)-14}' if len(spec)>14 else '')
    out.append(f'| `{g}` | {ct} | {", ".join(fns[:6])}{" +%d"%(len(fns)-6) if len(fns)>6 else ""} | {size[g]} | {spec_s or "-"} |')
grp_table='\n'.join(out)
# ---- inverted per file
files=sorted({x[2] for x in srcdef.values()})
allsim=sorted(set(re.sub(r'^',"",p) for p in __import__('glob').glob(R+'/src/sim/**/*.cpp',recursive=True)))
allsim=[os.path.relpath(p,R) for p in allsim]
files=sorted(set(files)|set(allsim))
out=['| Source file | Lines | Run these (tier, narrowest first) | Groups reaching it without the tick | Groups reaching it only via the tick | Verdict |','|---|---|---|---|---|---|']
unreached=[]
for f in files:
    try: nl=sum(1 for _ in open(R+'/'+f))
    except Exception: nl='?'
    t1=[g for g in real if f in cutf[g]]
    t2=[g for g in real if f in allf[g] and f not in cutf[g]]
    dr=[g for g in real if f in dirf[g]]
    if not t1 and not t2 and f not in sfiles(core) and not any(f in allf[g] for g in G):
        v='NO TEST REACHES IT'; unreached.append(f)
    elif not t1: v='tick-only (gated by runtime state)'
    elif len(t1)>=0.8*n: v='core: ~every group'
    else: v='specific'
    us=[u for u,x in srcdef.items() if x[2]==f]
    pgs=fmt(pick(us))
    out.append(f'| `{f}` | {nl} | {pgs} | {len(t1)} | {len(t2)} | {v} |')
file_table='\n'.join(out)
# ---- per function for big files
big=[f for f in files if f.startswith('src/sim') and isinstance(nl,int)]
fn_sections=[]
BIG=[]
for f in files:
    if not f.startswith('src/'): continue
    try: nl=sum(1 for _ in open(R+'/'+f))
    except: continue
    if nl>=500: BIG.append((f,nl))
for f,nl in BIG:
    rows=[]
    fs=[(u,x) for u,x in srcdef.items() if x[2]==f]
    fs.sort(key=lambda t:t[1][3])
    seenkey=set()
    for u,x in fs:
        ln=x[4]-x[3]+1
        if ln<(25 if f.endswith('.cpp') else 40): continue
        t1=[g for g in real if u in cutres[g]]
        t2=[g for g in real if u in res[g][1] and u not in cutres[g]]
        pgs=fmt(pick([u]))
        q=(x[1]+'::' if x[1] and not x[1].startswith(('src/','tests/')) else '')+x[0]
        if not t1 and not t2: v='**no test reaches**'; 
        elif not t1: v='per-tick only'
        else: v=''
        out2=f'| {x[3]}-{x[4]} | `{q}` | {pgs} | {len(t1)}/{len(t2)} | {v} |'
        rows.append(out2)
    fn_sections.append(f'### `{f}` ({nl} lines)\n\n| Lines | Function | Run these (tier, narrowest first) | Reached w/o tick / only via tick | Note |\n|---|---|---|---|---|\n'+'\n'.join(rows)+'\n')
pickle.dump((grp_table,file_table,'\n'.join(fn_sections),unreached,order,size,len(real),sfiles(core),allf,cutf,dirf),open(S+'/native_out.pkl','wb'))
print(len(grp_table),len(file_table),sum(len(s) for s in fn_sections),unreached)
print(order[:5],order[-5:],n)
