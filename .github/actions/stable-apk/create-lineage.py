#!/usr/bin/env python3
"""Bridge the already-public ChatGPT CI key to the existing protected dashboard key."""
import hashlib
from pathlib import Path
import subprocess
import sys

LEGACY_CERT = '9a06889e7614d140f6cd1fc45634bb1e2391968a9fcc60f1608c9e50e15a1ba4'


def create(apksigner, keystore, output):
    legacy = Path(__file__).with_name('legacy-chatgpt-debug.keystore')
    certificate = subprocess.check_output(['keytool', '-exportcert', '-keystore', str(legacy),
                                          '-alias', 'androiddebugkey', '-storepass', 'android'])
    if hashlib.sha256(certificate).hexdigest() != LEGACY_CERT:
        raise RuntimeError('Unexpected legacy CI key; refusing a different signing history')
    subprocess.run([str(apksigner), 'rotate', '--out', str(output),
        '--old-signer', '--ks', str(legacy), '--ks-key-alias', 'androiddebugkey',
        '--ks-pass', 'pass:android', '--key-pass', 'pass:android',
        '--new-signer', '--ks', str(keystore), '--ks-key-alias', 'scraperx-dashboard',
        '--ks-pass', 'env:SCRAPERX_APK_KEYSTORE_PASSWORD', '--key-pass', 'env:SCRAPERX_APK_KEYSTORE_PASSWORD'], check=True)
    return legacy


if __name__ == '__main__':
    create(*sys.argv[1:])
