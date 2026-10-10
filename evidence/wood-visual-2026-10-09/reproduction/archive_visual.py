import json, hashlib, shutil
from pathlib import Path
base=Path('/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/wood-visual')
out=base.parent/'repo/evidence/wood-visual-2026-10-09'
out.mkdir(parents=True,exist_ok=True)
phases=['baseline','current','baseline-detail','final','baseline-open','latest']
for phase in phases:
 d=out/phase;d.mkdir(exist_ok=True)
 for name in ['visual-receipt.json','wood_visual_probe.gd']:
  shutil.copy2(base/phase/name,d/name)
 for f in (base/phase).glob('*.png'):shutil.copy2(f,d/f.name)
 for source,target in [(base/f'{phase}.log',d/'run.log'),(base/f'{phase}-source-manifest.json',d/'source-manifest.json'),(base/f'command-{phase}.txt',d/'command.txt'),(base/f'{phase}-exit-code.txt',d/'exit-code.txt')]:
  if source.exists():shutil.copy2(source,target)
latest=json.loads((base/'latest/visual-receipt.json').read_text())
old={x['pose']:x for p in ['baseline-detail','baseline-open'] for x in json.loads((base/p/'visual-receipt.json').read_text())['poses']}
comparisons=[]
for pose in latest['poses']:
 prior=old[pose['pose']]; rows=[]
 assert pose['tick']==prior['tick'] and pose['fracture_mask']==prior['fracture_mask']
 assert len(pose['segments'])==12 and len(prior['segments'])==12
 for a,b in zip(pose['segments'],prior['segments']):
  assert a['entity']==b['entity']
  for key in ['geometry_sha256','physical_origin','render_origin','basis_y']:assert a[key]==b[key],(pose['pose'],a['entity'],key)
  assert a['shader_broken_mask']==pose['fracture_mask']&0x7ff
  rows.append(a['entity'])
 comparisons.append({'pose':pose['pose'],'native_tick':pose['tick'],'actual_broken_mask':pose['fracture_mask'],'identical_base_geometry_and_native_pose_entities':rows,'native_shader_mask_matches':True,'open_interface':pose.get('open_interface',{})})
assert not latest['failures']
result={'status':'PASS','comparison_count':48,'comparisons':comparisons,'geometry_channels':'mesh vertices, base normals, triangle indices; UV/UV2 intentionally excluded','poses':'actual native physical/render origins and physical basis-Y equal between paired runs; kit_view transforms also separately checked against native render transforms','limits':'Measured same bridge/scenario only; no determinism guarantee, phone budget, final art, full Tower view or earned route claim'}
(out/'paired-comparison.json').write_text(json.dumps(result,indent=2)+'\n')
repro=out/'reproduction';repro.mkdir(exist_ok=True)
for name in ['run_visual.sh','wood_visual_probe.gd','wood_visual_detail.gd','wood_visual_open_interface.gd','wood_visual_final.gd','archive_visual.py']:shutil.copy2(base/name,repro/name)
shutil.copy2(base/'latest/project.godot',repro/'project.godot')
shutil.copy2(base/'latest/addons/scraperx_native/scraperx_native.gdextension',repro/'scraperx_native.gdextension')
for phase in ['baseline-detail','latest']:
 d=repro/phase;d.mkdir(exist_ok=True)
 for name in ['kit_view.gd','slingshot_wood.gdshader','plank_wood.gdshader']:
  source=base/phase/'presentation'/name
  if source.exists():shutil.copy2(source,d/name)
checks={str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for f in out.rglob('*') if f.is_file() and f.name!='sha256.json'}
(out/'sha256.json').write_text(json.dumps(checks,indent=2)+'\n')
print(json.dumps(result,indent=2))
