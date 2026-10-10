from pathlib import Path
import shutil,hashlib,json
base=Path(__file__).resolve().parent
out=base/'runtime'
shutil.copytree(base.parent/'timber-feedback/runtime',out,dirs_exist_ok=True)
shutil.copy2(base.parent/'repo/godot/presentation/audio/audio_director.gd',out/'presentation/audio/audio_director.gd')
shutil.copy2(base/'jump_probe.gd',out/'jump_probe.gd')
files=['presentation/audio/audio_director.gd','addons/scraperx_native/bin/linux/libscraperx_native.so','jump_probe.gd']
manifest={f:hashlib.sha256((out/f).read_bytes()).hexdigest() for f in files}
(base/'source-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest,indent=2))
