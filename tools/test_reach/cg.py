import sys, json, glob, os
from multiprocessing import Pool
import clang.cindex as ci
from common import R,S,BUILD
import shlex
import subprocess
_RES=subprocess.check_output(['clang','-print-resource-dir'],text=True).strip()
JOLT=BUILD+'/_deps/jolt-src/Build/..'
GCPP=[BUILD+'/_deps/godot_cpp-src/include',BUILD+'/_deps/godot_cpp-build/gen/include',BUILD+'/_deps/godot_cpp-src/gdextension']
# The flags of build/host when no compile_commands.json is given.
ARGS=['-std=c++17','-x','c++','-I'+R+'/src','-I'+JOLT,'-DSCRAPERX_HAS_JOLT=1','-DJPH_DEBUG_RENDERER','-DJPH_OBJECT_STREAM','-DJPH_PROFILE_ENABLED','-DNDEBUG','-DJPH_USE_AVX2','-DJPH_USE_SSE4_1','-DJPH_USE_SSE4_2','-DJPH_USE_F16C','-DJPH_USE_FMADD','-DJPH_USE_LZCNT','-DJPH_USE_TZCNT','-mavx2','-mfma','-mf16c','-mlzcnt','-mbmi','-DSCRAPERX_SOURCE_DIR="'+R+'"','-DSCRAPERX_FAB_PUBLIC_KEY="00"','-I'+_RES+'/include']+['-I'+g for g in GCPP]+['-w','-ferror-limit=0']
_CC=os.environ.get('COMPILE_COMMANDS') or os.path.join(BUILD,'compile_commands.json')
_CMDS={}
if os.path.exists(_CC):
    for e in json.load(open(_CC)):
        c=e.get('arguments') or shlex.split(e['command'])
        keep=[];skip=False
        for a in c[1:]:
            if skip: skip=False; continue
            if a in ('-o','-MF','-MT','-MQ'): skip=True; continue
            if a in ('-c','-MD','-MMD') or a==e['file'] or a.endswith(('.cpp','.o')): continue
            keep.append(a)
        _CMDS[os.path.realpath(os.path.join(e.get('directory',''),e['file']))]=keep
def args_for(path):
    k=_CMDS.get(os.path.realpath(path))
    if k is None: return ARGS
    return ['-x','c++']+k+['-I'+_RES+'/include']+['-I'+g for g in GCPP]+['-w','-ferror-limit=0']
FN={ci.CursorKind.FUNCTION_DECL,ci.CursorKind.CXX_METHOD,ci.CursorKind.CONSTRUCTOR,ci.CursorKind.DESTRUCTOR,ci.CursorKind.FUNCTION_TEMPLATE}
def key(c):
    l=c.canonical.location
    return '%s:%d:%d:%s'%(rel(l.file.name) if l.file else '?',l.line,l.column,c.spelling)
def rel(p): return os.path.relpath(p,R) if p else None
def run(path):
    idx=ci.Index.create()
    tu=idx.parse(path,args_for(path),options=ci.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES*0)
    defs={}; edges=[]; names={}; over=[]; fields=set()
    def usr(c):
        r=c.referenced
        return (r.get_usr() or r.spelling) if r else None
    def walk(c,cur):
        for ch in c.get_children():
            k=ch.kind
            if k in FN and ch.is_definition():
                f=ch.location.file
                if f is None or not rel(f.name).startswith(('src/','tests/')): 
                    continue
                u=key(ch)
                defs[u]=(ch.spelling,ch.semantic_parent.spelling if ch.semantic_parent else '',rel(f.name),ch.extent.start.line,ch.extent.end.line)
                if k==ci.CursorKind.CONSTRUCTOR:
                    over.append((u,'CTOR@'+key(ch.semantic_parent)))
                if k==ci.CursorKind.CXX_METHOD:
                    try:
                        par=ch.semantic_parent; todo=[par]; seenc=set()
                        while todo:
                            c0=todo.pop()
                            if c0 is None or key(c0) in seenc: continue
                            seenc.add(key(c0))
                            for b0 in c0.get_children():
                                if b0.kind==ci.CursorKind.CXX_BASE_SPECIFIER:
                                    bd=b0.type.get_declaration(); todo.append(bd)
                                    for m0 in bd.get_children():
                                        if m0.kind==ci.CursorKind.CXX_METHOD and m0.spelling==ch.spelling and m0.is_virtual_method() and len(list(m0.get_arguments()))==len(list(ch.get_arguments())):
                                            over.append((u,key(m0)))
                    except Exception: pass
                try:
                    for o in list(ch.get_overridden_cursors())+list(ch.canonical.get_overridden_cursors()): over.append((u,key(o)))
                except Exception: pass
                walk(ch,u)
            elif k==ci.CursorKind.LAMBDA_EXPR:
                walk(ch,cur)
            else:
                if cur and k in (ci.CursorKind.CALL_EXPR,ci.CursorKind.DECL_REF_EXPR,ci.CursorKind.MEMBER_REF_EXPR):
                    r=ch.referenced
                    if r is not None and r.kind in FN:
                        ru=key(r); edges.append((cur,ru,ch.location.line))
                        if ru not in names: names[ru]=(r.semantic_parent.spelling if r.semantic_parent else '',r.spelling)
                if cur and k==ci.CursorKind.MEMBER_REF_EXPR:
                    r=ch.referenced
                    if r is not None and r.kind==ci.CursorKind.FIELD_DECL :
                        fields.add((cur,(r.semantic_parent.spelling if r.semantic_parent else '')+'.'+r.spelling))
                if cur and k==ci.CursorKind.CALL_EXPR and ch.spelling in ('make_unique','make_shared','emplace_back','emplace','emplace_front','allocate_shared','construct_at'):
                    try:
                        if ch.spelling.startswith('make') or ch.spelling=='allocate_shared' or ch.spelling=='construct_at':
                            t=ch.type.get_template_argument_type(0) if ch.spelling!='construct_at' else ch.type
                        else:
                            kids=list(ch.get_children()); t=kids[0].type.get_template_argument_type(0)
                        dc=t.get_canonical().get_declaration()
                        if dc.kind in (ci.CursorKind.CLASS_DECL,ci.CursorKind.STRUCT_DECL):
                            edges.append((cur,'CTOR@'+key(dc),ch.location.line))
                    except Exception: pass
                if k in (ci.CursorKind.CLASS_DECL,ci.CursorKind.STRUCT_DECL,ci.CursorKind.NAMESPACE,ci.CursorKind.TRANSLATION_UNIT,ci.CursorKind.CLASS_TEMPLATE) or cur:
                    walk(ch,cur)
                else:
                    walk(ch,cur)
    walk(tu.cursor,None)
    return path,defs,edges,names,over,sorted(fields)
if __name__=='__main__':
    files=sorted(glob.glob(R+'/src/**/*.cpp',recursive=True)+glob.glob(R+'/tests/*.cpp'))
    files=[f for f in files if '/src/bridge/' not in f or f.endswith('scraperx_simulation.cpp')] if len(sys.argv)<2 else [os.path.join(R,x) for x in sys.argv[1:]]
    with Pool(8) as p: res=p.map(run,files)
    out={}
    for path,d,e,n,o,fl in res: out[rel(path)]={'defs':d,'edges':e,'names':n,'over':o,'fields':fl}
    json.dump(out,open(S+'/cg.json','w'))
    print(len(files),sum(len(v['defs']) for v in out.values()),sum(len(v['edges']) for v in out.values()))
