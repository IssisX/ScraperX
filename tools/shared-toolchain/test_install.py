#!/usr/bin/env python3
"""Offline adversarial checks for the real shared-toolchain installer."""
import hashlib
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import tarfile
import tempfile
import unittest

HERE = Path(__file__).resolve().parent
INSTALL = HERE / "install.sh"
ASSET = "ScraperX-toolchain-linux-x86_64-v1.tar.gz"


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.work = tempfile.TemporaryDirectory(prefix="scraperx-install-test-")
        self.addCleanup(self.work.cleanup)
        self.root = Path(self.work.name)
        self.stage = self.root / "reusable-toolchain"
        for name in ("bin", "mcp/node_modules/godot-mcp-runtime/dist", "deps/jolt/Build",
                     "deps/godot-cpp", "deps/monocypher/src"):
            (self.stage / name).mkdir(parents=True)
        versions = {"cmake": "cmake version 3.31.10", "ninja": "1.12.1",
                    "godot": "4.7.stable.official", "node": "v22.1.0"}
        for binary, version in versions.items():
            file = self.stage / "bin" / binary
            script = f'#!/bin/sh\necho "{version}"\n'
            if binary == "node":
                script = f'#!/bin/sh\nif [ "$1" = "-p" ]; then echo "3.8.1"; else echo "{version}"; fi\n'
            file.write_text(script)
            file.chmod(0o755)
        for name in ("deps/jolt/Build/CMakeLists.txt", "deps/godot-cpp/CMakeLists.txt",
                     "deps/monocypher/src/monocypher.c",
                     "mcp/node_modules/godot-mcp-runtime/dist/index.js"):
            (self.stage / name).write_text("/* synthetic fixture */\n")
        (self.stage / "mcp/node_modules/godot-mcp-runtime/package.json").write_text(
            '{"version":"3.8.1"}\n')
        self.cache = self.root / "installed"

    def bundle(self):
        archive = self.root / ASSET
        with tarfile.open(archive, "w:gz") as tar:
            tar.add(self.stage, arcname="reusable-toolchain")
        return archive, hashlib.sha256(archive.read_bytes()).hexdigest()

    def execute(self, *options, env=None):
        variables = os.environ.copy()
        variables["SCRAPERX_TOOLCHAIN_CACHE_DIR"] = str(self.cache)
        variables.update(env or {})
        return subprocess.run(["bash", str(INSTALL), *map(str, options)], env=variables,
                              capture_output=True, text=True, timeout=20)

    def offline(self, archive, digest):
        return self.execute("--from-archive", archive,
                            env={"SCRAPERX_TOOLCHAIN_ARCHIVE_SHA256": digest})

    def test_offline_install_and_reuse(self):
        archive, digest = self.bundle()
        installed = self.offline(archive, digest)
        self.assertEqual(installed.returncode, 0, installed.stderr)
        self.assertEqual(installed.stdout, str(self.cache) + "\n")
        verified = self.execute("--verify")
        self.assertEqual(verified.returncode, 0, verified.stderr)
        self.assertEqual(verified.stdout, installed.stdout)

    def test_mutated_cached_source_is_rejected(self):
        archive, digest = self.bundle()
        self.assertEqual(self.offline(archive, digest).returncode, 0)
        (self.cache / "deps/jolt/Build/CMakeLists.txt").write_text("/* corrupted */\n")
        checked = self.execute("--verify")
        self.assertNotEqual(checked.returncode, 0)
        self.assertIn("integrity check failed", checked.stderr)

    def test_external_symlink_is_rejected(self):
        cmake = self.stage / "bin/cmake"
        cmake.unlink()
        cmake.symlink_to("/usr/bin/true")
        archive, digest = self.bundle()
        checked = self.offline(archive, digest)
        self.assertNotEqual(checked.returncode, 0)
        self.assertIn("unsafe link", checked.stderr)
        self.assertFalse(self.cache.exists())

    def test_relative_symlink_escape_is_rejected(self):
        cmake = self.stage / "bin/cmake"
        cmake.unlink()
        cmake.symlink_to("../../outside/cmake")
        archive, digest = self.bundle()
        checked = self.offline(archive, digest)
        self.assertNotEqual(checked.returncode, 0)
        self.assertIn("link escapes", checked.stderr)

    def test_internal_symlink_is_supported(self):
        cmake = self.stage / "bin/cmake"
        cmake.rename(self.stage / "bin/cmake-real")
        cmake.symlink_to("cmake-real")
        archive, digest = self.bundle()
        checked = self.offline(archive, digest)
        self.assertEqual(checked.returncode, 0, checked.stderr)

    def test_rejects_corrupt_archive(self):
        archive, digest = self.bundle()
        with archive.open("ab") as stream:
            stream.write(b"tampered")
        result = self.offline(archive, digest)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.cache.exists())

    def test_rejects_remote_archive_and_manifest_substituted_together(self):
        archive, digest = self.bundle()
        fake_bin = self.root / "fake-bin"
        fake_bin.mkdir()
        curl = fake_bin / "curl"
        curl.write_text('''#!/usr/bin/env bash
set -euo pipefail
while (($#)); do
    if [[ "$1" == "--output" ]]; then out="$2"; shift 2; else shift; fi
done
if [[ "$out" == *.sha256 ]]; then
    printf '%s  %s\\n' "$FORGED_HASH" 'ScraperX-toolchain-linux-x86_64-v1.tar.gz' > "$out"
else
    cp "$FORGED_ARCHIVE" "$out"
fi
''')
        curl.chmod(0o755)
        result = self.execute(env={"PATH": f"{fake_bin}:{os.environ['PATH']}",
                                   "FORGED_ARCHIVE": str(archive), "FORGED_HASH": digest})
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("pinned v1 release digest", result.stderr)
        self.assertFalse(self.cache.exists())

    def test_tampered_cached_executable_never_runs_before_rejection(self):
        archive, digest = self.bundle()
        self.assertEqual(self.offline(archive, digest).returncode, 0)
        marker = self.root / "tampered-cmake-executed"
        cmake = self.cache / "bin/cmake"
        cmake.write_text(
            "#!/bin/sh\n"
            + f"printf 'executed\\n' > {shlex.quote(str(marker))}\n"
            + 'echo "cmake version 3.31.10"\n'
        )
        cmake.chmod(0o755)
        checked = self.execute("--verify")
        self.assertNotEqual(checked.returncode, 0)
        self.assertIn("integrity check failed", checked.stderr)
        self.assertFalse(marker.exists(), "tampered executable ran before integrity verification")

    def test_concurrent_reuse(self):
        archive, digest = self.bundle()
        variables = os.environ.copy()
        variables.update(SCRAPERX_TOOLCHAIN_CACHE_DIR=str(self.cache),
                         SCRAPERX_TOOLCHAIN_ARCHIVE_SHA256=digest)
        commands = ["bash", str(INSTALL), "--from-archive", str(archive)]
        one = subprocess.Popen(commands, env=variables, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True)
        two = subprocess.Popen(commands, env=variables, stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE, text=True)
        out1, err1 = one.communicate(timeout=20)
        out2, err2 = two.communicate(timeout=20)
        self.assertEqual(one.returncode, 0, err1)
        self.assertEqual(two.returncode, 0, err2)
        self.assertEqual(out1, out2)
        self.assertEqual(self.execute("--verify").returncode, 0)

    def test_failed_install_does_not_leave_partial_cache(self):
        archive, digest = self.bundle()
        result = self.offline(archive, "0" * 64)
        self.assertNotEqual(result.returncode, 0)
        self.assertFalse(self.cache.exists())
        self.assertEqual(list(self.root.glob(".scraperx-toolchain.*")), [])


if __name__ == "__main__":
    unittest.main()
