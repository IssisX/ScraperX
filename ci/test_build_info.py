"""Focused final APK identity and payload regression tests."""
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile

ROOT = Path(__file__).resolve().parents[1]
def module(name, path):
    spec = importlib.util.spec_from_file_location(name, ROOT / path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result

apk = module('prepare_apk', '.github/actions/stable-apk/prepare-apk.py')
SHA = 'a7fbce49878830a4991d1c14498f66b45db4d045'

def manifest():
    resources = struct.pack('<HHII', 0x180, 8, 12, apk.VERSION_CODE_ID)
    element = bytearray(56)
    struct.pack_into('<HHI', element, 0, 0x102, 16, 56)
    struct.pack_into('<HHH', element, 24, 20, 20, 1)
    struct.pack_into('<IIIHBBI', element, 36, 0, 0, 0xffffffff, 8, 0, 0x10, 12)
    return struct.pack('<HHI', 3, 8, 8 + len(resources) + len(element)) + resources + element

class FinalIdentityTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.source = Path(self.temp.name) / 'input.apk'
        self.target = Path(self.temp.name) / 'output.apk'
        with zipfile.ZipFile(self.source, 'w') as out:
            out.writestr('AndroidManifest.xml', manifest())
            out.writestr('lib/arm64-v8a/libscraperx_native.so', b'actual native bytes\x00\xff')
            out.writestr('assets/presentation/main.gdc', b'compiled game script')
            out.writestr('META-INF/OLD.SF', b'old signature')

    def test_opt_in_embeds_actual_final_code_and_source(self):
        version = apk.prepare(self.source, self.target, previous=100, now=apk.EPOCH + 80, source_sha=SHA)
        self.assertEqual(version['version_code'], 101)
        with zipfile.ZipFile(self.target) as out:
            identity = json.loads(out.read('assets/build_info.json'))
            self.assertEqual(identity['version_code'], 101)
            self.assertEqual(identity['source_sha'], SHA)
            self.assertEqual(out.read('lib/arm64-v8a/libscraperx_native.so'), b'actual native bytes\x00\xff')
            self.assertNotIn('META-INF/OLD.SF', out.namelist())
        apk.verify_prepared(self.source, self.target, 101, SHA)

    def test_other_models_have_no_injected_metadata(self):
        apk.prepare(self.source, self.target, now=apk.EPOCH + 80)
        with zipfile.ZipFile(self.target) as out:
            self.assertNotIn('assets/build_info.json', out.namelist())
        apk.verify_prepared(self.source, self.target, 80)

    def test_verifier_rejects_wrong_identity_or_modified_native_bytes(self):
        apk.prepare(self.source, self.target, now=apk.EPOCH + 80, source_sha=SHA)
        for name, payload in [('assets/build_info.json', json.dumps({'version_code': 79, 'source_sha': SHA})),
                              ('assets/build_info.json', json.dumps({'version_code': 80, 'source_sha': 'b' * 40})),
                              ('lib/arm64-v8a/libscraperx_native.so', b'changed')]:
            with self.subTest(name=name, payload=payload):
                changed = Path(self.temp.name) / 'changed.apk'
                with zipfile.ZipFile(self.target) as inp, zipfile.ZipFile(changed, 'w') as out:
                    for entry in inp.infolist():
                        out.writestr(entry, payload if entry.filename == name else inp.read(entry))
                with self.assertRaises(ValueError):
                    apk.verify_prepared(self.source, changed, 80, SHA)

    def test_verifier_rejects_mismatched_final_manifest(self):
        apk.prepare(self.source, self.target, now=apk.EPOCH + 80, source_sha=SHA)
        with self.assertRaises(ValueError):
            apk.verify_prepared(self.source, self.target, 81, SHA)

    def test_stale_export_metadata_is_replaced_once(self):
        with zipfile.ZipFile(self.source, 'a') as out:
            out.writestr('assets/build_info.json', '{"source_sha": "stale", "version_code": null}')
        apk.prepare(self.source, self.target, now=apk.EPOCH + 80, source_sha=SHA)
        with zipfile.ZipFile(self.target) as out:
            self.assertEqual(out.namelist().count('assets/build_info.json'), 1)
        apk.verify_prepared(self.source, self.target, 80, SHA)

    def test_bad_source_is_rejected(self):
        with self.assertRaises(ValueError):
            apk.prepare(self.source, self.target, source_sha='unknown')


class ExportIdentityTest(unittest.TestCase):
    def test_unique_source_version_and_unsigned_code_not_claimed(self):
        exporter = module('prepare_build_info', 'ci/prepare_chatgpt_build_info.py')
        with tempfile.TemporaryDirectory() as temporary:
            project = Path(temporary)
            preset = project / 'export_presets.cfg'
            preset.write_text('[preset.0.options]\nversion/code=123\nversion/name="old"\n')
            identity = exporter.prepare(project, SHA, '981', '2')
            self.assertEqual(identity, {'source_sha': SHA, 'version_code': None})
            self.assertIn('version/name="0.1.0-chatgpt.a7fbce498788.981.2"', preset.read_text())
            self.assertIn('version/code=123', preset.read_text())
            self.assertEqual(json.loads((project / 'build_info.json').read_text()), identity)

if __name__ == '__main__':
    unittest.main()
