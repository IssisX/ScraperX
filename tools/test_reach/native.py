import json,re,collections,os
from common import R,S
d=json.load(open(S+'/cg.json'))
defs={};edges=collections.defaultdict(set);names={};over=[]
for f,v in d.items():
    for u,x in v['defs'].items(): defs[u]=x
    for a,b,l in v['edges']: edges[a].add(b)
    names.update({k:tuple(x) for k,x in v['names'].items()})
    over+=v['over']
# virtual dispatch: base -> override
for o,b in over: edges[b].add(o)
# Jolt callbacks: calls to JPH PhysicsSystem::Update reach src overrides of Jolt virtuals
overrides_of_jph=[o for o,b in over if b not in defs]
jupd=[u for u,(p,n) in names.items() if p=='PhysicsSystem' and n=='Update']
for u in jupd:
    for o in overrides_of_jph: edges[u].add(o)
# destructors run wherever their class is constructed
_ct=collections.defaultdict(list)
for u,x in defs.items(): _ct[(x[1],x[0])].append(u)
for u,x in defs.items():
    if x[0].startswith('~'):
        for c in _ct.get((x[1],x[0][1:]),[]):
            if c.split(':')[0]==u.split(':')[0]: edges[c].add(u)
# groups
t=open(R+'/tests/simulation_tests.cpp').read()
main=t[t.index('int main()'):]
groups={}
for m in re.finditer(r'std::string\(only\)\s*==\s*"([^"]+)"\)\s*\{(.*?)return (?:EXIT_SUCCESS|0);',main,re.S):
    k=m.group(1)
    if k in groups: k=k+' (shadowed second block: never runs)'
    groups[k]=re.findall(r'\b(run_\w+|test_\w+)\(\)',m.group(2))
tail=main[main.rindex('return EXIT_SUCCESS;',0,main.index('run_pulley_static_end();')):] if False else main[main.index('run_pulley_static_end();'):]
tail=re.sub(r'//.*','',tail)
default=re.findall(r'\b(run_\w+)\(\)',tail)
groups['(all: scraperx_sim.athletic_traversal)']=default
byname=collections.defaultdict(list)
for u,(n,c,f,a,b) in defs.items(): byname[n].append(u)
def seeds(fns):
    out=[]
    for n in fns:
        us=[u for u in byname.get(n,[]) if defs[u][2].startswith('tests/')]
        out+=us
        if not us: print('MISSING',n)
    return out
def bfs(sd):
    seen=set(sd);st=list(sd)
    while st:
        x=st.pop()
        for y in edges.get(x,()):
            if y not in seen: seen.add(y);st.append(y)
    return seen
# other executables
other={}
for nm,f in [('shared_survival (own exe)','tests/shared_survival_tests.cpp'),('parkour_flow (own exe)','tests/parkour_flow_tests.cpp')]:
    other[nm]=[u for u,x in defs.items() if x[2]==f and x[0]=='main']
res={}
for g,fns in groups.items(): res[g]=(fns,bfs(seeds(fns)))
for g,sd in other.items(): res[g]=(['main'],bfs(sd))
direct={}
for g,(fns,seen) in res.items():
    if g in other: sd=other[g]
    else: sd=seeds(fns)
    # test-side closure: functions in tests/ reached
    tf=[u for u in seen if u in defs and defs[u][2].startswith('tests/')]
    dd=set()
    for u in tf:
        for y in edges.get(u,()):
            if y in defs and defs[y][2].startswith(('src/')): dd.add(y)
    direct[g]=dd
import pickle
pickle.dump((groups,{g:(f,s) for g,(f,s) in res.items()},direct,defs,names),open(S+'/native.pkl','wb'))
print(len(groups),len(res))
srcfiles=sorted({x[2] for x in defs.values() if x[2].startswith('src/')})
for f in srcfiles: 
    cnt=sum(1 for g,(fn,s) in res.items() if any(u in defs and defs[u][2]==f for u in s))
    print(f,cnt)

# ---- hub-cut reach
hubs=[u for u,x in defs.items() if x[0] in ('advance_frame',) and x[2].startswith('src/sim/simulation')]
print('hubs',hubs)
def bfs_cut(sd,cut):
    seen=set(sd);st=list(sd)
    while st:
        x=st.pop()
        if x in cut: continue
        for y in edges.get(x,()):
            if y not in seen: seen.add(y);st.append(y)
    return seen
cutres={}
for g,(fns,seen) in res.items():
    sd=other[g] if g in other else seeds(fns)
    cutres[g]=bfs_cut(sd,set(hubs))
core=bfs(hubs)
pickle.dump((groups,{g:(f,s) for g,(f,s) in res.items()},direct,defs,names,cutres,core,hubs),open(S+'/native.pkl','wb'))
import collections
def files(seen): return {defs[u][2] for u in seen if u in defs and defs[u][2].startswith('src/')}
cnt=collections.Counter()
for g,s in cutres.items():
    for f in files(s): cnt[f]+=1
print('core files',len(files(core)),'cut:',sorted(cnt.items(),key=lambda x:x[1]))
