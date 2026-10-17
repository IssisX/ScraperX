#!/usr/bin/env python3
"""Capture Android configuration events around the complete compatibility driver.

This reuses an identified APK. It neither retries a failed launch nor changes
game state. A diagnostic workflow is not a release acceptance substitute.
"""
import argparse
import json
from pathlib import Path
import subprocess

from run_android_compatibility import main as compatibility_main


def main():
    parser = argparse.ArgumentParser(add_help=False)
    parser.add_argument('--output', required=True, type=Path)
    args, _ = parser.parse_known_args()
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
        except Exception as error:
            errors.append(f'{command!r}: {error}')

    capture('configuration-before.txt', 'shell', 'am', 'get-config')
    capture('events-clear.txt', 'logcat', '-b', 'events', '-c')
    event_log = (args.output / 'lifecycle-events.txt').open('wb')
    process = None
    failed = False
    try:
        process = subprocess.Popen(['adb', 'logcat', '-b', 'events', '-v', 'threadtime'],
                                   stdout=event_log, stderr=subprocess.STDOUT)
        compatibility_main()
    except BaseException:
        failed = True
        raise
    finally:
        capture('configuration-after.txt', 'shell', 'am', 'get-config')
        capture('activity-after.txt', 'shell', 'dumpsys', 'activity')
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
                 'scope': 'Existing complete driver plus passive OS lifecycle capture'}, indent=2) + '\n')
        if errors and not failed:
            raise RuntimeError('Android lifecycle capture failed: ' + '; '.join(errors))


if __name__ == '__main__':
    main()
