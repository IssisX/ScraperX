from pathlib import Path
import shutil,hashlib,json
base=Path(__file__).resolve().parent
repo=base.parent/'repo/godot'
source=base.parent/'wood-visual/latest'
out=base/'runtime'
out.mkdir(exist_ok=True)
for name in ['presentation','assets']:
 shutil.copytree(repo/name,out/name,dirs_exist_ok=True)
shutil.copytree(source/'addons',out/'addons',dirs_exist_ok=True)
(out/'.godot').mkdir(exist_ok=True)
shutil.copy2(source/'.godot/extension_list.cfg',out/'.godot/extension_list.cfg')
shutil.copytree(source/'.godot/imported',out/'.godot/imported',dirs_exist_ok=True)
shutil.copy2(source/'project.godot',out/'project.godot')
shutil.copy2(base/'feedback_probe.gd',out/'feedback_probe.gd')
files=['presentation/main.gd','presentation/audio/audio_director.gd','assets/audio/timber/timber_snap_400638_preview.wav','addons/scraperx_native/bin/linux/libscraperx_native.so','feedback_probe.gd']
manifest={f:hashlib.sha256((out/f).read_bytes()).hexdigest() for f in files}
(base/'source-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest,indent=2))
