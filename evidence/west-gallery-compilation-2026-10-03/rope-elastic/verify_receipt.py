#!/usr/bin/env python3
"""Validate recorded native feasibility evidence, without rerunning simulations."""
from pathlib import Path
import hashlib
import json
import math

p = Path(__file__).resolve().parent
r = json.loads((p / 'receipt.json').read_text())
for name, expected in r['hashes'].items():
    assert hashlib.sha256((p / name).read_bytes()).hexdigest() == expected, name
for phase in ['quiet', 'rider']:
    for rate in ['90', '360']:
        x = r['runs'][phase][rate]
        assert x['process_exit_code'] == 0 and x['update_errors'] == 0
        assert x['call'] == 'Update(float(h),4)' and x['added_force_calls'] == 0
        assert x['max_lambda'] <= 1e-5, 'strand applied compression'
        assert x['substep_callbacks'] == x['ticks'] * 4
        assert x['material_damping_n_s_per_m'] == 0
        if phase == 'quiet':
            assert x['contacts_added'] == x['contacts_persisted'] == 0
            assert x['max_pose_change_m'] < .0001 and x['max_span_change_m'] < .0001
            assert x['max_speed'] < .001 and x['max_abs_energy_delta_j'] < .001
            assert x['max_static_last_substep_force_error_n'] < .01
        else:
            assert abs(x['player_mass'] - 85) < .001 and x['player_gravity_factor'] == 1
            assert x['release_velocity_jump_m_s'] == 0
            assert x['max_hand_force_last_substep_n'] <= 1501
            assert x['player_floor_contact_events'] > 0
            assert x['player_other_contact_events'] == 0
            assert x['contacts_added'] + x['contacts_persisted'] == x['player_floor_contact_events']
            expected_load = (60*x['actual_rung_mass'] + x['player_mass'])*x['actual_gravity']
            assert abs(x['loaded_root_vertical_n']-expected_load)/expected_load < .02
            assert x['loaded_left_strand_extension_sum_m'] > .15, 'receipt must retain observed target exceedance'
            assert math.isfinite(x['final_unclosed_mechanical_delta_j'])
print('PASS: frozen native evidence, prestressed quiet equilibrium, real bounded 85 kg catch, neutral release.')
print('UNRESOLVED: settled .15 m extension; dynamic refinement; full energy ports; breeze/slack; production grips/rendering/route/recovery/device.')
