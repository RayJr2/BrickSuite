"""Portable negative-path regression tests for fail-closed release gates."""
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from audit_bundle import audit
from ci_release import configured_targets, containment_evidence, test_summary


class ReleaseGates(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name).resolve()
        self.bundle = self.root/'BrickSuite.app'
        self.main = self.bundle/'Contents/MacOS/BrickSuite'
        self.lib = self.bundle/'Contents/Frameworks/libfixture.dylib'
        for path in (self.main, self.lib):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'\xcf\xfa\xed\xfe')
        self.arch = 'arm64'
        self.minimum = '13.0'
        self.rpath = '@executable_path/../Frameworks'
        self.dependency = '@rpath/libfixture.dylib'

    def tearDown(self):
        self.temp.cleanup()

    def tool(self, *args):
        if args[0] == 'lipo':
            return self.arch
        if args[1] == '-l':
            return ('cmd LC_BUILD_VERSION\nminos '+self.minimum+'\n' +
                    (f'cmd LC_RPATH\ncmdsize 40\npath {self.rpath} (offset 12)' if self.rpath else ''))
        return args[-1]+':\n'+(' '+self.dependency+' (compatibility version 1.0)' if args[-1] == str(self.main) else '')

    def result(self):
        with patch('audit_bundle.output', self.tool):
            return audit(self.bundle, 'arm64', '13.0')

    def test_closed_single_architecture(self):
        self.assertEqual(self.result()['errors'], [])

    def test_mixed_and_wrong_architecture_rejected(self):
        for value in ('x86_64', 'arm64 x86_64'):
            self.arch = value
            self.assertTrue(self.result()['errors'])

    def test_runner_os_minimum_rejected(self):
        self.minimum = '15.0'
        self.assertTrue(self.result()['errors'])

    def test_no_invented_rpath_resolution(self):
        self.rpath = ''
        self.assertTrue(any('unresolved' in e for e in self.result()['errors']))

    def test_external_dependency_rejected(self):
        for dep in ('/opt/homebrew/lib/libfixture.dylib', '/Users/runner/work/build/libfixture.dylib'):
            self.dependency = dep
            self.assertTrue(self.result()['errors'])

    def test_external_rpath_and_symlink_rejected(self):
        self.rpath = '@loader_path/../../../../outside'
        self.assertTrue(self.result()['errors'])
        self.rpath = '@executable_path/../Frameworks'
        (self.bundle/'external').symlink_to(self.root)
        self.assertTrue(self.result()['errors'])

    def test_derive_targets_and_reject_external_commands(self):
        file = self.root/'CTestTestfile.cmake'
        file.write_text(f'add_test(One "{self.root}/OneTest")\nadd_test(Two "{self.root}/OneTest" "--case")\n')
        self.assertEqual(configured_targets(self.root), ['OneTest'])
        file.write_text('add_test(Unsafe "/other/path/Test")\n')
        with self.assertRaises(RuntimeError):
            configured_targets(self.root)

    def test_counts_distinguish_skipped_and_unregistered(self):
        inventory = {'tests': [{'name': n} for n in ('Pass', 'Fail', 'Skip')]}
        path = self.root/'ctest.xml'
        path.write_text('<testsuite><testcase name="Pass"/><testcase name="Fail"><failure/></testcase>'
                        '<testcase name="Skip"><skipped/></testcase></testsuite>')
        summary = test_summary(inventory, path)
        self.assertEqual([summary[k] for k in ('configured', 'passed', 'failed', 'skipped')], [3, 1, 1, 1])
        self.assertIn('PrintPreparation3021', summary['unregistered_ldraw'])
        inventory['tests'].append({'name': 'Missing'})
        with self.assertRaises(RuntimeError):
            test_summary(inventory, path)

    def test_containment_requires_bounded_cleanup_evidence(self):
        path = self.root/'ctest.xml'
        good = '\n'.join(f'Worker mode {mode}: elapsed=20 ms, cleanup=1' for mode in ('success', 'memory', 'timeout'))
        def write(log):
            path.write_text('<testsuite><testcase name="McutMeshBoolean"><system-out>'+log+
                            '</system-out></testcase></testsuite>')
        write(good)
        self.assertEqual(len(containment_evidence(path)), 3)
        for bad in (good.replace('cleanup=1', 'cleanup=0'), good.replace('20 ms', '4000 ms'), ''):
            write(bad)
            with self.assertRaises(RuntimeError):
                containment_evidence(path)


if __name__ == '__main__':
    unittest.main()
