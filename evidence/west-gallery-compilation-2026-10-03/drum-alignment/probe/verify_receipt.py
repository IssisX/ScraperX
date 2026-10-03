from pathlib import Path
import hashlib,json,math
p=Path(__file__).resolve().parent
r=json.loads((p/'receipt.json').read_text())
for name,h in r['hashes'].items(): assert hashlib.sha256((p/name).read_bytes()).hexdigest()==h,name
for x in r['runs'].values():
 assert x['process_exit_code']==0 and x['update_errors']==0 and x['finite_states']
 assert x['unsupported_force_samples']==0
 assert x['max_control_force_n']<=250.001 and x['positive_control_work_j']<=80.01
 assert x['max_grip_force_n']<=math.sqrt(240**2+40**2+40**2)+.1
 assert x['player_gravity_factor']==1 and x['player_friction']==0
assert abs(r['runs']['noinput']['final_tongue_x_m']-r['runs']['withdraw']['final_tongue_x_m'])<1e-6
print('PASS: frozen two-case evidence; finite native state and declared grounded proxy bounds.')
print('REJECTED ASSEMBLY: 42 mm initial guide/upright overlap; no .15 m release travel or drum exit during bounded withdrawal.')
raise SystemExit(1)
