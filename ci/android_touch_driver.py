"""Ordinary Android touch held across observed native ticks, with release cleanup."""
import math
import socket
import subprocess
import time


class TouchDriver:
    def __init__(self, session, api):
        self.session, self.api = session, api
        self.monkey = self.connection = self.stream = self.log = None
        self.port = None

    def _legacy(self):
        # API24's shell has no motionevent command. Its stock Monkey network
        # protocol can deliver separate ordinary DOWN/MOVE/UP events instead.
        self.port = self.session.adb('forward', 'tcp:0', 'tcp:1080').decode().strip()
        self.log = (self.session.output / 'touch-monkey.log').open('wb')
        self.monkey = subprocess.Popen(
            ['adb', 'shell', 'monkey', '--port', '1080'], stdout=self.log, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + 20
        while True:
            try:
                self.connection = socket.create_connection(('127.0.0.1', int(self.port)), timeout=5)
                self.connection.settimeout(30)
                self.stream = self.connection.makefile('rwb', buffering=0)
                return
            except OSError:
                if self.monkey.poll() is not None or time.monotonic() >= deadline:
                    raise RuntimeError('API24 touch server unavailable; see touch-monkey.log')
                time.sleep(0.2)

    def pointer(self, action, point):
        x, y = map(round, point)
        if self.api >= 35:
            self.session.adb('shell', 'input', 'touchscreen', 'motionevent', action, x, y)
            return
        if self.stream is None:
            self._legacy()
        command = f'touch {action.lower()} {x} {y}'
        self.stream.write((command + '\n').encode())
        response = self.stream.readline(1024).decode().strip()
        self.session.receipt.setdefault('touch_protocol', []).append(
            {'command': command, 'response': response})
        if response != 'OK':
            raise RuntimeError(f'API24 touch command {command!r} expected OK, got {response!r}')

    def swipe(self, start, end, duration_ms):
        before = self.session.snapshot()
        if before is None or not before['ready'] or before['paused']:
            raise RuntimeError('Held Android gesture requires an observed active native world')
        active = False
        release_after = before
        try:
            # An injection failure can occur after delivery, so release even
            # when DOWN raises. Never kill an unfinished swipe to fake a hold.
            active = True
            self.pointer('DOWN', start)
            self.pointer('MOVE', end)
            # Android/Godot may omit a stationary MOVE. A held machine button
            # needs its real DOWN; a stick/look drag needs the MOVE as well.
            event_count = 1 if tuple(map(round, start)) == tuple(map(round, end)) else 2
            accepted = self.session.wait('held Android gesture reaches native stepping',
                lambda s: s['instance'] == before['instance'] and
                s.get('input_trace', {}).get('events', 0) >= before.get('input_trace', {}).get('events', 0) + event_count and
                s.get('input_trace', {}).get('owned', False) and
                s['tick'] > before['tick'], after=before, timeout=45)
            release_after = accepted
            # Wall time on a software renderer is unrelated to game time.
            # Keep a real finger down for the requested native duration.
            ticks = max(1, math.ceil(duration_ms / 1000 / accepted['fixed_step_s']))
            held = self.session.wait('held Android gesture spans requested native ticks',
                lambda s: s['instance'] == before['instance'] and s['tick'] >= accepted['tick'] + ticks,
                after=accepted, timeout=90)
            release_after = held
            return held
        finally:
            if active:
                try:
                    self.pointer('UP', end)
                except Exception as error:
                    self.session.receipt.setdefault('touch_cleanup_errors', []).append(str(error))
                    if self.api >= 35:
                        self.pointer('CANCEL', end)
                    raise
                self.session.wait('Android gesture release clears actual touch ownership',
                    lambda s: s.get('input_trace', {}).get('owned', True) is False and
                    s.get('input_trace', {}).get('events', 0) > release_after.get('input_trace', {}).get('events', 0),
                    after=release_after,
                    timeout=45)

    def close(self):
        try:
            if self.stream:
                self.stream.write(b'quit\n')
        finally:
            if self.stream:
                self.stream.close()
            if self.connection:
                self.connection.close()
            if self.monkey:
                try:
                    self.monkey.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    self.monkey.terminate()
                    self.monkey.wait(timeout=5)
            if self.log:
                self.log.close()
            if self.port:
                self.session.adb('forward', '--remove', 'tcp:' + self.port)
