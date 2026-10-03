from pathlib import Path
import hashlib,json,math
p=Path(__file__).resolve().parent
r=json.loads((p/'receipt.json').read_text())
for name,expected in r['hashes'].items():
 assert hashlib.sha256((p/name).read_bytes()).hexdigest()==expected,name
for phase,x in r['runs'].items():
 assert x['process_exit_code']==0 and x['update_errors']==0
 assert x['velocity_iterations']==10 and x['position_iterations']==2
 assert abs(x['penetration_slop_m']-.02)<1e-7
 assert x['local_velocity_override']==40 and x['local_position_override']==8
 assert x['call']=='Update(float(h),4)' and x['substep_callbacks']==4*x['ticks']==5760
 assert x['added_force_calls']==0 and x['max_lambda']<=1e-5
 assert all(math.isfinite(v) for v in x.values() if isinstance(v,(int,float)))
 if phase=='quiet':
  assert x['contacts_added']==x['contacts_persisted']==0
  assert x['max_pose_change_m']<.0001 and x['max_span_change_m']<.0001
  assert x['max_speed']<.001 and x['max_abs_energy_delta_j']<.001
  assert x['max_static_last_substep_force_error_n']<.01
 else:
  assert abs(x['player_mass']-85)<.001 and x['player_gravity_factor']==1
  assert x['max_hand_force_last_substep_n']<=1501
  assert x['release_velocity_jump_m_s']==0
  assert x['contacts_added']+x['contacts_persisted']==x['player_floor_contact_events']>0
  assert x['player_other_contact_events']==0
  expected_load=(60*x['actual_rung_mass']+x['player_mass'])*x['actual_gravity']
  assert abs(x['loaded_root_vertical_n']-expected_load)/expected_load<.02
print('PASS: recorded production-global/local-island binding; quiet equilibrium; bounded gravity-on catch and neutral release.')
print('UNVERIFIED: settled static extension, dynamic frequency resolution/ports, cost and production gameplay integration.')
