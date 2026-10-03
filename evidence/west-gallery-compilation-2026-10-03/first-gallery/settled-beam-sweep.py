import importlib.util,json,math,itertools,pathlib
root=pathlib.Path(__file__).resolve().parent
sp=importlib.util.spec_from_file_location('stage','/data/data/com.termux/files/home/.codex/skills/causal-mechanism-compiler/scripts/stage1dof.py');st=importlib.util.module_from_spec(sp);sp.loader.exec_module(st)
g=9.81
# q increases clockwise: local point (x,y) has y_world=-x sin(q)+y cos(q).
def point(m,x,y,icm=0):return {'mass':m,'r':math.hypot(x,y),'phi':math.atan2(y,-x),'i_cm':icm}
def spec(dm,dx,rm,rx):
 return {'name':'First-gallery settled-drum bascule submodel; excludes free-drum/contact transfer','g':g,'model_kind':'beam','model':{'masses':[point(800,3,0,800*(6**2+.3**2)/12),point(800,-3.8,0,800*(1.2**2+.8**2)/12),point(160,-1.9,0,160*(3.8**2+.3**2)/12),point(dm,dx,.9,.5*dm*.6**2),point(rm,rx,.9)],'theta0':-.18,'torque_friction_kinetic':200,'torque_friction_static':350},'terminal':{'min_q':-.18,'stop_q':.06,'buffer':{'kind':'crush','start':-.06,'stroke':.12,'force':22000}},'band':{'model.torque_friction_kinetic':[100,200,300]},'t_max':20,'require':{'allowed_outcomes':['HELD_BY_CRUSH_BED'],'min_breakaway_ratio':1.5,'max_peak_speed':.45,'max_stop_impact_speed':0,'max_time_s':20,'max_ledger_residual_fraction':.02,'convergence':{'peak_speed':{'abs':.001},'time_s':{'abs':.01}}}}
(root/'settled-beam-nominal.json').write_text(json.dumps(spec(360,3.8,0,0),indent=2)+'\n')
rows=[]; failures=[]
for dm,dx,rider in itertools.product([330,360,390],[3.6,3.8,4.0],[(0,0),(85,0),(85,3),(85,5.7)]):
 s=spec(dm,dx,*rider);report=st.run(s,.001,True)
 for ci,case in enumerate(report['cases']):
  model,term,grav=st.build({**s,'model':{**s['model'],'torque_friction_kinetic':[200,100,200,300][ci]}})
  fine=st.simulate(model,term,.00025,s['t_max'],grav)
  end_q_diff=abs(case['end_q']-fine['end_q'])
  no_rider=spec(dm,dx,0,0);no_rider['model']['torque_friction_kinetic']=model.fk
  nm,nt,_=st.build(no_rider)
  q=case['end_q'];drive=nm.qcons(q);front=case['crush_front'];holding=nm.fs+nt.buffer_force(q,1,front)[1]
  exit_hold=drive>=0 and drive<=holding
  # Floor normal: N=m*(g cosq-x qddot-y omega²). Bound at stand-off y=.9.
  normal_lower=(grav*math.cos(max(abs(s['model']['theta0']),abs(q)))-abs(rider[1])*case['peak_accel']-.9*case['peak_speed']**2)*rider[0]
  accel_bound=6*(case['peak_accel']+case['peak_speed']**2)
  row={'drum_mass':dm,'drum_x':dx,'rider_mass':rider[0],'rider_x':rider[1],'case':case,'model_evidence_class':report['evidence_class'],'end_q_h_over_4_abs_diff':end_q_diff,'rider_exit_holds_same_consumed_front':exit_hold,'rider_normal_conservative_lower_N':normal_lower,'tip_accel_triangle_bound_g':accel_bound/grav}
  rows.append(row)
  if not case['pass'] or not exit_hold or normal_lower<0 or end_q_diff>.001 or accel_bound/grav>.5:failures.append(row)
output={'status':'PARTIAL_SUBMODEL_ONLY','h':.001,'h_fine':.00025,'external_cases':36,'cases_including_nominal_and_band':len(rows),'failures':len(failures),'rows':rows,'notes':['No real free drum transfer, impulse or rolling slip proof. Drum and rider treated as fixed point loads; native engine must retain actual bodies and contact.','tip acceleration is conservative triangle bound; exact contact/rider acceleration still requires coupled native model.','crush front consumed and reused on rider exit; no reset or recharging force.']}
(root/'settled-beam-report.json').write_text(json.dumps(output,indent=2)+'\n')
print(json.dumps({'rows':len(rows),'failures':len(failures),'first_failure':failures[:1],'min_breakaway':min(x['case']['breakaway_ratio'] for x in rows),'max_peak_speed':max(x['case']['peak_speed'] for x in rows),'q_min':min(x['case']['end_q'] for x in rows),'q_max':max(x['case']['end_q'] for x in rows),'max_tip_bound_g':max(x['tip_accel_triangle_bound_g'] for x in rows),'max_end_q_refinement':max(x['end_q_h_over_4_abs_diff'] for x in rows),'max_residual':max(abs(x['case']['ledger_residual_fraction']) for x in rows)}))
