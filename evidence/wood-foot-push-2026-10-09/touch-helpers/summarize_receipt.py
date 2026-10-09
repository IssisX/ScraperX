#!/usr/bin/env python3
"""Record the actual log/status/source identity; do not fill missing evidence."""
from pathlib import Path
import argparse,hashlib,json,re
p=argparse.ArgumentParser();p.add_argument('label');p.add_argument('--exit-code',type=int,required=True);p.add_argument('--project',required=True);p.add_argument('--helper-as-used');a=p.parse_args()
base=Path(__file__).resolve().parent;project=Path(a.project).resolve();log=base/(a.label+'.log')
lines=log.read_text(errors='replace').splitlines()
evidence=[v for v in lines if v.startswith(('WOOD_FOOT_PUSH_TOUCH','SCRAPERX_UITEST '))]
terminal=[v for v in evidence if v.startswith(('SCRAPERX_UITEST PASS ','SCRAPERX_UITEST FAIL '))]
errors=[v for v in lines if 'SCRIPT ERROR:' in v or v.startswith('ERROR:')]
phases=[]
for line in evidence:
    match=re.search(r'phase=(\S+)',line)
    if match and (not phases or phases[-1]!=match[1]):phases.append(match[1])
files=['main.tscn','presentation/main.gd','presentation/ui/ui_test_driver.gd','presentation/ui/input_router.gd','presentation/ui/touch_controls.gd','presentation/ui/hud.gd','presentation/kit_view.gd','project.godot','addons/scraperx_native/scraperx_native.gdextension','addons/scraperx_native/bin/linux/libscraperx_native.so']
receipt={'label':a.label,'result':'PASS' if a.exit_code==0 and terminal and ' PASS ' in terminal[-1] and not errors else 'FAIL','exit_code':a.exit_code,'terminal':terminal,'errors':errors,'phases':phases,'last_native_samples':evidence[-8:],'walk_probe_receipts':[v for v in lines if v.startswith('PIPE_WALK_TOUCH ')],'recovery_receipts':[v for v in evidence if 'event=actual_toe_support_loss ' in v or 'event=delayed_ordinary_air_braking ' in v or 'event=actual440_recovery_impact ' in v],'drop_receipts':[v for v in evidence if 'event=before_drop ' in v or 'event=drop_first_native_tick ' in v],'log_sha256':hashlib.sha256(log.read_bytes()).hexdigest(),'runtime_sha256':{f:hashlib.sha256((project/f).read_bytes()).hexdigest() for f in files},'harness_current_sha256':{f:hashlib.sha256((base/f).read_bytes()).hexdigest() for f in ['wood_driver.gd','prepare_runtime.py','run_touch.sh','summarize_receipt.py']},'scope':'One initial407steel stage followed by ordinary viewport touch through production input and native authority. No phone feel/performance, rendered visual/audio, CI or APK claim.'}
if a.helper_as_used:
    helper=Path(a.helper_as_used).resolve()
    receipt['helper_as_used']={'path':str(helper),'sha256':hashlib.sha256(helper.read_bytes()).hexdigest()}
(base/(a.label+'-receipt.json')).write_text(json.dumps(receipt,indent=2)+'\n')
print(receipt['result'],terminal[-1] if terminal else 'No terminal receipt')
