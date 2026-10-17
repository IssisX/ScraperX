"""Real Android system shade and physical rotation checks, not FPS evidence."""
import re
import time


def rotation_from_dump(text):
    match = re.search(r'\bmRotation=(ROTATION_)?(\d+)\b', text)
    if match is None:
        raise ValueError('Android window dump has no current display rotation')
    value = int(match[2])
    if match[1]:
        if value not in (0, 90, 180, 270):
            raise ValueError(f'Unexpected Android rotation angle: {value}')
        return value // 90
    if value not in range(4):
        raise ValueError(f'Unexpected Android rotation index: {value}')
    return value


def _rotation(session, label, wanted=None):
    deadline = time.monotonic() + 20
    actual = None
    while time.monotonic() < deadline:
        text = session.adb('shell', 'dumpsys', 'window', 'displays').decode()
        (session.output / (label + '-display.txt')).write_text(text)
        actual = rotation_from_dump(text)
        if wanted is None or actual == wanted:
            session.receipt.setdefault('display_rotations', []).append(
                {'label': label, 'rotation': actual})
            return actual
        time.sleep(0.5)
    raise RuntimeError(f'Android {label}: expected display rotation {wanted}, observed {actual}')


def exercise_display(session, state):
    """Keep the native world through system UI and a real 180-degree turn."""
    instance = state['instance']
    session.adb('shell', 'cmd', 'statusbar', 'expand-notifications')
    try:
        state = session.wait('Android notification shade pauses the retained native world',
                             lambda s: s['paused'] and s['instance'] == instance,
                             after=state, timeout=30)
        frozen = state
        state = session.wait('native tick stays frozen beneath Android system shade',
                             lambda s: s['paused'] and s['sequence'] >= frozen['sequence'] + 3,
                             after=frozen, timeout=30)
        if state['tick'] != frozen['tick']:
            raise RuntimeError('Native physics continued beneath Android system shade')
        session.capture('system-shade')
    finally:
        session.adb('shell', 'cmd', 'statusbar', 'collapse')
    state = session.wait('collapsing Android system shade retains the game pause menu',
                         lambda s: s['instance'] == instance and s['paused'] and s['menu_buttons'],
                         after=state)
    state = session.resume(state)

    original = _rotation(session, 'rotation-original')
    performed = 0
    before = state
    try:
        for _ in range(2):
            session.adb('emu', 'rotate')  # Emulator console: clockwise90 degrees.
            performed += 1
        _rotation(session, 'rotation-opposite', (original + 2) % 4)
        session.frame_key = None
        state = session.wait('physical device rotation retains native world and interactive controls',
                             lambda s: s['instance'] == instance and s['ready'] and
                             (s['paused'] or {'crouch', 'jump', 'pause'} <= s['controls'].keys()),
                             after=before)
        if state['paused']:
            state = session.resume(state)
        session.capture('opposite-landscape')
        session.button(state, 'crouch')
        state = session.wait('ordinary touch works after physical rotation',
                             lambda s: s['crouched'], after=state)
        session.button(state, 'crouch')
        state = session.wait('rotated touch can return to standing',
                             lambda s: not s['crouched'], after=state)
    finally:
        if performed:
            for _ in range(4 - performed):
                session.adb('emu', 'rotate')
            _rotation(session, 'rotation-restored', original)
        session.frame_key = None
    state = session.wait('restored device rotation retains native instance',
                         lambda s: s['instance'] == instance and s['ready'], after=state)
    return session.resume(state) if state['paused'] else state
