from pathlib import Path
import json,hashlib,shutil
base=Path(__file__).resolve().parent
out=base.parent/'repo/evidence/timber-feedback-2026-10-09'
out.mkdir(parents=True,exist_ok=True)
receipt=json.loads((base/'runtime/feedback-receipt.json').read_text())
assert receipt['status']=='PASS' and not receipt['failures']
assert receipt['native_audio_receipts']==2 and receipt['total_audio_receipts_including3source_pool_checks']==5
assert len(receipt['records'])==2
for name in ['feedback_probe.gd','run_feedback.sh','prepare_runtime.py','archive_feedback.py','source-manifest.json','feedback.log','exit-code.txt','import.log','preparation-first-failure.log']:shutil.copy2(base/name,out/name)
shutil.copy2(base/'runtime/feedback-receipt.json',out/'feedback-receipt.json')
shutil.copytree(base/'pre-lifecycle-fix',out/'pre-lifecycle-fix',dirs_exist_ok=True)
used=out/'source-as-used';used.mkdir(exist_ok=True)
for name in ['presentation/main.gd','presentation/audio/audio_director.gd']:
 target=used/name;target.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(base/'runtime'/name,target)
shutil.copy2(base/'runtime/project.godot',used/'project.godot')
(out/'command.txt').write_text('sh /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/timber-feedback/run_feedback.sh\n')
checks={str(p.relative_to(out)):hashlib.sha256(p.read_bytes()).hexdigest() for p in out.rglob('*') if p.is_file() and p.name!='sha256.json'}
(out/'sha256.json').write_text(json.dumps(checks,indent=2)+'\n')
print(json.dumps(receipt,indent=2))
