#!/usr/bin/env python3
"""Compile and reproduce the isolated native release observations.

Requires the same pinned Jolt ABI/configuration as this project's native build.
The optional runner allows a Termux host to invoke a Linux compiler via proot.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jolt-source', type=Path, required=True)
    parser.add_argument('--jolt-library', type=Path, required=True)
    parser.add_argument('--cxx', default='c++')
    parser.add_argument('--runner', default='')
    parser.add_argument('--output', type=Path, default=Path(__file__).parent / 'observations.json')
    parser.add_argument('--raw-dir', type=Path, required=True)
    parser.add_argument('--skip-build', action='store_true')
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    binaries = root / 'build'
    binaries.mkdir(exist_ok=True)
    args.raw_dir.mkdir(parents=True, exist_ok=True)
    runner = shlex.split(args.runner)
    flags = ['-fmax-errors=3', '-std=c++17', '-O2', '-fno-rtti', '-fno-exceptions', '-ffp-contract=on',
             '-pthread', '-DJPH_DEBUG_RENDERER', '-DJPH_OBJECT_STREAM',
             '-DJPH_PROFILE_ENABLED', '-DNDEBUG']
    if not args.skip_build:
        for stem in ['rack_release', 'bridge_prop_release']:
            subprocess.run(runner + [args.cxx] + flags + ['-I' + str(args.jolt_source),
                           str(root / (stem + '.cpp')), str(args.jolt_library),
                           '-o', str(binaries / stem)], check=True)
    cases = []
    for hz in [90, 360]:
        for force in [150, 0]:
            cases.append(('rack_release', [hz, force]))
        for offset in [.035, .04, .045]:
            cases.append(('bridge_prop_release', [hz, offset, 150]))
        cases.append(('bridge_prop_release', [hz, .04, 0]))
    results = []
    for stem, values in cases:
        tag = stem + '-' + '-'.join(map(str, values))
        command = runner + [str(binaries / stem)] + list(map(str, values))
        if stem == 'rack_release':
            command.append(str(args.raw_dir / (tag + '.csv')))
        run = subprocess.run(command, text=True, capture_output=True, check=True)
        (args.raw_dir / (tag + '.log')).write_text(run.stdout + run.stderr)
        metrics = {}
        for line in run.stdout.splitlines():
            if line.startswith('{'):
                metrics.update(json.loads(line))
        if not metrics:
            raise RuntimeError('No observation record from ' + tag)
        failures = []
        active = values[-1] != 0
        if stem == 'rack_release':
            if active and (metrics['pipe_crossed'] != 20 or metrics['pipe_in_receiver'] != 20):
                failures.append('All twenty pipes did not reach the fixed receiver')
            if not active and (metrics['pipe_crossed'] or metrics['crossed_deadcentre_s'] >= 0):
                failures.append('Uncommanded release')
            if metrics['handle_stroke_max_m'] > .3 or metrics['hand_work_J'] > 45:
                failures.append('Manual travel/work budget exceeded')
        else:
            if active and not (3 <= metrics['deadcentre_s'] <= 6):
                failures.append('Loaded prop did not cross dead centre')
            if not active and metrics['deadcentre_s'] >= 0:
                failures.append('Uncommanded release')
            if metrics['max_stroke_m'] > .3 or metrics['hand_work_J'] > 45:
                failures.append('Manual travel/work budget exceeded')
        result = {'case': tag, 'metrics': metrics, 'outcome_and_budget_pass': not failures,
                  'failures': failures}
        results.append(result)
        print(json.dumps(result, sort_keys=True))
    report = {
        'evidence_class': 'OBSERVED_NATIVE_FIXTURE',
        'jolt_commit': 'e77f175595e64cb44218cc9d9d56fc365ad0e36a',
        'solver': {'velocity_iterations': 20, 'position_iterations': 4,
                   'gravity_mps2': 9.81, 'damping': 0, 'sleeping': False},
        'source_sha256': {name: hashlib.sha256((root / name).read_bytes()).hexdigest()
                          for name in ['rack_release.cpp', 'bridge_prop_release.cpp', 'run_probes.py']},
        'scope': 'Loaded release contacts, input budgets and fixed-receiver collection.',
        'not_proven': [
            'Full moving bridge integration and reflected inertia in bridge-prop fixture',
            'Player carry/input route, geometry sweep of rendered cable paths',
            'Sheave rotational inertia and finite gate-stop impact ledger',
            'Full causal-compiler convergence certification and mechanism energy ledger',
            'Production Jolt settings, APK or Android-device behavior'],
        'all_outcome_and_budget_checks_pass': all(x['outcome_and_budget_pass'] for x in results),
        'cases': results}
    args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + '\n')
    return 0 if report['all_outcome_and_budget_checks_pass'] else 1


if __name__ == '__main__':
    sys.exit(main())
