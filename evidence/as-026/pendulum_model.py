"""Design pendulum only: uniform 9.2m rung frame, dry bearing, no rider.
No production pose driver. Full player/contact/traversal remains Jolt's owner.
"""
import json, math
G=9.81; L=9.2
reports=[]
for mass in [400.,520.,640.]:
 for amplitude in [.03,.06,.10]:
  for friction in [10.,20.,30.]:
   runs=[]
   for h in [.001,.00025]:
    q=amplitude; v=0.; inertia=mass*L*L/3
    e0=mass*G*L/2*(1-math.cos(q));loss=0.;peak=0.;peak_residual=0.
    for i in range(round(10/h)):
     torque=-mass*G*L/2*math.sin(q)
     if abs(v)<1e-8 and abs(torque)<=friction: v=0.;continue
     sign=math.copysign(1., v if abs(v)>1e-8 else torque)
     next_v=v+(torque-sign*friction)*h/inertia
     if next_v*sign<0.: next_v=0.
     dq=next_v*h;q+=dq;v=next_v;loss+=friction*abs(dq)
     peak=max(peak,abs(v)*L)
     energy=.5*inertia*v*v+mass*G*L/2*(1-math.cos(q))
     peak_residual=max(peak_residual,abs(energy+loss-e0))
    runs.append(dict(step=h,tip_speed=peak,residual_j=peak_residual))
   assert abs(runs[0]['tip_speed']-runs[1]['tip_speed'])<.002
   assert max(r['residual_j'] for r in runs)<.02*e0
   reports.append(dict(mass=mass,amplitude=amplitude,bearing_torque=friction,initial_j=e0,runs=runs))
print(json.dumps(dict(evidence='INTEGRATED design-only uniform-rod approximation',cases=reports),indent=2))
