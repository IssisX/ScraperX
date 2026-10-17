"""Sampled steering checks; the model is not Android/Jolt gameplay proof."""
import math
import unittest

from android_freight_route import _walking_intent


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


class AndroidRouteNavigationTests(unittest.TestCase):
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
