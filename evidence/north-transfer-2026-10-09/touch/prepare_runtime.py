#!/usr/bin/env python3
"""Create a private ordinary-touch runner; reuse existing texture import cache.
Does not build or import. Production native bridge must be supplied by root.
"""
from pathlib import Path
import argparse, hashlib, json, shutil, subprocess
p=argparse.ArgumentParser()
p.add_argument('--repo',default='/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/repo')
p.add_argument('--imported-project',default='/data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/inspection-touch/godot')
p.add_argument('--bridge',required=True)
p.add_argument('--target',required=True)
a=p.parse_args()
base=Path(__file__).resolve().parent
repo=Path(a.repo).resolve(); imported=Path(a.imported_project).resolve(); bridge=Path(a.bridge).resolve(); target=Path(a.target).resolve()
if target.exists(): raise SystemExit('Use a fresh private target; preserve existing evidence.')
if not (imported/'.godot/imported').is_dir(): raise SystemExit('Select an already imported project.')
if not bridge.is_file(): raise SystemExit('Root must supply the integrated bridge.')
target.mkdir(parents=True)
shutil.copytree(repo/'godot/presentation',target/'presentation')
(target/'assets').symlink_to(imported/'assets',target_is_directory=True)
(target/'.godot').mkdir()
(target/'.godot/imported').symlink_to(imported/'.godot/imported',target_is_directory=True)
for name in ['global_script_class_cache.cfg','uid_cache.bin','extension_list.cfg']:
    source=imported/'.godot'/name
    if source.exists(): shutil.copy2(source,target/'.godot'/name)
shutil.copy2(imported/'project.godot',target/'project.godot')
shutil.copy2(repo/'godot/main.tscn',target/'main.tscn')
# The imported project's extension descriptor already maps this host ABI.
descriptor=Path('addons/scraperx_native/scraperx_native.gdextension')
(target/descriptor).parent.mkdir(parents=True,exist_ok=True)
shutil.copy2(imported/descriptor,target/descriptor)
dest=target/'addons/scraperx_native/bin/linux/libscraperx_native.so'
dest.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(bridge,dest)
# Select ordinary normal-world dispatch, replacing just the existing diagnostic
# scenario name in this private project; production source remains unchanged.
main=target/'presentation/main.gd'
main.write_text(main.read_text().replace('"touch_west_brace_bay"','"touch_north_transfer"'))
driver=target/'presentation/ui/ui_test_driver.gd'
s=driver.read_text().replace('"touch_west_brace_bay"','"touch_north_transfer"')
s=s.replace('ok = await _touch_west_brace_bay()', 'ok = await _touch_north_transfer()')
driver.write_text(s+'\n'+(base/'north_driver.gd').read_text())
files=['main.tscn','presentation/main.gd','presentation/ui/ui_test_driver.gd','presentation/ui/input_router.gd','presentation/ui/touch_controls.gd','presentation/ui/hud.gd','presentation/kit_view.gd','project.godot',str(descriptor),str(dest.relative_to(target))]
manifest={'repository_head':subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip(),'target':str(target),'imported_project':str(imported),'supplied_bridge':str(bridge),'sha256':{f:hashlib.sha256((target/f).read_bytes()).hexdigest() for f in files if (target/f).is_file()},'runtime_status':'prepared; not executed'}
(target.parent/(target.name+'-prepared-source-manifest.json')).write_text(json.dumps(manifest,indent=2)+'\n')
print(target)
