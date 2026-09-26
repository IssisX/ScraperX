#!/usr/bin/env python3
"""Apply isolated static-fixture patch to an out-of-repo simulation.cpp copy."""
import argparse, pathlib, subprocess, hashlib, json
p=argparse.ArgumentParser()
p.add_argument('--repo',required=True,type=pathlib.Path)
p.add_argument('--out',required=True,type=pathlib.Path)
a=p.parse_args(); a.repo=a.repo.resolve(); a.out=a.out.resolve()
if a.out==a.repo or a.repo in a.out.parents: p.error('--out must be outside the repository')
a.out.mkdir(parents=True,exist_ok=True)
source=a.repo/'src/sim/simulation.cpp'; patch=pathlib.Path(__file__).resolve().with_name('static-walk-fixture.patch')
target=a.out/'simulation_static_walk.cpp'; target.write_bytes(source.read_bytes())
subprocess.run(['patch','--batch','--fuzz=0',str(target),str(patch)],check=True)
record={'status':'Isolated STATIC downstream fixture; not a mechanism',
        'source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),
        'patch_sha256':hashlib.sha256(patch.read_bytes()).hexdigest(),
        'generated_sha256':hashlib.sha256(target.read_bytes()).hexdigest(),
        'changes':['Include cstdlib','Inject static bridge, cheeks, dock and connector before build_world_solids'],
        'unchanged':['controller and movement methods','spawn selection','simulation stepping','public input interface']}
(a.out/'generated-fixture-manifest.json').write_text(json.dumps(record,indent=2)+'\n')
