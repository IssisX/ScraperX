"""Ordinary Android touch route from the freight arrival to the real forklift.

Consumes the existing AndroidCompatibilityProbe observations. It never writes
simulation state or invokes native commands. Emulator execution is required
before this route can be reported as gameplay evidence.
"""
import math
import time


CHASSIS_ENTITY = 2440


def _finite(values, label):
    result = tuple(float(value) for value in values)
    if not all(math.isfinite(value) for value in result):
        raise RuntimeError(f'Freight route: non-finite {label}: {values!r}')
    return result


def _require(ok, message, state):
    if not ok:
        raise RuntimeError(f'Freight route: {message}; actual state={state!r}')


def _healthy(state, instance):
    _require(state['instance'] == instance and state['ready'] and
             state['native_loaded'] and not state['paused'],
             'playable native instance must survive the route', state)
    _require(len(_finite(state['position'], 'position')) == 3 and
             len(_finite(state['velocity'], 'velocity')) == 3,
             'expected three-dimensional position and velocity', state)


def stick_endpoint(state, world_x, world_z):
    """Invert Main's yaw rotation and TouchControls' radial deadzone.

    ``stick_up`` is 100 authored units above home; full throw is 150.
    The requested world vector is normalized walking intent, not velocity.
    """
    world_x, world_z, yaw = _finite((world_x, world_z, state['yaw']), 'movement')
    start = _finite(state['stick'], 'stick home')
    up = _finite(state['stick_up'], 'stick up')
    if len(start) != 2 or len(up) != 2:
        raise RuntimeError('Freight route: stick observations must be two-dimensional')
    ux, uy = up[0] - start[0], up[1] - start[1]
    if math.hypot(ux, uy) < 1:
        raise RuntimeError('Freight route: observed stick calibration is too small')
    magnitude = math.hypot(world_x, world_z)
    if magnitude == 0:
        return start
    # Main: world = right * local.x + forward * local.y.
    right = world_x * math.cos(yaw) - world_z * math.sin(yaw)
    forward = -world_x * math.sin(yaw) - world_z * math.cos(yaw)
    radius = 1.5 * (0.1 + 0.9 * min(1.0, magnitude)) / magnitude
    # Physical up is (ux, uy), physical right is (-uy, ux).
    return (start[0] + radius * (-uy * right + ux * forward),
            start[1] + radius * (ux * right + uy * forward))


def _swipe(session, start, end, duration_ms):
    session.adb('shell', 'input', 'swipe', round(start[0]), round(start[1]),
                round(end[0]), round(end[1]), duration_ms)


def _fresh(session, label, before, *, ticks=1, timeout=20):
    instance = before['instance']
    state = session.wait(label, lambda s: s['tick'] >= before['tick'] + ticks,
                         after=before, timeout=timeout)
    _healthy(state, instance)
    return state


def _walk(session, state, label, target, *, caps, arrived, timeout=240):
    """Bounded feedback walking; every pulse ends with Android ACTION_UP."""
    deadline = time.monotonic() + timeout
    best = math.inf
    stalled = 0
    for pulse in range(100):
        if arrived(state):
            return state
        _require(time.monotonic() < deadline, f'{label}: navigation deadline expired', state)
        _require(state['operating'] == '', f'{label}: walking requires leaving controls', state)
        x, z = target(state)
        px, _, pz = _finite(state['position'], 'walking position')
        vx, _, vz = _finite(state['velocity'], 'walking velocity')
        distance = math.hypot(x - px, z - pz)
        if distance < best - 0.035:
            best, stalled = distance, 0
        else:
            stalled += 1
        _require(stalled < 12, f'{label}: no 3.5 cm progress in twelve pulses; '
                 f'target={(x, z)!r}, best_distance_m={best:.3f}', state)
        # Same proportional/velocity damping as the native arrival replay.
        wx = max(-caps[0], min(caps[0], 1.2 * (x - px) - 0.45 * vx))
        wz = max(-caps[1], min(caps[1], 1.2 * (z - pz) - 0.45 * vz))
        endpoint = stick_endpoint(state, wx, wz)
        before = state
        _swipe(session, state['stick'], endpoint, 650)
        state = _fresh(session, f'{label}: observed native pulse {pulse + 1}', before)
    raise RuntimeError(f'Freight route: {label}: exceeded 100 touch pulses; actual state={state!r}')


def _angle(value):
    return (value + math.pi) % math.tau - math.pi


def _face_door(session, state):
    """Face the open east doorway using the probe's existing look region."""
    # Native arrival acceptance faces -X before walking onto the real floor.
    target = math.pi / 2
    gain = None
    for attempt in range(24):
        error = _angle(target - float(state['yaw']))
        if abs(error) < 0.10:
            return state
        point = _finite(state['look'], 'look point')
        # First pulse measures actual sensitivity and canvas-to-screen scaling.
        pixels = math.copysign(20.0 if gain is None else
                               min(60.0, max(2.0, abs(error) / gain * 0.8)), -error)
        low, high = state['touch_safe_screen']
        available = high[0] - point[0] if pixels > 0 else point[0] - low[0]
        _require(available > 3, 'look region has no safe horizontal swipe room', state)
        pixels = math.copysign(min(abs(pixels), available - 2), pixels)
        before = state
        _swipe(session, point, (point[0] + pixels, point[1]), 250)
        state = _fresh(session, f'face forklift doorway: observed look pulse {attempt + 1}', before)
        changed = _angle(float(state['yaw']) - float(before['yaw']))
        _require(changed * pixels < -0.0001,
                 'ordinary look swipe did not produce the expected yaw direction', state)
        gain = abs(changed / pixels)
    raise RuntimeError(f'Freight route: could not face the open doorway; actual state={state!r}')


def _hold_hydraulic(session, state, name, field, minimum):
    baseline = state['forklift']
    initial, work, battery = _finite(
        (baseline[field], baseline['work_j'], baseline['battery_j']), 'hydraulic baseline')
    # The owner counts max(0, torque * angular displacement). Gravity can
    # assist tilt, so positive mechanical work is required for lifting only.
    minimum_work = 1.0 if field == 'lift_m' else 0.0
    for attempt in range(6):
        _require(state['operating'] == 'forklift' and state['grounded'] and
                 state['support'] == CHASSIS_ENTITY and state['forklift']['reachable'],
                 f'{name}: real cab footing and control authorization required', state)
        _require(name in state['controls'], f'{name}: active touch button absent', state)
        before = state
        point = state['controls'][name]['point']
        # A zero-distance swipe holds the existing button, then releases it.
        _swipe(session, point, point, 1100)
        state = _fresh(session, f'{name}: observed hydraulic pulse {attempt + 1}', before)
        actual = state['forklift']
        if (float(actual[field]) > initial + minimum and
                float(actual['work_j']) >= work + minimum_work and
                float(actual['battery_j']) < battery - 0.1):
            verified = session.wait(f'{name}: physical {field}, measured work and battery debit',
                                lambda s: s['operating'] == 'forklift' and s['grounded'] and
                                s['support'] == CHASSIS_ENTITY and s['forklift']['reachable'] and
                                float(s['forklift'][field]) > initial + minimum and
                                float(s['forklift']['work_j']) >= work + minimum_work and
                                float(s['forklift']['battery_j']) < battery - 0.1,
                                after=state, timeout=20)
            _healthy(verified, state['instance'])
            return verified
    raise RuntimeError(f'Freight route: {name} produced no required physical motion/work/battery '
                       f'response after six ordinary holds; baseline={baseline!r}; actual state={state!r}')


def exercise_forklift(session, state):
    """Walk from normal arrival, board, operate hydraulics and walk to grade."""
    instance = state['instance']
    _healthy(state, instance)
    _require(math.hypot(state['position'][0] + 94, state['position'][2] + 44) < 1.0,
             'route must begin at the normal freight arrival near (-94, -44)', state)
    _require(state['operating'] == '', 'arrival must start outside a machine mode', state)
    state = session.wait('freight arrival has actual grade footing',
                         lambda s: s['grounded'] and not s['forklift']['reachable'] and
                         s['position'][1] < 1.0, after=state, timeout=30)
    _healthy(state, instance)
    doorway = (-98.0, -57.65)
    state = _walk(session, state, 'arrival to forklift doorway', lambda s: doorway,
                  caps=(0.55, 0.55), arrived=lambda s:
                  math.hypot(s['position'][0] - doorway[0], s['position'][2] - doorway[1]) < 0.25)
    session.capture('freight-doorway')
    state = _face_door(session, state)
    # Observed station is local z=-.88; the operator floor is local z=-1.65.
    # The arrival truck is parked in the authored world frame, as in the replay.
    def floor_target(s):
        station = _finite(s['forklift']['station'], 'forklift station')
        return station[0], station[2] - 0.77

    def boarded(s):
        return s['grounded'] and s['support'] == CHASSIS_ENTITY and s['forklift']['reachable']

    state = _walk(session, state, 'walk onto real operator floor', floor_target,
                  caps=(0.30, 0.15), arrived=boarded, timeout=120)
    before = state
    state = session.wait('ordinary boarding retains real forklift floor contact',
                         lambda s: s['tick'] >= before['tick'] + 45 and boarded(s),
                         after=before, timeout=30)
    _healthy(state, instance)
    session.capture('freight-boarded')
    session.button(state, 'action')
    state = session.wait('contextual touch Action enters forklift controls',
                         lambda s: s['operating'] == 'forklift' and boarded(s),
                         after=state, timeout=30)
    _healthy(state, instance)
    state = _hold_hydraulic(session, state, 'pendant_up', 'lift_m', 0.05)
    state = _hold_hydraulic(session, state, 'pendant_left', 'tilt_rad', 0.015)
    released = state
    state = session.wait('released hydraulic touches clear both native targets',
                         lambda s: s['tick'] >= released['tick'] + 18 and
                         s['forklift']['lift_cmd'] == 0 and s['forklift']['tilt_cmd'] == 0,
                         after=released, timeout=20)
    _healthy(state, instance)
    session.capture('freight-hydraulics')
    session.button(state, 'action')
    state = session.wait('ordinary Done touch leaves forklift controls',
                         lambda s: s['operating'] == '' and boarded(s), after=state, timeout=30)
    _healthy(state, instance)
    state = _walk(session, state, 'walk off cab to grade', lambda s: doorway,
                  caps=(0.30, 0.15), arrived=lambda s:
                  s['grounded'] and s['support'] != CHASSIS_ENTITY and
                  not s['forklift']['reachable'] and s['position'][1] < 1.0 and
                  math.hypot(s['position'][0] - doorway[0], s['position'][2] - doorway[1]) < 0.28,
                  timeout=120)
    before = state
    state = session.wait('freight route finishes on real grade outside forklift reach',
                         lambda s: s['tick'] >= before['tick'] + 45 and s['grounded'] and
                         s['position'][1] < 1.0 and s['support'] != CHASSIS_ENTITY and
                         not s['forklift']['reachable'] and s['operating'] == '',
                         after=before, timeout=30)
    _healthy(state, instance)
    session.capture('freight-grade-exit')
    return state
