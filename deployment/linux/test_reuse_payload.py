"""Focused installer reuse tests; synthetic repositories and payloads only."""
import argparse
import contextlib
import copy
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

import package_linux as package
import reuse_payload as reuse
import build_release_artifact as release


class ReuseTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.source=self.root/'source';self.source.mkdir()
        self.git('init','-q');self.git('config','user.email','fixture@example.invalid');self.git('config','user.name','Fixture')
        for name in ['deployment/linux/installer.py','deployment/linux/test_installer.py','src/app.cpp','CMakeLists.txt','deployment/linux/runtime-requirements.json','resources/help/index.html','version.txt']:
            p=self.source/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('original\n')
        self.git('add','.');self.git('commit','-qm','payload');self.payload=self.git('rev-parse','HEAD').strip()
        (self.source/'deployment/linux/installer.py').write_text('new installer\n')
        (self.source/'deployment/linux/test_installer.py').write_text('new tests\n')
        self.git('add','.');self.git('commit','-qm','installer');self.packaging=self.git('rev-parse','HEAD').strip()
        self.build=self.root/'work/build';self.build.mkdir(parents=True)
        self.report={'source_sha':self.payload,'elf_count':1,'elf':[],'ceilings':{'GLIBC':'2.35'}}
        self.tests={'full':{'passed':132,'failed':0,'skipped':0},'focused':{'passed':27,'failed':0,'skipped':0}}
        self.bundle=self.root/'original/BrickSuite';self.bundle.mkdir(parents=True)
        self.metadata={'source_sha':self.payload,'schema':35,'protocol':'1.5','version':'0.4.0','architecture':'x86_64','glibc_floor':'2.35','package_kind':'ubuntu-22.04-baseline-release','test_gate':'passed','required_licenses':['fixture/LICENSE'],'packaging_tool_hashes':{'installer.py':'old'}}
        for name,data in [('share/build-metadata.json',self.metadata),('share/abi-audit.json',self.report),('share/test-summary.json',self.tests),('share/dependencies.json',{'plugins':reuse.POLICY['plugins'],'system_allowed_sonames':reuse.POLICY['system_sonames']})]:self.write(self.bundle/name,data)
        for name in ['installer.py','bin/BrickSuite','bin/worker','lib/runtime.so','plugins/plugin.so','runtime-requirements.json','share/licenses/fixture/LICENSE','share/help.html']:
            p=self.bundle/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(('original '+name).encode())
        self.write(self.build/'source-record.json',{'source_sha':self.payload,'schema':35,'protocol':'1.5'})
        self.write(self.build/'configured-tests.json',{'count':132})
        self.write(self.build.parent/'evidence/tests/summary.json',self.tests)
        self.archive=self.root/'qualified.tar.gz';package.archive_tree(self.bundle,self.archive,1)
        self.args=argparse.Namespace(source=self.source,build=self.build,output=self.root/('candidate-'+self.packaging[:7]),reuse_qualified_payload=self.archive,qualified_archive_sha256=reuse.digest(self.archive))
        self.audit=patch.object(reuse,'audit',return_value=self.report);self.audit.start();self.addCleanup(self.audit.stop)
    def git(self,*args):return subprocess.check_output(['git','-C',str(self.source),*args],text=True,stderr=subprocess.STDOUT)
    def write(self,path,value):path.parent.mkdir(parents=True,exist_ok=True);path.write_text(json.dumps(value))
    def changed_commit(self,name):
        (self.source/name).write_text('changed\n');self.git('add','.');self.git('commit','-qm','disallowed')
        self.packaging=self.git('rev-parse','HEAD').strip();self.args.output=self.root/('candidate-'+self.packaging[:7])
    def test_allowed_delta_provenance_and_all_payload_bytes(self):
        before=reuse.inventory(self.archive);result=reuse.reuse(self.args)
        metadata=json.loads((self.args.output/'build-metadata.json').read_text())
        self.assertEqual(metadata['source_sha'],self.payload)
        self.assertEqual(metadata['payload_source_commit'],self.payload)
        self.assertEqual(metadata['packaging_source_commit'],self.packaging)
        self.assertEqual(metadata['installer_source_commit'],self.packaging)
        self.assertEqual(metadata['package_kind'],'installer-reuse-candidate');self.assertTrue(metadata['payload_reused'])
        after=reuse.inventory(Path(result['archive']))
        reuse.compare_payload(before,after,b'new installer\n')
        self.assertEqual(json.loads((self.build/'source-record.json').read_text())['source_sha'],self.payload)
        self.assertEqual(reuse.digest(self.archive),self.args.qualified_archive_sha256)
        self.assertEqual(reuse.digest(Path(result['archive'])),result['sha256'])
    def test_no_implicit_opt_in(self):
        self.args.reuse_qualified_payload=None
        self.assertRaisesRegex(ValueError,'opt-in',reuse.reuse,self.args)
    def test_cli_normal_dispatch_unchanged(self):
        with patch.object(sys,'argv',['package_linux.py']),patch.object(package,'stage') as stage,patch.object(reuse,'reuse') as candidate:
            package.main();stage.assert_called_once();candidate.assert_not_called()
    def test_cli_reuse_requires_hash(self):
        with patch.object(sys,'argv',['package_linux.py','--reuse-qualified-payload','old.tar.gz']),contextlib.redirect_stderr(io.StringIO()):
            self.assertRaises(SystemExit,package.main)
    def test_cli_explicit_reuse_dispatch(self):
        with patch.object(sys,'argv',['package_linux.py','--reuse-qualified-payload','old.tar.gz','--qualified-archive-sha256','a'*64]),patch.object(reuse,'reuse',return_value={}) as candidate,patch.object(package,'stage') as normal,contextlib.redirect_stdout(io.StringIO()):
            package.main();candidate.assert_called_once();normal.assert_not_called()
    def test_cli_hash_without_opt_in_refused(self):
        with patch.object(sys,'argv',['package_linux.py','--qualified-archive-sha256','a'*64]),contextlib.redirect_stderr(io.StringIO()):
            self.assertRaises(SystemExit,package.main)
    def test_normal_source_guard_still_rejects_mismatch(self):
        original=Path.read_text
        def read(path,*args,**kwargs):return 'VERSION_ID="22.04"' if str(path)=='/etc/os-release' else original(path,*args,**kwargs)
        with patch.object(Path,'read_text',read):self.assertRaisesRegex(ValueError,'Source SHA mismatch',package.stage,self.args)
    def test_dirty_tree_rejected(self):
        (self.source/'deployment/linux/installer.py').write_text('uncommitted')
        self.assertRaisesRegex(ValueError,'clean',reuse.reuse,self.args)
    def test_untracked_tree_rejected(self):
        (self.source/'untracked').write_text('not reviewed')
        self.assertRaisesRegex(ValueError,'clean',reuse.reuse,self.args)
    def test_cpp_delta_rejected(self):
        self.changed_commit('src/app.cpp');self.assertRaisesRegex(ValueError,'Disallowed',reuse.reuse,self.args)
    def test_cmake_delta_rejected(self):
        self.changed_commit('CMakeLists.txt');self.assertRaisesRegex(ValueError,'Disallowed',reuse.reuse,self.args)
    def test_runtime_requirements_delta_rejected(self):
        self.changed_commit('deployment/linux/runtime-requirements.json');self.assertRaisesRegex(ValueError,'Disallowed',reuse.reuse,self.args)
    def test_version_delta_rejected(self):
        self.changed_commit('version.txt');self.assertRaisesRegex(ValueError,'Disallowed',reuse.reuse,self.args)
    def test_resource_delta_rejected(self):
        self.changed_commit('resources/help/index.html');self.assertRaisesRegex(ValueError,'Disallowed',reuse.reuse,self.args)
    def test_archive_hash_mismatch_rejected(self):
        with self.archive.open('ab') as stream:stream.write(b'damage')
        self.assertRaisesRegex(ValueError,'hash mismatch',reuse.reuse,self.args)
    def test_elf_audit_mismatch_rejected(self):
        with patch.object(reuse,'audit',return_value={}):self.assertRaisesRegex(ValueError,'audit/hash',reuse.reuse,self.args)
        self.assertFalse(self.args.output.exists())
    def test_payload_record_mismatch_rejected(self):
        self.write(self.build/'source-record.json',{'source_sha':self.packaging})
        self.assertRaisesRegex(ValueError,'record mismatch',reuse.reuse,self.args)
    def test_qualification_evidence_mismatch_rejected(self):
        self.write(self.build.parent/'evidence/tests/summary.json',{})
        self.assertRaisesRegex(ValueError,'evidence mismatch',reuse.reuse,self.args)
    def test_runtime_or_license_changes_rejected(self):
        before=reuse.inventory(self.archive)
        for name in ['bin/BrickSuite','lib/runtime.so','runtime-requirements.json','share/licenses/fixture/LICENSE','share/help.html']:
            after=copy.deepcopy(before);after['BrickSuite/'+name]['value']='wrong'
            with self.subTest(name=name):self.assertRaisesRegex(ValueError,'payload hash',reuse.compare_payload,before,after,b'anything')
    def test_installer_mismatch_rejected(self):
        before=reuse.inventory(self.archive)
        self.assertRaisesRegex(ValueError,'installer byte',reuse.compare_payload,before,before,b'new installer\n')
    def test_archive_creation_cannot_change_installer(self):
        original=package.archive_tree
        def corrupt(bundle,archive,epoch):
            (bundle/'installer.py').write_text('wrong installer');original(bundle,archive,epoch)
        with patch.object(reuse,'archive_tree',side_effect=corrupt):self.assertRaisesRegex(ValueError,'installer byte',reuse.reuse,self.args)
        self.assertFalse(self.args.output.exists())
    def test_archive_creation_cannot_change_runtime(self):
        original=package.archive_tree
        def corrupt(bundle,archive,epoch):
            (bundle/'lib/runtime.so').write_text('wrong runtime');original(bundle,archive,epoch)
        with patch.object(reuse,'archive_tree',side_effect=corrupt):self.assertRaisesRegex(ValueError,'payload hash',reuse.reuse,self.args)
        self.assertFalse(self.args.output.exists())
    def test_reuse_candidate_cannot_be_reused_as_qualified_release(self):
        self.metadata['package_kind']='installer-reuse-candidate'
        self.write(self.bundle/'share/build-metadata.json',self.metadata)
        package.archive_tree(self.bundle,self.archive,1);self.args.qualified_archive_sha256=reuse.digest(self.archive)
        self.assertRaisesRegex(ValueError,'single-source',reuse.reuse,self.args)
    def test_symlink_escape_refused(self):
        import tarfile
        with tarfile.open(self.archive,'w:gz') as stream:
            root=tarfile.TarInfo('BrickSuite');root.type=tarfile.DIRTYPE;stream.addfile(root)
            link=tarfile.TarInfo('BrickSuite/escape');link.type=tarfile.SYMTYPE;link.linkname='../outside';stream.addfile(link)
        self.assertRaisesRegex(ValueError,'Escaping',reuse.inventory,self.archive)
    def test_existing_output_preserved(self):
        self.args.output.mkdir();(self.args.output/'keep').write_text('keep')
        self.assertRaisesRegex(ValueError,'already exists',reuse.reuse,self.args)
        self.assertEqual((self.args.output/'keep').read_text(),'keep')
    def test_candidate_rejected_by_official_verifier(self):
        reuse.reuse(self.args)
        identity={'source_sha':self.payload,'version':'0.4.0','schema':35,'protocol':'1.5'}
        self.assertRaisesRegex(release.ReleaseError,'not a gated official',release.verify_output,self.args.output,self.build.parent,identity)
    def test_unsafe_archive_structure_rejected(self):
        import tarfile
        for name in ['../escape','BrickSuite/../../escape','/absolute']:
            with tarfile.open(self.archive,'w:gz') as stream:
                member=tarfile.TarInfo(name);stream.addfile(member)
            self.assertRaisesRegex(ValueError,'Unsafe',reuse.inventory,self.archive)


if __name__=='__main__':unittest.main()
