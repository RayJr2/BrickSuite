"""Installer lifecycle and consent regressions; all writes use synthetic roots."""
import contextlib
import io
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import installer as i

UBUNTU=i.detect_platform({'ID':'ubuntu','VERSION_ID':'22.04'},'x86_64')


class InstallerTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.base=Path(self.temp.name)
        self.root=self.base/'system';self.root.mkdir();self.source=self.base/'bundle';self.source.mkdir()
        for name,text in [('share/build-metadata.json','{"version":"0.4.0"}'),(i.BUNDLE_ICON,'synthetic icon'),('bin/BrickSuite','synthetic application'),('share/licenses/LICENSE','synthetic license')]:
            p=self.source/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text(text)
        (self.source/'bin/BrickSuite').chmod(0o755)
        (self.source/'BrickSuite').symlink_to('bin/BrickSuite')
        self.manager=patch.object(i,'package_manager',return_value=i.PackageCapabilities('/usr/bin/apt-get','/usr/bin/dpkg','amd64'));self.manager.start()
        self.plan=patch.object(i,'simulate_plan',return_value=('libsecret-tools=1.0',));self.plan.start()
        self.verify=patch.object(i,'verify_bundle',return_value='0.4.0');self.verify.start()
        self.output=io.StringIO();self.redirect=contextlib.redirect_stdout(self.output);self.redirect.__enter__()
    def tearDown(self):
        self.redirect.__exit__(None,None,None);self.verify.stop();self.manager.stop();self.plan.stop();self.temp.cleanup()
    def install(self):i.apply('install',self.root,self.source)
    def user_state(self):
        data='home/test/.local/share/RFStateSide/BrickSuite/'
        names=[data+p for p in ['BrickSuite.db','BrickSuite.db-wal','server/host-certificate.pem','server/paired-devices.json','host-state/host-data-epoch.json','LegoCalibration/Sessions/session.json','LegoCalibration/Profiles/verified.json','LegoCalibration/Workspaces/workspace.json','LegoCalibration/Backups/backup.json','printable-overrides/23422-hash.json','BrickSuite.log']]
        names+=['home/test/.config/RFStateSide/BrickSuite.conf','home/test/.local/share/keyrings/login.keyring','home/test/Documents/BrickSuite/Calibration Artifacts/fixture.3mf','home/test/Documents/BrickSuite/Calibration Artifacts/fixture-session.json','home/test/Documents/BrickSuite/Print Capability Audits/results.csv','home/test/Documents/BrickSuite Backups/BrickSuite.db','external/LDraw/parts/3001.dat','external/backups/BrickSuite.db','external/imports/source.csv','external/exports/mesh.stl','external/repairs/23422.blend','home/test/development/source.cpp','home/test/Downloads/BrickSuite.tar.gz']
        for name in names:
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('preserve '+name)
        return names
    def assert_user_state(self,names):
        for name in names:self.assertEqual((self.root/name).read_text(),'preserve '+name)
    def test_all_requirements_present(self):
        with patch.object(i,'missing_requirements',return_value=[]),patch.object(i,'privileged') as sudo:
            self.assertTrue(i.prerequisites(UBUNTU));sudo.assert_not_called()
        self.assertIn('Requirements satisfied.',self.output.getvalue())
    def test_declined_packages_never_sudo(self):
        with patch.object(i,'missing_requirements',return_value=['libsecret-tools']),patch.object(i,'agree',return_value=False),patch.object(i,'privileged') as sudo:
            self.assertFalse(i.prerequisites(UBUNTU));sudo.assert_not_called()
        self.assertIn('No packages installed.',self.output.getvalue())
    def test_only_missing_packages_after_consent(self):
        events=[]
        with patch.object(i,'missing_requirements',side_effect=[['libsecret-tools'],[]]),patch.object(i,'agree',side_effect=lambda _:events.append('consent') or True),patch.object(i,'privileged',side_effect=lambda cmd:events.append(cmd)):
            self.assertTrue(i.prerequisites(UBUNTU))
        self.assertEqual(events,['consent',['/usr/bin/apt-get',*i.APT_FLAGS,'--yes','install','libsecret-tools=1.0']])
    def test_package_failure_stops(self):
        with patch.object(i,'missing_requirements',return_value=['libsecret-tools']),patch.object(i,'agree',return_value=True),patch.object(i,'privileged',side_effect=subprocess.CalledProcessError(100,['apt-get'])):
            self.assertRaisesRegex(RuntimeError,'program files were not changed.*dependencies may now be installed',i.prerequisites,UBUNTU)
        self.assertFalse((self.root/i.APP).exists())
    def test_post_install_requirements_rechecked(self):
        with patch.object(i,'missing_requirements',return_value=['libsecret-tools']),patch.object(i,'agree',return_value=True),patch.object(i,'privileged'):
            self.assertRaisesRegex(RuntimeError,'still unavailable',i.prerequisites,UBUNTU)
    def test_synthetic_root_never_installs_host_packages(self):
        with patch.object(i,'missing_requirements',return_value=['libsecret-tools']),patch.object(i,'agree') as ask,patch.object(i,'privileged') as sudo:
            self.assertFalse(i.prerequisites(UBUNTU,True));ask.assert_not_called();sudo.assert_not_called()
    def platform(self,identity,version,arch):
        path=self.base/'os-release';path.write_text(f'ID={identity}\nVERSION_ID="{version}"\nPRETTY_NAME="Test OS"\n')
        return i.os_info(path,arch)
    def test_supported_versions(self):
        for version in ['22.04','24.04','26.04']:self.assertEqual(self.platform('ubuntu',version,'x86_64').base_version,tuple(map(int,version.split('.'))))
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
            self.assertEqual(i.missing_requirements(UBUNTU),[])
        cache=cache.replace('libGL.so.1 (libc6,x86-64) => /lib/libGL.so.1','')
        def which(name,**kw):return None if name=='secret-tool' else '/usr/bin/present'
        with patch.object(i,'run',side_effect=command),patch.object(Path,'is_file',return_value=True),patch.object(shutil,'which',side_effect=which):
            self.assertEqual(i.missing_requirements(UBUNTU),['libgl1','libsecret-tools'])
    def test_synthetic_install_desktop_icon_launcher_permissions(self):
        self.install();app=self.root/i.APP
        self.assertEqual((self.root/i.DESKTOP).read_text(),i.DESKTOP_TEXT)
        self.assertIn('Icon=bricksuite',i.DESKTOP_TEXT);self.assertNotIn('MimeType=',i.DESKTOP_TEXT)
        self.assertIn('Icon='+Path(i.ICON).stem+'\n',i.DESKTOP_TEXT)
        self.assertEqual(Path(i.ICON).parent,Path('usr/share/icons/hicolor/256x256/apps'))
        self.assertEqual((self.root/i.ICON).read_bytes(),(self.source/i.BUNDLE_ICON).read_bytes())
        self.assertEqual(os.readlink(self.root/i.COMMAND),'/opt/BrickSuite/BrickSuite')
        self.assertTrue(os.access(app/'bin/BrickSuite',os.X_OK));self.assertTrue((app/'BrickSuite').is_symlink())
        self.assertTrue((app/i.MARKER).is_file())
    def test_refresh_follows_install_upgrade_and_removal(self):
        observed=[]
        def refreshed(root):
            self.assertEqual(root,self.root)
            if (root/i.APP).exists():
                self.assertEqual((root/i.DESKTOP).read_text(),i.DESKTOP_TEXT)
                self.assertEqual((root/i.ICON).read_bytes(),(self.source/i.BUNDLE_ICON).read_bytes())
                self.assertTrue((root/i.COMMAND).is_symlink())
                observed.append((root/i.APP/'bin/BrickSuite').read_text())
            else:
                for name in [i.DESKTOP,i.ICON,i.COMMAND]:self.assertFalse(i.exists(root/name))
                observed.append('removed')
        with patch.object(i,'refresh',side_effect=refreshed):
            self.install()
            (self.source/'bin/BrickSuite').write_text('upgraded');self.install()
            i.apply('uninstall',self.root,self.source)
        self.assertEqual(observed,['synthetic application','upgraded','removed'])
    def test_refresh_spawn_failure_does_not_fail_completed_lifecycle(self):
        refresh=i.refresh
        with patch.object(i,'refresh',side_effect=lambda root:refresh(Path('/'))),patch.object(shutil,'which',side_effect=lambda name,**kw:'/usr/bin/'+name),patch.object(i,'run',side_effect=OSError('helper unavailable')):
            self.install();self.assertTrue((self.root/i.APP).is_dir())
            (self.source/'bin/BrickSuite').write_text('upgraded');self.install()
            self.assertEqual((self.root/i.APP/'bin/BrickSuite').read_text(),'upgraded')
            i.apply('uninstall',self.root,self.source);self.assertFalse((self.root/i.APP).exists())
        self.assertIn('Warning:',self.output.getvalue())
    def test_reinstall_upgrade_and_uninstall_preserve_user_state(self):
        state=self.user_state()
        self.install();self.assert_user_state(state);self.install();self.assert_user_state(state)
        (self.source/'share/build-metadata.json').write_text('{"version":"0.4.1"}')
        (self.source/'bin/BrickSuite').write_text('new application');self.install()
        self.assertEqual((self.root/i.APP/'bin/BrickSuite').read_text(),'new application')
        self.assert_user_state(state)
        i.apply('uninstall',self.root,self.source)
        for name in [i.APP,i.DESKTOP,i.ICON,i.COMMAND]:self.assertFalse(i.exists(self.root/name))
        self.assert_user_state(state)
        i.apply('uninstall',self.root,self.source)
    def test_copy_failure_keeps_existing_installation(self):
        state=self.user_state();self.install()
        with patch.object(shutil,'copytree',side_effect=OSError('disk full')):self.assertRaises(OSError,self.install)
        self.assertEqual((self.root/i.APP/'bin/BrickSuite').read_text(),'synthetic application')
        self.assertEqual((self.root/i.DESKTOP).read_text(),i.DESKTOP_TEXT)
        self.assert_user_state(state)
    def test_post_swap_verification_failure_rolls_back(self):
        state=self.user_state();self.install();(self.source/'bin/BrickSuite').write_text('replacement')
        with patch.object(i,'verify_bundle',side_effect=['0.4.0','0.4.0',RuntimeError('loader failed')]):self.assertRaises(RuntimeError,self.install)
        self.assertEqual((self.root/i.APP/'bin/BrickSuite').read_text(),'synthetic application')
        self.assertEqual((self.root/i.DESKTOP).read_text(),i.DESKTOP_TEXT)
        self.assert_user_state(state)
    def test_staging_verification_failure_preserves_old_install(self):
        self.install()
        with patch.object(i,'verify_bundle',side_effect=['0.4.0',RuntimeError('staged damage')]):self.assertRaises(RuntimeError,self.install)
        self.assertEqual((self.root/i.APP/'bin/BrickSuite').read_text(),'synthetic application')
    def test_missing_integration_recreated(self):
        self.install()
        for name in [i.COMMAND,i.ICON,i.DESKTOP]:(self.root/name).unlink()
        self.install();self.assertEqual((self.root/i.DESKTOP).read_text(),i.DESKTOP_TEXT)
        self.assertTrue((self.root/i.COMMAND).is_symlink());self.assertTrue((self.root/i.ICON).is_file())
    def test_modified_entries_refused_without_overwrite(self):
        self.install()
        for name in [i.DESKTOP,i.ICON]:
            path=self.root/name;original=path.read_bytes();path.write_text('modified')
            self.assertRaises(RuntimeError,self.install);self.assertEqual(path.read_text(),'modified');path.write_bytes(original)
    def test_conflicts_precede_prerequisites(self):
        for relative,kind in [(i.COMMAND,'file'),(i.APP,'directory'),('usr','symlink'),('opt','file'),('usr','fifo'),(i.DESKTOP,'fifo'),(i.ICON,'directory'),(i.COMMAND,'directory')]:
            with self.subTest(relative=relative,kind=kind):
                root=self.base/('conflict-'+kind+'-'+relative.replace('/','-'));root.mkdir()
                target=root/relative;target.parent.mkdir(parents=True,exist_ok=True)
                if kind=='file':target.write_text('unrelated')
                elif kind=='directory':target.mkdir()
                elif kind=='fifo':os.mkfifo(target)
                else:target.symlink_to(self.base/'outside')
                with patch.object(i,'os_info',return_value=UBUNTU),patch.object(i,'prerequisites') as requirements,patch.object(i,'privileged') as privileged,contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(i.main(['install','--root',str(root)]),1)
                    requirements.assert_not_called();privileged.assert_not_called()
                self.assertTrue(i.exists(target))
    def test_bundle_abi_failure_precedes_prerequisites(self):
        with patch.object(i,'os_info',return_value=UBUNTU),patch.object(i,'verify_bundle',side_effect=RuntimeError('ABI ceiling exceeded')),patch.object(i,'prerequisites') as requirements,contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(i.main(['install','--root',str(self.root)]),1);requirements.assert_not_called()
    def test_unrelated_launcher_refused(self):
        p=self.root/i.COMMAND;p.parent.mkdir(parents=True);p.write_text('someone else')
        self.assertRaises(RuntimeError,self.install);self.assertEqual(p.read_text(),'someone else')
    def test_unmanaged_directory_refused(self):
        (self.root/i.APP).mkdir(parents=True);self.assertRaises(RuntimeError,self.install)
    def test_symlink_parent_refused(self):
        (self.root/'usr').symlink_to(self.base/'outside');self.assertRaises(RuntimeError,self.install)
    def test_sudo_follows_install_consent(self):
        events=[]
        with patch.object(i,'os_info',return_value=UBUNTU),patch.object(i,'owned_paths',return_value=self.root/i.APP),patch.object(i,'prerequisites',return_value=True),patch.object(i.os,'geteuid',return_value=1000),patch.object(i,'agree',side_effect=lambda _:events.append('consent') or True),patch.object(i,'privileged',side_effect=lambda cmd:events.append(cmd)):
            self.assertEqual(i.main(['install']),0)
        self.assertEqual(events[0],'consent');self.assertIn('--apply',events[1])
    def test_decline_install_no_privilege_or_write(self):
        with patch.object(i,'os_info',return_value=UBUNTU),patch.object(i,'owned_paths',return_value=self.root/i.APP),patch.object(i,'prerequisites',return_value=True),patch.object(i,'agree',return_value=False),patch.object(i,'privileged') as sudo,patch.object(i,'apply') as apply:
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


class DesktopRefreshTests(unittest.TestCase):
    def test_available_helpers_use_standard_arguments(self):
        with patch.object(shutil,'which',side_effect=lambda name,**kw:'/usr/bin/'+name),patch.object(i,'run',return_value=subprocess.CompletedProcess([],0,'','')) as run:
            i.refresh(Path('/'))
            self.assertEqual([call.args[0] for call in run.call_args_list],[['/usr/bin/update-desktop-database','/usr/share/applications'],['/usr/bin/gtk-update-icon-cache','-f','-t','/usr/share/icons/hicolor']])
    def test_missing_helpers_are_optional(self):
        for available in [None,'gtk-update-icon-cache','update-desktop-database']:
            with patch.object(shutil,'which',side_effect=lambda name,**kw:'/usr/bin/'+name if name==available else None),patch.object(i,'run',return_value=subprocess.CompletedProcess([],0,'','')) as run:
                i.refresh(Path('/'));self.assertEqual(run.call_count,1 if available else 0)
    def test_nonzero_helper_warns_and_continues(self):
        with patch.object(shutil,'which',side_effect=lambda name,**kw:'/usr/bin/'+name),patch.object(i,'run',side_effect=[subprocess.CompletedProcess([],1,'','permission denied'),subprocess.CompletedProcess([],0,'','')]) as run,contextlib.redirect_stdout(io.StringIO()) as output:
            i.refresh(Path('/'));self.assertEqual(run.call_count,2)
            self.assertIn('Warning: update-desktop-database',output.getvalue());self.assertIn('permission denied',output.getvalue())
    def test_spawn_error_warns_and_continues(self):
        with patch.object(shutil,'which',side_effect=lambda name,**kw:'/usr/bin/'+name),patch.object(i,'run',side_effect=[OSError('helper disappeared'),subprocess.CompletedProcess([],0,'','')]) as run,contextlib.redirect_stdout(io.StringIO()) as output:
            i.refresh(Path('/'));self.assertEqual(run.call_count,2)
            self.assertIn('helper disappeared',output.getvalue())
    def test_synthetic_root_never_refreshes_host(self):
        with patch.object(shutil,'which') as which,patch.object(i,'run') as run:
            i.refresh(Path('/synthetic'));which.assert_not_called();run.assert_not_called()


class PlatformPlanTests(unittest.TestCase):
    MINT={'ID':'linuxmint','VERSION_ID':'22.3','PRETTY_NAME':'Linux Mint 22.3','ID_LIKE':'ubuntu debian','UBUNTU_CODENAME':'noble'}
    def test_mint_normalization(self):
        info=i.detect_platform(self.MINT,'x86_64')
        self.assertTrue(info.supported);self.assertEqual(info.distribution,'Linux Mint 22.3')
        self.assertEqual(info.version,'22.3');self.assertEqual(info.base_version,(24,4))
        self.assertEqual(info.package_base,'ubuntu');self.assertEqual(info.base_codename,'noble')
        self.assertEqual(i.provider('libglib2.0-0',info),'libglib2.0-0t64')
        self.assertEqual(i.provider('libglib2.0-0',UBUNTU),'libglib2.0-0')
        self.assertEqual(info.qualification,i.Qualification.QUALIFIED)
        self.assertEqual(info.distro_id,'linuxmint');self.assertEqual(info.id_like,('ubuntu','debian'))
    def test_recognized_derivatives_never_plan_or_install(self):
        for identity,version,name in [('pop','24.04','Pop!_OS 24.04 LTS'),('zorin','18','Zorin OS 18'),('neon','24.04','KDE neon')]:
            values=dict(self.MINT,ID=identity,VERSION_ID=version,PRETTY_NAME=name)
            with self.subTest(identity=identity):
                info=i.detect_platform(values,'x86_64')
                self.assertEqual(info.distribution,name);self.assertEqual(info.version,version)
                self.assertEqual(info.distro_id,identity);self.assertEqual(info.package_base,'ubuntu')
                self.assertEqual(info.base_version,(24,4));self.assertEqual(info.base_codename,'noble')
                self.assertEqual(info.qualification,i.Qualification.RECOGNIZED);self.assertFalse(info.supported)
                self.assertIn('not yet been qualified',info.diagnostic)
                with patch.object(i,'run') as run,patch.object(i,'missing_requirements') as missing,patch.object(i,'simulate_plan') as plan,patch.object(i,'privileged') as privileged:
                    self.assertRaises(RuntimeError,i.prerequisites,info)
                    self.assertRaises(RuntimeError,i.package_manager,info)
                    run.assert_not_called();missing.assert_not_called();plan.assert_not_called();privileged.assert_not_called()
                with tempfile.TemporaryDirectory() as directory,contextlib.redirect_stdout(io.StringIO()) as output:
                    path=Path(directory)/'os-release';path.write_text('\n'.join(key+'='+json.dumps(value) for key,value in values.items()))
                    with patch.object(i,'run') as run:
                        self.assertRaisesRegex(RuntimeError,'not yet been qualified',i.os_info,path,'x86_64');run.assert_not_called()
                    self.assertIn(name,output.getvalue());self.assertIn('24.04 noble',output.getvalue())
                for updates in [{'UBUNTU_CODENAME':''},{'UBUNTU_CODENAME':'jammy'},{'ID_LIKE':'debian'},{'VERSION_ID':'999'}]:
                    bad=i.detect_platform(dict(values,**updates),'x86_64')
                    self.assertEqual(bad.qualification,i.Qualification.UNSUPPORTED);self.assertEqual(bad.base_version,())
                self.assertEqual(i.detect_platform(values,'aarch64').qualification,i.Qualification.UNSUPPORTED)
    def test_derivative_evidence_refusals(self):
        for updates in [{'UBUNTU_CODENAME':''},{'UBUNTU_CODENAME':'jammy'},{'ID_LIKE':'debian'},{'ID_LIKE':''},{'VERSION_ID':'23'},{'ID':'lmde'},{'ID':'other'},{'ID':'debian'}]:
            with self.subTest(updates=updates):
                info=i.detect_platform(dict(self.MINT,**updates),'x86_64')
                self.assertFalse(info.supported);self.assertTrue(info.diagnostic)
        self.assertFalse(i.detect_platform(self.MINT,'aarch64').supported)
    def test_os_release_fixture_and_malformed_fields(self):
        with tempfile.TemporaryDirectory() as directory,contextlib.redirect_stdout(io.StringIO()) as output:
            path=Path(directory)/'os-release'
            path.write_text('\n'.join(key+'='+json.dumps(value) for key,value in self.MINT.items()))
            self.assertEqual(i.os_info(path,'x86_64').base_version,(24,4))
            self.assertIn('Platform: Linux Mint 22.3; version 22.3; architecture x86_64',output.getvalue())
            for text in ['ID=linuxmint\nVERSION_ID=22.3','ID=ubuntu\nID=linuxmint','ID="unterminated','ID=ubuntu extra']:
                path.write_text(text)
                with self.assertRaises((RuntimeError,ValueError)):i.os_info(path,'x86_64')
    def test_glib_detection_uses_normalized_base(self):
        cache='\n'.join(f'{name} (libc6,x86-64) => /lib/{name}' for package,names in i.REQUIREMENTS['libraries'].items() if package!='libglib2.0-0' for name in names)
        def command(args,**kw):return subprocess.CompletedProcess(args,0,cache if args[0].endswith('ldconfig') else 'font','')
        with patch.object(i,'run',side_effect=command),patch.object(Path,'is_file',return_value=True),patch.object(shutil,'which',return_value='/usr/bin/present'):
            self.assertEqual(i.missing_requirements(UBUNTU),['libglib2.0-0'])
            self.assertEqual(i.missing_requirements(i.detect_platform(self.MINT,'x86_64')),['libglib2.0-0t64'])
            for version in ['24.04','26.04']:
                self.assertEqual(i.missing_requirements(i.detect_platform({'ID':'ubuntu','VERSION_ID':version},'x86_64')),['libglib2.0-0t64'])
    def test_explicit_consent_and_eof(self):
        with patch.object(i.sys.stdin,'isatty',return_value=True):
            for answer,expected in [('',False),('n',False),('no',False),('y',True),('YES',True),('anything',False)]:
                with patch('builtins.input',return_value=answer) as prompt:
                    self.assertEqual(i.agree('Install?'),expected);self.assertIn('[y/N]',prompt.call_args.args[0])
            with patch('builtins.input',side_effect=EOFError):self.assertFalse(i.agree('Install?'))
        with patch.object(i.sys.stdin,'isatty',return_value=False),patch('builtins.input',return_value='yes') as prompt:
            self.assertFalse(i.agree('Install?'));prompt.assert_not_called()
    def test_package_capabilities(self):
        with patch.object(shutil,'which',side_effect=lambda name,**kw:'/usr/bin/'+name),patch.object(i,'run',return_value=subprocess.CompletedProcess([],0,'amd64\n','')) as run:
            self.assertEqual(i.package_manager(UBUNTU),i.PackageCapabilities('/usr/bin/apt-get','/usr/bin/dpkg','amd64'));self.assertEqual(run.call_args.args[0],['/usr/bin/dpkg','--print-architecture'])
        for missing in ['apt-get','dpkg']:
            with patch.object(shutil,'which',side_effect=lambda name,**kw:None if name==missing else '/usr/bin/'+name),patch.object(i,'run') as run:
                self.assertRaisesRegex(RuntimeError,missing,i.package_manager,UBUNTU);run.assert_not_called()
        with patch.object(shutil,'which',return_value='/usr/bin/tool'),patch.object(i,'run',return_value=subprocess.CompletedProcess([],0,'arm64\n','')):
            self.assertRaisesRegex(RuntimeError,'amd64',i.package_manager,UBUNTU)
        with patch.object(i,'run') as run:
            self.assertRaises(RuntimeError,i.package_manager,i.detect_platform(self.MINT,'aarch64'));run.assert_not_called()
    def test_privilege_only_when_needed(self):
        with patch.object(i.os,'geteuid',return_value=0,create=True),patch.object(shutil,'which',return_value=None) as which,patch.object(i,'run') as run:
            i.privileged(['fixture']);which.assert_not_called();self.assertEqual(run.call_args.args[0],['fixture'])
        with patch.object(i.os,'geteuid',return_value=1000,create=True),patch.object(shutil,'which',return_value=None),patch.object(i,'run') as run:
            self.assertRaisesRegex(RuntimeError,'sudo',i.privileged,['fixture']);run.assert_not_called()
    @staticmethod
    def simulation(packages=('libsecret-tools','libsecret-1-0')):
        return ('Reading package lists...\nBuilding dependency tree...\nReading state information...\nThe following NEW packages will be installed:\n  '+' '.join(packages)+'\n0 upgraded, '+str(len(packages))+' newly installed, 0 to remove and 12 not upgraded.\n'+''.join('Inst '+p+' (1.0 Ubuntu:24.04/noble [amd64])\n' for p in packages)+''.join('Conf '+p+' (1.0 Ubuntu:24.04/noble [amd64])\n' for p in packages))
    def plan(self,text,returncode=0,stderr=''):
        with patch.object(i,'run',return_value=subprocess.CompletedProcess([],returncode,text,stderr)) as run:
            value=i.simulate_plan('/usr/bin/apt-get',['libsecret-tools'])
            self.assertEqual(run.call_args.args[0],['/usr/bin/apt-get','--simulate','-o','APT::Get::Show-User-Simulation-Note=false',*i.APT_FLAGS,'install','libsecret-tools'])
            return value
    def test_safe_simulation_includes_required_dependencies(self):
        self.assertEqual(self.plan(self.simulation()),('libsecret-1-0=1.0','libsecret-tools=1.0'))
    def test_unsafe_and_unexpected_simulations(self):
        good=self.simulation()
        cases=[good+'Remv unrelated [1.0]\n',good.replace('Inst libsecret-1-0 (','Inst libsecret-1-0 [0.9] ('),good.replace('0 upgraded,','1 upgraded,'),good.replace('Conf libsecret-1-0','Conf unexpected'),good+'unexpected action\n',good.replace('2 newly installed','3 newly installed'),'',good.replace('libsecret-tools','foreign:arm64')]
        cases += [self.simulation(('libsecret-tools',name)) for name in ['libc6','libc-bin','libstdc++6','libgcc-s1','libqt6core6t64','qt6-base-dev','linux-image-generic']]
        for text in cases:
            with self.subTest(text=text),self.assertRaises(RuntimeError):self.plan(text)
        self.assertRaises(RuntimeError,self.plan,good,100,'APT failed')
        self.assertRaises(RuntimeError,self.plan,good,0,'W: unexpected warning')
    def test_unknown_provider_and_core_runtime_stop_before_apt(self):
        for package in ['unknown-provider','libglib2.0-0t64','libc6']:
            with patch.object(i,'missing_requirements',return_value=[package]),patch.object(i,'package_manager') as manager:
                self.assertRaises(RuntimeError,i.prerequisites,UBUNTU);manager.assert_not_called()
    def test_plan_change_stops_before_mutation(self):
        with patch.object(i,'missing_requirements',return_value=['libsecret-tools']),patch.object(i,'package_manager',return_value=i.PackageCapabilities('/usr/bin/apt-get','/usr/bin/dpkg','amd64')),patch.object(i,'simulate_plan',side_effect=[('libsecret-tools=1.0',),('libsecret-tools=2.0',)]),patch.object(i,'agree',return_value=True),patch.object(i.os,'geteuid',return_value=0,create=True),patch.object(i,'privileged') as privileged:
            self.assertRaisesRegex(RuntimeError,'changed after consent',i.prerequisites,UBUNTU);privileged.assert_not_called()
    def test_environment_excludes_apt_config(self):
        self.assertNotIn('APT_CONFIG',i.CLEAN_ENV)


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
    def test_archive_lifecycle_with_noninteractive_synthetic_apply(self):
        from package_linux import archive_tree
        bundle=self.bundle.with_name('BrickSuite');self.bundle.rename(bundle)
        tools=Path(__file__).resolve().parent
        for name in ['installer.py','runtime-requirements.json']:shutil.copy2(tools/name,bundle/name)
        archive=bundle.parent/'candidate.tar.gz';archive_tree(bundle,archive,12345)
        destination=bundle.parent/'acceptance'
        result=subprocess.run([sys.executable,'-B',str(tools/'test_install_archive.py'),str(archive),str(destination)],capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        report=json.loads((destination/'result.json').read_text())
        self.assertTrue(report['install'] and report['reinstall'] and report['uninstall'] and report['user_data_preserved'])
    def test_preflight_allows_only_missing_known_system_libraries(self):
        with patch.object(i,'run',return_value=subprocess.CompletedProcess([],0,'libGL.so.1 => not found\n','')):
            self.assertEqual(i.verify_bundle(self.bundle,allow_missing_requirements=True),'0.4.0')
            self.assertRaises(RuntimeError,i.verify_bundle,self.bundle)
        for output in ["/lib/libc.so.6: version `GLIBC_2.99' not found",'libQt6Core.so.6 => not found','libunknown.so => not found']:
            with patch.object(i,'run',return_value=subprocess.CompletedProcess([],0,output,'')):
                self.assertRaises(RuntimeError,i.verify_bundle,self.bundle,allow_missing_requirements=True)
    def test_damaged_binary_rejected(self):
        with (self.bundle/'bin/BrickSuite').open('ab') as f:f.write(b'damage')
        self.assertRaisesRegex(RuntimeError,'integrity',i.verify_bundle,self.bundle)
    def test_missing_plugin_rejected(self):
        (self.bundle/'plugins/sqldrivers/libqsqlite.so').unlink()
        self.assertRaisesRegex(RuntimeError,'inventory',i.verify_bundle,self.bundle)
    def test_absolute_runpath_report_rejected(self):
        self.report['elf'][0]['rpaths']=['/opt/sdk/lib'];self.save()
        self.assertRaisesRegex(RuntimeError,'RUNPATH',i.verify_bundle,self.bundle)
    def test_local_abi_policy_requires_explicit_package_kind(self):
        meta=self.bundle/'share/build-metadata.json';data=json.loads(meta.read_text())
        data['abi_policy']={'GLIBC':'2.43','GLIBCXX':'3.4.34','CXXABI':'1.3.15'}
        self.report['elf'][0]['requires']['GLIBC']='2.43';self.save()
        meta.write_text(json.dumps(data))
        self.assertRaisesRegex(RuntimeError,'ABI ceiling',i.verify_bundle,self.bundle)
        data['package_kind']='local-development';meta.write_text(json.dumps(data))
        self.assertEqual(i.verify_bundle(self.bundle),'0.4.0')
    def test_unsafe_symlink_rejected(self):
        (self.bundle/'lib/escape').symlink_to('/usr/lib')
        self.assertRaisesRegex(RuntimeError,'symlink',i.verify_bundle,self.bundle)


if __name__=='__main__':unittest.main()
