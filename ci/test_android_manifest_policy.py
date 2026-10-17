"""Exact binary manifest scope for Android font-size lifecycle handling."""
import struct
import unittest
import tempfile
from pathlib import Path
import zipfile

from test_build_info import apk, SHA


def activity_manifest(*, utf8=True, package='com.cory.scraperx.chatgpt',
                      mask=0x3ff4, value_type=0x11, duplicate=False, missing=False):
    strings = ['package', 'versionCode', 'name', 'configChanges', 'manifest',
               'application', 'activity', package, 'com.godot.game.GodotApp',
               'com.godot.ProcessPhoenix', 'http://schemas.android.com/apk/res/android']
    contents, offsets = bytearray(), []
    for text in strings:
        offsets.append(len(contents))
        if utf8:
            encoded = text.encode()
            contents += bytes((len(text), len(encoded))) + encoded + b'\0'
        else:
            contents += struct.pack('<H', len(text)) + text.encode('utf-16le') + b'\0\0'
    contents += b'\0' * (-len(contents) % 4)
    pool_start = 28 + len(strings) * 4
    pool = struct.pack('<HHIIIIII', 1, 28, pool_start + len(contents), len(strings),
                       0, 0x100 if utf8 else 0, pool_start, 0)
    pool += struct.pack('<' + 'I' * len(offsets), *offsets) + contents
    resources = struct.pack('<HHIIIII', 0x180, 8, 24, 0, apk.VERSION_CODE_ID,
                            0x01010003, 0x0101001f)

    def start(name, attrs):
        node = bytearray(36 + 20 * len(attrs))
        struct.pack_into('<HHIIIIIHHH', node, 0, 0x102, 16, len(node),
                         1, 0xffffffff, 0xffffffff, name, 20, 20, len(attrs))
        for index, (key, kind, value) in enumerate(attrs):
            struct.pack_into('<IIIHBBI', node, 36 + 20 * index,
                             0xffffffff if key == 0 else 10, key,
                             0xffffffff, 8, 0, kind, value)
        return node

    def end(name):
        return struct.pack('<HHIIIII', 0x103, 16, 24, 1, 0xffffffff, 0xffffffff, name)

    attrs = [(2, 3, 8)] + ([] if missing else [(3, value_type, mask)])
    target = start(6, attrs) + end(6)
    body = pool + resources + start(4, [(0, 3, 7), (1, 0x10, 12)]) + start(5, [])
    body += target + (target if duplicate else b'')
    body += start(6, [(2, 3, 9), (3, 0x11, 0)]) + end(6) + end(5) + end(4)
    return struct.pack('<HHI', 3, 8, 8 + len(body)) + body


class ManifestFontPolicyTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.source = Path(self.temp.name) / 'input.apk'
        self.target = Path(self.temp.name) / 'output.apk'
        with zipfile.ZipFile(self.source, 'w') as archive:
            archive.writestr('AndroidManifest.xml', activity_manifest())
            archive.writestr('lib/arm64-v8a/libscraperx_native.so', b'preserved native bytes')

    def replace_manifest(self, data):
        replacement = self.source.with_suffix('.next.apk')
        with zipfile.ZipFile(self.source) as original, zipfile.ZipFile(replacement, 'w') as out:
            for entry in original.infolist():
                out.writestr(entry, data if entry.filename == 'AndroidManifest.xml' else original.read(entry))
        replacement.replace(self.source)

    def test_utf8_and_utf16_change_only_authorized_integer_bytes(self):
        for utf8 in (True, False):
            with self.subTest(utf8=utf8):
                original = activity_manifest(utf8=utf8)
                self.replace_manifest(original)
                config, _ = apk.main_activity_config_location(original)
                apk.prepare(self.source, self.target, now=apk.EPOCH + 80,
                            source_sha=SHA, handle_font_scale=True)
                with zipfile.ZipFile(self.target) as archive:
                    actual = archive.read('AndroidManifest.xml')
                expected = bytearray(original)
                version, _ = apk.version_location(original)
                struct.pack_into('<I', expected, version + 16, 80)
                struct.pack_into('<I', expected, config + 16, 0x40003ff4)
                self.assertEqual(actual, expected)
                apk.verify_prepared(self.source, self.target, 80, SHA, handle_font_scale=True)

    def test_main_activity_and_template_must_be_unambiguous(self):
        for kwargs in ({'package': 'com.cory.scraperx.claude'}, {'duplicate': True},
                       {'missing': True}, {'value_type': 3, 'mask': 8}, {'mask': 0x3ff7}):
            with self.subTest(kwargs=kwargs), self.assertRaises(ValueError):
                apk.main_activity_config_location(activity_manifest(**kwargs))

    def test_opt_in_preserves_default_and_is_idempotent(self):
        apk.prepare(self.source, self.target, now=apk.EPOCH + 80)
        with zipfile.ZipFile(self.target) as archive:
            self.assertEqual(apk.main_activity_config_location(archive.read('AndroidManifest.xml'))[1], 0x3ff4)
        self.replace_manifest(activity_manifest(mask=0x40003ff4))
        apk.prepare(self.source, self.target, now=apk.EPOCH + 80, handle_font_scale=True)
        apk.verify_prepared(self.source, self.target, 80, handle_font_scale=True)

    def test_final_verifier_rejects_other_manifest_changes(self):
        apk.prepare(self.source, self.target, now=apk.EPOCH + 80, handle_font_scale=True)
        changed = self.target.with_suffix('.changed.apk')
        with zipfile.ZipFile(self.target) as original, zipfile.ZipFile(changed, 'w') as out:
            for entry in original.infolist():
                data = original.read(entry)
                if entry.filename == 'AndroidManifest.xml':
                    data = bytearray(data)
                    target, _ = apk.main_activity_config_location(data)
                    struct.pack_into('<I', data, target + 16, 0x40003ff7)
                out.writestr(entry, data)
        with self.assertRaisesRegex(ValueError, 'beyond authorized'):
            apk.verify_prepared(self.source, changed, 80, handle_font_scale=True)

if __name__ == '__main__':
    unittest.main()
