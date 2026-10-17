#!/usr/bin/env python3
"""Capture Android configuration events around the complete compatibility driver.

This reuses an identified APK. It neither retries a failed launch nor changes
game state. A diagnostic workflow is not a release acceptance substitute.
"""
import argparse
import json
from pathlib import Path
import subprocess
import sys

import run_android_compatibility as compatibility


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--startup-font-scale', choices=('none', '1.25'), default='none')
    args, forwarded = parser.parse_known_args()
    sys.argv = [sys.argv[0], *forwarded, '--output', str(args.output)]
    args.output.mkdir(parents=True, exist_ok=True)
    commands, errors = [], []

    def capture(name, *arguments):
        command = ['adb', *arguments]
        try:
            result = subprocess.run(command, capture_output=True, timeout=30)
            commands.append({'argv': command, 'returncode': result.returncode})
            (args.output / name).write_bytes(result.stdout + result.stderr)
            if result.returncode:
                errors.append(f'{command!r}: exit {result.returncode}')
            return result.stdout.decode(errors='replace').strip() if not result.returncode else None
        except Exception as error:
            errors.append(f'{command!r}: {error}')

    capture('configuration-before.txt', 'shell', 'am', 'get-config')
    capture('events-clear.txt', 'logcat', '-b', 'events', '-c')
    original_font = None
    font_injected = False
    original_session = compatibility.Session
    if args.startup_font_scale != 'none':
        original_font = capture('font-scale-before.txt', 'shell', 'settings', 'get', 'system', 'font_scale')
        if original_font is None or original_font == args.startup_font_scale:
            raise RuntimeError('Startup font diagnostic needs an observed different original font scale')

        class ConfigurationSession(original_session):
            def adb(self, *arguments, **kwargs):
                nonlocal font_injected
                result = super().adb(*arguments, **kwargs)
                if arguments[:4] == ('shell', 'am', 'start', '-W') and not font_injected:
                    font_injected = True
                    super().adb('shell', 'settings', 'put', 'system', 'font_scale', args.startup_font_scale)
                    actual = super().adb('shell', 'settings', 'get', 'system', 'font_scale').decode().strip()
                    self.receipt['startup_configuration_input'] = {
                        'operation': 'real Android system font_scale change after first activity launch',
                        'before': original_font, 'requested': args.startup_font_scale, 'readback': actual}
                    if actual != args.startup_font_scale:
                        raise RuntimeError(f'Android font-scale input expected {args.startup_font_scale}, observed {actual!r}')
                return result

    event_log = (args.output / 'lifecycle-events.txt').open('wb')
    process = None
    failed = False
    try:
        if args.startup_font_scale != 'none':
            compatibility.Session = ConfigurationSession
        process = subprocess.Popen(['adb', 'logcat', '-b', 'events', '-v', 'threadtime'],
                                   stdout=event_log, stderr=subprocess.STDOUT)
        compatibility.main()
    except BaseException:
        failed = True
        raise
    finally:
        compatibility.Session = original_session
        capture('configuration-after.txt', 'shell', 'am', 'get-config')
        capture('activity-after.txt', 'shell', 'dumpsys', 'activity')
        capture('memory-after.txt', 'shell', 'dumpsys', 'meminfo')
        capture('cpu-after.txt', 'shell', 'dumpsys', 'cpuinfo')
        if font_injected:
            if original_font == 'null':
                capture('font-scale-restore.txt', 'shell', 'settings', 'delete', 'system', 'font_scale')
            else:
                capture('font-scale-restore.txt', 'shell', 'settings', 'put', 'system', 'font_scale', original_font)
        try:
            if process is not None:
                if process.poll() is None:
                    process.terminate()
                    try:
                        process.wait(timeout=5)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait(timeout=5)
                elif process.returncode != 0:
                    errors.append(f'Events log reader exited {process.returncode}')
        finally:
            event_log.close()
            (args.output / 'lifecycle-capture.json').write_text(json.dumps(
                {'commands': commands, 'errors': errors,
                 'scope': 'Existing complete driver plus OS lifecycle capture and declared configuration input',
                 'startup_font_scale': args.startup_font_scale, 'font_injected': font_injected}, indent=2) + '\n')
        if errors and not failed:
            raise RuntimeError('Android lifecycle capture failed: ' + '; '.join(errors))


if __name__ == '__main__':
    main()
