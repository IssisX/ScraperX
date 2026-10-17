"""Focused checks of ABI routing and emulator export isolation, not gameplay."""
from pathlib import Path
import base64
import json
import subprocess
import shutil
import tempfile
import unittest
from unittest.mock import MagicMock, patch

from prepare_android_emulator_export import prepare
from run_android_compatibility import PACKAGE, PART_PREFIX, Session, app_window_frame, current_window_focus, read_observations, runtime_errors, screen_observation

ROOT = Path(__file__).resolve().parent.parent


class AndroidObservationTests(unittest.TestCase):
    def test_modern_focus_comes_from_the_top_display(self):
        game = 'Window{abc u0 ' + PACKAGE + '/com.godot.game.GodotAppLauncher}'
        shade = 'Window{def u0 NotificationShade}'
        text = ('Display: mDisplayId=0\n  mCurrentFocus=' + game + '\n'
                'Display: mDisplayId=2 (organized)\n  mCurrentFocus=' + shade + '\n'
                '  mTopFocusedDisplayId=2\n')
        self.assertEqual(current_window_focus(text), shade)
        self.assertEqual(current_window_focus(text.replace('mTopFocusedDisplayId=2',
                                                           'mTopFocusedDisplayId=0')), game)

    def test_missing_or_ambiguous_focus_is_not_inferred_from_visible_game(self):
        for text in (
                'Window #8 Window{abc u0 ' + PACKAGE + '/com.godot.game.GodotAppLauncher}:\n  mTopFocusedDisplayId=0\n',
                'Display: mDisplayId=0\n  mCurrentFocus=game\nDisplay: mDisplayId=2\n  mCurrentFocus=shade\n',
                'Display: mDisplayId=0\n  mCurrentFocus=game\n  mTopFocusedDisplayId=3\n',
                'mCurrentFocus=first\nmCurrentFocus=second\n'):
            with self.subTest(text=text):
                self.assertEqual(current_window_focus(text), '')

    def test_one_display_without_global_id_and_legacy_focus_remain_readable(self):
        for text in ('mCurrentFocus=Window{abc u0 package/activity}\n',
                     'Display: mDisplayId=0\n  mCurrentFocus=Window{abc u0 package/activity}\n'):
            self.assertEqual(current_window_focus(text), 'Window{abc u0 package/activity}')

    def test_focused_game_requires_no_emulator_recovery(self):
        focused = 'mCurrentFocus=Window{3940b90 u0 ' + PACKAGE + '/com.godot.game.GodotAppLauncher}\n'
        with tempfile.TemporaryDirectory() as directory:
            session = Session(Path(directory))
            session.capture = MagicMock()
            session.adb = MagicMock(return_value=focused.encode())
            self.assertFalse(session.prepare_input_focus())
            session.adb.assert_called_once_with('shell', 'dumpsys', 'window', '-a')
            session.capture.assert_not_called()
            self.assertNotIn('startup_external_crash', session.receipt)

    def test_bundled_messenger_crash_recovery_requires_observed_game_focus(self):
        crash = 'mCurrentFocus=Window{9fa8e4 u0 Application Error: com.google.android.apps.messaging}\n'
        focused = 'mCurrentFocus=Window{3940b90 u0 ' + PACKAGE + '/com.godot.game.GodotAppLauncher}\n'
        with tempfile.TemporaryDirectory() as directory:
            session = Session(Path(directory))
            session.capture = MagicMock()
            session.adb = MagicMock(side_effect=[crash.encode(), b'', focused.encode()])
            with patch('run_android_compatibility.time.sleep'):
                self.assertTrue(session.prepare_input_focus())
            self.assertEqual(session.receipt['startup_external_crash']['package'], 'com.google.android.apps.messaging')
            self.assertIn(PACKAGE + '/', session.receipt['startup_input_focus'])
            session.adb.assert_any_call('shell', 'am', 'force-stop', 'com.google.android.apps.messaging')
            self.assertEqual((Path(directory) / 'startup-crash-window.txt').read_text(), crash)
            self.assertEqual((Path(directory) / 'startup-input-focus.txt').read_text(), focused)
            session.capture.assert_called_once_with('startup-crash-dialog')

    def test_game_or_unknown_crash_cannot_be_dismissed_as_emulator_noise(self):
        for package in (PACKAGE, PACKAGE + '.other', 'com.android.systemui'):
            with self.subTest(package=package), tempfile.TemporaryDirectory() as directory:
                session = Session(Path(directory))
                session.capture = MagicMock()
                session.adb = MagicMock(return_value=(
                    'mCurrentFocus=Window{abc u0 Application Error: ' + package + '}\n').encode())
                with self.assertRaisesRegex(RuntimeError, 'blocked by application crash'):
                    session.prepare_input_focus()
                self.assertEqual(session.adb.call_count, 1)
                self.assertNotIn('startup_input_focus', session.receipt)

    def test_messenger_recovery_does_not_pass_without_focus_return(self):
        crash = b'mCurrentFocus=Window{abc u0 Application Error: com.google.android.apps.messaging}\n'
        with tempfile.TemporaryDirectory() as directory:
            session = Session(Path(directory))
            session.capture = MagicMock()
            session.adb = MagicMock(side_effect=[crash, b'', b'mCurrentFocus=null\n'])
            with patch('run_android_compatibility.time.monotonic', side_effect=[0, 0, 1, 21]), \
                 patch('run_android_compatibility.time.sleep'), \
                 self.assertRaisesRegex(RuntimeError, 'no game input focus'):
                session.prepare_input_focus()
            self.assertNotIn('startup_input_focus', session.receipt)

    def test_shader_link_failure_cannot_count_as_compatible_rendering(self):
        error = 'E/godot (3964): ERROR: CanvasShaderGLES3: Program linking failed:'
        self.assertEqual(runtime_errors('I/godot: ordinary startup\n' + error), [error])
        particles = error.replace('CanvasShader', 'ParticlesShader')
        self.assertEqual(runtime_errors(particles), [particles])
        self.assertEqual(runtime_errors('I/godot: OpenGL ES 3.0\nANDROID_COMPAT PASS ready'), [])

    def test_window_origin_comes_from_matching_app_bounds_in_both_android_formats(self):
        title = '  Window #2 Window{abc u0 com.cory.scraperx.chatgpt/com.godot.game.GodotAppLauncher}:\n'
        for line in ['mFrame=[0,441][1768,1767]',
                     'Frames: parent=[0,0][1768,2208] display=[0,0][1768,2208] frame=[0,441][1768,1767] last=[0,441][1768,1767]']:
            text = '  Window #1 Window{other u0 com.android.systemui}:\n mFrame=[0,0][1768,2208]\n' + title + line
            self.assertEqual(app_window_frame(text, [1768, 1326]), (0, 441, 1768, 1767))
        with self.assertRaises(ValueError):
            app_window_frame(title + 'mFrame=[0,0][1768,2208]', [1768, 1326])
        with self.assertRaises(ValueError):
            app_window_frame(title.replace('abc u0 ', 'abc u0 Starting ') + 'mFrame=[0,441][1768,1767]', [1768, 1326])

    def test_display_offset_moves_all_touch_targets_without_changing_native_source(self):
        native = {'controls': {'crouch': {'point': [1635, 970], 'min': [1567, 902], 'max': [1703, 1038]}},
                  'stick': [204, 1122], 'stick_up': [204, 1050], 'look': [1503, 398],
                  'touch_safe_screen': [[0, 0], [1768, 1326]],
                  'menu_buttons': [{'point': [800, 500]}], 'position': [-94, 1.86, -44]}
        state = screen_observation(native, (0, 441, 1768, 1767))
        self.assertEqual(state['controls']['crouch']['point'], [1635, 1411])
        self.assertEqual(state['controls']['crouch']['min'], [1567, 1343])
        self.assertEqual(state['controls']['crouch']['max'], [1703, 1479])
        self.assertEqual(state['stick'], [204, 1563])
        self.assertEqual(state['stick_up'], [204, 1491])
        self.assertEqual(state['look'], [1503, 839])
        self.assertEqual(state['touch_safe_screen'], [[0, 441], [1768, 1767]])
        self.assertEqual(state['menu_buttons'][0]['point'], [800, 941])
        self.assertEqual(state['position'], native['position'])
        self.assertEqual(native['controls']['crouch']['point'], [1635, 970])

    @staticmethod
    def packets(state):
        data = json.dumps(state, ensure_ascii=False).encode()
        chunks = [data[i:i + 512] for i in range(0, len(data), 512)]
        return [PART_PREFIX + json.dumps({'instance': state['instance'],
                'sequence': state['sequence'], 'part': i, 'count': len(chunks),
                'data': base64.b64encode(chunk).decode()}) for i, chunk in enumerate(chunks)]

    def test_large_unicode_observation_survives_android_log_limit(self):
        state = {'instance': 'android-123', 'sequence': 7, 'menu': '→' * 900, 'native_loaded': True}
        lines = self.packets(state)
        self.assertGreater(len(lines), 1)
        self.assertTrue(all(len(line.encode()) < 1023 for line in lines))
        self.assertEqual(list(read_observations('\n'.join(lines))), [state])

    def test_incomplete_observation_cannot_count_as_playable(self):
        state = {'instance': 'android-123', 'sequence': 7, 'data': 'x' * 3000, 'ready': True}
        lines = self.packets(state)
        self.assertEqual(list(read_observations('\n'.join(lines[:-1]))), [])
        self.assertEqual(list(read_observations('\n'.join(lines[:-1] + [lines[-1][:80]]))), [])

    def test_interleaved_instances_and_sequences_never_mix(self):
        states = [{'instance': instance, 'sequence': seq, 'data': str(seq) * 1700}
                  for instance, seq in [('old', 7), ('new', 1), ('old', 8)]]
        packets = [self.packets(state) for state in states]
        lines = [line for parts in zip(*packets) for line in parts]
        self.assertEqual(list(read_observations('\n'.join(lines))), states)


class AndroidExportTests(unittest.TestCase):
    def test_android_x86_does_not_require_optional_desktop_cpu_features(self):
        module = ROOT / 'cmake/ScraperXAndroidAbi.cmake'
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / 'cpu.cmake'
            script.write_text(f'include("{module.as_posix()}")\n'
                              'set(USE_AVX2 ON CACHE BOOL "test default")\n'
                              'scraperx_configure_android_cpu("arm64-v8a")\n'
                              'if(NOT USE_AVX2)\nmessage(FATAL_ERROR "ARM64 options changed")\nendif()\n'
                              'scraperx_configure_android_cpu("x86_64")\n'
                              'foreach(feature AVX AVX2 AVX512 LZCNT TZCNT F16C FMADD)\n'
                              ' if(USE_${feature})\nmessage(FATAL_ERROR "Optional x86 feature enabled")\nendif()\n'
                              'endforeach()\n')
            result = subprocess.run(['cmake', '-P', str(script)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_real_abi_routing_preserves_shipping_library(self):
        module = ROOT / 'cmake/ScraperXAndroidAbi.cmake'
        with tempfile.TemporaryDirectory() as directory:
            script = Path(directory) / 'abi.cmake'
            script.write_text(f'include("{module.as_posix()}")\n'
                              'scraperx_android_extension_directory("arm64-v8a" arm)\n'
                              'scraperx_android_extension_directory("x86_64" emulator)\n'
                              'if(NOT arm STREQUAL "arm64" OR NOT emulator STREQUAL "x86_64")\n'
                              '  message(FATAL_ERROR "Wrong Android library directory")\nendif()\n')
            result = subprocess.run(['cmake', '-P', str(script)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            script.write_text(f'include("{module.as_posix()}")\n'
                              'scraperx_android_extension_directory("armeabi-v7a" output)\n')
            result = subprocess.run(['cmake', '-P', str(script)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Unsupported ScraperX Android ABI', result.stderr)

    def test_emulator_export_uses_real_native_abi_and_normal_main(self):
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            shipping = (ROOT / 'godot/export_presets.cfg').read_text()
            main = (ROOT / 'godot/project.godot').read_text()
            (project / 'export_presets.cfg').write_text(shipping)
            (project / 'project.godot').write_text(main)
            for path in ('presentation/ui/touch_controls.gd', 'presentation/main.gd'):
                target = project / path
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / 'godot' / path, target)
            prepare(project)
            preset = (project / 'export_presets.cfg').read_text()
            config = (project / 'project.godot').read_text()
            self.assertIn('architectures/x86_64=true', preset)
            self.assertIn('architectures/arm64-v8a=false', preset)
            self.assertIn('run/main_scene="res://main.tscn"', config)
            self.assertIn('AndroidCompatibilityProbe=', config)
            self.assertEqual((project / 'presentation/ui/touch_controls.gd').read_text().count('.record_touch(event, self)'), 3)
            self.assertIn('.record_step(world_move,', (project / 'presentation/main.gd').read_text())
            self.assertEqual((ROOT / 'godot/export_presets.cfg').read_text(), shipping)
            self.assertEqual((ROOT / 'godot/project.godot').read_text(), main)
            with self.assertRaises(ValueError):
                prepare(project)


if __name__ == '__main__':
    unittest.main()
