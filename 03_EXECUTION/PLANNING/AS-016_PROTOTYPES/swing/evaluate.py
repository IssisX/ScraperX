#!/usr/bin/env python3
"""Independent one-coordinate elastic/plastic model and native cross-check.

This is a design evaluator, not a runtime controller. Its dry-friction impulse
is clamped to the static cone; no imposed trajectory or completion flag exists.
"""
import argparse
import collections
import itertools
import json
import math
import pathlib

parser = argparse.ArgumentParser()
parser.add_argument('--directory', type=pathlib.Path, default=pathlib.Path(__file__).parent)
args = parser.parse_args()
spec = json.loads(pathlib.Path(__file__).with_name('cases.json').read_text())
cases = sorted((friction, *rider, force) for friction, rider, force in itertools.product(
    spec['band']['hinge_friction_Nm'], spec['band']['rider_mass_kg_and_distance_m'],
    spec['band']['receiver_yield_N']))
try:
    native = json.loads((args.directory / 'native_results.json').read_text(),
                        parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))
    keys = ['friction', 'rider_mass', 'rider_r', 'crush_yield', 'hz']
    observed = collections.Counter(tuple(row[key] for key in keys) for row in native)
    expected = collections.Counter((*case, hz) for case in cases for hz in spec['native_hz'])
    if observed != expected:
        raise ValueError(f'case coverage mismatch: missing={list((expected-observed).elements())}, '
                         f'extra/duplicate={list((observed-expected).elements())}')
    def finite_values(value):
        if isinstance(value, dict):
            return all(finite_values(item) for item in value.values())
        if isinstance(value, (int, float)):
            return math.isfinite(value)
        return value is not None
    if not all(finite_values(row) for row in native):
        raise ValueError('nonfinite or absent native metric')
except (ValueError, KeyError, TypeError) as error:
    raise SystemExit(f'Invalid native evidence: {error}')
target = math.asin(7.4 / 20)
nominal_release = 9.81 * (2500 * math.sin(target) + 47300 * (1 - math.cos(target)))
bed_top = .6 - 5 * math.sin(target) + 2.8 * math.cos(target) - 1 + (nominal_release - 3000 * target) / 90000


def simulate(friction, rider_mass, rider_radius, yield_force, step):
    # Exact rectangular-deck and two 0.3 m square arm inertias. The pan stays
    # level: its own rotary inertia contributes zero to the ideal coordinate.
    inertia = 1703020.8333333333 + rider_mass * (rider_radius ** 2 + .0016666666666667)
    a = 2500 - rider_mass * rider_radius
    b = 47300
    stiffness = 15e6
    angle = velocity = plastic_front = 0.
    peak_speed = peak_accel = 0.
    first_turn = first_contact = None
    plastic_dissipation = friction_dissipation = 0.
    low_tip, high_tip, speed_after = math.inf, -math.inf, 0.
    for index in range(round(35 / step)):
        time = index * step
        sine, cosine = math.sin(angle), math.cos(angle)
        penetration = max(0., bed_top - (.6 - 5 * sine + 2.8 * cosine - 1))
        new_front = max(plastic_front, penetration - yield_force / stiffness)
        plastic_dissipation += yield_force * (new_front - plastic_front)
        plastic_front = new_front
        normal = stiffness * max(0., penetration - plastic_front)
        if penetration > 0 and first_contact is None:
            first_contact = time
        torque = 9.81 * (a * cosine + b * sine) - normal * (5 * cosine + 2.8 * sine)
        free_velocity = velocity + torque / inertia * step
        friction_torque = max(-friction, min(friction, inertia * free_velocity / step))
        next_velocity = free_velocity - friction_torque / inertia * step
        next_angle = angle + next_velocity * step
        friction_dissipation += friction_torque * (next_angle - angle)
        peak_speed = max(peak_speed, 20 * abs(next_velocity))
        acceleration = math.hypot((next_velocity - velocity) / step, next_velocity ** 2)
        peak_accel = max(peak_accel, 20 * acceleration / 9.81)
        if first_contact is not None and velocity > 0 >= next_velocity and first_turn is None:
            first_turn = time + step
        angle, velocity = next_angle, next_velocity
        if first_turn is not None and time > first_turn + 2:
            tip = .6 + 20 * math.sin(angle)
            low_tip, high_tip = min(low_tip, tip), max(high_tip, tip)
            speed_after = max(speed_after, 20 * abs(velocity))
    penetration = max(0., bed_top - (.6 - 5 * math.sin(angle) + 2.8 * math.cos(angle) - 1))
    new_front = max(plastic_front, penetration - yield_force / stiffness)
    plastic_dissipation += yield_force * (new_front - plastic_front)
    plastic_front = new_front
    elastic = max(0., penetration - plastic_front)
    released = 9.81 * (a * math.sin(angle) + b * (1 - math.cos(angle)))
    residual = released - .5 * inertia * velocity ** 2 - plastic_dissipation - friction_dissipation - .5 * stiffness * elastic ** 2
    return dict(h=step, q_end=angle, tip_y=.6 + 20 * math.sin(angle),
                peak_tip_speed=peak_speed, peak_total_accel_g=peak_accel,
                first_contact_s=first_contact, first_turn_s=first_turn,
                plastic_front=plastic_front, post_2s_tip_min=low_tip,
                post_2s_tip_max=high_tip, post_2s_speed_max=speed_after,
                energy_released_J=released, energy_residual_J=residual)


# Capture connection has at least 60 mm remaining vertical-step margin in the
# proposed receiver layout; cross-model pose error gets <= 1/3 (20 mm).
# These checks do not substitute for the native character walking proof.
convergence_limits = {
    'peak_tip_speed': .0005, 'first_turn_s': .003, 'plastic_front': .0002,
    'post_2s_tip_min': .0005, 'post_2s_tip_max': .0005,
    'post_2s_speed_max': .0005, 'peak_total_accel_g': .001,
}
cross_limits = {
    'first_turn_diff_s': .05, 'peak_speed_diff': .015,
    'front_diff': .005, 'envelope_low_diff': .020,
    'envelope_high_diff': .020, 'peak_total_accel_diff_g': .010,
}
reports, failures = [], []
for friction, mass, radius, yield_force in cases:
    case = (friction, mass, radius, yield_force)
    coarse = simulate(*case, .001)
    fine = simulate(*case, .00025)
    differences = {key: abs(coarse[key] - fine[key]) for key in convergence_limits}
    for key, difference in differences.items():
        if difference > convergence_limits[key]:
            failures.append(f'{case}: convergence {key}: {difference} > {convergence_limits[key]}')
    comparisons = []
    for row in native:
        if (row['friction'], row['rider_mass'], row['rider_r'], row['crush_yield']) != case:
            continue
        comparison = dict(hz=row['hz'],
                          first_turn_diff_s=row['first_turn_s'] - fine['first_turn_s'],
                          peak_speed_diff=row['peak_tip_speed'] - fine['peak_tip_speed'],
                          front_diff=row['plastic_front_m'] - fine['plastic_front'],
                          envelope_low_diff=row['after_turn_plus_2s']['tip_min'] - fine['post_2s_tip_min'],
                          envelope_high_diff=row['after_turn_plus_2s']['tip_max'] - fine['post_2s_tip_max'],
                          peak_total_accel_diff_g=row['peak_total_accel_g'] - fine['peak_total_accel_g'])
        comparisons.append(comparison)
        for key, limit in cross_limits.items():
            if abs(comparison[key]) > limit:
                failures.append(f'{case} at {row["hz"]} Hz: {key} exceeds {limit}')
        if row['peak_total_accel_g'] > .5:
            failures.append(f'{case}: native rigid-load acceleration exceeds 0.5 g')
        if row['ultimate_floor_contacts'] or row['penetration_max_m'] >= .65:
            failures.append(f'{case}: consumed receiver travel reached ultimate floor')
        if row['late_ledger_residual_max_J'] / row['energy_released_J'] > .02:
            failures.append(f'{case}: native energy residual exceeds 2%')
    if abs(fine['energy_residual_J'] / fine['energy_released_J']) > .002:
        failures.append(f'{case}: evaluator energy residual exceeds 0.2%')
    reports.append(dict(case=case, coarse=coarse, fine=fine,
                        convergence=differences, native_comparison=comparisons))
summary = {
    'cases': len(reports), 'native_runs': len(native), 'overall_pass': not failures,
    'evidence_class': 'INTEGRATED' if not failures else 'UNRESOLVED',
    'scope': 'isolated preloaded swing and receiver; no pipe loading, release, player walking, or mass detach',
    'bed_top': bed_top, 'h': .001, 'refinement_h': .00025,
    'convergence_limits': convergence_limits, 'cross_model_limits': cross_limits,
    'convergence_max': {key: max(row['convergence'][key] for row in reports) for key in convergence_limits},
    'cross_model_max': {key: max(abs(comparison[key]) for row in reports for comparison in row['native_comparison']) for key in cross_limits},
    'ledger_fraction_max': max(abs(row['fine']['energy_residual_J'] / row['fine']['energy_released_J']) for row in reports),
    'failures': failures,
}
(args.directory / 'evaluation_results.json').write_text(json.dumps(dict(summary=summary, cases=reports), indent=2) + '\n')
(args.directory / 'evaluation_summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary, indent=2))
raise SystemExit(0 if not failures else 1)
