import subprocess,pathlib,json,hashlib
root=pathlib.Path(__file__).resolve().parent
cases=[('nominal',360,.6,-100,.1),('far-rider',360,.6,5.7,.1),('near-rider',360,.6,1,.1),('opposite-load',360,.6,-3.4,.1),('source-absent',0,.6,-100,.1),('high-drop-low-friction',390,.3,-100,.25)]
rows=[]
for name,mass,mu,rider,drop in cases:
 for suffix,h in [('90hz',1/90),('360hz',1/360)]:
  dest=root/f'native-{name}-{suffix}.log'
  args=['proot-distro','login','ubuntu','--',str(root/'coupled-pan-probe'),str(h),str(mass),str(mu),str(rider),str(drop)]
  with dest.open('w') as out:r=subprocess.run(args,stdout=out,stderr=subprocess.PIPE,text=True,timeout=60)
  lines=dest.read_text().splitlines();result=next((line for line in lines if line.startswith('RESULT ')),None)
  row={'case':name,'h':h,'exit_code':r.returncode,'log':str(dest),'result':result}
  rows.append(row)
  print(json.dumps(row),flush=True)
receipt={'source_sha256':hashlib.sha256((root/'coupled-pan-probe.cpp').read_bytes()).hexdigest(),'binary_sha256':hashlib.sha256((root/'coupled-pan-probe').read_bytes()).hexdigest(),'cases':rows,'status':'PRIVATE_PARTIAL_CONTACT_EXPERIMENT_NOT_COMPLETE_COMPILE'}
(root/'native-cases-receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
