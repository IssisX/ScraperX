import subprocess,pathlib,re,json
root=pathlib.Path(__file__).parent
results={}; failures=[]
cases={'loaded':(90,85,200000,0,0,1200000),'refined':(360,85,200000,0,0,1200000),'reserve_cutoff':(90,85,200000,0,0,1000),'midstroke_cutoff':(90,85,200000,0,0,250000),'empty':(90,85,200000,0,0,0),'eccentric':(90,85,200000,0,7,1200000),'power_loss':(90,85,200000,1,0,1200000)}
for name,args in cases.items():
 p=subprocess.run([str(root/'lift'),*map(str,args)],capture_output=True,text=True,check=True)
 (root/(name+'.log')).write_text(p.stdout)
 lines=[x for x in p.stdout.splitlines() if x.startswith(('RESULT','LEDGER','ENERGY','REPLAY'))]
 v={k:float(v) for line in lines for k,v in re.findall(r'(\w+)=([-+\d.eE]+)',line)}
 results[name]=v
 def require(ok,why):
  if not ok: failures.append(name+': '+why)
 require(v['errors']==0 and v['peak_joint_m']<.005,'solver errors or >5mm hinge mismatch')
 require(v['remaining_j']>=0 and v['overdraft_j']==0,'electrical overdraft')
 require(v['peak_electrical_w']<=45000,'electrical power exceeds45kW')
 require(abs(v['remaining_j']+v['work_j']/.8-v['capacity_j'])<.01,'conversion ledger mismatch')
 if name in ('loaded','refined','eccentric'):require(23<v['rise']<23.35,'full travel not reached')
 if 'cutoff' in name:require(v['cutoff']==1 and v['cutoff_drop_m']<.02,'energy cutoff failed to hold within20mm')
 if name=='midstroke_cutoff':require(1<v['rise']<15,'midstroke cutoff outside partial stroke')
 if name=='empty':require(v['work_j']==0 and abs(v['rise'])<.03,'empty source drove mechanism')
 if name=='power_loss':require(0<v['off_drop_m']<.02,'external power-loss brake failed')
 print(name, {k:v[k] for k in ('rise','remaining_j','peak_electrical_w','cutoff','cutoff_drop_m')},flush=True)
require(abs(results['loaded']['rise']-results['refined']['rise'])<.005,'endpoint refinement >5mm')
(root/'results.json').write_text(json.dumps({'scope':'isolated energy controller screening; work quadrature and empirical displacement margin require native substep integration validation','results':results,'failures':failures},indent=2)+'\n')
print('SCREENING', 'FAIL' if failures else 'PASS', failures)
raise SystemExit(bool(failures))
