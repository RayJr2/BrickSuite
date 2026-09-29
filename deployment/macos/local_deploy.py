#!/usr/bin/env python3
"""Qt Creator adapter for the authoritative macOS packager and CI validators."""
import argparse
import fcntl
import json
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import uuid
from pathlib import Path

from ci_release import REPO, SCRIPTS, archive_bundle, output, packaged_probe, run, validate_bundle


def source_tree(explicit, candidates, required, option):
    paths = [Path(explicit)] if explicit else candidates
    for path in paths:
        if all((path/name).is_file() for name in required):
            return path.resolve()
    raise RuntimeError(f'Missing matching dependency sources. Set {option} in Qt Creator CMake configuration and refresh CMake.')


def publish(staged, destination):
    if destination.is_symlink():
        raise RuntimeError('Deploy output must not be a symlink')
    previous = None
    if destination.exists():
        marker = destination/'local-deploy.json'
        if not marker.is_file() or json.loads(marker.read_text()).get('owner') != 'bricksuite-macos-deploy':
            raise RuntimeError(f'Refusing to replace an unmanaged directory: {destination}')
        previous = destination.with_name('.deploy-previous-'+uuid.uuid4().hex)
        destination.rename(previous)
    try:
        staged.rename(destination)
    except BaseException:
        if previous:
            previous.rename(destination)
        raise
    if previous:
        shutil.rmtree(previous)


def validate_source_versions(qt_source, ssl_source, qt_version, ssl_version):
    for module in ('qtbase', 'qtsvg', 'qtimageformats', 'qtwebsockets'):
        match = re.search(r'set\(QT_REPO_MODULE_VERSION "([^"]+)"\)',
                          (qt_source/module/'.cmake.conf').read_text())
        if not match or match[1] != qt_version:
            raise RuntimeError(f'Mismatched Qt notice sources: {module}')
    values = dict(line.split('=', 1) for line in (ssl_source/'VERSION.dat').read_text().splitlines() if '=' in line)
    if '.'.join(values[key] for key in ('MAJOR', 'MINOR', 'PATCH')) != ssl_version:
        raise RuntimeError('Mismatched OpenSSL notice sources')


def deploy(args):
    if args.config != 'Release':
        raise RuntimeError('macOS Deploy requires a Release configuration.')
    if platform.system() != 'Darwin' or platform.machine() != 'arm64':
        raise RuntimeError('Local macOS Deploy requires a native ARM64 Mac. Use GitHub for x86_64.')
    if args.minimum != '13.0':
        raise RuntimeError('macOS Deploy requires CMAKE_OSX_DEPLOYMENT_TARGET=13.0.')
    if args.qt_version != '6.10.3' or output(args.qt/'bin/qmake', '-query', 'QT_VERSION') != args.qt_version:
        raise RuntimeError('macOS Deploy requires Qt 6.10.3.')
    args.build = args.build.resolve()
    qt_source = source_tree(args.qt_source, [args.qt.parent/'Src',
        args.qt.parent/'sources/qt-everywhere-src-6.10.3'],
        ['LICENSES/LGPL-3.0-only.txt', 'qtbase/.cmake.conf', 'qtsvg/.cmake.conf',
         'qtimageformats/.cmake.conf', 'qtwebsockets/.cmake.conf'], 'BRICKSUITE_MAC_DEPLOY_QT_SOURCE')
    ssl_source = source_tree(args.openssl_source,
        [args.openssl.parent/'sources'/('openssl-'+args.openssl_version)],
        ['LICENSE.txt', 'VERSION.dat'], 'BRICKSUITE_MAC_DEPLOY_OPENSSL_SOURCE')
    validate_source_versions(qt_source, ssl_source, args.qt_version, args.openssl_version)
    if not (args.lib3mf_source/'LICENSE').is_file():
        raise RuntimeError('Missing lib3mf source license')
    sha = output('git', '-C', REPO, 'rev-parse', 'HEAD')
    dirty = bool(output('git', '-C', REPO, 'status', '--porcelain'))
    metadata = dict(source_commit=sha, source_dirty=dirty, architecture='arm64',
        qt_version=args.qt_version, deployment_target='13.0', build_host=output('sw_vers'),
        schema=35, protocol='1.5', signing='ad-hoc / non-notarized',
        package_kind='local-development/release-candidate')
    with (args.build/'.macos-deploy.lock').open('w') as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        with tempfile.TemporaryDirectory(prefix='.macos-deploy-', dir=args.build) as temporary:
            stage = Path(temporary)
            bundle = stage/'BrickSuite.app'
            reports = stage/'reports'
            reports.mkdir()
            run(sys.executable, SCRIPTS/'package_macos.py', '--build', args.build,
                '--binary-dir', args.binary_dir, '--qt', args.qt, '--qt-source', qt_source,
                '--openssl', args.openssl, '--openssl-source', ssl_source,
                '--lib3mf-source', args.lib3mf_source, '--output', bundle,
                '--arch', 'arm64', '--source-sha', sha)
            audit = validate_bundle(bundle, 'arm64', sha)
            destination = stage/'deploy'
            destination.mkdir()
            archive = archive_bundle(bundle, destination, 'arm64', sha)
            # Probe a disposable extraction of the deliverable, never the build tree.
            extracted = stage/'extracted'
            run('/usr/bin/ditto', '-x', '-k', destination/archive['filename'], extracted)
            packaged_probe(extracted/'BrickSuite.app', args.probe, 'arm64', reports)
            metadata.update(archive=archive, dependency_audit='passed',
                codesign='deep strict passed', packaged_probe='passed')
            metadata['artifact_path'] = str(args.build/'deploy'/archive['filename'])
            (destination/'build-metadata.json').write_text(json.dumps(metadata, indent=2)+'\n')
            (destination/'bundle-audit.json').write_text(json.dumps(audit, indent=2)+'\n')
            shutil.copy2(reports/'packaged-probe.log', destination/'packaged-probe.log')
            (destination/'local-deploy.json').write_text(json.dumps({'owner': 'bricksuite-macos-deploy'})+'\n')
            publish(destination, args.build/'deploy')
    print('\nBrickSuite macOS deployment package created\n'
          f'Package: {metadata["artifact_path"]}\nArchive size: {archive["bytes"]} bytes\n'
          f'SHA-256: {archive["sha256"]}\nArchitecture: arm64\nSource commit: {sha}\n'
          f'Source state: {"dirty" if dirty else "clean"}\nmacOS deployment target: 13.0\n'
          f'Qt version: {args.qt_version}\nSigning mode: ad-hoc / non-notarized', flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('build', 'binary-dir', 'probe', 'qt', 'openssl', 'lib3mf-source'):
        parser.add_argument('--'+name, type=Path, required=True)
    for name in ('config', 'minimum', 'qt-version', 'openssl-version'):
        parser.add_argument('--'+name, required=True)
    for name in ('qt-source', 'openssl-source'):
        parser.add_argument('--'+name, default='')
    try:
        deploy(parser.parse_args())
    except (OSError, ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f'ERROR: macOS Deploy failed: {error}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
