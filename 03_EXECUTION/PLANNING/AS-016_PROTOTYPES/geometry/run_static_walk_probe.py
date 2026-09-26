#!/usr/bin/env python3
"""Build and run an isolated STATIC receiving-support prototype.

Example (native Linux):
 python run_static_walk_probe.py --repo /path/ScraperX --build /path/build \
   --jolt-src /path/JoltPhysics --compiler c++ --out /tmp/scraperx-receiver-proof
For Termux/Ubuntu proot add: --runner 'proot-distro login ubuntu --'
No repository writes. Refuses to overwrite an existing observation directory.
"""
import argparse, datetime, hashlib, json, os, pathlib, re, shlex, subprocess, sys
p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
p.add_argument('--repo',required=True,type=pathlib.Path)
p.add_argument('--build',required=True,type=pathlib.Path,help='Cached directory containing libscraperx_sim.a and _deps/jolt-build/libJolt.a')
p.add_argument('--jolt-src',type=pathlib.Path,help='Jolt checkout; default BUILD/_deps/jolt-src')
p.add_argument('--compiler',default='c++')
p.add_argument('--runner',default='',help='Optional execution prefix, parsed as argument words without a shell')
p.add_argument('--out',required=True,type=pathlib.Path,help='New output directory outside repository')
p.add_argument('--tip-heights',nargs='+',default=['7.55','8.0','8.17'],help='Static terminal tip heights to test; optional explicit envelope margins')
a=p.parse_args(); here=pathlib.Path(__file__).resolve().parent
repo=a.repo.resolve(); build=a.build.resolve(); out=a.out.resolve()
if out==repo or repo in out.parents:p.error('--out must be outside repository')
if out.exists():p.error('--out already exists; use a new directory to preserve prior observations')
out.mkdir(parents=True)
jolt=a.jolt_src.resolve() if a.jolt_src else build/'_deps/jolt-src'
runner=shlex.split(a.runner)
commands=[]
def run(cmd,**kw):
    commands.append(cmd)
    return subprocess.run(cmd,check=True,**kw)
run([sys.executable,str(here/'make_static_walk_probe.py'),'--repo',str(repo),'--out',str(out)])
# Definitions match the repository's cached Linux Jolt build used for this proof.
flags=['-std=c++17','-O2','-DNDEBUG','-pthread','-DJPH_DEBUG_RENDERER','-DJPH_OBJECT_STREAM','-DJPH_PROFILE_ENABLED','-DSCRAPERX_HAS_JOLT=1']
# The src/sim include dir preserves simulation.cpp's original relative fixture
# include after its translation unit is generated outside the repository.
includes=['-I'+str(repo/'src'),'-I'+str(repo/'src/sim'),'-I'+str(jolt)]
with (out/'build.log').open('w') as log:
    run(runner+[a.compiler]+flags+includes+['-c',str(out/'simulation_static_walk.cpp'),'-o',str(out/'simulation_static_walk.o')],stdout=log,stderr=subprocess.STDOUT)
    run(runner+[a.compiler,'-std=c++17','-O2','-DNDEBUG','-pthread','-I'+str(repo/'src'),str(here/'static_walk_main.cpp'),str(out/'simulation_static_walk.o'),str(build/'libscraperx_sim.a'),str(build/'_deps/jolt-build/libJolt.a'),'-o',str(out/'static_walk_probe')],stdout=log,stderr=subprocess.STDOUT)
cases=[]
for tip in a.tip_heights:
    logfile=out/f'static-walk-{tip}.log'
    cmd=runner+['env','SCRAPERX_PROBE_TIP_Y='+tip,str(out/'static_walk_probe')]
    with logfile.open('w') as log:run(cmd,stdout=log,stderr=subprocess.STDOUT)
    text=logfile.read_text()
    passed=[line for line in text.splitlines() if line.startswith('PASS STATIC_RECEIVER_PROBE')]
    if len(passed)!=1:raise RuntimeError('Missing unique PASS result in '+str(logfile))
    print(passed[0],flush=True)
    positions=[]
    for m in re.finditer(r'WAYPOINT (\S+) reached=(\d+) pos=\(([^)]+)\) grounded=(\d+) support=(\d+) deaths=(\d+) tick=(\d+)',text):
        positions.append({'name':m[1],'reached':bool(int(m[2])),'player_centre_xyz':[float(x) for x in m[3].split(',')],'grounded':bool(int(m[4])),'support_entity':int(m[5]),'death_count':int(m[6]),'tick':int(m[7])})
    cases.append({'tip_target_m':float(tip),'pass':True,'waypoints':positions,'log':logfile.name,'pass_line':passed[0]})
record={'status':'Isolated STATIC downstream fixture; not an implemented mechanism',
    'observed_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
    'source_commit':subprocess.check_output(['git','-c','safe.directory='+str(repo),'rev-parse','HEAD'],cwd=repo,text=True).strip(),
    'cases':cases,'commands':commands,
    'artifact_sha256':{name:hashlib.sha256((here/name).read_bytes()).hexdigest() for name in ['static-walk-fixture.patch','static_walk_main.cpp','make_static_walk_probe.py','run_static_walk_probe.py']},
    'limits':['Static bridge at terminal pose; no pipes, gate, pan, four-bar or arrest constructed',
    'Grounded support asserted at settled waypoints, not continuously every frame',
    'No teleport, player seeding, jump request, or movement retuning',
    'Normal public native walk/facing inputs; no Godot/touch/Android device verification']}
(out/'results.json').write_text(json.dumps(record,indent=2)+'\n')
print('Results:',out/'results.json')
