#!/usr/bin/env python3
"""Execute actual publisher shell; fixture network and Android boundaries."""
import base64
import copy
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import textwrap
import unittest

ROOT = Path(__file__).resolve().parents[1]
MODELS = {"ChatGPT": "ChatGPT", "Gemini": "Gemini", "Claude": "ScraperX-Claude", "Grok": "ScraperX-Grok"}
MOCK_GH = '''#!/usr/bin/env python3
import json, os, pathlib, sys, struct, zipfile
p = pathlib.Path(os.environ["TEST_STATE"])
s = json.loads(p.read_text())
a = sys.argv[1:]
s.setdefault("calls", []).append(a)
p.write_text(json.dumps(s))
if a[:2] == ["release", "view"]:
    if "--json" in a:
        print("fixture release assets")
elif a[:2] == ["release", "download"]:
    d = pathlib.Path(a[a.index("--dir") + 1])
    (d / "manifest.json").write_text(json.dumps(s["manifest"]))
elif a[0] == "api":
    url = a[1]
    if "/releases/tags/" in url:
        print(json.dumps(s["release"]))
    elif "/branches/" in url:
        print(s["heads"][url.rsplit("/", 1)[1]])
    elif "/artifacts?" in url:
        run = url.split("/runs/", 1)[1].split("/", 1)[0]
        print(json.dumps({"artifacts": s["artifacts"].get(run, [])}))
    elif "/actions/runs?" in url:
        branch = url.split("branch=", 1)[1].split("&", 1)[0]
        print(json.dumps({"workflow_runs": s["runs"].get(branch, [])}))
    else:
        sys.exit("unexpected API " + url)
elif a[:2] == ["run", "download"]:
    d = pathlib.Path(a[a.index("--dir") + 1])
    d.mkdir(exist_ok=True, parents=True)
    resource_map = struct.pack('<HHII', 0x180, 8, 12, 0x0101021B)
    attribute = struct.pack('<IIIHBBI', 0, 0, 0xFFFFFFFF, 8, 0, 0x10, 1)
    element = struct.pack('<HHIII', 0x102, 16, 56, 1, 0xFFFFFFFF) + struct.pack('<IIHHHHHH', 0xFFFFFFFF, 0, 20, 20, 1, 0, 0, 0) + attribute
    manifest = struct.pack('<HHI', 3, 8, 8+len(resource_map)+len(element)) + resource_map + element
    with zipfile.ZipFile(d / 'game.apk', 'w') as archive:
        archive.writestr('AndroidManifest.xml', manifest)
        archive.writestr('assets/game.pck', b'fixture-APK' * 150000)
    if s.get("ambiguous"):
        (d / "other.apk").write_bytes(b"fixture-other" * 150000)
elif a[:2] in (["release", "edit"], ["release", "upload"], ["release", "create"]):
    pass
else:
    sys.exit("unexpected gh " + repr(a))
'''
MOCK_ANDROID = '''#!/usr/bin/env python3
import json, os, pathlib, shutil, sys, zipfile, struct
tool = pathlib.Path(sys.argv[0]).name
a = sys.argv[1:]
p = pathlib.Path(os.environ["TEST_STATE"])
s = json.loads(p.read_text())
s.setdefault("android", []).append([tool] + a)
p.write_text(json.dumps(s))
if tool == "zipalign":
    shutil.copyfile(a[-2], a[-1])
elif tool == "apksigner" and a[0] == "rotate":
    pathlib.Path(a[a.index("--out") + 1]).write_bytes(b'fixture-lineage')
elif tool == "apksigner" and a[0] == "sign":
    shutil.copyfile(a[-1], a[a.index("--out") + 1])
elif tool == "apksigner" and a[0] == "verify":
    print("V3.0 Signer: certificate SHA-256 digest: " + os.environ["TEST_CERT"])
elif tool == "aapt":
    label = pathlib.Path(a[-1]).stem.split("-", 1)[1]
    manifest = zipfile.ZipFile(a[-1]).read('AndroidManifest.xml')
    code = struct.unpack_from('<I', manifest, 8+12+36+16)[0]
    print("package: name='com.cory.scraperx." + label.lower() + "' versionCode='"+str(code)+"'")
    print("application-label:'ScraperX-" + label + "'")
else:
    sys.exit("unexpected Android tool call")
'''


class PublisherTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.temp.name)
        cls.bin = cls.root / "bin"
        cls.bin.mkdir()
        (cls.bin / "gh").write_text(MOCK_GH)
        (cls.bin / "gh").chmod(0o755)
        cls.sdk = cls.root / "sdk"
        tools = cls.sdk / "build-tools/36.1.0"
        tools.mkdir(parents=True)
        for name in ("apksigner", "zipalign", "aapt"):
            (tools / name).write_text(MOCK_ANDROID)
            (tools / name).chmod(0o755)
        key, cert, p12 = [cls.root / name for name in ("key.pem", "cert.pem", "signing.p12")]
        subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                        "-keyout", str(key), "-out", str(cert), "-days", "1", "-subj", "/CN=PublisherFixture"],
                       check=True, capture_output=True)
        subprocess.run(["openssl", "pkcs12", "-export", "-inkey", str(key), "-in", str(cert),
                        "-out", str(p12), "-name", "scraperx-dashboard", "-passout", "pass:fixture"],
                       check=True, capture_output=True)
        cls.keystore = base64.b64encode(p12.read_bytes()).decode()
        der = subprocess.check_output(["openssl", "x509", "-in", str(cert), "-outform", "DER"])
        cls.cert = hashlib.sha256(der).hexdigest()
        workflow = (ROOT / ".github/workflows/apk-dashboard.yml").read_text()
        step = workflow.split("      - name: Publish newest successful APK from every model branch\n", 1)[1]
        cls.shell = textwrap.dedent(step.split("        run: |\n", 1)[1])

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def setUp(self):
        self.case = Path(tempfile.mkdtemp(dir=self.root))
        self.state_path = self.case / "state.json"
        builds, assets, runs, artifacts, heads = {}, [], {}, {}, {}
        for i, (label, branch) in enumerate(MODELS.items(), 1):
            sha, digest, name = str(i) * 40, str(i) * 64, "ScraperX-" + label + ".apk"
            builds[label] = dict(status="ok", branch=branch, source_sha=sha, source_artifact_id=i,
                                 signing_certificate_sha256=self.cert, package_id="com.cory.scraperx." + label.lower(),
                                 app_label="ScraperX-" + label, file=name, bytes=1234567, apk_sha256=digest,
                                 source_is_current_head=True, version_code="1", version_policy="monotonic-v1",
                                 signing_lineage=(["9a06889e7614d140f6cd1fc45634bb1e2391968a9fcc60f1608c9e50e15a1ba4",self.cert] if label=="ChatGPT" else [self.cert]))
            assets.append(dict(name=name, state="uploaded", size=1234567, digest="sha256:" + digest))
            runs[branch] = [dict(id=i, head_sha=sha, html_url="https://github.com/fixture/run/" + str(i),
                                 updated_at="2026-10-01T12:00:00Z")]
            artifacts[str(i)] = [dict(id=i, name="ScraperX-build-" + sha, expired=False)]
            heads[branch] = sha
        self.state = dict(manifest=dict(schema=1, repo="IssisX/ScraperX", builds=builds,
                                       signing_certificate_sha256=self.cert, generated_at="2026-10-01T12:00:00Z"),
                          release=dict(assets=assets), runs=runs, artifacts=artifacts, heads=heads)

    def execute(self):
        self.state_path.write_text(json.dumps(self.state))
        env = dict(os.environ, PATH=str(self.bin) + os.pathsep + os.environ["PATH"],
                   TEST_STATE=str(self.state_path), TEST_CERT=self.cert,
                   GITHUB_REPOSITORY="IssisX/ScraperX", GITHUB_WORKSPACE=str(ROOT), RUNNER_TEMP=str(self.case),
                   ANDROID_HOME=str(self.sdk), SCRAPERX_APK_KEYSTORE_B64=self.keystore,
                   SCRAPERX_APK_KEYSTORE_PASSWORD="fixture")
        r = subprocess.run(["bash", "-c", self.shell], cwd=self.case, env=env, capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.state = json.loads(self.state_path.read_text())
        return r.stdout

    def uploads(self):
        return [a for a in self.state["calls"] if a[:2] == ["release", "upload"]]

    def assert_no_publication(self):
        self.assertFalse(self.uploads())
        self.assertFalse(self.state.get("android"))
        self.assertFalse(any(a[:2] == ["run", "download"] for a in self.state["calls"]))
        self.assertFalse(any(a[:2] == ["release", "edit"] for a in self.state["calls"]))

    def test_unchanged_skips_all_mutations_and_keeps_manifest(self):
        before = copy.deepcopy(self.state["manifest"])
        self.assertIn("No APK changes to publish", self.execute())
        self.assert_no_publication()
        self.assertEqual(json.loads((self.case / "out/manifest.json").read_text()), before)

    def test_branch_advancement_without_new_artifact_is_noop(self):
        self.state["heads"]["ChatGPT"] = "a" * 40
        self.execute()
        self.assert_no_publication()

    def test_one_new_model_uploads_only_that_apk_and_retains_other_provenance(self):
        before = copy.deepcopy(self.state["manifest"]["builds"])
        self.state["runs"]["ChatGPT"][0]["head_sha"] = "a" * 40
        self.state["artifacts"]["1"][0]["id"] = 99
        self.execute()
        self.assertEqual(len(self.uploads()), 2)
        self.assertEqual(self.uploads()[0][-1], "out/ScraperX-ChatGPT.apk")
        self.assertEqual(self.uploads()[1][-1], "out/manifest.json")
        result = json.loads((self.case / "out/manifest.json").read_text())
        self.assertEqual(result["builds"]["ChatGPT"]["source_sha"], "a" * 40)
        for label in ("Gemini", "Claude", "Grok"):
            self.assertEqual(result["builds"][label], before[label])

    def test_recreated_artifact_for_same_commit_is_published(self):
        self.state["artifacts"]["1"][0]["id"] = 99
        self.execute()
        self.assertEqual(self.uploads()[0][-1], "out/ScraperX-ChatGPT.apk")

    def test_missing_release_asset_is_repaired(self):
        self.state["release"]["assets"].pop(0)
        self.execute()
        self.assertEqual(self.uploads()[0][-1], "out/ScraperX-ChatGPT.apk")

    def test_wrong_release_digest_is_repaired(self):
        self.state["release"]["assets"][0]["digest"] = "sha256:" + "f" * 64
        self.execute()
        self.assertEqual(self.uploads()[0][-1], "out/ScraperX-ChatGPT.apk")

    def test_no_candidate_preserves_published_provenance_during_other_update(self):
        before = copy.deepcopy(self.state["manifest"]["builds"]["Gemini"])
        self.state["runs"]["Gemini"] = []
        self.state["artifacts"]["1"][0]["id"] = 99
        self.execute()
        result = json.loads((self.case / "out/manifest.json").read_text())
        self.assertEqual(result["builds"]["Gemini"], before)

    def test_ambiguous_new_artifact_keeps_existing_release(self):
        self.state["ambiguous"] = True
        self.state["artifacts"]["1"][0]["id"] = 99
        self.execute()
        self.assertFalse(self.uploads())
        self.assertFalse(self.state.get("android"))


if __name__ == "__main__":
    unittest.main()
