#!/usr/bin/env python3
"""Exercise the APK transformation, including downgrade and payload preservation."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('prepare_apk', ROOT / '.github/actions/stable-apk/prepare-apk.py')
apk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(apk)


def manifest(code=1):
    # Binary XML resource map plus a manifest start element with versionCode.
    resource_map = struct.pack('<HHII', 0x180, 8, 12, apk.VERSION_CODE_ID)
    attribute = struct.pack('<IIIHBBI', 0, 0, 0xFFFFFFFF, 8, 0, 0x10, code)
    element = (struct.pack('<HHIII', 0x102, 16, 56, 1, 0xFFFFFFFF) +
               struct.pack('<IIHHHHHH', 0xFFFFFFFF, 0, 20, 20, 1, 0, 0, 0) + attribute)
    payload = resource_map + element
    return struct.pack('<HHI', 3, 8, 8+len(payload)) + payload


class VersionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.source, self.target = self.root/'in.apk', self.root/'out.apk'

    def tearDown(self):
        self.temp.cleanup()

    def source_apk(self, code=1):
        with zipfile.ZipFile(self.source, 'w') as archive:
            archive.writestr('AndroidManifest.xml', manifest(code))
            archive.writestr('lib/arm64-v8a/libscraperx_native.so', b'native game payload')
            archive.writestr('assets/game.pck', b'game content')
            archive.writestr('META-INF/CERT.RSA', b'old signature')
            archive.writestr('META-INF/CERT.SF', b'old signature')
            archive.writestr('META-INF/MANIFEST.MF', b'old signature')
            archive.writestr('META-INF/services/game', b'keep other metadata')

    def test_increases_from_the_previous_published_version(self):
        self.source_apk(1)
        result = apk.prepare(self.source, self.target, 500, apk.EPOCH+10)
        self.assertEqual(result['version_code'], 501)
        with zipfile.ZipFile(self.target) as archive:
            self.assertEqual(apk.version_location(archive.read('AndroidManifest.xml'))[1], 501)

    def test_keeps_a_higher_source_version(self):
        self.source_apk(900)
        self.assertEqual(apk.prepare(self.source, self.target, 1, apk.EPOCH+10)['version_code'], 900)

    def test_independent_build_and_publisher_use_a_time_floor(self):
        self.source_apk(1)
        self.assertEqual(apk.prepare(self.source, self.target, 1, apk.EPOCH+1000)['version_code'], 1000)

    def test_preserves_game_payload_and_non_signature_metadata(self):
        self.source_apk()
        apk.prepare(self.source, self.target, 1, apk.EPOCH+10)
        with zipfile.ZipFile(self.source) as before, zipfile.ZipFile(self.target) as after:
            for name in ('lib/arm64-v8a/libscraperx_native.so', 'assets/game.pck', 'META-INF/services/game'):
                self.assertEqual(before.read(name), after.read(name))
            self.assertNotIn('META-INF/CERT.RSA', after.namelist())
            self.assertNotIn('META-INF/CERT.SF', after.namelist())
            self.assertNotIn('META-INF/MANIFEST.MF', after.namelist())

    def test_invalid_manifest_is_rejected_before_output(self):
        with zipfile.ZipFile(self.source, 'w') as archive:
            archive.writestr('AndroidManifest.xml', b'not binary XML')
        with self.assertRaises(ValueError):
            apk.prepare(self.source, self.target)
        self.assertFalse(self.target.exists())

    def test_version_overflow_is_rejected(self):
        self.source_apk()
        with self.assertRaises(ValueError):
            apk.prepare(self.source, self.target, apk.LIMIT)


if __name__ == '__main__':
    unittest.main()
