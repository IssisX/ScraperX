#!/usr/bin/env python3
"""Normalize build-thread downloads to the existing dashboard's Android identity."""
import base64
import glob
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import time

CERTIFICATE = '91184379cf4aad862b2436f403230f3ba163a8a9cdc5dc7c32dffef2a517402a'
MODELS = {'ChatGPT': 'ChatGPT', 'Gemini': 'Gemini', 'ScraperX-Claude': 'Claude', 'ScraperX-Grok': 'Grok'}


def run(*args):
    return subprocess.check_output(args, text=True)


def main():
    encoded = os.environ.get('SCRAPERX_APK_KEYSTORE_B64', '')
    if not encoded or not os.environ.get('SCRAPERX_APK_KEYSTORE_PASSWORD'):
        raise RuntimeError('Stable signing secrets missing; refusing to distribute an incompatible APK')
    label = MODELS[os.environ['GITHUB_REF_NAME']]
    files = sorted(Path(p) for p in glob.glob(os.environ['SCRAPERX_APK_GLOB'], recursive=True))
    if not files:
        raise RuntimeError('No APKs matched the distribution path')
    sdk = Path(os.environ.get('ANDROID_HOME', '/usr/local/lib/android/sdk'))
    tools = max((p for p in (sdk/'build-tools').iterdir() if (p/'apksigner').exists()),
                key=lambda p: tuple(int(n) for n in re.findall(r'\d+', p.name)))
    spec = importlib.util.spec_from_file_location('prepare_apk', Path(__file__).with_name('prepare-apk.py'))
    apk = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(apk)
    now = int(time.time())
    with tempfile.TemporaryDirectory(dir=os.environ.get('RUNNER_TEMP')) as temporary:
        root = Path(temporary)
        key = root/'signer.p12'
        key.write_bytes(base64.b64decode(encoded, validate=True))
        key.chmod(0o600)
        certificate = subprocess.check_output(['keytool', '-exportcert', '-keystore', str(key),
                    '-alias', 'scraperx-dashboard', '-storepass:env', 'SCRAPERX_APK_KEYSTORE_PASSWORD'])
        if hashlib.sha256(certificate).hexdigest() != CERTIFICATE:
            raise RuntimeError('Signing key differs from the dashboard; no APKs changed')
        for index, path in enumerate(files):
            unsigned, aligned, signed = [root/(str(index)+suffix) for suffix in ('-unsigned.apk', '-aligned.apk', '-signed.apk')]
            version = apk.prepare(path, unsigned, now=now)
            run(str(tools/'zipalign'), '-p', '-f', '4', str(unsigned), str(aligned))
            run(str(tools/'apksigner'), 'sign', '--ks', str(key), '--ks-key-alias', 'scraperx-dashboard',
                '--ks-pass', 'env:SCRAPERX_APK_KEYSTORE_PASSWORD', '--key-pass', 'env:SCRAPERX_APK_KEYSTORE_PASSWORD',
                '--out', str(signed), str(aligned))
            verification = run(str(tools/'apksigner'), 'verify', '--print-certs', str(signed))
            certs = re.findall(r'certificate SHA-256 digest:\s*([0-9a-f]{64})', verification)
            if certs != [CERTIFICATE]:
                raise RuntimeError('Unexpected final APK signer')
            badging = run(str(tools/'aapt'), 'dump', 'badging', str(signed))
            package = 'com.cory.scraperx.'+label.lower()
            if "package: name='"+package+"'" not in badging or "application-label:'ScraperX-"+label+"'" not in badging:
                raise RuntimeError('Wrong model package or application label')
            if "versionCode='"+str(version['version_code'])+"'" not in badging:
                raise RuntimeError('Final APK version verification failed')
            path.write_bytes(signed.read_bytes())
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            (path.parent/'SHA256SUMS.txt').write_text(digest+'  '+path.name+'\n')
            (path.parent/'STABLE-APK.json').write_text(json.dumps(dict(version, source_sha=os.environ['GITHUB_SHA'],
                signing_certificate_sha256=CERTIFICATE, package_id=package, apk_sha256=digest), indent=2)+'\n')
            print(f'{path}: stable signer verified; versionCode={version["version_code"]}')


if __name__ == '__main__':
    main()
