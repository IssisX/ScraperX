#!/usr/bin/env python3
"""Drive the ordinary Android game through adb; retain source-bound observations."""
import argparse
import base64
import binascii
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import time
import xml.etree.ElementTree as ET

PREFIX = 'SCRAPERX_ANDROID_COMPAT '
PART_PREFIX = 'SCRAPERX_ANDROID_COMPAT_PART '
PACKAGE = 'com.cory.scraperx.chatgpt'


def fullscreen_confirmation(xml):
    """Locate only Android's first-run fullscreen help button, never game UI."""
    for node in ET.fromstring(xml).iter('node'):
        if (node.get('package') in ('android', 'com.android.systemui') and
                node.get('text', '').strip().upper() == 'GOT IT' and
                node.get('clickable') == 'true'):
            bounds = re.fullmatch(r'\[(\d+),(\d+)\]\[(\d+),(\d+)\]', node.get('bounds', ''))
            if bounds:
                x0, y0, x1, y1 = map(int, bounds.groups())
                if x1 > x0 and y1 > y0:
                    return [(x0 + x1) // 2, (y0 + y1) // 2]
    return None


def runtime_errors(text):
    return [line for line in text.splitlines() if re.search(
        r'(?:SCRIPT ERROR:|(?:Scene|Canvas)ShaderGLES3: Program linking failed|'
        r'Error loading script|Failed loading resource|Can.t open dynamic library)', line)]


def read_observations(text):
    """Accept only complete observations; Android may truncate/drop log lines."""
    pending = {}
    for line in text.splitlines():
        try:
            if PART_PREFIX in line:
                packet = json.loads(line.split(PART_PREFIX, 1)[1])
                key = (packet['instance'], packet['sequence'])
                part, count = packet['part'], packet['count']
                if not (isinstance(part, int) and isinstance(count, int) and 0 <= part < count <= 128):
                    continue
                group = pending.setdefault(key, {'count': count, 'parts': {}})
                if group['count'] != count:
                    continue
                group['parts'][part] = base64.b64decode(packet['data'], validate=True)
                if len(group['parts']) == count:
                    state = json.loads(b''.join(group['parts'][i] for i in range(count)))
                    if (state['instance'], state['sequence']) == key:
                        yield state
                    del pending[key]
            elif PREFIX in line:
                yield json.loads(line.split(PREFIX, 1)[1])
        except (ValueError, KeyError, TypeError, binascii.Error):
            continue


def check_touch_bounds(state):
    low, high = state['touch_safe_screen']
    for name, button in state['controls'].items():
        if not all(low[axis] - 1 <= button['min'][axis] <= button['max'][axis] <= high[axis] + 1
                   for axis in (0, 1)):
            raise RuntimeError(f'Android compatibility: {name} touch target outside safe bounds: {state!r}')


def fold_states(text):
    return [(int(identifier), name) for identifier, name in
            re.findall(r"identifier=(\d+), name='([^']+)'", text)]


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
                               f'stdout={result.stdout.decode(errors="replace")!r}; '
                               f'stderr={result.stderr.decode(errors="replace")!r}')
        return result.stdout

    def snapshot(self):
        text = self.adb('logcat', '-d', '-v', 'brief').decode(errors='replace')
        (self.output / 'logcat.txt').write_text(text)
        observations = list(read_observations(text))
        if observations:
            self.last = observations[-1]
            return self.last
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

    def require_runtime_clean(self):
        self.snapshot()
        errors = runtime_errors((self.output / 'logcat.txt').read_text())
        if errors:
            raise RuntimeError('Android game reported rendering/script/resource failures: ' + '\n'.join(errors))

    def tap(self, point):
        self.adb('shell', 'input', 'tap', round(point[0]), round(point[1]))

    def dismiss_fullscreen_help(self, state):
        path = '/sdcard/scraperx-compatibility-ui.xml'
        self.adb('shell', 'uiautomator', 'dump', '--compressed', path)
        xml = self.adb('shell', 'cat', path).decode()
        (self.output / 'opening-system-ui.xml').write_text(xml)
        point = fullscreen_confirmation(xml)
        if point is not None:
            self.tap(point)
            state = self.wait('ordinary touch dismisses Android fullscreen help',
                              lambda s: s['instance'] == state['instance'], after=state)
            self.adb('shell', 'uiautomator', 'dump', '--compressed', path)
            if fullscreen_confirmation(self.adb('shell', 'cat', path).decode()) is not None:
                raise RuntimeError('Android fullscreen help still intercepts game input after confirmation')
        return self.resume(state) if state['paused'] else state

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

    def cutout(self, state):
        available = self.adb('shell', 'cmd', 'overlay', 'list', '--user', 0).decode()
        (self.output / 'available-overlays.txt').write_text(available)
        prefix = 'com.android.internal.display.cutout.emulation.'
        disabled = re.findall(r'^\[ \] (' + re.escape(prefix) + r'\S+)$', available, re.MULTILINE)
        candidates = [name for name in disabled if name.endswith(('.tall', '.double', '.corner'))]
        if not candidates:
            raise RuntimeError('Android cutout check: no disabled supported notch overlay found; see available-overlays.txt')
        overlay = sorted(candidates, key=lambda name: not name.endswith('.tall'))[0]
        before = state
        self.adb('shell', 'cmd', 'overlay', 'enable', '--user', 0, overlay)
        try:
            state = self.wait('Android notch updates reported safe area and retains native instance',
                              lambda s: s['instance'] == before['instance'] and s['safe_area'] != before['safe_area'],
                              after=before)
            if state['paused']:
                state = self.resume(state)
            check_touch_bounds(state)
            self.capture('cutout')
            self.button(state, 'crouch')
            state = self.wait('ordinary touch remains usable with Android cutout',
                              lambda s: s['crouched'], after=state)
            self.button(state, 'crouch')
            state = self.wait('cutout input can return to standing', lambda s: not s['crouched'], after=state)
        finally:
            self.adb('shell', 'cmd', 'overlay', 'disable', '--user', 0, overlay)
        state = self.wait('removing Android notch restores reported safe area',
                          lambda s: s['safe_area'] == before['safe_area'], after=state)
        if state['paused']:
            state = self.resume(state)
        return state

    def fold(self, state):
        supported = self.adb('shell', 'cmd', 'device_state', 'print-states').decode()
        (self.output / 'supported-fold-states.txt').write_text(supported)
        states = fold_states(supported)
        closed = [(number, name) for number, name in states if 'CLOSED' in name.upper()]
        opened = [(number, name) for number, name in states if 'OPEN' in name.upper() and 'HALF' not in name.upper()]
        if not closed or not opened:
            raise RuntimeError(f'Android compatibility: foldable has no identifiable open/closed states: {supported}')
        original = self.adb('shell', 'cmd', 'device_state', 'print-state').decode().strip()
        targets = [closed[0], opened[0]] if original != str(closed[0][0]) else [opened[0], closed[0]]
        try:
            for number, name in targets:
                before = state
                self.adb('emu', 'fold' if (number, name) in closed else 'unfold')
                deadline = time.monotonic() + 10
                actual = ''
                while time.monotonic() < deadline:
                    actual = self.adb('shell', 'cmd', 'device_state', 'print-state').decode().strip()
                    if actual == str(number):
                        break
                    time.sleep(0.2)
                if actual != str(number):
                    raise RuntimeError(f'Android fold state requested {number}, observed {actual!r}')
                state = self.wait(f'fold posture {name} retains native instance and updates display',
                                  lambda s: s['instance'] == before['instance'] and s['window'] != before['window'],
                                  after=before)
                if state['paused']:
                    state = self.resume(state)
                check_touch_bounds(state)
                self.capture('fold-' + str(number))
        finally:
            self.adb('emu', 'fold' if original == str(closed[0][0]) else 'unfold')
        return state


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
    state = session.wait('normal Main initializes with real native module',
                         lambda s: s['ready'] and s['native_loaded'], timeout=240)
    state = session.dismiss_fullscreen_help(state)
    session.require_runtime_clean()
    if not math.isclose(state['fixed_step_s'], 1 / 90, abs_tol=1e-7):
        raise RuntimeError(f'Android compatibility: native step changed: {state["fixed_step_s"]}')
    if state['source']['source_sha'] != args.source:
        raise RuntimeError(f'Android compatibility: unexpected APK source: {state["source"]!r}')
    session.capture('opening')
    check_touch_bounds(state)
    initial_window = state['window']
    if args.freight_route:
        from android_freight_route import exercise_forklift
        state = exercise_forklift(session, state)
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
    before = state
    look = state['look']
    session.adb('shell', 'input', 'swipe', round(look[0]), round(look[1]), round(look[0] + 80), round(look[1]), 400)
    state = session.wait('ordinary touch drag turns camera', lambda s: abs(s['yaw'] - before['yaw']) > 0.02, after=before)
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
    check_touch_bounds(state)
    session.adb('shell', 'wm', 'size', 'reset')
    state = session.wait('reset Android display restores its original bounds',
                         lambda s: s['window'] == initial_window, after=state)
    if state['paused']:
        state = session.resume(state)
    if args.profile.startswith('foldable'):
        state = session.fold(state)
    if args.profile.startswith('tall'):
        state = session.cutout(state)
    session.adb('shell', 'am', 'force-stop', PACKAGE)
    session.adb('shell', 'am', 'start', '-W', '-n', activity)
    state = session.wait('process restart reloads persisted player settings',
                         lambda s: s['instance'] != before['instance'] and s['ready'] and
                         s['settings_sha256'] == settings_hash, after=before, timeout=240)
    session.capture('restarted')
    session.require_runtime_clean()
    session.adb('shell', 'wm', 'size', 'reset')
    session.receipt['status'] = 'passed'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apk', required=True, type=Path)
    parser.add_argument('--source', required=True)
    parser.add_argument('--profile', required=True)
    parser.add_argument('--resize', default='1280x800')
    parser.add_argument('--freight-route', action='store_true')
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
