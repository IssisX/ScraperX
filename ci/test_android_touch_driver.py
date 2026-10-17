"""Driver lifecycle checks; these do not establish Android gameplay."""
from copy import deepcopy
import unittest

from android_touch_driver import TouchDriver


class SessionFixture:
    def __init__(self, *, fail_move=False, fail_hold=False, fail_up=False):
        self.receipt = {}
        self.commands = []
        self.fail_move, self.fail_hold, self.fail_up = fail_move, fail_hold, fail_up
        self.state = {'instance': 'one', 'ready': True, 'paused': False,
                      'tick': 100, 'fixed_step_s': 1 / 90,
                      'input_trace': {'events': 0, 'owned': False}}

    def snapshot(self):
        return deepcopy(self.state)

    def adb(self, *args):
        self.commands.append(args)
        action = args[-3]
        if action == 'MOVE' and self.fail_move:
            raise RuntimeError('injected move failure')
        if action == 'UP' and self.fail_up:
            raise RuntimeError('injected release failure')
        self.state['input_trace']['events'] += 1
        self.state['input_trace']['owned'] = action not in ('UP', 'CANCEL')
        return b''

    def wait(self, label, predicate, **kwargs):
        if 'requested native ticks' in label and self.fail_hold:
            raise RuntimeError('injected hold failure')
        # A deterministic observer fixture, not a sleep or fake Android proof.
        for _ in range(150):
            self.state['tick'] += 1
            if predicate(self.state):
                return self.snapshot()
        raise RuntimeError('fixture predicate was never met: ' + label)


class TouchDriverTests(unittest.TestCase):
    def test_real_release_after_observed_native_duration(self):
        session = SessionFixture()
        state = TouchDriver(session, 36).swipe((10, 20), (10, 5), 650)
        self.assertEqual([c[-3] for c in session.commands], ['DOWN', 'MOVE', 'UP'])
        self.assertGreaterEqual(state['tick'], 160)
        self.assertFalse(session.state['input_trace']['owned'])

    def test_move_and_observation_failures_still_release(self):
        for failure in ('fail_move', 'fail_hold'):
            with self.subTest(failure=failure):
                session = SessionFixture(**{failure: True})
                with self.assertRaisesRegex(RuntimeError, 'injected'):
                    TouchDriver(session, 35).swipe((10, 20), (10, 5), 650)
                self.assertEqual(session.commands[-1][-3], 'UP')
                self.assertFalse(session.state['input_trace']['owned'])

    def test_failed_release_attempts_cancel_and_remains_failure(self):
        session = SessionFixture(fail_up=True)
        with self.assertRaisesRegex(RuntimeError, 'release failure'):
            TouchDriver(session, 36).swipe((10, 20), (10, 5), 650)
        self.assertEqual([c[-3] for c in session.commands][-2:], ['UP', 'CANCEL'])
        self.assertFalse(session.state['input_trace']['owned'])
        self.assertEqual(len(session.receipt['touch_cleanup_errors']), 1)


if __name__ == '__main__':
    unittest.main()
