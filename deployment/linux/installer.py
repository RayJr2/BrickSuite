#!/usr/bin/env python3
"""Qualified Ubuntu-base system installer; standard-library only, no SDK needed."""
import argparse
from dataclasses import dataclass
from enum import Enum
import fcntl
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shlex
import shutil
import subprocess
import sys
import tempfile
import uuid

HERE = Path(__file__).resolve().parent
REQUIREMENTS = json.loads((HERE/'runtime-requirements.json').read_text())
APP = 'opt/BrickSuite'
DESKTOP = 'usr/share/applications/bricksuite.desktop'
ICON = 'usr/share/icons/hicolor/256x256/apps/bricksuite.png'
COMMAND = 'usr/local/bin/bricksuite'
BUNDLE_ICON = 'share/icons/hicolor/256x256/apps/BrickSuite.png'
MARKER = 'share/installation.json'
DESKTOP_TEXT = '''[Desktop Entry]
# Installed by BrickSuite Linux installer
Type=Application
Name=BrickSuite
Exec=/opt/BrickSuite/BrickSuite
TryExec=/opt/BrickSuite/BrickSuite
Icon=bricksuite
Terminal=false
Categories=Utility;
'''
CLEAN_ENV = {k:v for k,v in os.environ.items() if not k.startswith(('LD_', 'QT_'))}
CLEAN_ENV.pop('APT_CONFIG',None)
CLEAN_ENV.update(PATH='/usr/sbin:/usr/bin:/sbin:/bin', LC_ALL='C')


def run(command, **kwargs):
    return subprocess.run(command, env=CLEAN_ENV, **kwargs)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def has_developer_path(raw):
    relative_chars=b'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_./-'
    for prefix in [b'/home/ray/',b'/home/qt/',b'/work/build/',b'/src/',b'/opt/qt/',b'/tmp/bricksuite-build']:
        # Literal prefix search avoids repeatedly scanning large ICU/Qt files
        # with a per-byte lookbehind during the end-user installation check.
        for match in re.finditer(re.escape(prefix),raw):
            if match.start()==0 or raw[match.start()-1] not in relative_chars:return True
    return False


class Qualification(Enum):
    QUALIFIED='Qualified'
    RECOGNIZED='Recognized but not qualified'
    UNSUPPORTED='Unsupported / unknown'


# Explicit release/base evidence, not a blanket ID_LIKE authorization. A future
# qualification changes this policy and its acceptance tests, not base detection.
NOBLE_DERIVATIVES={
    ('linuxmint','22.3'): Qualification.QUALIFIED,
    ('pop','24.04'): Qualification.RECOGNIZED,
    ('zorin','18'): Qualification.RECOGNIZED,
    ('neon','24.04'): Qualification.RECOGNIZED,
}


@dataclass(frozen=True)
class PlatformInfo:
    distribution: str
    version: str
    distro_id: str
    id_like: tuple
    machine: str
    package_base: str
    base_version: tuple
    base_codename: str
    qualification: Qualification
    diagnostic: str

    @property
    def supported(self):return self.qualification==Qualification.QUALIFIED


@dataclass(frozen=True)
class PackageCapabilities:
    apt_get: str
    dpkg: str
    native_architecture: str


def detect_platform(values, machine):
    identity=values.get('ID','unknown');version=values.get('VERSION_ID','')
    id_like=tuple(values.get('ID_LIKE','').split())
    base='';base_version=();codename=''
    qualification=Qualification.UNSUPPORTED
    diagnostic='Automatic installation requires Ubuntu 22.04+ or qualified Linux Mint 22.3/Noble on x86_64.'
    if identity=='ubuntu' and re.fullmatch(r'\d+\.\d+(?:\.\d+)?',version):
        base='ubuntu';base_version=tuple(map(int,version.split('.')))
        codename=values.get('UBUNTU_CODENAME',values.get('VERSION_CODENAME',''))
        if base_version >= (22,4):qualification=Qualification.QUALIFIED
    elif identity in {key[0] for key in NOBLE_DERIVATIVES}:
        if (identity,version) in NOBLE_DERIVATIVES and {'ubuntu','debian'} <= set(id_like) and values.get('UBUNTU_CODENAME')=='noble':
            base='ubuntu';base_version=(24,4);codename='noble'
            qualification=NOBLE_DERIVATIVES[(identity,version)]
        else:diagnostic='Unqualified '+identity+' release or missing/conflicting package-base evidence: expected a recognized VERSION_ID, ID_LIKE containing ubuntu and debian, and UBUNTU_CODENAME=noble.'
    if qualification!=Qualification.UNSUPPORTED:
        diagnostic=f'Ubuntu package base: {base_version[0]}.{base_version[1]:02d} {codename}. '
        diagnostic+=('Installer eligible.' if qualification==Qualification.QUALIFIED else 'BrickSuite automatic installation has not yet been qualified for this distribution.')
    if machine!='x86_64':
        qualification=Qualification.UNSUPPORTED;diagnostic='Unsupported machine architecture: '+machine+'; x86_64 is required.'
    return PlatformInfo(values.get('PRETTY_NAME',identity),version,identity,id_like,machine,base,base_version,codename,qualification,diagnostic)


def os_info(path=Path('/etc/os-release'), machine=None):
    values={}
    for line in path.read_text().splitlines():
        if '=' in line and not line.startswith('#'):
            key,value=line.split('=',1);tokens=shlex.split(value)
            if key in values or len(tokens)>1:raise RuntimeError('Malformed or duplicate os-release field: '+key)
            values[key]=tokens[0] if tokens else ''
    info=detect_platform(values,machine or platform.machine())
    print(f'Platform: {info.distribution}; version {info.version}; architecture {info.machine}')
    print(info.diagnostic)
    if not info.supported:raise RuntimeError(info.diagnostic+' No package-manager action taken; manual portable use is separate.')
    return info


def missing_requirements(info):
    cache=run(['/sbin/ldconfig','-p'],capture_output=True,text=True,check=True).stdout
    available={}
    for line in cache.splitlines():
        match=re.match(r'\s*(\S+)\s+\([^)]*x86-64[^)]*\)\s+=>\s+(\S+)',line)
        if match and Path(match[2]).is_file():available[match[1]]=match[2]
    missing=[]
    for package,names in REQUIREMENTS['libraries'].items():
        if any(name not in available for name in names):
            missing.append(provider(package,info))
    for package,names in REQUIREMENTS['commands'].items():
        if any(not shutil.which(name,path=CLEAN_ENV['PATH']) for name in names):missing.append(package)
    if not any(Path(p).is_file() for p in ['/usr/lib/systemd/user/dbus.socket','/lib/systemd/user/dbus.socket']):missing.append('dbus-user-session')
    if not any(shutil.which(name,path=CLEAN_ENV['PATH']) for name in ['gnome-keyring-daemon','kwalletd5','kwalletd6']):missing.append('gnome-keyring')
    fc=shutil.which('fc-list',path=CLEAN_ENV['PATH'])
    if not fc or not run([fc],capture_output=True,text=True,check=True).stdout.strip():missing.append(REQUIREMENTS['fonts'])
    return sorted(set(missing))


def agree(prompt):
    if not sys.stdin.isatty():return False
    try:return input(prompt+' [y/N] ').strip().lower() in ('y','yes')
    except EOFError:return False


def privileged(command):
    action=command if os.geteuid()==0 else [required_command('sudo'),*command]
    print('Running: '+shlex.join(action),flush=True)
    run(action,check=True)


def provider(package, info):
    return 'libglib2.0-0t64' if package=='libglib2.0-0' and info.base_version >= (24,4) else package


def required_command(name):
    path=shutil.which(name,path=CLEAN_ENV['PATH'])
    if not path:raise RuntimeError('Required command unavailable: '+name+'. No automatic package changes possible.')
    return path


def package_manager(info):
    if not info.supported or info.machine!='x86_64':raise RuntimeError('Unsupported package environment')
    apt=required_command('apt-get');dpkg=required_command('dpkg')
    result=run([dpkg,'--print-architecture'],capture_output=True,text=True,check=True)
    if result.stdout.strip()!='amd64':raise RuntimeError('Native dpkg architecture must be amd64; refusing package changes.')
    return PackageCapabilities(apt,dpkg,result.stdout.strip())


def unsafe_package(name):
    name=name.split(':')[0]
    return name.startswith(('libc6','libc-bin','libc-dev','linux-image','linux-headers','libqt','qt5','qt6')) or name in ('libstdc++6','libgcc-s1')


APT_FLAGS=['--no-remove','--no-upgrade','--no-install-recommends']


def simulate_plan(apt, packages):
    result=run([apt,'--simulate','-o','APT::Get::Show-User-Simulation-Note=false',*APT_FLAGS,'install',*packages],capture_output=True,text=True)
    if result.returncode or result.stderr.strip():raise RuntimeError('APT simulation failed: '+result.stderr+result.stdout)
    planned={};configured=set();summary=None
    for line in result.stdout.splitlines():
        if line.startswith('Remv '):raise RuntimeError('APT plan would remove packages: '+line)
        if line.startswith(('Inst ','Conf ')):
            match=re.fullmatch(r'(Inst|Conf) ([a-z0-9][a-z0-9+.-]*(?::(?:amd64|all))?) \(([^\s()]+) .+\)',line)
            if not match:raise RuntimeError('APT plan includes upgrade or unexpected operation: '+line)
            operation,name,version=match.groups()
            if unsafe_package(name):raise RuntimeError('APT plan would change core runtime or system Qt: '+name)
            if not re.fullmatch(r'[a-zA-Z0-9.+:~_-]+',version):raise RuntimeError('Unexpected APT version: '+version)
            if operation=='Inst':
                if name in planned:raise RuntimeError('Duplicate APT operation: '+name)
                planned[name]=version
            else:
                if planned.get(name)!=version or name in configured:raise RuntimeError('Unexpected APT configuration: '+line)
                configured.add(name)
        elif re.fullmatch(r'\d+ upgraded, \d+ newly installed, \d+ to remove and \d+ not upgraded\.',line):
            if summary is not None:raise RuntimeError('Duplicate APT summary')
            summary=tuple(map(int,re.findall(r'\d+',line)))
        elif line and not (line.startswith(('Reading package lists','Building dependency tree','Reading state information','The following additional packages will be installed:','The following NEW packages will be installed:','Suggested packages:','Recommended packages:','  '))):
            raise RuntimeError('Unexpected APT simulation output: '+line)
    requested={p.split('=')[0].split(':')[0] for p in packages}
    if summary is None or summary[:3]!=(0,len(planned),0) or configured!=set(planned) or not requested <= {p.split(':')[0] for p in planned}:
        raise RuntimeError('APT plan is incomplete or would change existing packages; resolve dependencies manually.')
    return tuple(sorted(name+'='+version for name,version in planned.items()))


def prerequisites(info, synthetic=False):
    if not info.supported:raise RuntimeError(info.diagnostic+' No package-manager action taken.')
    missing=missing_requirements(info)
    if missing:
        allowed={provider(p,info) for p in REQUIREMENTS['libraries']} | set(REQUIREMENTS['commands']) | set(REQUIREMENTS['services']) | {REQUIREMENTS['fonts']}
        if not set(missing) <= allowed:raise RuntimeError('Unknown runtime package provider: '+', '.join(sorted(set(missing)-allowed)))
        if any(unsafe_package(p) for p in missing):raise RuntimeError('Core runtime unavailable; repair the operating system manually. No package changes made.')
        print('BrickSuite requires the following Ubuntu-base packages:\n\n  '+'\n  '.join(missing)+'\n')
        if synthetic:
            print('Synthetic root: no host APT or sudo action. Install prerequisites separately.')
            return False
        apt=package_manager(info).apt_get
        plan=simulate_plan(apt,missing)
        print('Reviewed APT plan (new packages and required dependencies only):\n  '+'\n  '.join(plan))
        if not agree('Install this package plan now?'):
            print('No packages installed. Resolve the listed prerequisites manually or rerun the installer.')
            return False
        if os.geteuid()!=0:required_command('sudo')
        # Pin every reviewed version and reject a changed resolver plan before mutation.
        if simulate_plan(apt,list(plan))!=plan:raise RuntimeError('APT plan changed after consent; retry and review the new plan.')
        try:
            privileged([apt,*APT_FLAGS,'--yes','install',*plan])
            remaining=missing_requirements(info)
            if remaining:raise RuntimeError('Requirements still unavailable: '+', '.join(remaining))
        except (OSError,RuntimeError,subprocess.CalledProcessError) as error:
            raise RuntimeError('BrickSuite program files were not changed. Some system dependencies may now be installed. Resolve the package-manager/runtime failure before retrying: '+str(error)) from error
    print('Requirements satisfied.')
    return True


def verify_bundle(bundle, resolve=True, allow_missing_requirements=False):
    """Verify audited ELF identity, layout and actual loader closure without binutils."""
    bundle=bundle.resolve()
    meta=json.loads((bundle/'share/build-metadata.json').read_text())
    report=json.loads((bundle/'share/abi-audit.json').read_text())
    if report['source_sha']!=meta['source_sha']:raise RuntimeError('Source SHA mismatch')
    if meta['architecture']!='x86_64':raise RuntimeError('Wrong bundle architecture')
    # Native developer packages declare their measured host policy explicitly;
    # ordinary/baseline archives keep the unchanged Ubuntu 22.04 ceiling.
    ceilings={'GLIBC':'2.35','GLIBCXX':'3.4.30','CXXABI':'1.3.13'}
    if meta.get('package_kind')=='local-development':ceilings=meta['abi_policy']
    rows={r['path']:r for r in report['elf']}
    for name in ['bin/BrickSuite','bin/BrickSuiteMeshBooleanWorker','BrickSuite','install.sh','uninstall.sh']:
        if not (bundle/name).is_file() or not os.access(bundle/name,os.X_OK):raise RuntimeError('Missing executable: '+name)
    needed={'bin/BrickSuite','bin/BrickSuiteMeshBooleanWorker','plugins/platforms/libqxcb.so','plugins/platforms/libqwayland.so','plugins/sqldrivers/libqsqlite.so','plugins/tls/libqopensslbackend.so'}
    if not needed <= rows.keys():raise RuntimeError('Incomplete ELF audit inventory')
    actual=set()
    for path in bundle.rglob('*'):
        if path.is_symlink():
            if os.path.isabs(os.readlink(path)) or not path.resolve().is_relative_to(bundle) or not path.exists():raise RuntimeError('Unsafe bundle symlink: '+str(path))
        elif path.is_file():
            with path.open('rb') as stream:magic=stream.read(4)
            if magic==b'\x7fELF':actual.add(str(path.relative_to(bundle)))
    if actual!=rows.keys():raise RuntimeError('ELF inventory differs from audited release')
    for name,row in rows.items():
        path=bundle/name
        if not path.resolve().is_relative_to(bundle) or digest(path)!=row['sha256']:raise RuntimeError('ELF integrity mismatch: '+name)
        expected='$ORIGIN'+('/'+os.path.relpath(bundle/'lib',path.parent) if path.parent!=bundle/'lib' else '')
        if row['rpaths']!=[expected]:raise RuntimeError('Non-relative audited RUNPATH: '+name)
        for family,ceiling in ceilings.items():
            if tuple(map(int,row['requires'][family].split('.')))>tuple(map(int,ceiling.split('.'))):raise RuntimeError('ABI ceiling exceeded: '+name)
        raw=path.read_bytes()
        if has_developer_path(raw):raise RuntimeError('Development path in '+name)
        if resolve:
            result=run(['ldd',str(path)],capture_output=True,text=True)
            output=result.stdout+result.stderr
            if result.returncode or 'not found' in output:
                missing_lines=[line.strip() for line in output.splitlines() if 'not found' in line]
                permitted={soname+' => not found' for names in REQUIREMENTS['libraries'].values() for soname in names}
                # Missing known system providers may be installed later, but ABI
                # version errors or missing private libraries cannot be repaired by APT.
                if result.returncode or not allow_missing_requirements or not missing_lines or not set(missing_lines) <= permitted:
                    raise RuntimeError('Unresolved installed runtime: '+name+'\n'+output)
            verify_resolution(bundle,row,result.stdout)
    if (bundle/'bin/qt.conf').read_text()!='[Paths]\nPrefix=..\nLibraries=lib\nPlugins=plugins\n':raise RuntimeError('Invalid qt.conf')
    for name in meta['required_licenses']:
        if not (bundle/'share/licenses'/name).is_file():raise RuntimeError('Missing license: '+name)
    if not (bundle/BUNDLE_ICON).is_file():raise RuntimeError('Missing application icon')
    return meta['version']


def verify_resolution(bundle, row, output):
    for soname,location in re.findall(r'(\S+)\s+=>\s+(.+?)\s+\(0x[0-9a-f]+\)',output):
        resolved=Path(location).resolve()
        private=resolved.is_relative_to(bundle/'lib')
        if not private and not str(resolved).startswith(('/lib/','/usr/lib/','/lib64/')):
            raise RuntimeError('Unexpected runtime library location: '+location)
        # System font/graphics libraries retain their own transitive runtime.
        # Require direct private dependencies and every Qt/MCUT/TLS/ICU edge
        # to resolve in the bundle, without overriding the host driver stack.
        required=soname in row['needed'] or soname.startswith(('libQt6','libmcut','libssl.so','libcrypto.so','libicu'))
        if required and (bundle/'lib'/soname).exists() and not private:
            raise RuntimeError('Private library resolved outside bundle: '+soname)


def safe_target(root, relative):
    path=root/relative
    for parent in [*path.parents]:
        if parent==root.parent:break
        if parent.is_symlink():raise RuntimeError('Refusing symlink installation parent: '+str(parent))
        if parent.exists() and not parent.is_dir():raise RuntimeError('Refusing non-directory installation parent: '+str(parent))
    return path


def exists(path):return path.exists() or path.is_symlink()


def remove(path):
    if path.is_dir() and not path.is_symlink():shutil.rmtree(path)
    elif exists(path):path.unlink()


def owned_paths(root):
    app=safe_target(root,APP)
    if exists(app):
        marker_path=safe_target(root,APP+'/'+MARKER)
        if app.is_symlink() or not app.is_dir() or marker_path.is_symlink() or not marker_path.is_file():raise RuntimeError('Existing /opt/BrickSuite is not managed by this installer. Move it aside manually before continuing.')
        marker=json.loads((app/MARKER).read_text())
        if not isinstance(marker,dict) or marker.get('owner')!='BrickSuite Linux installer':raise RuntimeError('Unrecognized installation marker')
    for relative in [DESKTOP,ICON,COMMAND]:
        path=safe_target(root,relative)
        if not exists(path):continue
        if relative==COMMAND:valid=path.is_symlink() and os.readlink(path)=='/opt/BrickSuite/BrickSuite'
        elif relative==DESKTOP:valid=not path.is_symlink() and path.is_file() and path.read_text()==DESKTOP_TEXT
        else:valid=not path.is_symlink() and path.is_file() and (app/BUNDLE_ICON).is_file() and digest(path)==digest(app/BUNDLE_ICON)
        if not valid:raise RuntimeError('Refusing to replace unrelated or modified file: '+str(path))
    return app


def replace_transaction(changes, check):
    """Rename complete staged payloads; restore old files on any caught failure."""
    backups=[];completed=[]
    try:
        for target,staged in changes:
            backup=target.with_name('.'+target.name+'.previous-'+uuid.uuid4().hex)
            if exists(target):target.rename(backup);backups.append((target,backup))
            if staged is not None:staged.rename(target)
            completed.append(target)
        check()
    except BaseException:
        for target in reversed(completed):remove(target)
        for target,backup in reversed(backups):backup.rename(target)
        raise
    else:
        for _,backup in backups:remove(backup)


def refresh(root):
    if root!=Path('/'):return
    for command in [['update-desktop-database','/usr/share/applications'],['gtk-update-icon-cache','-f','-t','/usr/share/icons/hicolor']]:
        if shutil.which(command[0],path=CLEAN_ENV['PATH']):
            result=run(command,capture_output=True,text=True)
            if result.returncode:print('Desktop cache refresh could not complete; log out/in if the menu has not refreshed.')


def apply(action, root, source):
    root=root.absolute()
    safe_target(root,APP).parent.mkdir(parents=True,exist_ok=True)
    lock=root/'opt/.bricksuite-install.lock'
    with os.fdopen(os.open(lock,os.O_CREAT|os.O_RDWR|os.O_NOFOLLOW,0o600),'w') as stream:
        fcntl.flock(stream,fcntl.LOCK_EX)
        app=owned_paths(root)
        if action=='uninstall':
            changes=[(root/p,None) for p in [COMMAND,DESKTOP,ICON,APP] if exists(root/p)]
            replace_transaction(changes,lambda:None);refresh(root)
            print('BrickSuite uninstalled. User data, credentials and shared prerequisite packages were preserved.')
            return
        if source.resolve()==app.resolve():raise RuntimeError('Run installation from a separately extracted archive, not the installed copy.')
        version=verify_bundle(source)
        if app.exists():print('Replacing installed version '+json.loads((app/'share/build-metadata.json').read_text())['version'])
        staged=[]
        try:
            stage=Path(tempfile.mkdtemp(prefix='.bricksuite-stage-',dir=app.parent));staged.append(stage)
            payload=stage/'BrickSuite';shutil.copytree(source,payload,symlinks=True)
            (payload/MARKER).write_text(json.dumps({'owner':'BrickSuite Linux installer','version':version})+'\n')
            verify_bundle(payload)
            changes=[(app,payload)]
            for relative in [DESKTOP,ICON,COMMAND]:
                target=safe_target(root,relative);target.parent.mkdir(parents=True,exist_ok=True)
                area=Path(tempfile.mkdtemp(prefix='.bricksuite-stage-',dir=target.parent));staged.append(area);entry=area/'entry'
                if relative==DESKTOP:entry.write_text(DESKTOP_TEXT);entry.chmod(0o644)
                elif relative==ICON:shutil.copy2(payload/BUNDLE_ICON,entry);entry.chmod(0o644)
                else:entry.symlink_to('/opt/BrickSuite/BrickSuite')
                changes.append((target,entry))
            replace_transaction(changes,lambda:verify_bundle(app))
        finally:
            for path in staged:remove(path)
        refresh(root)
        print('BrickSuite installed successfully.\nLaunch: Applications → BrickSuite\nOr run: bricksuite')


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('action',choices=['install','uninstall']);p.add_argument('--root',type=Path,default=Path('/'),help='Synthetic filesystem root; never invokes sudo or apt')
    p.add_argument('--check',action='store_true',help='Report prerequisites only; no installation or sudo')
    p.add_argument('--apply',action='store_true',help=argparse.SUPPRESS)
    args=p.parse_args(argv);root=args.root.absolute()
    try:
        info=os_info()
        if args.apply:
            if root==Path('/') and os.geteuid()!=0:raise RuntimeError('Privileged installation requires root')
            apply(args.action,root,HERE);return 0
        if args.action=='install':
            if args.check:
                missing=missing_requirements(info);print('Missing: '+', '.join(missing) if missing else 'Requirements satisfied.');return 1 if missing else 0
            app=owned_paths(root)
            if HERE.resolve()==app.resolve():raise RuntimeError('Run installation from a separately extracted archive, not the installed copy.')
            verify_bundle(HERE,allow_missing_requirements=True)
            if app.exists():verify_bundle(app,resolve=False)
            if not prerequisites(info,root!=Path('/')):return 0
            verify_bundle(HERE)
        elif args.check:raise RuntimeError('--check is for installation prerequisites')
        print('User data and credentials are preserved. Close BrickSuite before replacing or removing application files.')
        if not agree(('Install to ' if args.action=='install' else 'Uninstall from ')+str(root/APP)+'?'):
            print('Cancelled. No application files changed.');return 0
        if root==Path('/') and os.geteuid()!=0:privileged(['/usr/bin/python3',str(HERE/'installer.py'),args.action,'--apply'])
        else:apply(args.action,root,HERE)
        return 0
    except (OSError,ValueError,KeyError,RuntimeError,subprocess.CalledProcessError) as error:
        print('BrickSuite installation stopped: '+str(error),file=sys.stderr);return 1


if __name__=='__main__':raise SystemExit(main())
