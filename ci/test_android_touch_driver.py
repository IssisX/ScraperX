"""Driver lifecycle checks; these do not establish Android gameplay."""
from copy import deepcopy
import unittest
from unittest.mock import MagicMock, patch
from pathlib import Path
import tempfile

from android_touch_driver import TouchDriver


class SessionFixture:
    def __init__(self, *, fail_move=False, fail_hold=False, fail_up=False, drop_move=False):
        self.receipt = {}
        self.commands = []
        self.fail_move, self.fail_hold, self.fail_up = fail_move, fail_hold, fail_up
        self.drop_move = drop_move
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
        if action == 'MOVE' and self.drop_move:
            return b''
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
    def test_response_hold_releases_when_real_predicate_is_met(self):
        session = SessionFixture()
        state = TouchDriver(session, 36).swipe((10, 20), (10, 5), 1200,
                                              until=lambda s: s['tick'] >= 103)
        self.assertEqual(state['tick'], 103)
        self.assertFalse(session.state['input_trace']['owned'])

    def test_response_hold_does_not_pass_when_required_effect_is_absent(self):
        session = SessionFixture()
        with self.assertRaisesRegex(RuntimeError, 'predicate was never met'):
            TouchDriver(session, 36).swipe((10, 20), (10, 5), 1200, until=lambda s: False)
        self.assertFalse(session.state['input_trace']['owned'])

    def test_api24_forwarded_socket_is_not_ready_until_protocol_responds(self):
        session = SessionFixture()
        session.adb = MagicMock(return_value=b'12345\n')
        first, ready = MagicMock(), MagicMock()
        first.makefile.return_value.readline.return_value = b''
        ready.makefile.return_value.readline.return_value = b'OK:build.version.sdk monkey.version\n'
        process = MagicMock()
        process.poll.return_value = None
        with tempfile.TemporaryDirectory() as directory:
            session.output = Path(directory)
            driver = TouchDriver(session, 24)
            with patch('android_touch_driver.socket.create_connection', side_effect=[first, ready]), \
                 patch('android_touch_driver.subprocess.Popen', return_value=process), \
                 patch('android_touch_driver.time.sleep'):
                driver._legacy()
                first.close.assert_called_once()
                ready.makefile.return_value.write.assert_called_once_with(b'listvar\n')
                self.assertTrue(session.receipt['touch_server_ready'].startswith('OK:'))
                driver.close()

    def test_stationary_button_hold_accepts_down_when_platform_omits_move(self):
        session = SessionFixture(drop_move=True)
        TouchDriver(session, 36).swipe((10, 20), (10, 20), 650)
        self.assertEqual(session.state['input_trace']['events'], 2)
        self.assertFalse(session.state['input_trace']['owned'])

    def test_small_drag_cannot_pass_with_only_down_and_advancing_native_ticks(self):
        # Captured tall API36 fifth pulse: Android withheld this 20.4px MOVE.
        session = SessionFixture(drop_move=True)
        with self.assertRaisesRegex(RuntimeError, 'held Android gesture reaches native stepping'):
            TouchDriver(session, 36).swipe((65, 385), (61, 365), 100)
        self.assertEqual([c[-3:] for c in session.commands],
                         [('DOWN', 65, 385), ('MOVE', 61, 365), ('UP', 61, 365)])
        self.assertFalse(session.state['input_trace']['owned'])

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
