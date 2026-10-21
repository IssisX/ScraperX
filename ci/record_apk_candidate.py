"""Bind an Actions APK candidate to its source, native payload and real identity."""
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import zipfile

apk = Path('artifacts/apk/ScraperX-ChatGPT.apk')
native = Path('godot/addons/scraperx_native/bin/android/arm64/libscraperx_native.so')
sha = lambda data: hashlib.sha256(data).hexdigest()
with zipfile.ZipFile(apk) as package:
    payload = package.read('lib/arm64-v8a/libscraperx_native.so')
    if payload != native.read_bytes():
        raise SystemExit('Packaged native library differs from this build')
    climber_files = {}
    for name in ('climber_body.glb.bytes', 'SOURCE.md', 'LICENSE.MakeHuman.txt'):
        relative = 'presentation/life/climber/' + name
        try:
            data = package.read('assets/' + relative)
        except KeyError as error:
            raise SystemExit('APK body resource missing: assets/' + relative) from error
        if data != (Path('godot') / relative).read_bytes():
            raise SystemExit('APK body resource differs from source: ' + relative)
        climber_files[name] = {'bytes': len(data), 'sha256': sha(data)}
if payload[:6] != b'\x7fELF\x02\x01' or struct.unpack_from('<H', payload, 18)[0] != 183:
    raise SystemExit('Expected ELF64 little-endian AArch64 native library')
header = struct.unpack_from('<16sHHIQQQIHHHHHH', payload)
offset, stride, count = header[5], header[9], header[10]
loads = [struct.unpack_from('<IIQQQQQQ', payload, offset + i*stride)
         for i in range(count)]
alignments = [row[7] for row in loads if row[0] == 1]
if not alignments or any(value < 16384 for value in alignments):
    raise SystemExit(f'Native LOAD alignment below16K: {alignments}')
build_tools = sorted((Path(os.environ['ANDROID_HOME'])/'build-tools').iterdir(), key=lambda p: p.name)
aapt = next(p/'aapt' for p in reversed(build_tools) if (p/'aapt').is_file())
signer = next(p/'apksigner' for p in reversed(build_tools) if (p/'apksigner').is_file())
badging = subprocess.check_output([str(aapt), 'dump', 'badging', str(apk)], text=True)
identity = re.search(r"package: name='([^']+)' versionCode='(\d+)' versionName='([^']*)'", badging)
if not identity or identity[1] != 'com.cory.scraperx.chatgpt':
    raise SystemExit('Incorrect APK package identity')
if "application-label:'ScraperX-ChatGPT'" not in badging:
    raise SystemExit('Incorrect APK app label')
if int(identity[2]) < 213671857:
    raise SystemExit('APK version is older than preserved local857')
certs = subprocess.check_output([str(signer), 'verify', '--verbose', '--print-certs', '--min-sdk-version', '28', str(apk)], text=True)
expected_signer = '91184379cf4aad862b2436f403230f3ba163a8a9cdc5dc7c32dffef2a517402a'
if 'Number of signers: 1' not in certs or not any(
    'certificate SHA-256 digest:' in line and line.split()[-1].lower() == expected_signer
    for line in certs.splitlines()
):
    raise SystemExit('Incorrect stable download-channel signing identity')
Path('artifacts/apk-final-signing.txt').write_text(certs)
record = {
    'quality_status': 'build-candidate-only',
    'source_commit': os.environ['GITHUB_SHA'],
    'source_branch': os.environ['GITHUB_REF_NAME'],
    'run_url': f"{os.environ['GITHUB_SERVER_URL']}/{os.environ['GITHUB_REPOSITORY']}/actions/runs/{os.environ['GITHUB_RUN_ID']}",
    'package': identity[1], 'version_code': int(identity[2]), 'version_name': identity[3],
    'signer_sha256': expected_signer, 'signer_verification_min_sdk': 28, 'apk_sha256': sha(apk.read_bytes()),
    'native_sha256': sha(payload), 'native_matches_build': True,
    'native_abi': 'AArch64', 'native_load_alignments': alignments,
    'climber_body_resources': climber_files,
    'validation': ['host/Android native build', 'Godot resource import', 'signed export/package identity', '16K ELF/ZIP alignment'],
    'full_gameplay_proof': 'separate required wo000-delivery-spine.yml workflow; not certified by this artifact',
    'installed_android_play': 'not run',
    'release_promotion': 'none; candidate artifact prefix is excluded from apk-dashboard',
}
Path('artifacts/apk/CANDIDATE-PROVENANCE.json').write_text(json.dumps(record, indent=2)+'\n')
print(json.dumps(record, indent=2))
