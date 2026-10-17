"""Ordinary touch-only crane visit after ``exercise_forklift``.

Coordinates are world meters; adb coordinates come from the existing physical
screen observations. This module reads native observations and sends ordinary
Android touches only. Host contract checks do not establish Android gameplay.
"""
import math

from android_freight_route import _angle, _finite, _fresh, _healthy, _swipe, _walk


BOOTH_ENTITY = 1450
# Authored four-meter service strip, then west of the crane foundation. The
# stores at X=-75 end north of Z=-111.2; cross behind them at Z=-138.
GRADE_WAYPOINTS = ((-83.0, -57.65), (-83.0, -138.0),
                   (-69.0, -138.0), (-69.0, -150.0), (-68.5, -150.0))


def _require(ok, message, state):
    if not ok:
        raise RuntimeError(f'Crane route: {message}; actual state={state!r}')


def _intact(state, instance):
    _healthy(state, instance)
    crane = state['crane']
    _require(crane['connected'] and crane['failure'] == 0,
             'native hoist must remain connected without a crane failure', state)
    _require(len(_finite(crane['load_point'], 'crane load point')) == 3,
             'native hoist load endpoint must be three-dimensional', state)
    _finite((crane['hoist_m'], crane['work_j'], crane['source_used_j'],
             crane['hoist_cmd'], crane['slew_cmd'], crane['trolley_cmd']),
            'native crane motion and work')


def _boarded(state):
    return (state['grounded'] and state['support'] == BOOTH_ENTITY and
            state['crane']['reachable'])


def _neutral(state):
    return all(state['crane'][key] == 0
               for key in ('hoist_cmd', 'slew_cmd', 'trolley_cmd'))


def _travel(session, state, label, destination, *, caps=(0.55, 0.55)):
    """Split long straight legs to retain the existing pulse/stall bounds."""
    instance = state['instance']
    start = _finite((state['position'][0], state['position'][2]), 'leg start')
    pieces = max(1, math.ceil(math.dist(start, destination) / 6.0))
    for piece in range(1, pieces + 1):
        target = tuple(start[i] + (destination[i] - start[i]) * piece / pieces
                       for i in range(2))

        def arrived(actual):
            _intact(actual, instance)
            return (actual['grounded'] and actual['position'][1] < 1.0 and
                    math.hypot(actual['position'][0] - target[0],
                               actual['position'][2] - target[1]) < 0.20)

        state = _walk(session, state, f'{label} {piece}/{pieces}', lambda s: target,
                      caps=caps, arrived=arrived, timeout=120)
    return state


def _face_booth(session, state):
    # Main's forward is (-sin(yaw), -cos(yaw)); the west door faces +X.
    target = -math.pi / 2
    gain = None
    for attempt in range(24):
        error = _angle(target - float(state['yaw']))
        if abs(error) < 0.10:
            return state
        point = _finite(state['look'], 'look point')
        pixels = math.copysign(20.0 if gain is None else
                               min(60.0, max(2.0, abs(error) / gain * 0.8)), -error)
        low, high = state['touch_safe_screen']
        available = high[0] - point[0] if pixels > 0 else point[0] - low[0]
        _require(available > 3, 'look region has no safe horizontal swipe room', state)
        pixels = math.copysign(min(abs(pixels), available - 2), pixels)
        before = state
        _swipe(session, point, (point[0] + pixels, point[1]), 250)
        state = _fresh(session, f'face crane booth: look pulse {attempt + 1}', before)
        _intact(state, before['instance'])
        changed = _angle(float(state['yaw']) - float(before['yaw']))
        _require(changed * pixels < -0.0001,
                 'ordinary look swipe did not produce expected yaw direction', state)
        gain = abs(changed / pixels)
    raise RuntimeError(f'Crane route: could not face actual west booth door; state={state!r}')


def _hoist_raised(state, baseline):
    actual = state['crane']
    return (actual['hoist_m'] < baseline['hoist_m'] - 0.20 and
            actual['load_point'][1] > baseline['load_point'][1] + 0.15 and
            actual['work_j'] > baseline['work_j'] + 1.0 and
            actual['source_used_j'] > baseline['source_used_j'] + 1.0)


def exercise_crane(session, state):
    """Walk from forklift exit, board, hoist, release, Done, and leave to grade.

    Requires crane probe fields ``reachable, station, hoist_m, work_j,
    source_used_j, connected, failure, load_point, hoist_cmd, slew_cmd,
    trolley_cmd``. ``load_point`` is native hoist line zero's load endpoint.
    """
    instance = state['instance']
    _intact(state, instance)
    _require(state['operating'] == '' and state['grounded'] and
             state['position'][1] < 1.0 and not state['forklift']['reachable'] and
             math.hypot(state['position'][0] + 98,
                        state['position'][2] + 57.65) < 0.5,
             'entry requires the ordinary forklift route grade exit', state)
    for index, waypoint in enumerate(GRADE_WAYPOINTS, 1):
        state = _travel(session, state, f'crane ground approach leg {index}', waypoint)
    session.capture('crane-west-doorway')
    state = _face_booth(session, state)

    def boarded(actual):
        _intact(actual, instance)
        return _boarded(actual)

    def station(actual):
        point = _finite(actual['crane']['station'], 'crane station')
        _require(len(point) == 3, 'crane station must be three-dimensional', actual)
        return point[0], point[2]

    state = _walk(session, state, 'walk through west door onto actual crane floor',
                  station, caps=(0.30, 0.15), arrived=boarded, timeout=120)
    before = state
    state = session.wait('ordinary crane boarding retains native floor contact',
                         lambda s: s['tick'] >= before['tick'] + 45 and boarded(s),
                         after=before, timeout=30)
    session.capture('crane-boarded')
    session.button(state, 'action')
    state = session.wait('contextual touch Action enters crane controls',
                         lambda s: s['operating'] == 'yard_crane' and boarded(s) and
                         {'pendant_up', 'action'} <= s['controls'].keys(),
                         after=state, timeout=30)
    baseline = dict(state['crane'])
    for attempt in range(6):
        _intact(state, instance)
        _require(state['operating'] == 'yard_crane' and _boarded(state),
                 'hoist requires real cab footing and control authorization', state)
        _require('pendant_up' in state['controls'], 'active hoist touch button absent', state)
        point = state['controls']['pendant_up']['point']
        before = state
        _swipe(session, point, point, 1100)
        state = _fresh(session, f'crane hoist: native pulse {attempt + 1}', before)
        _intact(state, instance)
        if _hoist_raised(state, baseline):
            break
    else:
        raise RuntimeError('Crane route: six ordinary hoist holds did not raise the real '
                           f'load endpoint with positive work/source use; '
                           f'baseline={baseline!r}; actual state={state!r}')
    released = state
    state = session.wait('released crane touch neutralizes all native motor targets',
                         lambda s: s['tick'] >= released['tick'] + 18 and
                         s['operating'] == 'yard_crane' and boarded(s) and _neutral(s) and
                         _hoist_raised(s, baseline), after=released, timeout=20)
    held = state
    state = session.wait('neutral crane hold adds no native source work',
                         lambda s: s['tick'] >= held['tick'] + 45 and
                         s['operating'] == 'yard_crane' and boarded(s) and _neutral(s) and
                         s['crane']['source_used_j'] == held['crane']['source_used_j'],
                         after=held, timeout=20)
    session.capture('crane-hoist-neutral')
    session.button(state, 'action')
    state = session.wait('ordinary Done touch leaves crane controls on actual booth floor',
                         lambda s: s['operating'] == '' and boarded(s) and _neutral(s),
                         after=state, timeout=30)
    state = _travel(session, state, 'leave crane west door onto grade', (-69.0, -150.0),
                    caps=(0.30, 0.15))
    before = state
    state = session.wait('crane route finishes on actual grade outside booth reach',
                         lambda s: s['tick'] >= before['tick'] + 45 and
                         s['grounded'] and s['position'][1] < 1.0 and
                         s['support'] != BOOTH_ENTITY and not s['crane']['reachable'] and
                         s['operating'] == '' and _neutral(s), after=before, timeout=30)
    _intact(state, instance)
    session.capture('crane-grade-exit')
    return state
