from pathlib import Path
import shutil,json,hashlib
base=Path(__file__).resolve().parent
out=base.parent/'repo/evidence/wood-jump-feedback-2026-10-09'
out.mkdir(parents=True,exist_ok=True)
a=json.loads((base/'runtime/jump-receipt.json').read_text());b=json.loads((base/'runtime/guard-receipt.json').read_text())
assert a['status']==b['status']=='PASS' and not a['failures'] and not b['failures']
assert len(a['records'])==2 and a['wood_spam_inputs']>1 and a['wood_peak_vy_mps']>.8
assert b['ordinary_overlap_jump_cues']==1 and b['invalid_stop_cases']==8
for name in ['jump_probe.gd','guard_probe.gd','prepare_runtime.py','run_jump.sh','archive_jump.py','jump.log','guard.log','exit-code.txt','native-source-manifest.json','final-guard-source-manifest.json','native-audio-source-as-used.gd']:shutil.copy2(base/name,out/name)
for name in ['jump-receipt.json','guard-receipt.json']:shutil.copy2(base/'runtime'/name,out/name)
shutil.copy2(base/'runtime/presentation/audio/audio_director.gd',out/'final-audio-source-as-used.gd')
(out/'command-native.txt').write_text('sh /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/wood-jump-feedback/run_jump.sh\n')
(out/'command-guard.txt').write_text('proot-distro login ubuntu --shared-tmp -- /usr/bin/env GODOT_SILENCE_ROOT_WARNING=1 /usr/bin/timeout 30s /data/data/com.termux/files/usr/tmp/scraperx-godot-arm64/Godot_v4.7-stable_linux.arm64 --headless --path /data/data/com.termux/files/usr/tmp/scraperx-launch-repair-09h3foao/wood-jump-feedback/runtime --audio-driver Dummy --script res://guard_probe.gd\n')
(out/'guard-tool-exit-receipt.json').write_text(json.dumps({'observed_exec_session':65586,'exit_code':0,'result':'actual tools.write_stdin completion; no gameplay inference'},indent=2)+'\n')
checks={str(f.relative_to(out)):hashlib.sha256(f.read_bytes()).hexdigest() for f in out.rglob('*') if f.is_file() and f.name!='sha256.json'}
(out/'sha256.json').write_text(json.dumps(checks,indent=2)+'\n')
print('PASS archive native + final focused source gates')
