import re,os,glob,collections,pickle,json
from common import R,S
G=R+'/godot'
files=[p for p in glob.glob(G+'/**/*.gd',recursive=True) if '/.godot/' not in p and '/addons/' not in p]
rel=lambda p:os.path.relpath(p,R)
funcs={}   # (file,name)->(start,end,body)
byname=collections.defaultdict(list)
extends={}
src={}
for p in sorted(files):
    L=open(p,encoding='utf-8').read().split('\n'); src[rel(p)]=L
    m=re.search(r'^extends\s+"res://([^"]+)"',"\n".join(L[:5]),re.M)
    extends[rel(p)]='godot/'+m.group(1) if m else None
    starts=[(i,re.match(r'^\s*(?:static\s+)?func\s+(\w+)',l).group(1)) for i,l in enumerate(L) if re.match(r'^(?:static\s+)?func\s+(\w+)',l)]
    for k,(i,n) in enumerate(starts):
        j=len(L)
        for t in range(i+1,len(L)):
            if L[t] and not L[t][0] in ' \t#\n' : j=t;break
        # trim trailing blank/comments
        funcs[(rel(p),n)]=(i+1,j,'\n'.join(L[i:j]))
        byname[n].append((rel(p),n))
# families via extends
parent={f:f for f in src}
def find(x):
    while parent[x]!=x: parent[x]=parent[parent[x]]; x=parent[x]
    return x
for f,e in extends.items():
    if e and e in src: parent[find(f)]=find(e)
fam={f:find(f) for f in src}
bound=dict(re.findall(r'D_METHOD\("(\w+)"[^)]*\)\s*,\s*&ScraperXSimulation::(\w+)',open(R+'/src/bridge/scraperx_simulation.cpp').read()))
boundnames=set(bound)
KW={'if','elif','while','for','match','func','return','await','and','or','not','in','is','as','var','const','print','range','str','int','float','bool','abs','min','max','sign','clamp','lerp','sqrt','floor','ceil','round','len','size','assert','push_error','push_warning','Vector2','Vector3','Color','Array','Dictionary','typeof','load','preload','Callable','StringName'}
def calls(body):
    body=re.sub(r'"(?:\\.|[^"\\])*"','""',body)
    body=re.sub(r'#.*','',body)
    out=[]
    for m in re.finditer(r'\b([A-Za-z_]\w*)\s*\(',body):
        dotted=body[max(0,m.start(1)-1):m.start(1)]=='.'
        rcv=None
        if dotted:
            mm=re.search(r'(\w+)\.$',body[:m.start(1)])
            rcv=mm.group(1) if mm else None
        out.append((m.group(1),dotted,rcv))
    return out
edges=collections.defaultdict(set); nat=collections.defaultdict(set)
for (f,n),(a,b,body) in funcs.items():
    # names passed as callables
    raw=re.sub(r'#.*','',body)
    cand=set()
    for c,dotted,rcv in calls(body):
        if c in KW: continue
        if c in boundnames and dotted: nat[(f,n)].add(c)
        if c in byname:
            ts=byname[c]
            if not dotted:
                same=[t for t in ts if fam[t[0]]==fam[f]]
                ts=same or ts
            elif rcv and len({fam[t[0]] for t in ts})>1:
                rr=rcv.lstrip('_').replace('_','').lower()
                def okf(t):
                    b=os.path.basename(t[0])[:-3].replace('_','').lower()
                    return rr and (rr in b or b in rr)
                pick=[t for t in ts if okf(t)]
                if rr in ('main',): pick=[t for t in ts if os.path.basename(t[0]).startswith('main')]
                ts=pick or ts
            for t in ts: cand.add(t)
    for m in re.finditer(r'(?:connect|call_deferred|Callable)\(([^)]*)\)',raw):
        for w in re.findall(r'\b_?[A-Za-z]\w+\b',m.group(1)):
            for t in byname.get(w,[]):
                if fam[t[0]]==fam[f]: cand.add(t)
    edges[(f,n)]|=cand
# scenario arms
drv=src['godot/presentation/ui/ui_test_driver.gd']
txt='\n'.join(drv)
scen_decl=re.search(r'const SCENARIOS := \{(.*?)\n\}',txt,re.S).group(1)
scen={}
for m in re.finditer(r'^\t"(\w+)":\s*(\d+)',scen_decl,re.M): scen[m.group(1)]=int(m.group(2))
runb=txt[txt.index('match _scenario:'):txt.index('if _scenario.begins_with("touch_"):\n\t\tvar counts')]
arms={}
for m in re.finditer(r'^\t\t((?:"\w+"(?:,\s*)?)+):\n((?:\t\t\t.*\n)+)',runb,re.M):
    names=re.findall(r'"(\w+)"',m.group(1))
    fn=re.findall(r'await\s+(\w+)\(',m.group(2))
    for nme in names: arms[nme]=fn
missing=[s for s in scen if s not in arms]
print('scenarios',len(scen),'arms',len(arms),'missing',missing)
pickle.dump((funcs,dict(edges),dict(nat),fam,extends,scen,arms,bound,list(src.keys())),open(S+'/gd.pkl','wb'))
print(len(funcs),sum(len(v) for v in edges.values()),len(boundnames))
