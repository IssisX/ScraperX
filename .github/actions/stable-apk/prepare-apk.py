#!/usr/bin/env python3
"""Set an increasing Android version without rebuilding or changing game payloads."""
import argparse
import json
from pathlib import Path
import re
import struct
import time
import zipfile

VERSION_CODE_ID = 0x0101021B
EPOCH = 1577836800  # 2020-01-01; leaves decades of headroom in Android's integer.
LIMIT = 2100000000


def version_location(data):
    if len(data) < 8 or struct.unpack_from('<HHI', data) != (3, 8, len(data)):
        raise ValueError('Invalid binary Android manifest')
    resources = []
    found = []
    offset = 8
    while offset < len(data):
        kind, header, size = struct.unpack_from('<HHI', data, offset)
        if header < 8 or size < header or offset + size > len(data):
            raise ValueError('Invalid manifest chunk')
        if kind == 0x0180:
            resources = list(struct.unpack_from('<' + 'I' * ((size-header)//4), data, offset+header))
        elif kind == 0x0102:
            start, stride, count = struct.unpack_from('<HHH', data, offset+24)
            if stride < 20 or offset+16+start+stride*count > offset+size:
                raise ValueError('Invalid manifest attributes')
            for index in range(count):
                attr = offset+16+start+stride*index
                name = struct.unpack_from('<I', data, attr+4)[0]
                if name < len(resources) and resources[name] == VERSION_CODE_ID:
                    value_size, _, value_type, value = struct.unpack_from('<HBBI', data, attr+12)
                    if value_size != 8 or value_type not in (0x10, 0x11):
                        raise ValueError('versionCode must be an integer')
                    found.append((attr, value))
        offset += size
    if len(found) != 1:
        raise ValueError('Expected exactly one Android versionCode')
    return found[0]


def prepare(source, target, previous=0, now=None):
    if source.resolve() == target.resolve():
        raise ValueError('Input and output must be different files')
    if previous < 0 or previous >= LIMIT:
        raise ValueError('Published version is outside the supported range')
    with zipfile.ZipFile(source) as archive:
        if archive.namelist().count('AndroidManifest.xml') != 1:
            raise ValueError('Expected exactly one AndroidManifest.xml')
        manifest = bytearray(archive.read('AndroidManifest.xml'))
        attr, original = version_location(manifest)
        code = max(previous+1, original, int(time.time() if now is None else now)-EPOCH)
        if not 1 <= code <= LIMIT:
            raise ValueError('No valid increasing Android version available')
        struct.pack_into('<I', manifest, attr+8, 0xFFFFFFFF)  # use typed value
        struct.pack_into('<HBBI', manifest, attr+12, 8, 0, 0x10, code)
        with zipfile.ZipFile(target, 'w') as output:
            for entry in archive.infolist():
                if re.fullmatch(r'META-INF/(?:[^/]+\.(?:RSA|DSA|EC|SF)|MANIFEST\.MF)', entry.filename, re.I):
                    continue
                payload = bytes(manifest) if entry.filename == 'AndroidManifest.xml' else archive.read(entry)
                output.writestr(entry, payload)
    return {'source_version_code': original, 'version_code': code, 'version_policy': 'monotonic-v1'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('target', type=Path)
    parser.add_argument('--previous-code', type=int, default=0)
    args = parser.parse_args()
    print(json.dumps(prepare(args.source, args.target, args.previous_code)))
