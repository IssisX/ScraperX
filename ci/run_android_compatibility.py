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
from copy import deepcopy

PREFIX = 'SCRAPERX_ANDROID_COMPAT '
PART_PREFIX = 'SCRAPERX_ANDROID_COMPAT_PART '
PACKAGE = 'com.cory.scraperx.chatgpt'


def app_window_frame(text, size):
    """Read actual activity bounds; never infer letterboxing or input scaling."""
    frames = set()
    for block in re.split(r'(?m)^\s*Window #\d+ ', text):
        title = block.split('\n', 1)[0]
        if PACKAGE not in title or any(name in title for name in ('Starting ', 'Splash Screen')):
            continue
        match = re.search(r'\b(?:mFrame|frame)=\[(-?\d+),\s*(-?\d+)\]\[(-?\d+),\s*(-?\d+)\]', block)
        if match:
            frame = tuple(map(int, match.groups()))
            if [frame[2] - frame[0], frame[3] - frame[1]] == list(size):
                frames.add(frame)
    if len(frames) != 1:
        raise ValueError(f'Expected one Android app frame matching native size {size!r}; got {sorted(frames)!r}')
    return frames.pop()


def screen_observation(native, frame):
    """Derive adb coordinates from native window pixels and measured OS origin."""
    state = deepcopy(native)
    def point(value):
        return [value[0] + frame[0], value[1] + frame[1]] if len(value) == 2 else value
    for key in ('stick', 'stick_up', 'look'):
        state[key] = point(state[key])
    state['touch_safe_screen'] = [point(value) for value in state['touch_safe_screen']]
    for button in state['controls'].values():
        for key in ('point', 'min', 'max'):
            button[key] = point(button[key])
    for button in state['menu_buttons']:
        button['point'] = point(button['point'])
    state['input_window_frame'] = list(frame)
    return state


def runtime_errors(text):
    return [line for line in text.splitlines() if re.search(
        r'(?:SCRIPT ERROR:|\w+ShaderGLES3: Program linking failed|'
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
        self.frame_key = None
        self.frame = None
        self.frame_error = None
        self.touch_driver = None
        self.display_overridden = False

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
            state = observations[-1]
            if state['ready'] and (state['controls'] or state['menu_buttons']):
                key = (state['instance'], tuple(state['window']), state['safe_area'])
                if key != self.frame_key:
                    windows = self.adb('shell', 'dumpsys', 'window', '-a', 'windows').decode()
                    (self.output / 'window-frames.txt').write_text(windows)
                    try:
                        self.frame = app_window_frame(windows, state['window'])
                    except ValueError as error:
                        self.frame_error = str(error)
                        return None  # A configuration transition must settle to a measured frame.
                    self.frame_key, self.frame_error = key, None
                state = screen_observation(state, self.frame)
            self.last = state
            return state
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
        raise RuntimeError(f'Android compatibility timeout: {label}; last actual state={self.last!r}; frame={self.frame_error!r}')

    def capture(self, name):
        (self.output / f'{name}.png').write_bytes(self.adb('exec-out', 'screencap', '-p'))

    def require_runtime_clean(self):
        self.snapshot()
        errors = runtime_errors((self.output / 'logcat.txt').read_text())
        if errors:
            raise RuntimeError('Android game reported rendering/script/resource failures: ' + '\n'.join(errors))

    def tap(self, point):
        self.adb('shell', 'input', 'tap', round(point[0]), round(point[1]))

    def prepare_input_focus(self):
        """Recover only the observed bundled Messenger crash on disposable AVDs."""
        deadline = time.monotonic() + 20
        recovered = False
        focus = ''
        while time.monotonic() < deadline:
            windows = self.adb('shell', 'dumpsys', 'window', '-a', 'windows').decode()
            match = re.search(r'(?m)^\s*mCurrentFocus=(.*)$', windows)
            focus = match[1].strip() if match else ''
            crash = re.search(r'\bApplication Error: ([\w.]+)\}', focus)
            if crash:
                package = crash[1]
                (self.output / 'startup-crash-window.txt').write_text(windows)
                self.capture('startup-crash-dialog')
                # Never conceal a game crash or suppress system error dialogs.
                if package != 'com.google.android.apps.messaging' or recovered:
                    raise RuntimeError(f'Android startup blocked by application crash: {focus}')
                self.receipt['startup_external_crash'] = {'package': package, 'focus': focus}
                self.adb('shell', 'am', 'force-stop', package)
                recovered = True
            elif re.search(r'\bu\d+ ' + re.escape(PACKAGE) + r'/', focus):
                (self.output / 'startup-input-focus.txt').write_text(windows)
                self.receipt['startup_input_focus'] = focus
                return recovered
            time.sleep(0.2)  # Require the OS focus result, not elapsed time.
        raise RuntimeError(f'Android startup has no game input focus; actual focus={focus!r}')

    def swipe(self, start, end, duration_ms, *, until=None):
        if self.touch_driver is None:
            from android_touch_driver import TouchDriver
            api = int(self.receipt['ro.build.version.sdk'])
            self.touch_driver = TouchDriver(self, api)
        return self.touch_driver.swipe(start, end, duration_ms, until=until)

    def resize_display(self, state, size, label, *, expected=None):
        # Track cleanup ownership separately from actual display evidence.
        # The observed Android window remains the acceptance authority.
        if size != 'reset':
            self.display_overridden = True
        self.adb('shell', 'wm', 'size', size)
        if size == 'reset':
            self.display_overridden = False
        self.frame_key = None
        before = state
        state = self.wait(label, lambda s: s['instance'] == before['instance'] and
            (s['window'] == expected if expected is not None else s['window'] != before['window']),
            after=before)
        state = self.first_controls(state)
        check_touch_bounds(state)
        return state

    def first_controls(self, state):
        if state['paused']:
            state = self.resume(state)
        return self.wait('first-person controls accept input after startup',
                         lambda s: not s['paused'] and {'crouch', 'jump', 'pause'} <= s['controls'].keys(),
                         after=state)

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
    # Disposable AVD setup only: Android's one-time fullscreen tutorial is
    # already acknowledged. The game and every gameplay input stay ordinary.
    session.adb('shell', 'settings', '--user', 'current', 'put', 'secure',
                'immersive_mode_confirmations', 'confirmed')
    confirmed = session.adb('shell', 'settings', '--user', 'current', 'get', 'secure',
                            'immersive_mode_confirmations').decode().strip()
    if confirmed != 'confirmed':
        raise RuntimeError(f'Android fullscreen tutorial setup was not applied: {confirmed!r}')
    session.receipt['fullscreen_tutorial'] = 'OS-only prelaunch confirmation in disposable AVD'
    session.adb('logcat', '-c')
    session.adb('shell', 'am', 'start', '-W', '-n', activity)
    state = session.wait('normal Main initializes with real native module',
                         lambda s: s['ready'] and s['native_loaded'], timeout=240)
    if session.prepare_input_focus():
        before = state
        state = session.wait('external emulator dialog dismissal retains native instance',
                             lambda s: s['ready'] and s['instance'] == before['instance'], after=before)
    state = session.first_controls(state)
    session.require_runtime_clean()
    if not math.isclose(state['fixed_step_s'], 1 / 90, abs_tol=1e-7):
        raise RuntimeError(f'Android compatibility: native step changed: {state["fixed_step_s"]}')
    if state['source']['source_sha'] != args.source:
        raise RuntimeError(f'Android compatibility: unexpected APK source: {state["source"]!r}')
    session.capture('opening')
    check_touch_bounds(state)
    initial_window = state['window']
    # Exercise actual touch at full device size before reducing only the CI
    # framebuffer. Full-size software rendering overloaded Android SystemUI
    # in the API36 tablet run; this is not a phone performance measurement.
    before = state
    session.button(state, 'crouch')
    state = session.wait('ordinary touch crouches native player', lambda s: s['crouched'], after=before)
    session.button(state, 'crouch')
    state = session.wait('ordinary touch stands native player', lambda s: not s['crouched'], after=state)
    state = session.resize_display(state, '800x450',
        'functional Android test window retains actual native instance')
    functional_window = state['window']
    session.capture('functional-test-window')
    if args.freight_route:
        from android_freight_route import exercise_forklift
        state = exercise_forklift(session, state)
        if args.crane_route:
            from android_crane_route import exercise_crane
            state = exercise_crane(session, state)
    before = state
    session.button(state, 'jump')
    state = session.wait('ordinary touch jump produces upward velocity',
                         lambda s: s['jump_work_j'] > before['jump_work_j'] and
                         s['peak_upward_speed'] > 0.5, after=state)
    before = state
    start, end = state['stick'], state['stick_up']
    session.swipe(start, end, 1200, until=lambda s:
        math.dist(s['position'][::2], before['position'][::2]) > 0.1 and
        s['input_trace']['nonzero_native_ticks'] > before['input_trace']['nonzero_native_ticks'])
    state = session.wait('ordinary touch stick moves native player',
                         lambda s: math.dist(s['position'][::2], before['position'][::2]) > 0.1,
                         after=before)
    session.capture('after-input')
    before = state
    look = state['look']
    session.swipe(look, (look[0] + 80, look[1]), 400,
                  until=lambda s: abs(s['yaw'] - before['yaw']) > 0.02)
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
    settings_data = session.adb('exec-out', 'run-as', PACKAGE, 'cat', 'files/settings.cfg')
    (session.output / 'persisted-settings.cfg').write_bytes(settings_data)
    if not settings_data.strip() or hashlib.sha256(settings_data).hexdigest() != settings_hash:
        raise RuntimeError('Android compatibility: persisted settings bytes are empty or disagree with observer hash')
    before = state
    session.adb('shell', 'input', 'keyevent', 'KEYCODE_HOME')
    session.adb('shell', 'am', 'start', '-W', '-n', activity)
    state = session.wait('background and return preserve pause and native instance',
                         lambda s: s['paused'] and s['instance'] == before['instance'], after=before)
    state = session.resume(state)
    # Resize is a real Android configuration change. Native state must survive.
    state = session.resize_display(state, args.resize,
        'Android resize updates layout without replacing native instance')
    session.capture('resized')
    state = session.resize_display(state, '800x450',
        'functional Android bounds return after large-window layout check', expected=functional_window)
    if args.profile.startswith('foldable'):
        # A display-size override would mask the physical fold's changing
        # panel dimensions. Restore native device bounds for this check.
        state = session.resize_display(state, 'reset',
            'native display restored before physical fold', expected=initial_window)
        state = session.fold(state)
        state = session.resize_display(state, '800x450',
            'functional bounds restored after physical fold', expected=functional_window)
    if args.profile.startswith('tall'):
        state = session.cutout(state)
    if args.display_route:
        from android_display_route import exercise_display
        state = exercise_display(session, state)
    before = state
    session.adb('shell', 'am', 'force-stop', PACKAGE)
    session.adb('shell', 'am', 'start', '-W', '-n', activity)
    state = session.wait('process restart reloads persisted player settings',
                         lambda s: s['instance'] != before['instance'] and s['ready'] and
                         s['settings_sha256'] == settings_hash, after=before, timeout=240)
    state = session.resize_display(state, 'reset',
        'reset Android display restores original bounds after process restart', expected=initial_window)
    session.capture('restarted')
    session.require_runtime_clean()
    session.receipt['status'] = 'passed'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--apk', required=True, type=Path)
    parser.add_argument('--source', required=True)
    parser.add_argument('--profile', required=True)
    parser.add_argument('--resize', default='1280x800')
    parser.add_argument('--freight-route', action='store_true')
    parser.add_argument('--crane-route', action='store_true')
    parser.add_argument('--display-route', action='store_true')
    parser.add_argument('--output', type=Path, default=Path('artifacts/android-compatibility'))
    args = parser.parse_args()
    if args.crane_route and not args.freight_route:
        parser.error('--crane-route requires ordinary --freight-route arrival first')
    session = Session(args.output)
    primary_failed = False
    try:
        exercise(args, session)
    except BaseException as error:
        primary_failed = True
        session.receipt.update(status='failed', error=str(error))
        try:
            session.capture('failure')
            session.snapshot()
        except Exception as diagnostic_error:
            session.receipt['diagnostic_error'] = str(diagnostic_error)
        raise
    finally:
        cleanup_errors = []
        if session.touch_driver is not None:
            try:
                session.touch_driver.close()
            except Exception as error:
                session.receipt['touch_driver_close_error'] = str(error)
                cleanup_errors.append('touch driver close: ' + str(error))
        if session.display_overridden:
            try:
                session.adb('shell', 'wm', 'size', 'reset')
            except Exception as error:
                cleanup_errors.append('restore Android display: ' + str(error))
        if cleanup_errors:
            session.receipt.update(status='failed', cleanup_errors=cleanup_errors)
        session.save()
        if cleanup_errors and not primary_failed:
            raise RuntimeError('Android compatibility cleanup failed: ' + '; '.join(cleanup_errors))


if __name__ == '__main__':
    main()
