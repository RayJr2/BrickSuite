#!/usr/bin/env python3
"""Build the official Linux artifact using the authoritative baseline pipeline."""
import argparse
import fcntl
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import signal
import ssl
import subprocess
import sys
import time
import uuid

from audit_bundle import digest

MARKER = '.bricksuite-release-workspace'
MARKER_TEXT = 'BrickSuite official Linux release workspace v1\n'
TOOLS = {'git': 'git', 'tar': 'tar', 'xz': 'xz-utils', 'bwrap': 'bubblewrap'}


class ReleaseError(Exception):
    def __init__(self, message, code=1):
        super().__init__(message)
        self.code = code


class Runner:
    def __init__(self, log):
        self.log = log
        self.stage = 'Preflight'

    def say(self, message):
        print(message, flush=True)
        self.log.write(message + '\n')
        self.log.flush()

    def run(self, arguments):
        arguments = [str(arg) for arg in arguments]
        self.say('$ ' + shlex.join(arguments))
        child = subprocess.Popen(arguments, stdout=subprocess.PIPE,
                                 stderr=subprocess.STDOUT, text=True, errors='replace',
                                 start_new_session=True)
        try:
            for line in child.stdout:
                self.say(line.rstrip('\n'))
            code = child.wait()
            if code:
                raise ReleaseError(f'{self.stage}: command exited {code}', code if code > 0 else 1)
        except BaseException:
            # Also stop grandchildren when the stage leader has already exited.
            try:
                os.killpg(child.pid, signal.SIGTERM)
                child.wait(timeout=10)
            except subprocess.TimeoutExpired:
                os.killpg(child.pid, signal.SIGKILL)
                child.wait()
            except ProcessLookupError:
                pass
            raise
        finally:
            child.stdout.close()


def missing_prerequisites():
    missing = {package for tool, package in TOOLS.items() if not shutil.which(tool)}
    try:
        context = ssl.create_default_context(cafile='/etc/ssl/certs/ca-certificates.crt')
        if not context.get_ca_certs():
            missing.add('ca-certificates')
    except (OSError, ssl.SSLError):
        missing.add('ca-certificates')
    return sorted(missing)


def preflight(runner, no_install):
    if platform.system() != 'Linux' or platform.machine() != 'x86_64':
        raise ReleaseError('Official baseline generation requires a Linux x86_64 host.')
    missing = missing_prerequisites()
    if missing:
        runner.say('Missing host prerequisites: ' + ', '.join(missing))
        command = ['sudo', 'apt-get', 'install', '-y', *missing]
        os_release = Path('/etc/os-release').read_text()
        if not re.search(r'^ID=["\']?ubuntu["\']?$', os_release, re.M):
            raise ReleaseError('Install the missing prerequisites with your distribution package manager, then retry.')
        runner.say('Install command: sudo apt-get update && ' + shlex.join(command))
        if no_install or not sys.stdin.isatty():
            raise ReleaseError('Prerequisites missing; run the install command above, then retry.')
        if input('Install missing packages now? [Y/n] ').strip().lower() not in ('', 'y', 'yes'):
            raise ReleaseError('Installation declined; run the install command above when ready.')
        runner.run(['sudo', 'apt-get', 'update'])
        runner.run(command)
        if missing_prerequisites():
            raise ReleaseError('Prerequisites remain unavailable after installation.')
    # Detect namespace/AppArmor restrictions before downloading or rebuilding.
    runner.run(['bwrap', '--unshare-user', '--uid', '0', '--gid', '0',
                '--ro-bind', '/', '/', '--', '/bin/true'])


def source_identity(repo):
    def git(*args):
        return subprocess.check_output(['git', '-C', str(repo), *args], text=True).strip()
    status = git('status', '--short', '--untracked-files=all')
    if status:
        raise ReleaseError('Refusing official release build: working tree is not clean.\n' + status)
    cmake = (repo / 'CMakeLists.txt').read_text()
    schema = (repo / 'src/database/DatabaseSchema.h').read_text()
    protocol = (repo / 'src/network/BrickSuiteProtocol.h').read_text()
    return {'source_sha': git('rev-parse', 'HEAD'),
            'branch_tag': git('describe', '--all', '--always'),
            'version': re.search(r'project\(BrickSuite\s+VERSION\s+(\S+)', cmake)[1],
            'schema': int(re.search(r'CurrentSchemaVersion\s*=\s*(\d+)', schema)[1]),
            'protocol': '.'.join(re.search(r'\b' + key + r'\s*=\s*(\d+)', protocol)[1]
                                 for key in ('Major', 'Minor'))}


def workspace(base):
    # Only our fixed repository-relative directory is used by the CLI. Reject
    # symlink ancestors too, so cleanup cannot traverse a redirected build/.
    if any(path.is_symlink() for path in [base, *base.parents]):
        raise ReleaseError('Refusing a symlinked release workspace.')
    if base.exists():
        marker = base / MARKER
        if marker.is_symlink() or not marker.is_file() or marker.read_text() != MARKER_TEXT:
            raise ReleaseError('Refusing unmanaged release workspace: ' + str(base))
    else:
        base.mkdir(parents=True)
        (base / MARKER).write_text(MARKER_TEXT)
    for name in ('downloads', 'rootfs', 'work', 'logs', 'published', 'bootstrap-complete.json', '.lock'):
        if (base / name).is_symlink():
            raise ReleaseError('Refusing redirected managed path: ' + str(base / name))
    (base / 'logs').mkdir(exist_ok=True)


def fresh_work(base):
    for name in ('rootfs', 'work'):
        path = base / name
        if path.is_symlink():
            raise ReleaseError('Refusing redirected managed path: ' + str(path))
        if path.exists():
            shutil.rmtree(path)
    (base / 'bootstrap-complete.json').unlink(missing_ok=True)
    # downloads/ is bootstrap.py's rehashed cache, deliberately retained.


def stages(repo, base, jobs):
    scripts = repo / 'deployment/linux'
    baseline = [sys.executable, scripts / 'run_baseline.py', '--root', base / 'rootfs',
                '--work', base / 'work', '--']
    def inside(script, *args):
        return [*baseline, 'python3', '/src/deployment/linux/' + script, *args]
    return [
        ('Bootstrap Ubuntu 22.04 baseline', [sys.executable, scripts / 'bootstrap.py', '--directory', base, '--jobs', str(jobs)]),
        ('Build Release and all configured tests', inside('build_release.py', '--jobs', str(jobs))),
        ('Run isolated full and focused application tests', inside('isolated_session.py', '--output', '/work/evidence/tests-session', '--', 'python3', '/src/deployment/linux/run_tests.py')),
        ('Run release-tool regressions', inside('test_release_tools.py')),
        ('Run installer regressions', inside('test_installer.py')),
        ('Build package probes', inside('build_probe.py')),
        ('Package and audit official artifact', inside('package_linux.py')),
    ]


def verify_output(output, work, identity):
    metadata = json.loads((output / 'build-metadata.json').read_text())
    for key in ('source_sha', 'version', 'schema', 'protocol'):
        if metadata[key] != identity[key]:
            raise ReleaseError('Package identity mismatch: ' + key)
    if metadata['package_kind'] != 'ubuntu-22.04-baseline-release' or metadata['test_gate'] != 'passed':
        raise ReleaseError('Package is not a gated official baseline release.')
    configured = json.loads((work / 'build/configured-tests.json').read_text())
    tests = json.loads((work / 'evidence/tests/summary.json').read_text())
    if configured['count'] <= 0 or tests['full'] != {'passed': configured['count'], 'failed': 0, 'skipped': 0}:
        raise ReleaseError('Full test summary does not match configured tests.')
    if tests['focused']['passed'] <= 0 or tests['focused']['failed'] or tests['focused']['skipped']:
        raise ReleaseError('Focused test gate failed.')
    archive = output / f"BrickSuite-v{metadata['version']}-Linux-x86_64.tar.gz"
    checksum = (output / (archive.name + '.sha256')).read_text().split()
    actual = digest(archive)
    if checksum != [actual, archive.name]:
        raise ReleaseError('Checksum verification failed; artifact MUST NOT be distributed.')
    abi = json.loads((output / 'abi-audit.json').read_text())
    if not (output / 'dependencies.json').is_file():
        raise ReleaseError('Missing dependency manifest.')
    return {'metadata': metadata, 'tests': tests, 'archive': archive.name,
            'bytes': archive.stat().st_size, 'sha256': actual, 'abi': abi['ceilings']}


def publish(base, candidate):
    # Immutable generations plus one atomic symlink replacement: the old release
    # remains accessible throughout build, verification, and publication.
    target = base / 'release'
    if target.exists() and not target.is_symlink():
        raise ReleaseError('Refusing unmanaged release output: ' + str(target))
    generations = base / 'published'
    generations.mkdir(exist_ok=True)
    name = uuid.uuid4().hex
    generation = generations / name
    candidate.rename(generation)
    link = base / ('.release-' + name)
    try:
        link.symlink_to(Path('published') / name, target_is_directory=True)
        os.replace(link, target)
    finally:
        link.unlink(missing_ok=True)


def summary(result, base, elapsed):
    meta = result['metadata']
    return '\n'.join([
        '=' * 60, 'BrickSuite Linux release artifact created successfully', '=' * 60,
        'Source commit: ' + meta['source_sha'], 'Version: ' + meta['version'],
        'Architecture: ' + meta['architecture'],
        'Baseline: ' + meta['build_distro'] + ' / glibc ' + meta['glibc_floor'],
        'Qt: ' + meta['qt'],
        f"Tests: {result['tests']['full']['passed']}/{result['tests']['full']['passed']} passed, 0 skipped",
        f"Focused tests: {result['tests']['focused']['passed']} passed, 0 skipped",
        'Package: ' + str(base / 'release' / result['archive']),
        f"Size: {result['bytes']} bytes", 'SHA-256: ' + result['sha256'],
        'Checksum: verified', 'Measured ABI: ' + json.dumps(result['abi']),
        'Release metadata: ' + str(base / 'release') + '/{build-metadata,dependencies,abi-audit}.json',
        f'Duration: {elapsed:.1f} seconds', 'Ready for clean-machine/user acceptance.'])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs', type=int, default=6)
    parser.add_argument('--no-install-prerequisites', action='store_true')
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('--jobs must be positive')
    repo = Path(__file__).resolve().parents[2]
    base = repo / 'build/linux-release'
    started = time.monotonic()
    runner = None
    try:
        workspace(base)
        with (base / '.lock').open('a') as lock:
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                raise ReleaseError('Another release generation is already running.')
            logpath = base / 'logs/release-build.log'
            if logpath.exists():
                logpath.rename(logpath.with_name('release-build-' + uuid.uuid4().hex + '.log'))
            with logpath.open('w') as log:
                runner = Runner(log)
                try:
                    runner.say('[1/10] Preflight')
                    preflight(runner, args.no_install_prerequisites)
                    identity = source_identity(repo)
                    runner.say('Source identity: ' + json.dumps(identity))
                    (base / 'logs/source-identity.json').write_text(json.dumps(identity, indent=2) + '\n')
                    runner.stage = 'Prepare fresh managed workspace'
                    runner.say('[2/10] ' + runner.stage)
                    fresh_work(base)
                    for number, (name, command) in enumerate(stages(repo, base, args.jobs), 3):
                        runner.stage = name
                        runner.say(f'[{number}/10] {name}')
                        runner.run(command)
                    runner.stage = 'Verify checksum and publish'
                    runner.say('[10/10] ' + runner.stage)
                    if source_identity(repo) != identity:
                        raise ReleaseError('Source identity changed during release generation.')
                    result = verify_output(base / 'work/release', base / 'work', identity)
                    publish(base, base / 'work/release')
                    runner.say(summary(result, base, time.monotonic() - started))
                except KeyboardInterrupt:
                    runner.say('Release generation canceled.')
                    return 130
                except (ReleaseError, OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
                    runner.say(f'FAILED: {runner.stage}: {error}\nLog: {logpath}')
                    return error.code if isinstance(error, ReleaseError) else 1
    except (ReleaseError, OSError) as error:
        print(str(error), file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print('Release generation canceled.', file=sys.stderr)
        return 130
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
