"""Sampled steering checks; the model is not Android/Jolt gameplay proof."""
import math
import unittest
from copy import deepcopy
from unittest.mock import patch

from android_crane_route import _travel
from android_freight_route import _walk, _walking_intent


def sampled_approach(controller, hold_s, start, target, caps):
    """Independent finite-acceleration plant with coarse observation delays.

    Native-scale integration uses 22 m/s² acceleration toward a 5.5 m/s
    normalized speed request. The command persists through observer overhead;
    release applies neutral braking for 100 ms before the next observation.
    The three fixed overheads span the reported 0.9–1.2 s old pulse intervals.
    This deliberately excludes collisions, slopes, moving supports and Jolt.
    """
    position, velocity = list(start), [0.0, 0.0]
    step_s, interval_s = 1 / 90, 0.4
    best, stalled = math.inf, 0
    for pulse in range(100):
        distance = math.dist(position, target)
        if distance < 0.20:
            return 'arrived', pulse, distance
        if distance < best - 0.035:
            best, stalled = distance, 0
        else:
            stalled += 1
        if stalled >= 12:
            return 'stalled', pulse, distance
        intent = controller(position, velocity, target, caps, interval_s)
        overhead_s = (0.25, 0.45, 0.15)[pulse % 3]
        held_ticks = round((hold_s + overhead_s) / step_s)
        release_ticks = 9
        for tick in range(held_ticks + release_ticks):
            desired = [5.5 * value if tick < held_ticks else 0.0 for value in intent]
            change = [wanted - actual for wanted, actual in zip(desired, velocity)]
            length = math.hypot(*change)
            scale = min(1.0, 22.0 * step_s / length) if length else 1.0
            velocity = [actual + delta * scale for actual, delta in zip(velocity, change)]
            position = [actual + speed * step_s for actual, speed in zip(position, velocity)]
        interval_s = (held_ticks + release_ticks) * step_s
    return 'exhausted', 100, math.dist(position, target)


class GradeSession:
    """Rounded touch + finite acceleration plant at captured observer cadence.

    Each observation advances 9 native ticks; software rendering costs ten
    wall seconds per native second. Injection/acceptance takes 27 ticks and
    release/cleanup 45. A 100 ms pulse plus the next observation totals 90
    ticks, inside the captured 81–99 tick cycles. This excludes Jolt/contact.
    """
    def __init__(self, start, *, blocked=False):
        self.wall = 0.0
        self.blocked = blocked
        self.held = False
        self.holds = 0
        self.state = {'instance': 'grade', 'ready': True, 'native_loaded': True,
                      'paused': False, 'operating': '', 'grounded': True, 'support': 1,
                      'position': [start[0], 0.9, start[1]], 'velocity': [0.0, 0.0, 0.0],
                      'tick': 100, 'fixed_step_s': 1 / 90, 'yaw': 0.0,
                      'stick': [65, 385], 'stick_up': [65, 362],
                      'crane': {'connected': True, 'failure': 0, 'load_point': [0, 4, 0],
                                'hoist_m': 30, 'work_j': 0, 'source_used_j': 0,
                                'hoist_cmd': 0, 'slew_cmd': 0, 'trolley_cmd': 0}}

    def advance(self, intent, ticks):
        for _ in range(ticks):
            self.state['tick'] += 1
            self.wall += 10 / 90
            velocity = self.state['velocity'][::2]
            change = [5.5 * desired - actual for desired, actual in zip(intent, velocity)]
            length = math.hypot(*change)
            scale = min(1.0, 22 / 90 / length) if length else 1.0
            for axis, actual, delta in zip((0, 2), velocity, change):
                self.state['velocity'][axis] = 0.0 if self.blocked else actual + delta * scale
                self.state['position'][axis] += self.state['velocity'][axis] / 90

    def swipe(self, start, end, duration_ms, *, until=None):
        self.holds += 1
        self.held = True
        # Invert the existing stick radial deadzone after actual pixel rounding.
        local = [(round(end[0]) - round(start[0])) / 34.5,
                 (round(start[1]) - round(end[1])) / 34.5]
        magnitude = math.hypot(*local)
        scale = max(0.0, min(1.0, (magnitude - 0.1) / 0.9)) / magnitude if magnitude else 0
        intent = (local[0] * scale, -local[1] * scale)
        try:
            self.advance(intent, 27)
            if until is None:
                self.advance(intent, math.ceil(duration_ms / 1000 * 90))
            else:
                deadline = self.wall + 90
                while not until(self.state):
                    if self.wall >= deadline:
                        raise RuntimeError('fixture required game response never arrived')
                    self.advance(intent, 9)
            return deepcopy(self.state)
        finally:
            self.held = False
            self.advance((0, 0), 45)

    def wait(self, label, predicate, **kwargs):
        self.advance((0, 0), 9)
        if not predicate(self.state):
            raise RuntimeError('fixture rejected observation: ' + label)
        return deepcopy(self.state)


class AndroidRouteNavigationTests(unittest.TestCase):
    def test_sustained_crane_travel_rejects_captured_slow_pulse_approach(self):
        start, target = (-83.18502045, -63.18006134), (-83.15859, -69.10938)
        baseline = GradeSession(start)
        arrived = lambda s: math.dist(s['position'][::2], target) < 0.20
        with patch('android_crane_route.time.monotonic', lambda: baseline.wall), \
             self.assertRaisesRegex(RuntimeError, 'navigation deadline expired'):
            _walk(baseline, deepcopy(baseline.state), 'captured crane piece', lambda s: target,
                  caps=(0.55, 0.55), arrived=arrived, timeout=120)
        self.assertFalse(baseline.held)
        repaired = GradeSession(start)
        with patch('android_crane_route.time.monotonic', lambda: repaired.wall):
            state = _travel(repaired, deepcopy(repaired.state), 'captured crane piece', target)
        self.assertLess(math.dist(state['position'][::2], target), 0.20)
        self.assertTrue(state['grounded'])
        self.assertLess(state['position'][1], 1.0)
        self.assertLess(repaired.wall, 120)
        self.assertLess(repaired.holds, baseline.holds)
        self.assertFalse(repaired.held)

    def test_sustained_crane_travel_fails_without_physical_progress_and_releases(self):
        session = GradeSession((-83.18502045, -63.18006134), blocked=True)
        with patch('android_crane_route.time.monotonic', lambda: session.wall), \
             self.assertRaisesRegex(RuntimeError, 'required game response never arrived'):
            _travel(session, deepcopy(session.state), 'blocked crane piece', (-83.15859, -69.10938))
        self.assertFalse(session.held)
        self.assertEqual(session.state['position'], [-83.18502045, 0.9, -63.18006134])

    def test_long_crane_service_strip_retains_bounded_ground_checkpoints(self):
        session = GradeSession((-83.18502045, -57.62761307))
        target = (-83.0, -138.0)
        with patch('android_crane_route.time.monotonic', lambda: session.wall):
            state = _travel(session, deepcopy(session.state), 'long service strip', target)
        self.assertLess(math.dist(state['position'][::2], target), 0.20)
        self.assertGreater(session.wall, 120)  # One unsplit hold cannot satisfy its deadline.
        self.assertLess(session.wall, 14 * 120)
        self.assertTrue(state['grounded'])
        self.assertFalse(session.held)

    def test_sustained_crane_travel_never_accepts_airborne_endpoint(self):
        session = GradeSession((-83.18502045, -69.0))
        session.state['grounded'] = False
        with patch('android_crane_route.time.monotonic', lambda: session.wall), \
             self.assertRaisesRegex(RuntimeError, 'required game response never arrived'):
            _travel(session, deepcopy(session.state), 'airborne endpoint', (-83.15859, -69.10938))
        self.assertFalse(session.held)

    def test_direction_and_dimensioned_speed(self):
        wx, wz = _walking_intent((0, 0), (0, 0), (1, -1), (0.55, 0.55), 0.4)
        self.assertAlmostEqual(5.5 * wx, 1 / 0.8)
        self.assertAlmostEqual(5.5 * wz, -1 / 0.8)
        slow, _ = _walking_intent((0, 0), (0, 0), (1, 0), (0.55, 0.55), 1.2)
        self.assertLess(slow, wx)

    def test_caps_and_normalized_magnitude(self):
        for caps in ((0.30, 0.15), (1.0, 1.0)):
            with self.subTest(caps=caps):
                wx, wz = _walking_intent((0, 0), (0, 0), (100, -100), caps, 0.4)
                self.assertLessEqual(abs(wx), caps[0])
                self.assertLessEqual(abs(wz), caps[1])
                self.assertLessEqual(math.hypot(wx, wz), 1.0)

    def test_velocity_predicts_motion_without_full_strength_reversal(self):
        still, _ = _walking_intent((0, 0), (0, 0), (1, 0), (0.55, 0.55), 0.4)
        moving, _ = _walking_intent((0, 0), (2, 0), (1, 0), (0.55, 0.55), 0.4)
        self.assertGreater(moving, 0)
        self.assertLess(moving, still)
        braking, _ = _walking_intent((0, 0), (2, 0), (0, 0), (0.55, 0.55), 0.4)
        self.assertLess(braking, 0)
        self.assertLess(abs(braking), 0.1)

    def test_rejects_nonfinite_observations_and_invalid_control_bounds(self):
        valid = [(0, 0), (0, 0), (1, 1), (0.55, 0.55), 0.4]
        for index in range(5):
            args = list(valid)
            args[index] = math.nan if index == 4 else (math.inf, 0)
            with self.subTest(index=index), self.assertRaisesRegex(RuntimeError, 'non-finite'):
                _walking_intent(*args)
        for interval, caps in ((0, (0.55, 0.55)), (-1, (0.55, 0.55)),
                               (0.4, (0, 0.55)), (0.4, (0.55, 1.1))):
            with self.subTest(interval=interval, caps=caps), self.assertRaises(RuntimeError):
                _walking_intent((0, 0), (0, 0), (1, 1), caps, interval)

    def test_delayed_approach_arrives_and_original_controller_stalls(self):
        def original(position, velocity, target, caps, interval):
            return tuple(max(-cap, min(cap, 1.2 * (goal - actual) - 0.45 * speed))
                         for actual, speed, goal, cap in zip(position, velocity, target, caps))

        start, target, caps = (-94, -44), (-98, -57.65), (0.55, 0.55)
        repaired = sampled_approach(_walking_intent, 0.1, start, target, caps)
        baseline = sampled_approach(original, 0.65, start, target, caps)
        self.assertEqual(repaired[0], 'arrived', repaired)
        self.assertEqual(baseline[0], 'stalled', baseline)
        self.assertGreater(baseline[2], 0.25)

    def test_recovery_with_operator_floor_axis_caps(self):
        result = sampled_approach(_walking_intent, 0.1, (-96.99, -59.05),
                                  (-98, -57.65), (0.30, 0.15))
        self.assertEqual(result[0], 'arrived', result)


if __name__ == '__main__':
    unittest.main()
