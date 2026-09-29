"""Focused local adapter tests; the release policy remains the default."""
import argparse
import copy
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
from audit_bundle import check_info, POLICY
import local_deploy
import package_linux


class LocalDeployTests(unittest.TestCase):
    def test_debug_rejected_before_tools_or_staging(self):
        with patch.object(local_deploy,'require_tools') as tools:
            with self.assertRaisesRegex(RuntimeError,'requires Release'):local_deploy.deploy(argparse.Namespace(config='Debug'))
            tools.assert_not_called()
    def test_missing_tools_actionable(self):
        with patch.object(local_deploy.shutil,'which',side_effect=lambda n:None if n=='patchelf' else '/usr/bin/'+n):
            with self.assertRaisesRegex(RuntimeError,'patchelf'):local_deploy.require_tools()
    def info(self):
        return {'machine':POLICY['architecture'],'class':'ELF64','requires':{'GLIBC':'2.43','GLIBCXX':'3.4.34','CXXABI':'1.3.15'},'rpaths':['$ORIGIN/../lib']}
    def test_newer_abi_rejected_by_default_release_gate(self):
        with self.assertRaisesRegex(ValueError,'exceeds'):check_info(self.info(),'app','$ORIGIN/../lib')
    def test_explicit_local_policy_does_not_mutate_release_policy(self):
        before=copy.deepcopy(POLICY)
        check_info(self.info(),'app','$ORIGIN/../lib',self.info()['requires'])
        self.assertEqual(before,POLICY)
        with self.assertRaises(ValueError):check_info(self.info(),'app','$ORIGIN/../lib')
    def test_local_policy_still_rejects_absolute_rpath(self):
        info=self.info();info['rpaths']=['/development/lib']
        with self.assertRaisesRegex(ValueError,'RUNPATH'):check_info(info,'app','$ORIGIN/../lib',info['requires'])
    def test_local_policy_still_rejects_wrong_architecture(self):
        info=self.info();info['machine']='AArch64'
        with self.assertRaisesRegex(ValueError,'architecture'):check_info(info,'app','$ORIGIN/../lib',info['requires'])
    def test_baseline_entry_still_rejects_newer_ubuntu(self):
        with patch.object(Path,'read_text',return_value='VERSION_ID="26.04"'):
            with self.assertRaisesRegex(ValueError,'Ubuntu 22.04'):package_linux.stage(argparse.Namespace())
    def test_publish_repeated_deploy_and_unmanaged_refusal(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);output=root/'deploy'
            for n in ['first','second']:
                stage=root/n;stage.mkdir();(stage/'local-deploy.json').write_text('{}');(stage/'archive').write_text(n)
                local_deploy.publish(stage,output)
                self.assertEqual((output/'archive').read_text(),n)
            (output/'local-deploy.json').unlink();stage=root/'third';stage.mkdir()
            with self.assertRaisesRegex(RuntimeError,'unmanaged'):local_deploy.publish(stage,output)
            self.assertEqual((output/'archive').read_text(),'second')
    def test_publish_failure_restores_previous_artifact(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);output=root/'deploy';output.mkdir();(output/'local-deploy.json').write_text('{}');(output/'archive').write_text('keep')
            with self.assertRaises(OSError):local_deploy.publish(root/'missing-stage',output)
            self.assertEqual((output/'archive').read_text(),'keep')
    def test_missing_notice_cache_fails_clearly(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaisesRegex(RuntimeError,'notice source'):local_deploy.prepare_sources(Path(d),Path(d))


if __name__=='__main__':unittest.main()
