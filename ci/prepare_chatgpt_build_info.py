#!/usr/bin/env python3
"""Give candidate exports a source/run version; final signing owns versionCode."""
import argparse
import json
import os
from pathlib import Path
import re


def prepare(project, source_sha, run_id, attempt):
    if not re.fullmatch(r'[0-9a-f]{40}', source_sha):
        raise ValueError('Expected the full source Git commit SHA')
    if not re.fullmatch(r'[0-9]+', run_id) or not re.fullmatch(r'[0-9]+', attempt):
        raise ValueError('Expected numeric GitHub run identity')
    preset = project / 'export_presets.cfg'
    version_name = f'0.1.0-chatgpt.{source_sha[:12]}.{run_id}.{attempt}'
    text, count = re.subn(r'^version/name="[^"]*"$', f'version/name="{version_name}"',
                          preset.read_text(), flags=re.MULTILINE)
    if count != 1:
        raise ValueError('Expected exactly one Android version name')
    preset.write_text(text)
    identity = {'source_sha': source_sha, 'version_code': None}
    (project / 'build_info.json').write_text(json.dumps(identity) + '\n')
    return identity


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--project', type=Path, default=Path('godot'))
    args = parser.parse_args()
    print(json.dumps(prepare(args.project, os.environ['GITHUB_SHA'],
                             os.environ['GITHUB_RUN_ID'], os.environ['GITHUB_RUN_ATTEMPT'])))
