#!/usr/bin/env python3
"""Set an increasing Android version without rebuilding or changing game payloads."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import time
import zipfile

VERSION_CODE_ID = 0x0101021B
EPOCH = 1577836800  # 2020-01-01; leaves decades of headroom in Android's integer.
LIMIT = 2100000000
BUILD_INFO = "assets/build_info.json"
SPARSE_PACK = "assets/assets.sparsepck"
SIGNATURE = r"META-INF/(?:[^/]+\.(?:RSA|DSA|EC|SF)|MANIFEST\.MF)"


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


def update_sparse_identity(data, payload):
    """Keep Godot 4.7's sparse resource length/hash in sync with signed JSON."""
    if len(data) < 104 or data[:4] != b'GDPC':
        raise ValueError('Invalid Godot sparse pack header')
    version, _, _, _, flags = struct.unpack_from('<IIIII', data, 4)
    if version != 4 or flags != 6:
        raise ValueError('Expected an unencrypted Godot V4 sparse pack')
    offset = struct.unpack_from('<Q', data, 32)[0]
    if offset < 104 or offset + 4 > len(data):
        raise ValueError('Invalid Godot sparse pack directory')
    count = struct.unpack_from('<I', data, offset)[0]
    offset += 4
    result = bytearray(data)
    found = 0
    for _ in range(count):
        if offset + 4 > len(data):
            raise ValueError('Truncated Godot sparse pack entry')
        size = struct.unpack_from('<I', data, offset)[0]
        offset += 4
        if offset + size + 36 > len(data):
            raise ValueError('Truncated Godot sparse pack entry')
        path = data[offset:offset + size].rstrip(b'\0')
        offset += size
        if path in (b'build_info.json', b'res://build_info.json'):
            if struct.unpack_from('<I', data, offset + 32)[0] != 0:
                raise ValueError('Expected a plain build identity sparse entry')
            struct.pack_into('<Q', result, offset + 8, len(payload))
            result[offset + 16:offset + 32] = hashlib.md5(payload).digest()
            found += 1
        offset += 36
    if found != 1:
        raise ValueError('Expected exactly one build identity sparse entry')
    return bytes(result)


def prepare(source, target, previous=0, now=None, source_sha=None):
    if source_sha is not None and not re.fullmatch(r"[0-9a-f]{40}", source_sha):
        raise ValueError("Build source must be the full Git commit SHA")
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
        identity = (json.dumps({'version_code': code, 'source_sha': source_sha}) + '\n').encode() if source_sha is not None else None
        with zipfile.ZipFile(target, 'w') as output:
            for entry in archive.infolist():
                if re.fullmatch(SIGNATURE, entry.filename, re.I) or (source_sha is not None and entry.filename == BUILD_INFO):
                    continue
                payload = bytes(manifest) if entry.filename == 'AndroidManifest.xml' else archive.read(entry)
                if identity is not None and entry.filename == SPARSE_PACK:
                    payload = update_sparse_identity(payload, identity)
                output.writestr(entry, payload)
            if source_sha is not None:
                output.writestr(BUILD_INFO, identity)
    return {'source_version_code': original, 'version_code': code, 'version_policy': 'monotonic-v1'}


def verify_prepared(source, target, code, source_sha=None):
    """Verify final signed bytes against source payload and final manifest identity."""
    with zipfile.ZipFile(source) as original, zipfile.ZipFile(target) as final:
        names = final.namelist()
        if len(names) != len(set(names)):
            raise ValueError('Duplicate final APK entries')
        if version_location(final.read('AndroidManifest.xml'))[1] != code:
            raise ValueError('Final manifest version differs from prepared version')
        if source_sha is not None:
            identity = json.loads(final.read(BUILD_INFO))
            if identity != {'version_code': code, 'source_sha': source_sha}:
                raise ValueError('Embedded build identity differs from final APK provenance')
        def payload_names(archive):
            return {name for name in archive.namelist()
                    if name != 'AndroidManifest.xml'
                    and not re.fullmatch(SIGNATURE, name, re.I)
                    and not (source_sha is not None and name == BUILD_INFO)}
        if payload_names(original) != payload_names(final):
            raise ValueError('APK game payload entries changed')
        for name in payload_names(original):
            expected = original.read(name)
            if source_sha is not None and name == SPARSE_PACK:
                expected = update_sparse_identity(expected, final.read(BUILD_INFO))
            if expected != final.read(name):
                raise ValueError('APK game payload changed: ' + name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('target', type=Path)
    parser.add_argument('--previous-code', type=int, default=0)
    parser.add_argument('--source-sha', help='Opt in to final build identity metadata')
    args = parser.parse_args()
    print(json.dumps(prepare(args.source, args.target, args.previous_code, source_sha=args.source_sha)))
