"""Installer lifecycle and consent regressions; all writes use synthetic roots."""
import contextlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest
from unittest.mock import patch
import installer as i


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.base=Path(self.temp.name)
        self.root=self.base/'system';self.root.mkdir();self.source=self.base/'bundle';self.source.mkdir()
        for name,text in [('share/build-metadata.json','{"version":"0.4.0"}'),(i.BUNDLE_ICON,'synthetic icon'),('bin/BrickSuite','synthetic application'),('share/licenses/LICENSE','synthetic license')]:
            p=self.source/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
        (self.source/'bin/BrickSuite').chmod(0o755)
        (self.source/'BrickSuite').symlink_to('bin/BrickSuite')
        self.verify=patch.object(i,'verify_bundle',return_value='0.4.0');self.verify.start()
        self.output=io.StringIO();self.redirect=contextlib.redirect_stdout(self.output);self.redirect.__enter__()
    def tearDown(self):
        self.redirect.__exit__(None,None,None);self.verify.stop();self.temp.cleanup()
    def install(self):i.apply('install',self.root,self.source)
    def test_all_requirements_present(self):
        with patch.object(i,'missing_requirements',return_value=[]),patch.object(i,'privileged') as sudo:
            self.assertTrue(i.prerequisites((22,4)));sudo.assert_not_called()
        self.assertIn('Requirements satisfied.',self.output.getvalue())
    def test_declined_packages_never_sudo(self):
        with patch.object(i,'missing_requirements',return_value=['libsecret-tools']),patch.object(i,'agree',return_value=False),patch.object(i,'privileged') as sudo:
            self.assertFalse(i.prerequisites((22,4)));sudo.assert_not_called()
        self.assertIn('sudo apt-get install --no-remove libsecret-tools',self.output.getvalue())
    def test_only_missing_packages_after_consent(self):
        events=[]
        with patch.object(i,'missing_requirements',side_effect=[['libsecret-tools'],[]]),patch.object(i,'agree',side_effect=lambda _:events.append('consent') or True),patch.object(i,'privileged',side_effect=lambda cmd:events.append(cmd)):
            self.assertTrue(i.prerequisites((22,4)))
        self.assertEqual(events,['consent',['apt-get','install','--no-remove','libsecret-tools']])
    def test_package_failure_stops(self):
        with patch.object(i,'missing_requirements',return_value=['libsecret-tools']),patch.object(i,'agree',return_value=True),patch.object(i,'privileged',side_effect=subprocess.CalledProcessError(100,['apt-get'])):
            self.assertRaises(subprocess.CalledProcessError,i.prerequisites,(22,4))
        self.assertFalse((self.root/i.APP).exists())
    def test_post_install_requirements_rechecked(self):
        with patch.object(i,'missing_requirements',return_value=['libsecret-tools']),patch.object(i,'agree',return_value=True),patch.object(i,'privileged'):
            self.assertRaisesRegex(RuntimeError,'still unavailable',i.prerequisites,(22,4))
    def test_synthetic_root_never_installs_host_packages(self):
        with patch.object(i,'missing_requirements',return_value=['libsecret-tools']),patch.object(i,'agree') as ask,patch.object(i,'privileged') as sudo:
            self.assertFalse(i.prerequisites((22,4),True));ask.assert_not_called();sudo.assert_not_called()
    def platform(self,identity,version,arch):
        path=self.base/'os-release';path.write_text(f'ID={identity}\nVERSION_ID="{version}"\nPRETTY_NAME="Test OS"\n')
        return i.os_info(path,arch)
    def test_supported_versions(self):
        for version in ['22.04','24.04','26.04']:self.assertEqual(self.platform('ubuntu',version,'x86_64'),tuple(map(int,version.split('.'))))
    def test_unsupported_distribution(self):self.assertRaises(RuntimeError,self.platform,'debian','26.04','x86_64')
    def test_old_ubuntu(self):self.assertRaises(RuntimeError,self.platform,'ubuntu','20.04','x86_64')
    def test_wrong_architecture(self):self.assertRaises(RuntimeError,self.platform,'ubuntu','26.04','aarch64')
    def test_detection_contract_covers_audited_system_libraries(self):
        policy=json.loads(Path(__file__).with_name('policy.json').read_text())
        names={name for group in i.REQUIREMENTS['libraries'].values() for name in group}
        self.assertEqual(names,set(policy['system_sonames']))
        self.assertFalse(any(name.startswith(('libQt','libicu','libssl','libcrypto','libmcut')) for name in names))
    def test_detection_available_and_missing(self):
        cache='\n'.join(f'{name} (libc6,x86-64) => /lib/{name}' for names in i.REQUIREMENTS['libraries'].values() for name in names)
        def command(args,**kw):return subprocess.CompletedProcess(args,0,cache if args[0].endswith('ldconfig') else 'DejaVu Sans\n','')
        with patch.object(i,'run',side_effect=command),patch.object(Path,'is_file',return_value=True),patch.object(shutil,'which',return_value='/usr/bin/present'):
            self.assertEqual(i.missing_requirements((22,4)),[])
        cache=cache.replace('libGL.so.1 (libc6,x86-64) => /lib/libGL.so.1','')
        def which(name,**kw):return None if name=='secret-tool' else '/usr/bin/present'
        with patch.object(i,'run',side_effect=command),patch.object(Path,'is_file',return_value=True),patch.object(shutil,'which',side_effect=which):
            self.assertEqual(i.missing_requirements((22,4)),['libgl1','libsecret-tools'])
    def test_synthetic_install_desktop_icon_launcher_permissions(self):
        self.install();app=self.root/i.APP
        self.assertEqual((self.root/i.DESKTOP).read_text(),i.DESKTOP_TEXT)
        self.assertIn('Icon=bricksuite',i.DESKTOP_TEXT);self.assertNotIn('MimeType=',i.DESKTOP_TEXT)
        self.assertEqual((self.root/i.ICON).read_bytes(),(self.source/i.BUNDLE_ICON).read_bytes())
        self.assertEqual(os.readlink(self.root/i.COMMAND),'/opt/BrickSuite/BrickSuite')
        self.assertTrue(os.access(app/'bin/BrickSuite',os.X_OK));self.assertTrue((app/'BrickSuite').is_symlink())
        self.assertTrue((app/i.MARKER).is_file())
    def test_reinstall_upgrade_and_uninstall_preserve_user_state(self):
        state=['home/test/.local/share/BrickSuite/BrickSuite.db','home/test/.config/BrickSuite.conf','home/test/.local/share/keyrings/login.keyring','home/test/LDraw/parts/3001.dat','home/test/backups/BrickSuite.db','home/test/.local/share/BrickSuite/host-identity','home/test/.local/share/BrickSuite/logs/run.log','home/test/.local/share/BrickSuite/paired-device']
        for name in state:p=self.root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('preserve '+name)
        self.install();self.install()
        (self.source/'share/build-metadata.json').write_text('{"version":"0.4.1"}')
        (self.source/'bin/BrickSuite').write_text('new application');self.install()
        self.assertEqual((self.root/i.APP/'bin/BrickSuite').read_text(),'new application')
        i.apply('uninstall',self.root,self.source)
        for name in [i.APP,i.DESKTOP,i.ICON,i.COMMAND]:self.assertFalse(i.exists(self.root/name))
        for name in state:self.assertEqual((self.root/name).read_text(),'preserve '+name)
        i.apply('uninstall',self.root,self.source)
    def test_copy_failure_keeps_existing_installation(self):
        self.install()
        with patch.object(shutil,'copytree',side_effect=OSError('disk full')):self.assertRaises(OSError,self.install)
        self.assertEqual((self.root/i.APP/'bin/BrickSuite').read_text(),'synthetic application')
        self.assertEqual((self.root/i.DESKTOP).read_text(),i.DESKTOP_TEXT)
    def test_post_swap_verification_failure_rolls_back(self):
        self.install();(self.source/'bin/BrickSuite').write_text('replacement')
        with patch.object(i,'verify_bundle',side_effect=['0.4.0','0.4.0',RuntimeError('loader failed')]):self.assertRaises(RuntimeError,self.install)
        self.assertEqual((self.root/i.APP/'bin/BrickSuite').read_text(),'synthetic application')
        self.assertEqual((self.root/i.DESKTOP).read_text(),i.DESKTOP_TEXT)
    def test_unrelated_launcher_refused(self):
        p=self.root/i.COMMAND;p.parent.mkdir(parents=True);p.write_text('someone else')
        self.assertRaises(RuntimeError,self.install);self.assertEqual(p.read_text(),'someone else')
    def test_unmanaged_directory_refused(self):
        (self.root/i.APP).mkdir(parents=True);self.assertRaises(RuntimeError,self.install)
    def test_symlink_parent_refused(self):
        (self.root/'usr').symlink_to(self.base/'outside');self.assertRaises(RuntimeError,self.install)
    def test_sudo_follows_install_consent(self):
        events=[]
        with patch.object(i,'os_info',return_value=(22,4)),patch.object(i,'prerequisites',return_value=True),patch.object(i.os,'geteuid',return_value=1000),patch.object(i,'agree',side_effect=lambda _:events.append('consent') or True),patch.object(i,'privileged',side_effect=lambda cmd:events.append(cmd)):
            self.assertEqual(i.main(['install']),0)
        self.assertEqual(events[0],'consent');self.assertIn('--apply',events[1])
    def test_decline_install_no_privilege_or_write(self):
        with patch.object(i,'os_info',return_value=(22,4)),patch.object(i,'prerequisites',return_value=True),patch.object(i,'agree',return_value=False),patch.object(i,'privileged') as sudo,patch.object(i,'apply') as apply:
            self.assertEqual(i.main(['install']),0);sudo.assert_not_called();apply.assert_not_called()
    def test_system_font_transitive_library_allowed(self):
        (self.source/'lib').mkdir();(self.source/'lib/libbrotlidec.so.1').write_text('fixture')
        output='libbrotlidec.so.1 => /usr/lib/libbrotlidec.so.1 (0x1234)'
        i.verify_resolution(self.source,{'needed':['libfreetype.so.6']},output)
        self.assertRaises(RuntimeError,i.verify_resolution,self.source,{'needed':['libbrotlidec.so.1']},output)
    def test_private_qt_transitive_library_cannot_escape(self):
        (self.source/'lib').mkdir();(self.source/'lib/libQt6Core.so.6').write_text('fixture')
        self.assertRaises(RuntimeError,i.verify_resolution,self.source,{'needed':[]},'libQt6Core.so.6 => /usr/lib/libQt6Core.so.6 (0x1234)')
    def test_development_path_boundary(self):
        self.assertTrue(i.has_developer_path(b'\x00/home/ray/build/source.cpp'))
        self.assertTrue(i.has_developer_path(b'/src/source.cpp'))
        self.assertFalse(i.has_developer_path(b'\x00qtbase/src/widgets/source.cpp'))


class BundleVerificationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from test_release_tools import GateTests
        cls.gates=GateTests;cls.gates.setUpClass()
    @classmethod
    def tearDownClass(cls):cls.gates.tearDownClass()
    def setUp(self):
        from audit_bundle import audit
        from test_release_tools import SHA
        self.temp=tempfile.TemporaryDirectory();self.bundle=Path(self.temp.name)/'bundle'
        shutil.copytree(self.gates.template,self.bundle,symlinks=True)
        for name in ['BrickSuite','install.sh','uninstall.sh']:
            (self.bundle/name).write_text('#!/bin/sh\nexit 0\n');(self.bundle/name).chmod(0o755)
        icon=self.bundle/i.BUNDLE_ICON;icon.parent.mkdir(parents=True);icon.write_text('fixture icon')
        meta=self.bundle/'share/build-metadata.json';data=json.loads(meta.read_text());data.update(version='0.4.0',architecture='x86_64');meta.write_text(json.dumps(data))
        self.report=audit(self.bundle,SHA);self.save()
    def save(self):(self.bundle/'share/abi-audit.json').write_text(json.dumps(self.report))
    def tearDown(self):self.temp.cleanup()
    def test_real_elf_loader_verification(self):self.assertEqual(i.verify_bundle(self.bundle),'0.4.0')
    def test_damaged_binary_rejected(self):
        with (self.bundle/'bin/BrickSuite').open('ab') as f:f.write(b'damage')
        self.assertRaisesRegex(RuntimeError,'integrity',i.verify_bundle,self.bundle)
    def test_missing_plugin_rejected(self):
        (self.bundle/'plugins/sqldrivers/libqsqlite.so').unlink()
        self.assertRaisesRegex(RuntimeError,'inventory',i.verify_bundle,self.bundle)
    def test_absolute_runpath_report_rejected(self):
        self.report['elf'][0]['rpaths']=['/opt/sdk/lib'];self.save()
        self.assertRaisesRegex(RuntimeError,'RUNPATH',i.verify_bundle,self.bundle)
    def test_unsafe_symlink_rejected(self):
        (self.bundle/'lib/escape').symlink_to('/usr/lib')
        self.assertRaisesRegex(RuntimeError,'symlink',i.verify_bundle,self.bundle)


if __name__=='__main__':unittest.main()
