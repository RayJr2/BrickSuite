#!/usr/bin/env python3
"""Fetch hash-locked official source archives; extract only the required Qt modules."""
import argparse
import hashlib
import json
import subprocess
import tarfile
from pathlib import Path, PurePosixPath

MODULES = {'qtbase', 'qtsvg', 'qtimageformats', 'qtwebsockets', 'LICENSES', 'cmake'}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    args.destination.mkdir(parents=True, exist_ok=True)
    lock = json.loads(Path(__file__).with_name('source-lock.json').read_text())
    for name, spec in lock.items():
        archive = args.destination / spec['url'].rsplit('/', 1)[1]
        if not archive.exists():
            subprocess.run(['curl', '--fail', '--location', '--retry', '3', spec['url'],
                            '--output', str(archive)], check=True)
        with archive.open('rb') as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        if digest != spec['sha256']:
            raise SystemExit(f'{name}: source SHA-256 mismatch')
        destination = args.destination / spec['directory']
        if destination.exists():
            raise SystemExit(f'Refusing to reuse possibly modified sources: {destination}')
        with tarfile.open(archive) as source:
            for member in source:
                parts = PurePosixPath(member.name).parts
                if not parts or parts[0] != spec['directory'] or '..' in parts:
                    raise SystemExit(f'Unexpected archive path: {member.name}')
                if name == 'qt' and len(parts) > 2 and parts[1] not in MODULES:
                    continue
                if name == 'qt' and len(parts) == 2 and member.isdir() and parts[1] not in MODULES:
                    continue
                source.extract(member, args.destination, filter='data')
        print(f'{name} {spec["version"]}: verified {digest}; source {destination}', flush=True)


if __name__ == '__main__':
    main()
