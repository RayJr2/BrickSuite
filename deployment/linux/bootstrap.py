#!/usr/bin/env python3
"""Create a hash-pinned rootless Ubuntu 22.04 build image; no host packages changed."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import urllib.request
from audit_bundle import digest
from run_baseline import command

LOCK=json.loads(Path(__file__).with_name('source-lock.json').read_text())


def fetch(record, destination):
    destination.parent.mkdir(parents=True,exist_ok=True)
    if not destination.exists():
        temporary=destination.with_suffix(destination.suffix+'.partial')
        request=urllib.request.Request(record['url'],headers={'User-Agent':'Mozilla/5.0 BrickSuite release bootstrap'})
        with urllib.request.urlopen(request,timeout=120) as response, temporary.open('wb') as out:shutil.copyfileobj(response,out)
        if digest(temporary)!=record['sha256']:raise ValueError('Download hash mismatch: '+destination.name)
        temporary.rename(destination)
    if digest(destination)!=record['sha256']:raise ValueError('Cached input hash mismatch: '+destination.name)
    return destination


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--image-only',action='store_true',help='Validate rootfs/package bootstrap without compiling dependencies');p.add_argument('--directory',type=Path,default=Path('build/linux-l2'));p.add_argument('--jobs',default='6');args=p.parse_args()
    base=args.directory.resolve();downloads=base/'downloads';root=base/'rootfs';work=base/'work';repo=Path(__file__).resolve().parents[2]
    if (base/'bootstrap-complete.json').exists():raise SystemExit('Environment already prepared; use run_baseline.py. Choose a fresh directory to reproduce.')
    if root.exists():raise SystemExit('Refusing to overwrite an existing rootfs. Choose a fresh directory.')
    archive=fetch(LOCK['ubuntu'],downloads/Path(LOCK['ubuntu']['url']).name)
    root.mkdir(parents=True);work.mkdir(parents=True,exist_ok=True)
    subprocess.run(['tar','--no-same-owner','-xzf',str(archive),'-C',str(root)],check=True)
    # Bootstrap CA data only; all executable code comes from verified Jammy packages.
    ca=root/'etc/ssl/certs/ca-certificates.crt';ca.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile('/etc/ssl/certs/ca-certificates.crt',ca)
    resolv=root/'etc/resolv.conf'
    if resolv.is_symlink():resolv.unlink()
    resolv.touch()
    snapshot=LOCK['ubuntu']['snapshot']
    (root/'etc/apt/sources.list').write_text(''.join(f'deb https://snapshot.ubuntu.com/ubuntu/{snapshot}/ {suite} main universe restricted multiverse\n' for suite in ['jammy','jammy-updates','jammy-security']))
    (root/'etc/apt/apt.conf.d/99bricksuite').write_text('APT::Sandbox::User "root";\nAcquire::Check-Valid-Until "false";\n')
    policy=root/'usr/sbin/policy-rc.d';policy.write_text('#!/bin/sh\nexit 101\n');policy.chmod(0o755)
    def run(argv):subprocess.run(command(root,work,repo,argv),check=True)
    run(['apt-get','update'])
    # The user namespace has one UID/GID. fakeroot handles package ownership
    # bookkeeping; the unused system-bus launcher cannot acquire a host setuid bit.
    run(['apt-get','download','fakeroot','libfakeroot'])
    for deb in sorted(work.glob('*fakeroot*.deb')):run(['dpkg-deb','-x','/work/'+deb.name,'/'])
    run(['dpkg-statoverride','--add','root','root','0755','/usr/lib/dbus-1.0/dbus-daemon-launch-helper'])
    # fontconfig's postinst chowns this only when creating it. The build image
    # has no staff GID mapping and needs no shared writable font directory.
    (root/'usr/local/share/fonts').mkdir(parents=True,exist_ok=True)
    packages=Path(__file__).with_name('baseline-packages.list').read_text().split()
    run(['fakeroot-sysv','apt-get','install','-y','--no-install-recommends',*packages,'fakeroot'])
    state=subprocess.check_output(command(root,work,repo,['dpkg','--audit']),text=True)
    if state.strip():raise RuntimeError('Unconfigured baseline packages: '+state)
    with (work/'ubuntu-packages.tsv').open('w') as out:subprocess.run(command(root,work,repo,['dpkg-query','-W','-f=${binary:Package}\t${Version}\n']),check=True,stdout=out)
    if args.image_only:
        print('Ubuntu baseline image and package bootstrap verified');return
    qt=work/'qt-dist';qt.mkdir()
    for record in LOCK['qt']['archives']:
        path=fetch(record,downloads/record['file'])
        shutil.copy2(path,work/path.name)
        run(['7z','x','-y','-o/work/qt-dist','/work/'+path.name])
    for path in qt.glob('libicu*.so*'):
        target=qt/'lib'/path.name
        if path.is_symlink():target.symlink_to(path.readlink())
        else:shutil.copy2(path,target)
    (root/'opt/qt').symlink_to('/work/qt-dist')
    sources=work/'sources';sources.mkdir()
    qtarchive=fetch(LOCK['qt']['source'],downloads/Path(LOCK['qt']['source']['url']).name)
    prefix=LOCK['qt']['source']['directory']
    subprocess.run(['tar','-xJf',str(qtarchive),'-C',str(sources),*[prefix+'/'+d for d in ['qtbase','qtsvg','qtimageformats','qtwebsockets','qtwayland','LICENSES','cmake']]],check=True)
    ssl=fetch(LOCK['openssl'],downloads/Path(LOCK['openssl']['url']).name)
    subprocess.run(['tar','-xzf',str(ssl),'-C',str(sources)],check=True)
    fetch(LOCK['icu_license'],sources/'icu-73.2-LICENSE')
    ldraw=fetch(LOCK['ldraw'],downloads/'ldraw-complete.zip')
    shutil.copy2(ldraw,work/ldraw.name);run(['7z','x','-y','-o/work','/work/'+ldraw.name])
    run(['python3','/src/deployment/linux/prepare_openssl.py','--jobs',args.jobs])
    (base/'bootstrap-complete.json').write_text(json.dumps({'lock_sha256':digest(Path(__file__).with_name('source-lock.json')),'ubuntu_snapshot':snapshot},indent=2)+'\n')
if __name__=='__main__':main()
