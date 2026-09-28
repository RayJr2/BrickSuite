#!/usr/bin/env python3
"""Shared native-architecture CI gate. Never upload or publish from this script."""
import argparse
import hashlib
import json
import os
import plistlib
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

from audit_bundle import audit
from package_macos import PLUGINS, normalize_binary
from sign_bundle import sign_bundle

REPO = Path(__file__).resolve().parents[2]
SCRIPTS = REPO / 'deployment/macos'


def run(*args, **kwargs):
    print('+', shlex.join(map(str, args)), flush=True)
    return subprocess.run(list(map(str, args)), check=True, **kwargs)


def output(*args):
    return subprocess.check_output(list(map(str, args)), text=True).strip()


def sha256(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def configured_targets(build):
    # CTest JSON omits commands for executables not built yet. Its generated
    # declarations retain them, including tests excluded from the ALL target.
    targets = set()
    for file in build.rglob('CTestTestfile.cmake'):
        for line in file.read_text().splitlines():
            if line.startswith('add_test('):
                words = shlex.split(line[len('add_test('):-1])
                executable = Path(words[1])
                if not executable.is_absolute() or not executable.is_relative_to(build):
                    raise RuntimeError(f'Cannot map test to local executable target: {line}')
                targets.add(executable.name)
    if not targets:
        raise RuntimeError('No configured test targets')
    return sorted(targets)


def test_summary(inventory, junit):
    cases = ET.parse(junit).getroot().findall('.//testcase')
    expected = {test['name'] for test in inventory['tests']}
    actual = [case.attrib['name'] for case in cases]
    if set(actual) != expected or len(actual) != len(expected):
        raise RuntimeError('JUnit results do not cover the configured CTest inventory exactly')
    failed = sum(case.find('failure') is not None or case.find('error') is not None for case in cases)
    skipped = sum(case.find('skipped') is not None for case in cases)
    # Explicitly list conditional integrations; these are not CTest skips.
    integrations = ['FitCalibrationSource' + name for name in (
        'BallSocket', 'PinBarrelHinge', 'InterleavedFingerHinge', 'ClickHinge',
        'RetainedRotatingWheel', 'PlainRoundBoreWheel')]
    integrations += ['PrintCompositionRouting', 'PrintPreparation3021']
    return dict(configured=len(expected), passed=len(cases)-failed-skipped,
                failed=failed, skipped=skipped,
                unregistered_ldraw=[name for name in integrations if name not in expected],
                concurrency=1)


def containment_evidence(junit):
    cases = ET.parse(junit).getroot().findall('.//testcase')
    case = next((case for case in cases if case.attrib['name'] == 'McutMeshBoolean'), None)
    if case is None or case.find('failure') is not None or case.find('skipped') is not None:
        raise RuntimeError('Controlled worker test did not pass')
    log = case.findtext('system-out', '')
    evidence = {}
    for mode in ('success', 'memory', 'timeout'):
        match = re.search(rf'Worker mode {mode}: elapsed=(\d+) ms, cleanup=1', log)
        if not match or int(match[1]) >= 4000:
            raise RuntimeError(f'Missing bounded worker containment evidence: {mode}')
        evidence[mode] = dict(elapsed_ms=int(match[1]), cleanup=True)
    return evidence


def validate_bundle(bundle, arch, source_sha):
    info = plistlib.loads((bundle/'Contents/Info.plist').read_bytes())
    expected = dict(CFBundleIdentifier='com.rfstateside.bricksuite',
                    CFBundleShortVersionString='0.4.0', CFBundleVersion='0.4.0',
                    CFBundleName='BrickSuite', LSMinimumSystemVersion='13.0',
                    CFBundleIconFile='BrickSuite.icns', BrickSuiteSourceCommit=source_sha,
                    BrickSuiteArchitecture=arch)
    for key, value in expected.items():
        if info.get(key) != value:
            raise RuntimeError(f'Bundle metadata mismatch: {key}: {info.get(key)!r}')
    executables = {p.name for p in (bundle/'Contents/MacOS').iterdir()}
    if executables != {'BrickSuite', 'BrickSuiteMeshBooleanWorker'}:
        raise RuntimeError(f'Unexpected production executables: {executables}')
    plugin_root = bundle/'Contents/PlugIns'
    plugins = {str(p.relative_to(plugin_root)) for p in plugin_root.rglob('*') if p.is_file()}
    if plugins != set(PLUGINS):
        raise RuntimeError(f'Unexpected plugin inventory: {plugins}')
    if not (bundle/'Contents/Resources/BrickSuite.icns').is_file():
        raise RuntimeError('Missing bundle icon')
    report = audit(bundle, arch, '13.0')
    if report['errors']:
        raise RuntimeError('\n'.join(report['errors']))
    run('codesign', '--verify', '--deep', '--strict', bundle)
    return report


def packaged_probe(bundle, probe, arch, reports):
    # Keep the production bundle byte-for-byte untouched. Only this disposable
    # acceptance copy contains a diagnostic executable, and it is never archived.
    with tempfile.TemporaryDirectory(prefix='bricksuite-ci-probe-') as temp:
        root = Path(temp)
        copy = root/'BrickSuite.app'
        shutil.copytree(bundle, copy, symlinks=True)
        target = copy/'Contents/MacOS/BrickSuitePackageProbe'
        shutil.copy2(probe, target)
        normalize_binary(target, copy/'Contents/Frameworks')
        result = audit(copy, arch, '13.0')
        if result['errors']:
            raise RuntimeError('\n'.join(result['errors']))
        sign_bundle(copy)
        home = root/'home'
        home.mkdir()
        environment = dict(PATH='/usr/bin:/bin:/usr/sbin:/sbin', HOME=str(home),
                           CFFIXED_USER_HOME=str(home), TMPDIR=str(root))
        with (reports/'packaged-probe.log').open('w') as log:
            run(target, env=environment, cwd=root, stdout=log, stderr=subprocess.STDOUT, timeout=90)
        print((reports/'packaged-probe.log').read_text(), flush=True)


def archive_bundle(bundle, destination, arch, source_sha):
    archive = destination/f'BrickSuite-v0.4.0-macOS-{arch}.zip'
    if archive.exists():
        raise RuntimeError('Archive output already exists')
    run('/usr/bin/ditto', '-c', '-k', '--sequesterRsrc', '--keepParent', bundle, archive)
    # Test the deliverable after extraction, including symlinks and signatures.
    with tempfile.TemporaryDirectory(prefix='bricksuite-archive-check-') as temp:
        run('/usr/bin/ditto', '-x', '-k', archive, temp)
        extracted = Path(temp)/'BrickSuite.app'
        validate_bundle(extracted, arch, source_sha)
        for path in bundle.rglob('*'):
            other = extracted/path.relative_to(bundle)
            if path.is_symlink():
                if not other.is_symlink() or os.readlink(path) != os.readlink(other):
                    raise RuntimeError(f'Archive lost symlink: {path}')
            elif path.is_file():
                if sha256(path) != sha256(other) or path.stat().st_mode & 0o777 != other.stat().st_mode & 0o777:
                    raise RuntimeError(f'Archive changed bytes or permissions: {path}')
    digest = sha256(archive)
    archive.with_suffix('.zip.sha256').write_text(f'{digest}  {archive.name}\n')
    return dict(filename=archive.name, bytes=archive.stat().st_size, sha256=digest)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    build, destination = args.build.resolve(), args.output.resolve()
    arch, source_sha = os.environ['ARCH'], os.environ['SOURCE_SHA']
    if arch not in ('arm64', 'x86_64') or output('uname', '-m') != arch:
        raise RuntimeError('Native runner architecture does not match job')
    if output('git', 'rev-parse', 'HEAD') != source_sha or output('git', 'status', '--porcelain'):
        raise RuntimeError('Source must be a clean checkout of the workflow SHA')
    cmake = os.environ['CMAKE']
    ctest = str(Path(cmake).with_name('ctest'))
    deps = Path(os.environ['DEPS_ROOT'])
    qt = deps/'qt'
    openssl = deps/'openssl'
    version = output(qt/'bin/qmake', '-query', 'QT_VERSION')
    qt_arch = output('lipo', '-archs', qt/'lib/QtCore.framework/Versions/A/QtCore')
    if version != '6.10.3' or qt_arch != arch:
        raise RuntimeError(f'Qt version/architecture mismatch: {version} {qt_arch}')
    print(f'Qt version={version}, architecture={qt_arch}, prefix={qt}', flush=True)
    reports = build/'reports'
    reports.mkdir(parents=True, exist_ok=True)
    metadata = dict(source_commit=source_sha, architecture=arch,
                    runner=os.environ['RUNNER_LABEL'], os=output('sw_vers'),
                    xcode=output('xcodebuild', '-version'), clang=output('/usr/bin/clang', '--version'),
                    sdk=output('xcrun', '--sdk', 'macosx', '--show-sdk-version'),
                    qt_version=version, qt_architecture=qt_arch, qt_prefix=str(qt),
                    deployment_target='13.0', schema=35, protocol='1.5',
                    signing='ad-hoc; non-notarized development/release-candidate',
                    source_lock=json.loads((SCRIPTS/'source-lock.json').read_text()))
    metadata['project_dependency_pins'] = {
        name: re.findall(r'GIT_TAG ([0-9a-f]{40})', (REPO/'cmake'/name).read_text())
        for name in ('BrickSuiteMcut.cmake', 'BrickSuiteLib3mf.cmake')}
    (reports/'build-metadata.json').write_text(json.dumps(metadata, indent=2)+'\n')
    run(cmake, '-S', REPO, '-B', build, '-G', 'Unix Makefiles', '-DCMAKE_BUILD_TYPE=Release',
        '-DBUILD_TESTING=ON', '-DCMAKE_OSX_DEPLOYMENT_TARGET=13.0', f'-DCMAKE_OSX_ARCHITECTURES={arch}',
        f'-DCMAKE_PREFIX_PATH={qt}', f'-DOPENSSL_ROOT_DIR={openssl}',
        '-DCMAKE_C_COMPILER=/usr/bin/clang', '-DCMAKE_CXX_COMPILER=/usr/bin/clang++',
        '-DBRICKSUITE_CALIBRATION_LDRAW_ROOT=', '-DCMAKE_IGNORE_PREFIX_PATH=/opt/homebrew;/usr/local')
    targets = configured_targets(build)
    metadata['test_targets'] = targets
    run(cmake, '--build', build, '--parallel', os.environ.get('JOBS', '3'), '--target',
        'BrickSuite', 'BrickSuiteMeshBooleanWorker', *targets)
    inventory = json.loads(output(ctest, '--test-dir', build, '--show-only=json-v1'))
    (reports/'ctest-inventory.json').write_text(json.dumps(inventory, indent=2)+'\n')
    environment = dict(os.environ, DYLD_LIBRARY_PATH=str(openssl/'lib'))
    # Development-tree tests need runtime OpenSSL discovery. This variable is
    # deliberately absent from the packaged probe's clean environment.
    junit = reports/'ctest.xml'
    result = subprocess.run([ctest, '--test-dir', str(build), '--output-on-failure',
                             '--parallel', '1', '--timeout', '180', '--output-junit', str(junit)], env=environment)
    shutil.copy2(build/'Testing/Temporary/LastTest.log', reports/'ctest.log')
    metadata['tests'] = test_summary(inventory, junit)
    (reports/'build-metadata.json').write_text(json.dumps(metadata, indent=2)+'\n')
    if result.returncode or metadata['tests']['failed'] or metadata['tests']['skipped']:
        raise RuntimeError('Full configured CTest suite did not pass')
    # Existing bounded synthetic fixture checks admission, termination, deadline,
    # parent survival and temp-directory cleanup; require its explicit evidence.
    metadata['worker_containment'] = containment_evidence(junit)
    metadata['worker_memory_policy'] = '64 MiB allocation / 32 MiB test budget; sampled watchdog'
    metadata['unregistered_native_ui'] = ['LDrawViewportRenderTest (native UI acceptance is separate)']
    run(sys.executable, SCRIPTS/'build_probe.py', build, build/'BrickSuitePackageProbe')
    bundle = build/'staged/BrickSuite.app'
    run(sys.executable, SCRIPTS/'package_macos.py', '--build', build, '--qt', qt,
        '--qt-source', os.environ['QT_SOURCE'], '--openssl', openssl,
        '--openssl-source', os.environ['OPENSSL_SOURCE'], '--lib3mf-source', build/'_deps/lib3mf-src',
        '--output', bundle, '--arch', arch, '--source-sha', source_sha)
    audit_report = validate_bundle(bundle, arch, source_sha)
    packaged_probe(bundle, build/'BrickSuitePackageProbe', arch, reports)
    metadata['packaged_probe'] = 'passed: TLS, worker/self-launch, SQLite, Help/F1/icon, providers, runtime closure'
    metadata['mach_o_count'] = len(audit_report['binaries'])
    metadata['dependency_audit'] = 'passed'
    metadata['codesign'] = 'deep strict verification passed'
    # Nothing enters the upload directory until every preceding gate passes.
    destination.mkdir(parents=True, exist_ok=False)
    metadata['archive'] = archive_bundle(bundle, destination, arch, source_sha)
    (destination/'build-metadata.json').write_text(json.dumps(metadata, indent=2)+'\n')
    (destination/'bundle-audit.json').write_text(json.dumps(audit_report, indent=2)+'\n')
    for name in ('ctest.xml', 'packaged-probe.log'):
        shutil.copy2(reports/name, destination/name)
    summary = json.dumps(metadata, indent=2)+'\n'
    print(summary)
    if os.environ.get('GITHUB_STEP_SUMMARY'):
        with open(os.environ['GITHUB_STEP_SUMMARY'], 'a') as stream:
            stream.write(f'### {arch}: non-notarized candidate\n\n```json\n{summary}```\n')


if __name__ == '__main__':
    main()
