#!/usr/bin/env python3
"""Qt Creator adapter: discover native inputs, then call the one Linux packager."""
import argparse
import ctypes
import fcntl
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import uuid
from audit_bundle import digest, version
from bootstrap import fetch
from package_linux import stage

LOCK=json.loads(Path(__file__).with_name('source-lock.json').read_text())


def require_tools():
    names=['git','readelf','objdump','strip','patchelf','ldd','dpkg-query','tar']
    missing=[name for name in names if not shutil.which(name)]
    if missing:raise RuntimeError('Missing Linux packaging prerequisites: '+', '.join(missing)+'. Install the corresponding Ubuntu packages (binutils, patchelf, libc-bin, dpkg, tar, git), then rebuild Deploy.')


def host_policy(compiler):
    cxx=Path(subprocess.check_output([str(compiler),'-print-file-name=libstdc++.so.6'],text=True).strip()).resolve()
    symbols=subprocess.check_output(['objdump','-T',str(cxx)],text=True)
    ceilings={'GLIBC':os.confstr('CS_GNU_LIBC_VERSION').split()[-1]}
    for name in ['GLIBCXX','CXXABI']:
        ceilings[name]=max(re.findall(r'\b'+name+r'_(\d+(?:\.\d+)+)',symbols),key=version)
    return ceilings


def prepare_sources(build, provided):
    if provided:
        sources=provided.resolve()
    else:
        sources=build/'deploy-inputs/sources';downloads=build/'deploy-inputs/downloads'
        sources.mkdir(parents=True,exist_ok=True)
        qt=LOCK['qt']['source'];target=sources/qt['directory']
        if not (sources/'qt-source-verified.json').exists():
            print('Preparing matching Qt license sources (first Deploy only; hash-verified download).',flush=True)
            archive=fetch(qt,downloads/Path(qt['url']).name)
            subprocess.run(['tar','-xJf',str(archive),'-C',str(sources),*[qt['directory']+'/'+name for name in ['qtbase','qtsvg','qtimageformats','qtwebsockets','qtwayland','LICENSES','cmake']]],check=True)
            (sources/'qt-source-verified.json').write_text(json.dumps({'sha256':qt['sha256']})+'\n')
        fetch(LOCK['icu_license'],sources/'icu-73.2-LICENSE')
    for required in ['qt-everywhere-src-6.10.3/LICENSES/LGPL-3.0-only.txt','icu-73.2-LICENSE']:
        if not (sources/required).is_file():raise RuntimeError('Missing matching notice source: '+str(sources/required))
    return sources


def openssl_license(crypto):
    owners=subprocess.check_output(['dpkg-query','-S',str(crypto.resolve())],text=True).splitlines()
    package=owners[0].split(': ')[0].split(':')[0]
    license=Path('/usr/share/doc')/package/'copyright'
    if not license.is_file():raise RuntimeError('Missing runtime OpenSSL license: '+str(license))
    return license


def publish(staged, output):
    if output.is_symlink():raise RuntimeError('Deploy output must not be a symlink')
    backup=None
    if output.exists():
        if not (output/'local-deploy.json').is_file():raise RuntimeError('Refusing to replace an unmanaged deploy directory: '+str(output))
        backup=output.with_name('.deploy-previous-'+uuid.uuid4().hex);output.rename(backup)
    try:staged.rename(output)
    except BaseException:
        if backup:backup.rename(output)
        raise
    if backup:shutil.rmtree(backup)


def deploy(args):
    if args.config!='Release':raise RuntimeError('Linux Deploy requires Release. Select the Release configuration in Qt Creator and refresh CMake.')
    if platform.system()!='Linux' or platform.machine()!='x86_64':raise RuntimeError('Local Linux Deploy currently supports x86_64 Linux only')
    if args.qt_version!=LOCK['qt']['version']:raise RuntimeError('Linux Deploy currently requires the pinned Qt '+LOCK['qt']['version'])
    require_tools()
    args.source=args.source.resolve();args.build=args.build.resolve();args.qt=args.qt.resolve();args.binary_dir=args.binary_dir.resolve();args.crypto=args.crypto.absolute()
    for name in ['BrickSuite','BrickSuiteMeshBooleanWorker']:
        if not (args.binary_dir/name).is_file():raise RuntimeError('Missing production target: '+name)
    print('LOCAL DEVELOPMENT PACKAGE: built with this host runtime. Not an official Ubuntu 22.04 baseline release.',flush=True)
    with (args.build/'.linux-deploy.lock').open('w') as lock:
        fcntl.flock(lock,fcntl.LOCK_EX)
        sources=prepare_sources(args.build,args.sources)
        sha=subprocess.check_output(['git','-C',str(args.source),'rev-parse','HEAD'],text=True).strip()
        dirty=bool(subprocess.check_output(['git','-C',str(args.source),'status','--porcelain'],text=True).strip())
        distro=platform.freedesktop_os_release()['PRETTY_NAME']
        ceilings=host_policy(args.compiler)
        crypto=ctypes.CDLL(str(args.crypto));crypto.OpenSSL_version.restype=ctypes.c_char_p
        ssl_version=crypto.OpenSSL_version(0).decode()
        metadata={'record':{'source_sha':sha,'schema':35,'protocol':'1.5'},'source_dirty':dirty,
                  'package_kind':'local-development','version':args.version,'build_distro':distro,
                  'build_glibc':ceilings['GLIBC'],'abi_policy':ceilings,'glibc_floor':None,
                  'compiler':subprocess.check_output([str(args.compiler),'-dumpfullversion'],text=True).strip(),
                  'qt':args.qt_version,'openssl':ssl_version,
                  'system_cxx_runtime':{'policy':'native host libstdc++/libgcc; not bundled','exports':ceilings},
                  'source_lock_role':'reference baseline pins; native Qt/OpenSSL/system inputs recorded separately',
                  'integration_hashes':{name:digest(args.source/name) for name in ['CMakeLists.txt','cmake/BrickSuiteLinuxDeploy.cmake','cmake/RequireLinuxDeployRelease.cmake']}}
        with tempfile.TemporaryDirectory(prefix='.linux-deploy-',dir=args.build) as temporary:
            staged=Path(temporary)/'deploy'
            options=argparse.Namespace(source=args.source,build=args.build,binary_dir=args.binary_dir,qt=args.qt,
                openssl=args.crypto.parent.parent,crypto=args.crypto,sources=sources,output=staged,stage_only=False,
                openssl_license=openssl_license(args.crypto),local_metadata=metadata)
            result=stage(options)
            (staged/'local-deploy.json').write_text(json.dumps({'source_sha':sha,'package_kind':'local-development'})+'\n')
            output=args.build/'deploy';publish(staged,output)
        archive=output/Path(result['archive']).name
        print('\nBrickSuite Linux deployment package created\n'
              'Package kind: LOCAL DEVELOPMENT PACKAGE (not Ubuntu 22.04 baseline release)\n'
              f'Package: {archive}\nArchive size: {result["bytes"]} bytes\nSHA-256: {result["sha256"]}\n'
              f'Architecture: x86_64\nSource commit: {sha}'+(' (working tree modified)' if dirty else '')+'\n'
              f'Build OS: {distro}\nBuild-host glibc: {ceilings["GLIBC"]}\n'
              f'Observed ABI floor: {json.dumps(result["ceilings"])}\nQt version: {args.qt_version}',flush=True)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ['source','build','binary-dir','qt','crypto','compiler']:p.add_argument('--'+name,type=Path,required=True)
    for name in ['config','qt-version','version']:p.add_argument('--'+name,required=True)
    p.add_argument('--sources',type=Path)
    try:deploy(p.parse_args())
    except (OSError,ValueError,KeyError,RuntimeError,subprocess.CalledProcessError) as error:
        print('ERROR: Linux Deploy failed: '+str(error),file=sys.stderr);return 1
    return 0


if __name__=='__main__':raise SystemExit(main())
