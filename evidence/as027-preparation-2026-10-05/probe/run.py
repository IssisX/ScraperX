import subprocess,re,json,pathlib
root=pathlib.Path(__file__).parent
cases={'loaded':(90,85,200000,0,0),'refined':(360,85,200000,0,0),'unloaded':(90,0,200000,0,0),'stall':(90,85,100000,0,0),'power_loss':(90,85,200000,1,0),'eccentric':(90,85,200000,0,7),'restart':(90,85,200000,2,7),'return':(90,85,200000,3,7)}
results={};failures=[]
for name,args in cases.items():
 p=subprocess.run([str(root/'lift'),*map(str,args)],capture_output=True,text=True,check=True)
 (root/f'final-{name}.log').write_text(p.stdout)
 lines=[x for x in p.stdout.splitlines() if x.startswith(('RESULT ','LEDGER ','REPLAY '))]
 values={k:float(v) for line in lines for k,v in re.findall(r'(\w+)=([-+\d.eE]+)',line)}
 results[name]=values
 def require(condition,description):
  if not condition:failures.append(name+': '+description)
 require(values['errors']==0,'native update errors')
 require(all((int(a),int(b))==(2973,2979) for a,b in re.findall(r'CONTACT (\d+) (\d+)',p.stdout)),'unexpected mechanism self-contact')
 require(values['peak_joint_m']<.005,'hinge mismatch exceeds5mm screening budget')
 require(values['max_rider_offset']<.1,'rider moves more than10cm relative to initial deck footprint')
 require(values['peak_force_n']<=args[2]+1,'force cap exceeded')
 if name=='stall':require(abs(values['rise'])<.05,'underpowered fixture did not stall')
 elif name=='return':require(abs(values['rise'])<.05,'did not physically return')
 elif name=='power_loss':require(0<values['off_drop_m']<.02,'brake loss exceeds20mm')
 else:require(23<values['rise']<23.35,'stroke outside independent geometric window')
 if 'position_error_m' in values:require(values['position_error_m']<.01 and values['velocity_error_mps']<.02,'cold checkpoint replay exceeds10mm/20mmps screening budget')
 print(name, 'rise', values['rise'], 'joint', values['peak_joint_m'])
if abs(results['loaded']['rise']-results['refined']['rise'])>.005:failures.append('90/360endpoint exceeds5mm')
report={'scope':'isolated paired linkage screening only; selected numerical thresholds, not production/gameplay acceptance','results':results,'failures':failures}
(root/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print('SCREENING', 'FAIL' if failures else 'PASS', failures)
raise SystemExit(bool(failures))
