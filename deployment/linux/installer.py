#!/usr/bin/env python3
"""Ubuntu system installer; standard-library only, no SDK or build tools needed."""
import argparse
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


def os_info(path=Path('/etc/os-release'), machine=None):
    values={}
    for line in path.read_text().splitlines():
        if '=' in line and not line.startswith('#'):
            key,value=line.split('=',1);values[key]=shlex.split(value)[0] if value else ''
    arch=machine or platform.machine()
    print(f"Platform: {values.get('PRETTY_NAME',values.get('ID','unknown'))}; version {values.get('VERSION_ID','unknown')}; architecture {arch}")
    parts=values.get('VERSION_ID','').split('.')
    supported=values.get('ID')=='ubuntu' and all(p.isdigit() for p in parts) and tuple(map(int,parts)) >= (22,4) and arch=='x86_64'
    if not supported:raise RuntimeError('Automatic installation supports Ubuntu 22.04 or newer on x86_64 only. No package-manager action taken; manual portable use is separate.')
    return tuple(map(int,parts))


def missing_requirements(version):
    cache=run(['/sbin/ldconfig','-p'],capture_output=True,text=True,check=True).stdout
    available={}
    for line in cache.splitlines():
        match=re.match(r'\s*(\S+)\s+\([^)]*x86-64[^)]*\)\s+=>\s+(\S+)',line)
        if match and Path(match[2]).is_file():available[match[1]]=match[2]
    missing=[]
    for package,names in REQUIREMENTS['libraries'].items():
        if any(name not in available for name in names):
            missing.append('libglib2.0-0t64' if package=='libglib2.0-0' and version >= (24,4) else package)
    for package,names in REQUIREMENTS['commands'].items():
        if any(not shutil.which(name,path=CLEAN_ENV['PATH']) for name in names):missing.append(package)
    if not any(Path(p).is_file() for p in ['/usr/lib/systemd/user/dbus.socket','/lib/systemd/user/dbus.socket']):missing.append('dbus-user-session')
    if not any(shutil.which(name,path=CLEAN_ENV['PATH']) for name in ['gnome-keyring-daemon','kwalletd5','kwalletd6']):missing.append('gnome-keyring')
    fc=shutil.which('fc-list',path=CLEAN_ENV['PATH'])
    if not fc or not run([fc],capture_output=True,text=True,check=True).stdout.strip():missing.append(REQUIREMENTS['fonts'])
    return sorted(set(missing))


def agree(prompt):
    try:return input(prompt+' [Y/n] ').strip().lower() in ('','y','yes')
    except EOFError:return False


def privileged(command):
    action=command if os.geteuid()==0 else ['sudo',*command]
    print('Running: '+shlex.join(action),flush=True)
    run(action,check=True)


def prerequisites(version, synthetic=False):
    missing=missing_requirements(version)
    if missing:
        command=['apt-get','install','--no-remove',*missing]
        print('BrickSuite requires the following Ubuntu packages:\n\n  '+'\n  '.join(missing)+'\n')
        if synthetic or not agree('Install these packages now?'):
            print('No packages installed. To continue manually: sudo '+shlex.join(command))
            return False
        # apt-get retains its own confirmation and terminal; never remove packages.
        privileged(command)
        remaining=missing_requirements(version)
        if remaining:raise RuntimeError('Requirements still unavailable after package installation: '+', '.join(remaining))
    print('Requirements satisfied.')
    return True


def verify_bundle(bundle, resolve=True):
    """Verify audited ELF identity, layout and actual loader closure without binutils."""
    bundle=bundle.resolve()
    meta=json.loads((bundle/'share/build-metadata.json').read_text())
    report=json.loads((bundle/'share/abi-audit.json').read_text())
    if report['source_sha']!=meta['source_sha']:raise RuntimeError('Source SHA mismatch')
    if meta['architecture']!='x86_64':raise RuntimeError('Wrong bundle architecture')
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
        for family,ceiling in [('GLIBC','2.35'),('GLIBCXX','3.4.30'),('CXXABI','1.3.13')]:
            if tuple(map(int,row['requires'][family].split('.')))>tuple(map(int,ceiling.split('.'))):raise RuntimeError('ABI ceiling exceeded: '+name)
        raw=path.read_bytes()
        if has_developer_path(raw):raise RuntimeError('Development path in '+name)
        if resolve:
            result=run(['ldd',str(path)],capture_output=True,text=True)
            if result.returncode or 'not found' in result.stdout+result.stderr:raise RuntimeError('Unresolved installed runtime: '+name+'\n'+result.stdout+result.stderr)
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
    return path


def exists(path):return path.exists() or path.is_symlink()


def remove(path):
    if path.is_dir() and not path.is_symlink():shutil.rmtree(path)
    elif exists(path):path.unlink()


def owned_paths(root):
    app=safe_target(root,APP)
    if exists(app):
        if app.is_symlink() or not (app/MARKER).is_file():raise RuntimeError('Existing /opt/BrickSuite is not managed by this installer. Move it aside manually before continuing.')
        marker=json.loads((app/MARKER).read_text())
        if marker.get('owner')!='BrickSuite Linux installer':raise RuntimeError('Unrecognized installation marker')
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
        version=os_info()
        if args.apply:
            if root==Path('/') and os.geteuid()!=0:raise RuntimeError('Privileged installation requires root')
            apply(args.action,root,HERE);return 0
        if args.action=='install':
            if args.check:
                missing=missing_requirements(version);print('Missing: '+', '.join(missing) if missing else 'Requirements satisfied.');return 1 if missing else 0
            if not prerequisites(version,root!=Path('/')):return 0
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
