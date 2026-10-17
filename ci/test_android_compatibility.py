"""Focused checks of ABI routing and emulator export isolation, not gameplay."""
from pathlib import Path
import subprocess
import tempfile
import unittest

from prepare_android_emulator_export import prepare

ROOT = Path(__file__).resolve().parent.parent


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
            prepare(project)
            preset = (project / 'export_presets.cfg').read_text()
            config = (project / 'project.godot').read_text()
            self.assertIn('architectures/x86_64=true', preset)
            self.assertIn('architectures/arm64-v8a=false', preset)
            self.assertIn('run/main_scene="res://main.tscn"', config)
            self.assertIn('AndroidCompatibilityProbe=', config)
            self.assertEqual((ROOT / 'godot/export_presets.cfg').read_text(), shipping)
            self.assertEqual((ROOT / 'godot/project.godot').read_text(), main)
            with self.assertRaises(ValueError):
                prepare(project)


if __name__ == '__main__':
    unittest.main()
