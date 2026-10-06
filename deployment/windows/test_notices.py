"""Windows notice staging regressions; synthetic inputs, no installed SDK required."""
from pathlib import Path
import subprocess
import shutil
import tempfile
import unittest

SCRIPT = Path(__file__).with_name('stage_notices.ps1').resolve()
LIB3MF = ['LICENSE', 'Libraries/libressl/COPYING', 'submodules/zlib/LICENSE',
          'submodules/libzip/LICENSE', 'submodules/cpp-base64/LICENSE', 'submodules/fast_float/LICENSE-MIT']
MCUT = ['LICENSE.txt', 'COPYING', 'COPYING.LESSER', 'include/mcut/internal/cdt/LICENSE.txt',
        'BrickSuite-integration.md', 'BrickSuiteMcutPortability.cmake', 'BrickSuiteMcutLinuxQueue.cmake']

class NoticeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='bricksuite-notices-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.build = self.root/'build'
        self.stage = self.root/'stage'
        self.qt = self.root/'qt/sdk'
        self.source = self.root/'qt/Src'
        self.ssl = self.root/'openssl'
        self.compiler = self.root/'compiler'
        def put(path, text='Synthetic notice\n'):
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(text)
        self.put = put
        for family, names in [('lib3mf', LIB3MF), ('MCUT', MCUT)]:
            for name in names: put(self.build/'deployment/licenses'/family/name)
        put(self.ssl/'share/licenses/openssl/LICENSE')
        for name in ['gcc/COPYING.RUNTIME', 'gcc/COPYING3', 'gcc/COPYING3.LIB',
                     'mingw-w64/COPYING', 'winpthreads/COPYING']:
            put(self.compiler/'licenses'/name)
        put(self.build/'CMakeCache.txt', f'CMAKE_CXX_COMPILER:FILEPATH={self.compiler}/bin/g++.exe\n')
        put(self.qt/'lib/cmake/Qt6/Qt6ConfigVersionImpl.cmake', 'set(PACKAGE_VERSION "6.10.3")')
        for module in ['qtbase', 'qtsvg', 'qtimageformats', 'qtwebsockets']:
            put(self.source/module/'.cmake.conf', 'set(QT_REPO_MODULE_VERSION "6.10.3")')
            put(self.source/module/'LICENSE')
            if module != 'qtwebsockets':
                put(self.source/module/'thirdparty/qt_attribution.json', '[{"LicenseFile":"terms.txt"}]')
                put(self.source/module/'thirdparty/terms.txt')
        for name in ['LGPL-3.0-only.txt', 'GPL-3.0-only.txt', 'GPL-2.0-only.txt']:
            put(self.source/'LICENSES'/name)

    def run_stage(self):
        return subprocess.run(['powershell.exe', '-NoProfile', '-ExecutionPolicy', 'Bypass',
            '-File', str(SCRIPT), '-BuildDir', str(self.build), '-StageDir', str(self.stage),
            '-QtRoot', str(self.qt), '-OpenSslRoot', str(self.ssl), '-QtSource', str(self.source)],
            capture_output=True, text=True)

    def test_complete_inventory_and_exact_copies(self):
        result = self.run_stage()
        self.assertEqual(result.returncode, 0, result.stderr)
        manifest = (self.stage/'licenses/manifest.txt').read_text(encoding='utf-8-sig').splitlines()
        self.assertEqual(len(manifest), len(set(manifest)))
        for family, names in [('lib3mf', LIB3MF), ('MCUT', MCUT)]:
            for name in names:
                self.assertIn(f'{family}/{name}', manifest)
                self.assertEqual((self.stage/'licenses'/family/name).read_bytes(),
                                 (self.build/'deployment/licenses'/family/name).read_bytes())
        self.assertIn('Qt/qtbase/thirdparty/terms.txt', manifest)
        self.assertIn('OpenSSL/LICENSE', manifest)
        self.assertIn('MinGW/gcc/COPYING.RUNTIME', manifest)
        self.assertFalse((self.stage/'licenses/MCUT/LICENSE').exists())
        actual = {p.relative_to(self.stage/'licenses').as_posix()
                  for p in (self.stage/'licenses').rglob('*') if p.is_file()}
        self.assertEqual(actual, set(manifest) | {'manifest.txt'})

    def test_missing_required_inputs_fail_before_copy(self):
        for path in [self.build/'deployment/licenses/lib3mf'/p for p in LIB3MF] + [
                self.build/'deployment/licenses/MCUT/COPYING.LESSER',
                self.ssl/'share/licenses/openssl/LICENSE',
                self.source/'LICENSES/LGPL-3.0-only.txt',
                self.source/'qtbase/thirdparty/terms.txt',
                self.compiler/'licenses/gcc/COPYING.RUNTIME']:
            with self.subTest(path=path.relative_to(self.root)):
                original = path.read_bytes()
                path.unlink()
                result = self.run_stage()
                path.write_bytes(original)
                self.assertNotEqual(result.returncode, 0)
                self.assertIn('Required notice missing or empty', result.stderr)
                self.assertFalse(self.stage.exists())

    def test_empty_notice_rejected(self):
        self.put(self.build/'deployment/licenses/lib3mf/LICENSE', '')
        self.assertNotEqual(self.run_stage().returncode, 0)
        self.assertFalse(self.stage.exists())

    def test_mismatched_qt_rejected(self):
        self.put(self.source/'qtbase/.cmake.conf', 'set(QT_REPO_MODULE_VERSION "6.9.0")')
        result = self.run_stage()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('version mismatch', result.stderr)

    def test_changed_software_renderer_rejected(self):
        self.put(self.qt/'bin/opengl32sw.dll', 'Unreviewed renderer')
        result = self.run_stage()
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Software OpenGL runtime changed', result.stderr)

    def test_sdk_notice_required_when_d3d_is_bundled(self):
        self.put(self.qt/'bin/d3dcompiler_47.dll', 'Synthetic SDK runtime')
        self.assertNotEqual(self.run_stage().returncode, 0)
        self.put(self.root/'Licenses/sdk_license')
        result = self.run_stage()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertTrue((self.stage/'licenses/WindowsSDK/sdk_license').is_file())

    def test_real_batch_stops_before_deployment_on_missing_lib3mf(self):
        repo = self.root/'repo'
        scripts = repo/'deployment/windows'
        scripts.mkdir(parents=True)
        for name in ['package_windows.bat', 'stage_notices.ps1']:
            shutil.copyfile(SCRIPT.with_name(name), scripts/name)
        for name in ['LICENSE', 'THIRD_PARTY_NOTICES.md', 'resources/icons/bricksuite.ico']:
            self.put(repo/name)
        for name in ['BrickSuite.exe', 'BrickSuiteMeshBooleanWorker.exe', 'libmcut.dll']:
            self.put(self.build/name, 'Not executable; must never be launched')
        self.put(self.qt/'bin/windeployqt.exe', 'Not executable; must never be launched')
        for name in ['libcrypto-3-x64.dll', 'libssl-3-x64.dll']:
            self.put(self.ssl/'bin'/name)
        with (self.build/'CMakeCache.txt').open('a') as stream:
            stream.write(f'Qt6_DIR:PATH={self.qt}/lib/cmake/Qt6\nOPENSSL_ROOT_DIR:PATH={self.ssl}\n')
        (self.build/'deployment/licenses/lib3mf/LICENSE').unlink()
        result = subprocess.run(['cmd.exe', '/d', '/c', str(scripts/'package_windows.bat'), str(self.build)],
                                capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('Required third-party notice staging failed', result.stdout)
        self.assertNotIn('Running windeployqt', result.stdout)
        self.assertFalse((self.build/'deploy/installer').exists())

    def test_installer_integration_is_fail_closed(self):
        batch = SCRIPT.with_name('package_windows.bat').read_text()
        start = batch.index('powershell.exe -NoProfile')
        end = batch.index('echo Copying BrickSuite icon', start)
        self.assertIn('if errorlevel 1 (', batch[start:end])
        self.assertIn('exit /b 1', batch[start:end])
        self.assertLess(start, batch.index('"%ISCC%" /DStageDir='))
        installer = SCRIPT.with_name('BrickSuite.iss').read_text()
        self.assertIn('Source: "{#StageDir}\\*"; DestDir: "{app}";', installer)
        self.assertIn('recursesubdirs', installer)

if __name__ == '__main__':
    unittest.main()
