#!/usr/bin/env python3
"""Recreate the tested private input project, reusing imported assets/cache."""
from pathlib import Path
import argparse,hashlib,json,shutil
p=argparse.ArgumentParser(); p.add_argument('--imported-project',required=True); p.add_argument('--bridge',required=True); p.add_argument('--target',required=True); a=p.parse_args()
base=Path(__file__).resolve().parent
source=Path(a.imported_project).resolve(); bridge=Path(a.bridge).resolve(); target=Path(a.target).resolve()
manifest=json.loads((base/'tested-source-manifest.json').read_text())
expected=manifest['tested_runtime_sha256']['addons/scraperx_native/bin/linux/libscraperx_native.so']
if hashlib.sha256(bridge.read_bytes()).hexdigest()!=expected: raise SystemExit('Bridge differs from tested receipt; select the attested native bridge.')
if target.exists(): raise SystemExit('Target must be a fresh private directory; preserve existing projects.')
if not (source/'.godot/imported').is_dir(): raise SystemExit('Select a fully imported existing Godot project.')
# Copy only small scripts and metadata. Assets and texture import products
# are read from the already imported project; local runtime caches stay local.
target.mkdir(parents=True)
shutil.copytree(source/'presentation',target/'presentation')
(target/'assets').symlink_to(source/'assets',target_is_directory=True)
(target/'.godot').mkdir()
(target/'.godot/imported').symlink_to(source/'.godot/imported',target_is_directory=True)
for name in ['global_script_class_cache.cfg','uid_cache.bin','extension_list.cfg']:
    path=source/'.godot'/name
    if path.exists(): shutil.copy2(path,target/'.godot'/name)
for path in (base/'tested-scripts').rglob('*'):
    if path.is_file():
        dest=target/path.relative_to(base/'tested-scripts'); dest.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(path,dest)
dest=target/'addons/scraperx_native/bin/linux/libscraperx_native.so'; dest.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(bridge,dest)
print(target)
