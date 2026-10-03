#!/usr/bin/env python3
"""Synthetic orchestration regressions; no baseline downloads or user data."""
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import build_release_artifact as release


class ReleaseArtifactTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.base = Path(self.temp.name) / 'managed'
        self.runner = release.Runner(io.StringIO())

    def identity(self):
        return {'source_sha': 'abc', 'version': '0.4.0', 'schema': 35, 'protocol': '1.5'}

    def candidate(self):
        output = self.base / 'work/release'
        output.mkdir(parents=True)
        metadata = dict(self.identity(), architecture='x86_64', build_distro='Ubuntu 22.04.5',
                        glibc_floor='2.35', qt='6.10.3',
                        package_kind='ubuntu-22.04-baseline-release', test_gate='passed')
        for name, value in [('build-metadata', metadata), ('dependencies', {}),
                            ('abi-audit', {'ceilings': {'GLIBC': '2.35'}})]:
            (output / (name + '.json')).write_text(json.dumps(value))
        archive = output / 'BrickSuite-v0.4.0-Linux-x86_64.tar.gz'
        archive.write_bytes(b'synthetic archive')
        (output / (archive.name + '.sha256')).write_text(release.digest(archive) + '  ' + archive.name + '\n')
        for name, value in [('build/configured-tests.json', {'count': 137}),
                            ('evidence/tests/summary.json', {'full': {'passed': 137, 'failed': 0, 'skipped': 0},
                                                            'focused': {'passed': 29, 'failed': 0, 'skipped': 0}})]:
            path = self.base / 'work' / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(json.dumps(value))
        return output

    def test_clean_source(self):
        repo = Path(__file__).resolve().parents[2]
        with patch.object(subprocess, 'check_output', side_effect=['', 'abc', 'heads/main']):
            identity = release.source_identity(repo)
        self.assertEqual(identity, dict(self.identity(), branch_tag='heads/main'))

    def test_dirty_source(self):
        with patch.object(subprocess, 'check_output', return_value='?? untracked.cpp'):
            with self.assertRaisesRegex(release.ReleaseError, 'not clean.*\n.*untracked'):
                release.source_identity(self.base)

    def test_missing_tools_and_ca(self):
        with patch.object(release.shutil, 'which', side_effect=lambda tool: None if tool == 'bwrap' else '/bin/' + tool), \
                patch.object(release.ssl, 'create_default_context', side_effect=OSError('no CA')):
            self.assertEqual(release.missing_prerequisites(), ['bubblewrap', 'ca-certificates'])

    def test_decline_never_runs_sudo(self):
        with patch.object(release, 'missing_prerequisites', return_value=['bubblewrap']), \
                patch.object(Path, 'read_text', return_value='ID=ubuntu\n'), \
                patch.object(sys.stdin, 'isatty', return_value=True), \
                patch('builtins.input', return_value='n'), patch.object(self.runner, 'run') as run:
            with self.assertRaisesRegex(release.ReleaseError, 'declined'):
                release.preflight(self.runner, False)
            run.assert_not_called()
        self.assertIn('sudo apt-get install -y bubblewrap', self.runner.log.getvalue())

    def test_noninteractive_never_installs(self):
        with patch.object(release, 'missing_prerequisites', return_value=['xz-utils']), \
                patch.object(Path, 'read_text', return_value='ID=ubuntu\n'), \
                patch.object(self.runner, 'run') as run:
            with self.assertRaises(release.ReleaseError):
                release.preflight(self.runner, True)
            run.assert_not_called()

    def test_child_failure_stops_sequence_and_preserves_code(self):
        sentinel = Path(self.temp.name) / 'should-not-exist'
        with self.assertRaises(release.ReleaseError) as error:
            for command in [[sys.executable, '-c', 'print("failure evidence"); raise SystemExit(7)'],
                            [sys.executable, '-c', f'open({str(sentinel)!r}, "w").close()']]:
                self.runner.run(command)
        self.assertEqual(error.exception.code, 7)
        self.assertFalse(sentinel.exists())
        self.assertIn('failure evidence', self.runner.log.getvalue())

    def test_main_failure_preserves_release_and_returns_child_code(self):
        repo = Path(self.temp.name) / 'repo'
        base = repo / 'build/linux-release'
        release.workspace(base)
        previous = base / 'published/previous'
        previous.mkdir(parents=True)
        (previous / 'archive').write_bytes(b'previous valid release')
        (base / 'release').symlink_to('published/previous')
        commands = [('Synthetic failure', [sys.executable, '-c', 'raise SystemExit(9)']),
                    ('Must not run', [sys.executable, '-c', 'print("UNEXPECTED STAGE")'])]
        with patch.object(release, '__file__', str(repo / 'deployment/linux/wrapper.py')), \
                patch.object(sys, 'argv', ['wrapper']), \
                patch.object(release, 'preflight'), \
                patch.object(release, 'source_identity', return_value=self.identity()), \
                patch.object(release, 'stages', return_value=commands):
            self.assertEqual(release.main(), 9)
        log = (base / 'logs/release-build.log').read_text()
        self.assertIn('FAILED: Synthetic failure', log)
        self.assertNotIn('UNEXPECTED STAGE', log)
        self.assertNotIn('created successfully', log)
        self.assertEqual((base / 'release/archive').read_bytes(), b'previous valid release')

    def test_checksum_and_dynamic_summary(self):
        output = self.candidate()
        result = release.verify_output(output, self.base / 'work', self.identity())
        summary = release.summary(result, self.base, 12.3)
        for text in ['137/137 passed', '29 passed', 'Checksum: verified', '12.3 seconds', result['sha256']]:
            self.assertIn(text, summary)
        self.assertEqual(result['bytes'], len(b'synthetic archive'))

    def test_checksum_mismatch(self):
        output = self.candidate()
        next(output.glob('*.tar.gz')).write_bytes(b'damaged')
        with self.assertRaisesRegex(release.ReleaseError, 'MUST NOT be distributed'):
            release.verify_output(output, self.base / 'work', self.identity())

    def test_wrong_identity(self):
        output = self.candidate()
        with self.assertRaisesRegex(release.ReleaseError, 'identity mismatch'):
            release.verify_output(output, self.base / 'work', dict(self.identity(), source_sha='wrong'))

    def test_skips_rejected(self):
        output = self.candidate()
        path = self.base / 'work/evidence/tests/summary.json'
        data = json.loads(path.read_text())
        data['full']['skipped'] = 1
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(release.ReleaseError, 'summary'):
            release.verify_output(output, self.base / 'work', self.identity())

    def test_unmanaged_workspace_refused(self):
        self.base.mkdir()
        (self.base / 'personal').write_text('preserve')
        with self.assertRaisesRegex(release.ReleaseError, 'unmanaged'):
            release.workspace(self.base)
        self.assertTrue((self.base / 'personal').exists())

    def test_redirected_workspace_refused(self):
        self.base.symlink_to(Path(self.temp.name), target_is_directory=True)
        with self.assertRaisesRegex(release.ReleaseError, 'symlinked'):
            release.workspace(self.base)

    def test_fresh_workspace_preserves_cache_logs_and_release(self):
        release.workspace(self.base)
        for name in ['rootfs', 'work', 'downloads', 'published']:
            (self.base / name).mkdir()
            (self.base / name / 'data').write_text(name)
        (self.base / 'bootstrap-complete.json').write_text('{}')
        (self.base / 'logs/evidence').write_text('log')
        release.fresh_work(self.base)
        for name in ['rootfs', 'work', 'bootstrap-complete.json']:
            self.assertFalse((self.base / name).exists())
        for name in ['downloads/data', 'published/data', 'logs/evidence']:
            self.assertTrue((self.base / name).exists())

    def test_redirected_cleanup_refused(self):
        release.workspace(self.base)
        (self.base / 'rootfs').symlink_to(Path(self.temp.name))
        with self.assertRaises(release.ReleaseError):
            release.fresh_work(self.base)

    def test_failed_publication_keeps_previous(self):
        release.workspace(self.base)
        candidate = self.candidate()
        release.publish(self.base, candidate)
        previous = (self.base / 'release').resolve()
        candidate.mkdir(parents=True)
        (candidate / 'incomplete').touch()
        with patch.object(release.os, 'replace', side_effect=OSError('synthetic failure')):
            with self.assertRaises(OSError):
                release.publish(self.base, candidate)
        self.assertEqual((self.base / 'release').resolve(), previous)
        self.assertTrue(next(previous.glob('*.tar.gz')).exists())
        release.fresh_work(self.base)
        self.assertEqual((self.base / 'release').resolve(), previous)

    def test_stage_order_uses_authoritative_runners(self):
        commands = [list(map(str, command)) for _, command in release.stages(Path('/repo'), self.base, 3)]
        for command, name in zip(commands, ['bootstrap.py', 'build_release.py', 'isolated_session.py',
                                          'test_release_tools.py', 'test_installer.py', 'build_probe.py', 'package_linux.py']):
            self.assertTrue(any(item.endswith('/' + name) for item in command))
        self.assertIn('/src/deployment/linux/run_tests.py', commands[2])
        self.assertNotIn('--stage-only', commands[-1])
        for command in commands[1:]:
            self.assertIn('/repo/deployment/linux/run_baseline.py', command)


if __name__ == '__main__':
    unittest.main()
