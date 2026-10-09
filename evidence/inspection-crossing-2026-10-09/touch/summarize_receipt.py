#!/usr/bin/env python3
"""Summarize a bounded viewport-touch run without asserting missing evidence."""
from pathlib import Path
import argparse,hashlib,json,re
p=argparse.ArgumentParser(); p.add_argument('label'); p.add_argument('--exit-code',type=int); a=p.parse_args()
base=Path(__file__).resolve().parent
log=base/(a.label+'.log'); text=log.read_text(errors='replace')
lines=text.splitlines(); evidence=[v for v in lines if v.startswith(('INSPECTION_TOUCH ','INSPECTION_TOUCH_BUTTONS ','SCRAPERX_UITEST '))]
terminal=[v for v in lines if v.startswith('SCRAPERX_UITEST PASS ') or v.startswith('SCRAPERX_UITEST FAIL ')]
errors=[v for v in lines if 'SCRIPT ERROR:' in v or v.startswith('ERROR:')]
phases=[]
for v in evidence:
    match=re.search(r'phase=(\S+)',v)
    if match and (not phases or phases[-1]!=match[1]): phases.append(match[1])
files=['presentation/main.gd','presentation/ui/ui_test_driver.gd','presentation/ui/input_router.gd','presentation/ui/touch_controls.gd','presentation/ui/hud.gd','presentation/kit_view.gd','project.godot','addons/scraperx_native/bin/linux/libscraperx_native.so']
receipt={'label':a.label,'result':'PASS' if terminal and ' PASS ' in terminal[-1] and not errors and a.exit_code==0 else ('FAIL' if terminal or errors or a.exit_code is not None else 'INCOMPLETE'),'exit_code':a.exit_code,'terminal':terminal,'errors':errors,'phases':phases,'last_native_samples':evidence[-8:],'caught_button_labels':[v for v in evidence if v.startswith('INSPECTION_TOUCH_BUTTONS ')],'coarse_lean_receipts':[v for v in lines if v.startswith('INSPECTION_COARSE_LEAN ')],'non_touch_hud_context_receipts':[v for v in lines if v.startswith('INSPECTION_SWING_HUD ')],'log_sha256':hashlib.sha256(log.read_bytes()).hexdigest(),'runtime_sha256':{v:hashlib.sha256((base/'godot'/v).read_bytes()).hexdigest() for v in files},'baseline_source_receipt':'source-start.json','scope':'One staged normal-world entry396 followed only by real viewport touch; native pose/support/gravity readback. No phone, human usability, visual, audio, CI or APK claim.'}
(base/(a.label+'-receipt.json')).write_text(json.dumps(receipt,indent=2)+'\n')
print(receipt['result'],terminal[-1] if terminal else 'No terminal receipt')
