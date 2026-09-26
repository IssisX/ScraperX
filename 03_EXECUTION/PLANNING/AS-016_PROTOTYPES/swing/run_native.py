#!/usr/bin/env python3
"""Run the isolated four-bar probe; no production/game state is touched."""
import argparse
import csv
import itertools
import json
import pathlib
import shlex
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--exe', type=pathlib.Path, default=pathlib.Path(__file__).with_name('native_probe'))
parser.add_argument('--command-prefix', default='')
parser.add_argument('--output', type=pathlib.Path, default=pathlib.Path(__file__).parent)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)
results = []
spec = json.loads(pathlib.Path(__file__).with_name('cases.json').read_text())
for friction, rider, yield_force, hz in itertools.product(
        spec['band']['hinge_friction_Nm'], spec['band']['rider_mass_kg_and_distance_m'],
        spec['band']['receiver_yield_N'], spec['native_hz']):
    mass, radius = rider
    tag = f'{hz}-{friction}-{mass}-{radius}-{yield_force}'
    csv_path = args.output / f'native-{tag}.csv'
    command = shlex.split(args.command_prefix) + [
        str(args.exe.resolve()), str(hz), str(friction), str(mass), str(radius),
        str(yield_force), str(csv_path.resolve()), '15000000', '10', '2', '90000']
    completed = subprocess.run(command, capture_output=True, text=True, check=True)
    result = json.loads(completed.stdout.strip().splitlines()[-1])
    result.update(solver_velocity_steps=10, solver_position_steps=2,
                  csv=csv_path.name, role='isolated preloaded dynamics fixture')
    with csv_path.open() as stream:
        rows = [{key: float(value) for key, value in row.items()}
                for row in csv.DictReader(stream)]
    for delay in [0, 1, 2, 5]:
        selected = [row for row in rows if row['t'] >= result['first_turn_s'] + delay]
        result[f'after_turn_plus_{delay}s'] = {
            'tip_min': min(row['tip_y'] for row in selected),
            'tip_max': max(row['tip_y'] for row in selected),
            'tip_speed_max': max(20 * abs(row['omega']) for row in selected),
            'support_force_min': min(row['force'] for row in selected),
            'support_force_max': max(row['force'] for row in selected),
            'crush_face_gap_max': max(row['plastic_front'] - row['penetration'] for row in selected),
        }
    results.append(result)
(args.output / 'native_results.json').write_text(json.dumps(results, indent=2) + '\n')
keys = ['tip_y', 'first_turn_s', 'peak_tip_speed', 'peak_tangent_accel_g',
        'peak_total_accel_g', 'hinge_drift_max_m', 'pan_tilt_max_rad',
        'penetration_max_m', 'late_ledger_residual_max_J', 'ultimate_floor_contacts']
summary = {
    'runs': len(results),
    'ranges': {key: [min(row[key] for row in results), max(row[key] for row in results)] for key in keys},
    'after_plus_2_max_span': max(row['after_turn_plus_2s']['tip_max'] - row['after_turn_plus_2s']['tip_min'] for row in results),
    'after_plus_2_max_speed': max(row['after_turn_plus_2s']['tip_speed_max'] for row in results),
    'max_ledger_fraction': max(row['late_ledger_residual_max_J'] / row['energy_released_J'] for row in results),
}
(args.output / 'native_summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary, indent=2))
