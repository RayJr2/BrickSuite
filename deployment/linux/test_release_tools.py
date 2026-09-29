"""Regression tests for release rejection gates using real ELF payloads."""
import copy
import json
import os
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile
import unittest
from unittest.mock import patch
from audit_bundle import POLICY, audit, check_info, inspect
from package_linux import archive_tree
from normalize_qt_diagnostics import normalize, PREFIX, FILES

SHA='a'*40


class GateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory();cls.base=Path(cls.temp.name)
        source=cls.base/'main.c';source.write_text('int main(void) { return 0; }\n')
        cls.exe=cls.base/'exe'
        subprocess.run(['cc',str(source),'-o',str(cls.exe)],check=True)
        cls.info=inspect(cls.exe);cls.info['rpaths']=['$ORIGIN/../lib']
        cls.template=cls.base/'template';cls.make(cls.template)
    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()
    @classmethod
    def make(cls,bundle):
        for name in ['bin','lib','share/licenses','plugins']:(bundle/name).mkdir(parents=True,exist_ok=True)
        for name in ['BrickSuite','BrickSuiteMeshBooleanWorker']:
            shutil.copy2(cls.exe,bundle/'bin'/name)
            subprocess.run(['patchelf','--set-rpath','$ORIGIN/../lib',str(bundle/'bin'/name)],check=True)
        for name in POLICY['plugins']:
            p=bundle/'plugins'/name;p.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(cls.exe,p)
            subprocess.run(['patchelf','--set-rpath','$ORIGIN/../../lib',str(p)],check=True)
        for name in ['libssl.so.3','libcrypto.so.3']:
            shutil.copy2(cls.exe,bundle/'lib'/name)
            subprocess.run(['patchelf','--set-rpath','$ORIGIN',str(bundle/'lib'/name)],check=True)
        (bundle/'share/licenses/LICENSE').write_text('Synthetic test license\n')
        (bundle/'share/build-metadata.json').write_text(json.dumps({'source_sha':SHA,'required_licenses':['LICENSE']}))
        (bundle/'bin/qt.conf').write_text('[Paths]\nPrefix=..\nLibraries=lib\nPlugins=plugins\n')
        (bundle/'lib/relative-link').symlink_to('libssl.so.3')
    def setUp(self):
        self.run=tempfile.TemporaryDirectory(dir=self.base);self.bundle=Path(self.run.name)/'bundle';shutil.copytree(self.template,self.bundle,symlinks=True)
    def tearDown(self):self.run.cleanup()
    def test_valid_relocated_closure(self):
        target=Path(self.run.name)/'path with spaces/BrickSuite';target.parent.mkdir();self.bundle.rename(target)
        self.assertGreater(audit(target,SHA)['elf_count'],2)
    def test_private_library_resolution_with_spaces(self):
        source=Path(self.run.name)/'lib.c';source.write_text('int package_fixture(void) { return 0; }\n')
        library=self.bundle/'lib/libfixture.so'
        subprocess.run(['cc','-shared','-fPIC',str(source),'-o',str(library)],check=True)
        subprocess.run(['patchelf','--set-rpath','$ORIGIN',str(library)],check=True)
        subprocess.run(['patchelf','--add-needed','libfixture.so',str(self.bundle/'bin/BrickSuite')],check=True)
        target=Path(self.run.name)/'path with spaces/BrickSuite';target.parent.mkdir();self.bundle.rename(target)
        self.assertGreater(audit(target,SHA)['elf_count'],2)
    def test_glibc_ceiling(self):
        info=copy.deepcopy(self.info);info['requires']['GLIBC']='2.36'
        with self.assertRaisesRegex(ValueError,'exceeds'):check_info(info,'app','$ORIGIN/../lib')
    def test_cxx_runtime_ceiling(self):
        for prefix,bad in [('GLIBCXX','3.4.31'),('CXXABI','1.3.14')]:
            info=copy.deepcopy(self.info);info['requires'][prefix]=bad
            with self.assertRaisesRegex(ValueError,'exceeds'):check_info(info,'app','$ORIGIN/../lib')
    def test_absolute_runpath(self):
        subprocess.run(['patchelf','--set-rpath','/opt/development/lib',str(self.bundle/'bin/BrickSuite')],check=True)
        with self.assertRaisesRegex(ValueError,'RUNPATH'):audit(self.bundle,SHA)
    def test_unresolved_dependency(self):
        subprocess.run(['patchelf','--add-needed','libNotPackaged.so.1',str(self.bundle/'bin/BrickSuite')],check=True)
        with self.assertRaisesRegex(ValueError,'dependency'):audit(self.bundle,SHA)
    def test_architecture(self):
        info=copy.deepcopy(self.info);info['machine']='AArch64'
        with self.assertRaisesRegex(ValueError,'architecture'):check_info(info,'app','$ORIGIN/../lib')
    def test_missing_helper(self):
        (self.bundle/'bin/BrickSuiteMeshBooleanWorker').unlink()
        with self.assertRaisesRegex(ValueError,'helper'):audit(self.bundle,SHA)
    def test_missing_plugin(self):
        (self.bundle/'plugins/sqldrivers/libqsqlite.so').unlink()
        with self.assertRaisesRegex(ValueError,'allowlist'):audit(self.bundle,SHA)
    def test_missing_license(self):
        (self.bundle/'share/licenses/LICENSE').unlink()
        with self.assertRaisesRegex(ValueError,'license'):audit(self.bundle,SHA)
    def test_source_sha(self):
        with self.assertRaisesRegex(ValueError,'SHA mismatch'):audit(self.bundle,'b'*40)
    def test_extra_sql_plugin(self):
        shutil.copy2(self.exe,self.bundle/'plugins/sqldrivers/libqsqlmysql.so')
        with self.assertRaisesRegex(ValueError,'allowlist'):audit(self.bundle,SHA)
    def test_non_elf_payload(self):
        (self.bundle/'bin/BrickSuite').write_text('placeholder')
        with self.assertRaisesRegex(ValueError,'not an ELF'):audit(self.bundle,SHA)
    def test_escaping_symlink(self):
        (self.bundle/'lib/escape').symlink_to('/usr/lib')
        with self.assertRaisesRegex(ValueError,'symlink'):audit(self.bundle,SHA)
    def test_absolute_source_string(self):
        with (self.bundle/'bin/BrickSuite').open('ab') as out:out.write(b'\0/src/private.cpp\0')
        with self.assertRaisesRegex(ValueError,'Developer path'):audit(self.bundle,SHA)
    def test_relative_source_string(self):
        with (self.bundle/'bin/BrickSuite').open('ab') as out:out.write(b'\0qtbase/src/widgets/example.cpp\0')
        audit(self.bundle,SHA)
    def vendor_fixture(self, names):
        p=Path(self.run.name)/'libQt6Widgets.so.6.10.3';source=Path(self.run.name)/'vendor.c'
        source.write_text('\n'.join('const char path%d[] = %s;'%(i,json.dumps((PREFIX+name).decode())) for i,name in enumerate(names)))
        subprocess.run(['cc','-shared','-fPIC',str(source),'-o',str(p)],check=True)
        return p
    def test_vendor_diagnostics_keep_offsets(self):
        p=self.vendor_fixture(FILES);raw=p.read_bytes()
        instructions=subprocess.check_output(['objdump','-d',str(p)])
        records=normalize(p);changed=p.read_bytes()
        self.assertEqual(len(changed),len(raw));self.assertEqual(len(records),2)
        self.assertNotIn(PREFIX,changed)
        self.assertEqual(instructions,subprocess.check_output(['objdump','-d',str(p)]))
        for name in FILES:self.assertEqual(raw.index(name),changed.index(name))
    def test_unknown_vendor_inventory_rejected_atomically(self):
        p=self.vendor_fixture(FILES[:1]);raw=p.read_bytes()
        with self.assertRaisesRegex(ValueError,'inventory'):normalize(p)
        self.assertEqual(p.read_bytes(),raw)
    def test_tar_permissions_links_and_determinism(self):
        archive=Path(self.run.name)/'release.tar.gz';archive_tree(self.bundle,archive,12345);original=archive.read_bytes()
        archive_tree(self.bundle,archive,12345);self.assertEqual(original,archive.read_bytes())
        extracted=Path(self.run.name)/'extracted';extracted.mkdir()
        with tarfile.open(archive) as tf:tf.extractall(extracted)
        unpacked=extracted/'bundle';self.assertTrue(os.access(unpacked/'bin/BrickSuite',os.X_OK));self.assertEqual(os.readlink(unpacked/'lib/relative-link'),'libssl.so.3')
        audit(unpacked,SHA)

if __name__=='__main__':unittest.main()
