#!/usr/bin/env python3
"""Drive the ordinary Android game through adb; retain source-bound observations."""
import argparse
import hashlib
import json
import math
from pathlib import Path
import subprocess
import time

PREFIX = 'SCRAPERX_ANDROID_COMPAT '
PACKAGE = 'com.cory.scraperx.chatgpt'


class Session:
    def __init__(self, output: Path):
        self.output = output
        output.mkdir(parents=True, exist_ok=True)
        self.receipt = {'checks': [], 'observations': [], 'commands': [], 'status': 'running'}
        self.last = None

    def adb(self, *args, timeout=30):
        command = ['adb', *map(str, args)]
        result = subprocess.run(command, capture_output=True, timeout=timeout)
        self.receipt['commands'].append({'argv': command, 'returncode': result.returncode})
        if result.returncode:
            raise RuntimeError(f'Android compatibility command failed: {command!r}: '
                               f'{result.stderr.decode(errors="replace")}')
        return result.stdout

    def snapshot(self):
        text = self.adb('logcat', '-d', '-v', 'brief').decode(errors='replace')
        (self.output / 'logcat.txt').write_text(text)
        for line in reversed(text.splitlines()):
            if PREFIX in line:
                try:
                    self.last = json.loads(line.split(PREFIX, 1)[1])
                    return self.last
                except json.JSONDecodeError:
                    continue
        return None

    def wait(self, label, predicate, *, after=None, timeout=120):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            state = self.snapshot()
            fresh = state and (after is None or
                               (state['instance'], state['sequence']) !=
                               (after['instance'], after['sequence']))
            if fresh and predicate(state):
                self.receipt['checks'].append(label)
                self.receipt['observations'].append({'check': label, 'state': state})
                print(f'ANDROID_COMPAT PASS {label}', flush=True)
                return state
            time.sleep(0.5)  # Condition polling; elapsed time never counts as success.
        raise RuntimeError(f'Android compatibility timeout: {label}; last actual state={self.last!r}')

    def capture(self, name):
        (self.output / f'{name}.png').write_bytes(self.adb('exec-out', 'screencap', '-p'))

    def tap(self, point):
        self.adb('shell', 'input', 'tap', round(point[0]), round(point[1]))

    def button(self, state, name):
        if name not in state['controls']:
            raise RuntimeError(f'Android compatibility: required touch button {name!r} absent')
        self.tap(state['controls'][name]['point'])

    def resume(self, state):
        buttons = [b for b in state['menu_buttons'] if b['text'].startswith('RESUME')]
        if len(buttons) != 1:
            raise RuntimeError(f'Android compatibility: expected one visible RESUME button: {buttons!r}')
        self.tap(buttons[0]['point'])
        return self.wait('touch resume restores native stepping',
                         lambda s: not s['paused'] and s['tick'] > state['tick'], after=state)

    def save(self):
        (self.output / 'receipt.json').write_text(json.dumps(self.receipt, indent=2) + '\n')


def exercise(args, session):
    session.receipt.update({'source_sha': args.source, 'profile': args.profile,
                            'apk_sha256': hashlib.sha256(args.apk.read_bytes()).hexdigest(),
                            'scope': 'Android compatibility; no physical-phone performance claim'})
    for prop in ('ro.build.version.sdk', 'ro.product.cpu.abi', 'ro.product.model'):
        session.receipt[prop] = session.adb('shell', 'getprop', prop).decode().strip()
    session.adb('install', '-r', '-t', str(args.apk), timeout=120)
    activity = session.adb('shell', 'cmd', 'package', 'resolve-activity', '--brief', PACKAGE).decode().strip().splitlines()[-1]
    if '/' not in activity or PACKAGE not in activity:
        raise RuntimeError(f'Android compatibility: no launcher activity for {PACKAGE}: {activity!r}')
    session.adb('logcat', '-c')
    session.adb('shell', 'am', 'start', '-W', '-n', activity)
    state = session.wait('normal Main becomes playable with real native module',
                         lambda s: s['ready'] and s['native_loaded'] and not s['paused'], timeout=240)
    if not math.isclose(state['fixed_step_s'], 1 / 90, abs_tol=1e-7):
        raise RuntimeError(f'Android compatibility: native step changed: {state["fixed_step_s"]}')
    if state['source']['source_sha'] != args.source:
        raise RuntimeError(f'Android compatibility: unexpected APK source: {state["source"]!r}')
    session.capture('opening')
    before = state
    session.button(state, 'crouch')
    state = session.wait('ordinary touch crouches native player', lambda s: s['crouched'], after=before)
    session.button(state, 'crouch')
    state = session.wait('ordinary touch stands native player', lambda s: not s['crouched'], after=state)
    before = state
    session.button(state, 'jump')
    state = session.wait('ordinary touch jump produces upward velocity',
                         lambda s: s['jump_work_j'] > before['jump_work_j'] and
                         s['peak_upward_speed'] > 0.5, after=state)
    before = state
    start, end = state['stick'], state['stick_up']
    session.adb('shell', 'input', 'swipe', round(start[0]), round(start[1]),
                round(end[0]), round(end[1]), 1200)
    state = session.wait('ordinary touch stick moves native player',
                         lambda s: math.dist(s['position'][::2], before['position'][::2]) > 0.1,
                         after=before)
    session.capture('after-input')
    session.button(state, 'pause')
    state = session.wait('touch pause freezes native stepping', lambda s: s['paused'], after=state)
    frozen = state
    state = session.wait('paused observer advances while native tick stays frozen',
                         lambda s: s['paused'] and s['sequence'] >= frozen['sequence'] + 3, after=frozen)
    if state['tick'] != frozen['tick']:
        raise RuntimeError('Android compatibility: native physics continued while paused')
    session.capture('pause')
    state = session.resume(state)
    settings_hash = state['settings_sha256']
    if len(settings_hash) != 64:
        raise RuntimeError('Android compatibility: normal resume did not save player settings')
    before = state
    session.adb('shell', 'input', 'keyevent', 'KEYCODE_HOME')
    session.adb('shell', 'am', 'start', '-W', '-n', activity)
    state = session.wait('background and return preserve pause and native instance',
                         lambda s: s['paused'] and s['instance'] == before['instance'], after=before)
    state = session.resume(state)
    before = state
    # Resize is a real Android configuration change. Native state must survive.
    session.adb('shell', 'wm', 'size', args.resize)
    state = session.wait('Android resize updates layout without replacing native instance',
                         lambda s: s['window'] != before['window'] and s['instance'] == before['instance'],
                         after=before)
    session.capture('resized')
    if state['paused']:
        state = session.resume(state)
    session.adb('shell', 'am', 'force-stop', PACKAGE)
    session.adb('shell', 'am', 'start', '-W', '-n', activity)
    state = session.wait('process restart reloads persisted player settings',
                         lambda s: s['instance'] != before['instance'] and s['ready'] and
                         s['settings_sha256'] == settings_hash, after=before, timeout=240)
    session.capture('restarted')
    session.adb('shell', 'wm', 'size', 'reset')
    session.receipt['status'] = 'passed'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apk', required=True, type=Path)
    parser.add_argument('--source', required=True)
    parser.add_argument('--profile', required=True)
    parser.add_argument('--resize', default='1280x800')
    parser.add_argument('--output', type=Path, default=Path('artifacts/android-compatibility'))
    args = parser.parse_args()
    session = Session(args.output)
    try:
        exercise(args, session)
    except BaseException as error:
        session.receipt.update(status='failed', error=str(error))
        try:
            session.capture('failure')
            session.snapshot()
        except Exception as diagnostic_error:
            session.receipt['diagnostic_error'] = str(diagnostic_error)
        raise
    finally:
        session.save()


if __name__ == '__main__':
    main()
