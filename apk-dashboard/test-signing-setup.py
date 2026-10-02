#!/usr/bin/env python3
"""Real crypto, fixture GitHub boundary; never touches live secrets."""
import base64
import json
import os
from pathlib import Path
import subprocess
import tempfile
import textwrap
import unittest

SCRIPT = Path(__file__).with_name("setup-stable-signing-termux.sh")
MOCK_GH = '''#!/usr/bin/env python3
import json, os, pathlib, sys
p = pathlib.Path(os.environ["TEST_GH_STATE"])
s = json.loads(p.read_text())
a = sys.argv[1:]
if s.get("fail") == " ".join(a[:2]):
    sys.exit(1)
if a[:2] == ["auth", "status"]:
    sys.exit(0)
elif a[:2] == ["secret", "list"]:
    print(json.dumps([{"name": n} for n in s.get("secrets", {})]))
elif a[0] == "api":
    print(json.dumps([{"tag_name": "apk-latest"}] if s.get("release") else []))
elif a[:2] == ["release", "download"]:
    d = pathlib.Path(a[a.index("--dir") + 1])
    (d / "manifest.json").write_text(json.dumps({"signing_certificate_sha256": s["cert"]}))
elif a[:2] == ["secret", "set"]:
    s.setdefault("secrets", {})[a[2]] = sys.stdin.read()
    s.setdefault("writes", []).append(a[2])
    p.write_text(json.dumps(s))
elif a[:2] == ["workflow", "run"]:
    s["dispatches"] = s.get("dispatches", 0) + 1
    p.write_text(json.dumps(s))
else:
    sys.exit("Unexpected gh call: " + repr(a))
'''


class SigningSetupTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.tmp.name)
        cls.bin = cls.root / "bin"
        cls.bin.mkdir()
        (cls.bin / "gh").write_text(MOCK_GH)
        (cls.bin / "gh").chmod(0o755)
        cls.seed = cls.root / "seed"
        cls.seed.mkdir()
        state = cls.seed / "state.json"
        state.write_text('{}')
        env = cls.environment(state, cls.seed / "signing")
        result = subprocess.run(["bash", str(SCRIPT)], env=env, capture_output=True, text=True)
        if result.returncode:
            raise RuntimeError(result.stderr)
        cls.key = cls.seed / "signing/scraperx-dashboard.p12"
        cls.password = cls.seed / "signing/password"
        cls.cert = next(line.split(": ", 1)[1] for line in result.stdout.splitlines()
                        if line.startswith("Signer SHA-256:"))

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    @classmethod
    def environment(cls, state, signing):
        return dict(os.environ, PATH=str(cls.bin) + os.pathsep + os.environ["PATH"],
                    TEST_GH_STATE=str(state), SCRAPERX_SIGNING_DIR=str(signing))

    def setUp(self):
        self.case = Path(tempfile.mkdtemp(dir=self.root))
        self.state = self.case / "state.json"
        self.signing = self.case / "signing"

    def run_setup(self, state, *args, backup=False):
        self.state.write_text(json.dumps(state))
        if backup:
            self.signing.mkdir(exist_ok=True)
            (self.signing / self.key.name).write_bytes(self.key.read_bytes())
            (self.signing / "password").write_bytes(self.password.read_bytes())
        result = subprocess.run(["bash", str(SCRIPT), *map(str, args)],
                                env=self.environment(self.state, self.signing),
                                capture_output=True, text=True)
        return result, json.loads(self.state.read_text())

    def assert_no_write(self, result, state):
        self.assertNotEqual(result.returncode, 0)
        self.assertEqual(state.get("writes", []), [])
        self.assertEqual(state.get("dispatches", 0), 0)

    def test_first_setup_backs_up_key_before_upload(self):
        s = json.loads((self.seed / "state.json").read_text())
        self.assertEqual(base64.b64decode(s["secrets"]["SCRAPERX_APK_KEYSTORE_B64"]),
                         self.key.read_bytes())
        self.assertEqual(s["secrets"]["SCRAPERX_APK_KEYSTORE_PASSWORD"], self.password.read_text())
        self.assertEqual(self.key.stat().st_mode & 0o777, 0o600)
        self.assertEqual(self.password.stat().st_mode & 0o777, 0o600)
        self.assertEqual(self.key.parent.stat().st_mode & 0o777, 0o700)

    def test_repeat_setup_keeps_same_private_key(self):
        s = dict(release=True, cert=self.cert, secrets={"SCRAPERX_APK_KEYSTORE_B64": "existing"})
        r, s = self.run_setup(s, backup=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(base64.b64decode(s["secrets"]["SCRAPERX_APK_KEYSTORE_B64"]), self.key.read_bytes())
        self.assertEqual(s["dispatches"], 1)

    def test_existing_secrets_without_backup_never_rotate(self):
        for secrets in ({"SCRAPERX_APK_KEYSTORE_B64": "existing"},
                        {"SCRAPERX_APK_KEYSTORE_PASSWORD": "existing"}):
            with self.subTest(secrets=list(secrets)):
                r, s = self.run_setup(dict(secrets=secrets))
                self.assert_no_write(r, s)
                self.assertFalse(self.signing.exists())

    def test_published_identity_without_secrets_blocks_new_key(self):
        r, s = self.run_setup(dict(release=True, cert=self.cert))
        self.assert_no_write(r, s)

    def test_matching_backup_restores_both_secrets(self):
        r, s = self.run_setup(dict(release=True, cert=self.cert),
                              "--restore", self.key, self.password)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertEqual(len(s["writes"]), 2)
        self.assertEqual(s["dispatches"], 1)

    def test_wrong_certificate_cannot_replace_published_identity(self):
        r, s = self.run_setup(dict(release=True, cert="0" * 64),
                              "--restore", self.key, self.password)
        self.assert_no_write(r, s)

    def test_lookup_failure_never_creates_or_uploads_key(self):
        for failure in ("secret list", "api repos/IssisX/ScraperX/releases?per_page=100", "release download"):
            with self.subTest(failure=failure):
                r, s = self.run_setup(dict(fail=failure, release=True, cert=self.cert))
                self.assert_no_write(r, s)
                self.assertFalse(self.signing.exists())

    def test_incomplete_backup_cannot_be_overwritten(self):
        self.signing.mkdir()
        (self.signing / "password").write_text("keep me")
        r, s = self.run_setup({})
        self.assert_no_write(r, s)
        self.assertEqual((self.signing / "password").read_text(), "keep me")

    def test_malformed_published_certificate_fails_closed(self):
        for cert in (None, "", "invalid"):
            with self.subTest(cert=cert):
                r, s = self.run_setup(dict(release=True, cert=cert))
                self.assert_no_write(r, s)
                self.assertFalse(self.signing.exists())

    def test_actual_publisher_preflight_accepts_only_same_key(self):
        workflow = SCRIPT.parents[1] / ".github/workflows/apk-dashboard.yml"
        text = workflow.read_text()
        snippet = text[text.index("          keytool -exportcert"):text.index('          SIGNING_CERT=""')]
        for cert, expected in ((self.cert, 0), ("0" * 64, 1)):
            with self.subTest(cert=cert):
                (self.case / "work").mkdir(exist_ok=True)
                env = dict(os.environ, KEYSTORE=str(self.key), SIGNING_ALIAS="scraperx-dashboard",
                           SCRAPERX_APK_KEYSTORE_PASSWORD=self.password.read_text(), previous_cert=cert)
                r = subprocess.run(["bash", "-euo", "pipefail", "-c", textwrap.dedent(snippet)],
                                   cwd=self.case, env=env, capture_output=True, text=True)
                self.assertEqual(r.returncode, expected, r.stderr)


if __name__ == "__main__":
    unittest.main()
