"""Portable negative-path regression tests for fail-closed release gates."""
import tempfile
import unittest
import plistlib
import subprocess
import sys
from pathlib import Path
from unittest.mock import patch

from audit_bundle import audit
from ci_release import configured_targets, containment_evidence, test_summary
from sign_bundle import sign_bundle, signing_order


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


class SigningOrder(unittest.TestCase):
    def test_signing_failure_propagates_before_main_or_bundle(self):
        order = [Path('/fixture/helper'), Path('/fixture/main'), Path('/fixture/app')]
        failure = subprocess.CalledProcessError(1, ['codesign'])
        with patch('sign_bundle.signing_order', return_value=order), \
             patch('sign_bundle.subprocess.run', side_effect=failure) as run:
            with self.assertRaises(subprocess.CalledProcessError):
                sign_bundle(Path('/fixture/app'))
            self.assertEqual(run.call_count, 1)

    def test_main_and_bundle_last_and_missing_helper_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            bundle = Path(temp).resolve()/'BrickSuite.app'
            paths = ['Contents/MacOS/BrickSuite', 'Contents/MacOS/BrickSuiteMeshBooleanWorker',
                     'Contents/MacOS/BrickSuitePackageProbe', 'Contents/PlugIns/test.dylib',
                     'Contents/Frameworks/Test.framework/Versions/A/Test', 'Contents/Frameworks/libtest.dylib']
            for name in paths:
                path = bundle/name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'\xcf\xfa\xed\xfe')
            (bundle/'Contents/Info.plist').write_bytes(plistlib.dumps({'CFBundleExecutable': 'BrickSuite'}))
            order = signing_order(bundle)
            self.assertEqual(order[-2:], [bundle/paths[0], bundle])
            framework = bundle/'Contents/Frameworks/Test.framework'
            self.assertLess(order.index(bundle/paths[4]), order.index(framework))
            self.assertLess(order.index(framework), order.index(bundle/paths[1]))
            with patch('sign_bundle.subprocess.run') as run:
                sign_bundle(bundle)
                self.assertEqual(len(run.call_args_list), len(order)+1)
                for call in run.call_args_list[:-1]:
                    self.assertIn('--force', call.args[0])
                    self.assertIn('--timestamp=none', call.args[0])
                    self.assertNotIn('--deep', call.args[0])
                self.assertIn('--strict', run.call_args_list[-1].args[0])
            (bundle/paths[1]).unlink()
            with patch('sign_bundle.subprocess.run') as run:
                with self.assertRaisesRegex(ValueError, 'Missing required Mach-O'):
                    sign_bundle(bundle)
                run.assert_not_called()


@unittest.skipUnless(sys.platform == 'darwin', 'Native macOS codesign integration')
class NativeSigning(unittest.TestCase):
    def fixture(self, root, arch, dependencies=False):
        bundle = root/arch/'BrickSuite.app'
        executable = bundle/'Contents/MacOS'
        executable.mkdir(parents=True)
        source = root/'fixture.c'
        source.write_text('int main(void) { return 0; }\n')
        info = dict(CFBundleExecutable='BrickSuite', CFBundleIdentifier='com.rfstateside.signingfixture',
                    CFBundlePackageType='APPL', CFBundleVersion='1')
        (bundle/'Contents/Info.plist').write_bytes(plistlib.dumps(info))
        for name in ('BrickSuite', 'BrickSuiteMeshBooleanWorker', 'BrickSuitePackageProbe'):
            subprocess.run(['/usr/bin/clang', '-arch', arch, '-mmacosx-version-min=13.0',
                            str(source), '-o', str(executable/name)], check=True)
        if dependencies:
            framework = bundle/'Contents/Frameworks/Fixture.framework'
            resources = framework/'Versions/A/Resources'
            resources.mkdir(parents=True)
            info.update(CFBundleExecutable='Fixture', CFBundleIdentifier='com.rfstateside.fixtureframework',
                        CFBundlePackageType='FMWK')
            (resources/'Info.plist').write_bytes(plistlib.dumps(info))
            (framework/'Versions/Current').symlink_to('A')
            (framework/'Resources').symlink_to('Versions/Current/Resources')
            (framework/'Fixture').symlink_to('Versions/Current/Fixture')
            for path in (framework/'Versions/A/Fixture', bundle/'Contents/Frameworks/libfixture.dylib',
                         bundle/'Contents/PlugIns/platforms/libfixture.dylib'):
                path.parent.mkdir(parents=True, exist_ok=True)
                subprocess.run(['/usr/bin/clang', '-arch', arch, '-mmacosx-version-min=13.0',
                                '-dynamiclib', str(source), '-o', str(path)], check=True)
        from audit_bundle import binaries
        for path in binaries(bundle):
            # Deliberately remove incidental linker signatures on either arch.
            subprocess.run(['codesign', '--remove-signature', str(path)], check=True,
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return bundle

    def test_unsigned_helper_reproduces_failure_then_signs_and_detects_tampering(self):
        with tempfile.TemporaryDirectory() as temp:
            for arch in ('arm64', 'x86_64'):
                with self.subTest(architecture=arch):
                    bundle = self.fixture(Path(temp).resolve(), arch)
                    main = bundle/'Contents/MacOS/BrickSuite'
                    helper = bundle/'Contents/MacOS/BrickSuiteMeshBooleanWorker'
                    self.assertNotEqual(subprocess.run(['codesign', '-d', str(helper)], capture_output=True).returncode, 0)
                    old = subprocess.run(['codesign', '--force', '--sign', '-', str(main)], capture_output=True, text=True)
                    self.assertNotEqual(old.returncode, 0)
                    self.assertIn('BrickSuiteMeshBooleanWorker', old.stderr)
                    sign_bundle(bundle)  # Includes the production deep/strict gate.
                    subprocess.run(['codesign', '--remove-signature', str(helper)], check=True)
                    self.assertNotEqual(subprocess.run(['codesign', '--verify', '--deep', '--strict', str(bundle)],
                                                       capture_output=True).returncode, 0)
                    sign_bundle(bundle)  # Replaces mixed existing/missing signatures.
                    helper.unlink()
                    with self.assertRaisesRegex(ValueError, 'Missing required Mach-O'):
                        sign_bundle(bundle)

    def test_unsigned_framework_plugins_dylibs_and_probe_use_same_order(self):
        with tempfile.TemporaryDirectory() as temp:
            orders = []
            for arch in ('arm64', 'x86_64'):
                with self.subTest(architecture=arch):
                    bundle = self.fixture(Path(temp).resolve(), arch, dependencies=True)
                    orders.append([str(p.relative_to(bundle)) for p in signing_order(bundle)])
                    sign_bundle(bundle)
                    sign_bundle(bundle)  # Already-signed inputs follow exactly the same algorithm.
            self.assertEqual(orders[0], orders[1])


if __name__ == '__main__':
    unittest.main()
