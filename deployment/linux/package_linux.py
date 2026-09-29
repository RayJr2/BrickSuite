#!/usr/bin/env python3
"""Stage, audit and archive the Ubuntu 22.04 Release closure (run inside Jammy)."""
import argparse
import gzip
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tarfile
from audit_bundle import POLICY, audit, digest, elf_files, inspect
import notices
from normalize_qt_diagnostics import normalize


def write_json(path,value):
    path.parent.mkdir(parents=True,exist_ok=True)
    path.write_text(json.dumps(value,indent=2)+'\n')


def archive_tree(bundle, output, epoch):
    with output.open('wb') as raw, gzip.GzipFile(filename='',mode='wb',fileobj=raw,mtime=0) as compressed:
        with tarfile.open(fileobj=compressed,mode='w') as archive:
            for path in [bundle,*sorted(bundle.rglob('*'))]:
                info=archive.gettarinfo(str(path),str(path.relative_to(bundle.parent)))
                info.uid=info.gid=0;info.uname=info.gname='';info.mtime=epoch
                if info.isfile():
                    with path.open('rb') as stream: archive.addfile(info,stream)
                else: archive.addfile(info)


def stage(args):
    if 'VERSION_ID="22.04"' not in Path('/etc/os-release').read_text():
        raise ValueError('Package inside Ubuntu 22.04, not on a newer host')
    subprocess.run(['git','-C',str(args.source),'diff','--exit-code','HEAD','--','src','resources','cmake','CMakeLists.txt'],check=True)
    sha=subprocess.check_output(['git','-C',str(args.source),'rev-parse','HEAD'],text=True).strip()
    record=json.loads((args.build/'source-record.json').read_text())
    if record['source_sha'] != sha: raise ValueError('Source SHA mismatch between build and checkout')
    gate=None
    if not args.stage_only:
        gate=json.loads(Path('/work/evidence/tests/summary.json').read_text())
        configured=json.loads((args.build/'configured-tests.json').read_text())
        if gate['full']!={'passed':configured['count'],'failed':0,'skipped':0}:raise ValueError('Full CTest gate not satisfied')
        if gate['focused']['failed'] or gate['focused']['skipped'] or not gate['focused']['passed']:raise ValueError('Focused CTest gate not satisfied')
    lock=json.loads(Path(__file__).with_name('source-lock.json').read_text())
    for name in ['mcut','lib3mf']:
        actual=subprocess.check_output(['git','-C',str(args.build/f'_deps/{name}-src'),'rev-parse','HEAD'],text=True).strip()
        if actual!=lock[name]['revision']:raise ValueError(name+' source revision mismatch')
    bundle=args.output/'BrickSuite'
    bundle.mkdir(parents=True,exist_ok=False)
    for name in ['bin','lib','plugins','share/applications','share/icons/hicolor/256x256/apps']:(bundle/name).mkdir(parents=True)
    for name in ['BrickSuite','BrickSuiteMeshBooleanWorker']:shutil.copy2(args.build/name,bundle/'bin'/name)
    for name in POLICY['plugins']:
        target=bundle/'plugins'/name;target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(args.qt/'plugins'/name,target)
    candidates={}
    for root in [Path('/lib/x86_64-linux-gnu'),Path('/usr/lib/x86_64-linux-gnu'),args.build/'bin',args.build/'_deps/mcut-build',args.qt/'lib',args.openssl/'lib']:
        for path in root.rglob('*.so*'):
            if path.is_file(): candidates[path.name]=path
    origin={}
    def copy_library(name):
        if (bundle/'lib'/name).exists():return
        src=candidates.get(name)
        if src is None:raise ValueError('Missing private dependency '+name)
        real=src.resolve();dest=bundle/'lib'/real.name
        if not dest.exists():shutil.copy2(real,dest);origin[real.name]={'input_sha256':digest(real),'origin':'Qt SDK' if real.is_relative_to(args.qt.resolve()) else 'baseline build/system'}
        if name!=real.name:(bundle/'lib'/name).symlink_to(real.name)
    for name in ['libssl.so.3','libcrypto.so.3']:copy_library(name)
    scanned=set()
    while True:
        pending=[p for p in elf_files(bundle) if p not in scanned]
        if not pending:break
        for binary in pending:
            scanned.add(binary)
            for name in inspect(binary)['needed']:
                if name.startswith(('libQt6','libicu','libmcut')) or name in POLICY['private_sonames']:copy_library(name)
                elif name not in POLICY['system_sonames']:raise ValueError('Undeclared system dependency '+name)
    remappings=[]
    for binary in elf_files(bundle):
        subprocess.run(['strip','--strip-unneeded',str(binary)],check=True)
        remappings.extend(normalize(binary))
        relative=os.path.relpath(bundle/'lib',binary.parent)
        subprocess.run(['patchelf','--set-rpath','$ORIGIN'+('/'+relative if relative!='.' else ''),str(binary)],check=True)
    (bundle/'bin/qt.conf').write_text('[Paths]\nPrefix=..\nLibraries=lib\nPlugins=plugins\n')
    (bundle/'BrickSuite').write_text('#!/bin/sh\nset -eu\nroot=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\nexec "$root/bin/BrickSuite" "$@"\n')
    (bundle/'BrickSuite').chmod(0o755)
    for name in ['install.sh','uninstall.sh','installer.py','runtime-requirements.json']:
        shutil.copy2(Path(__file__).with_name(name),bundle/name)
    for name in ['install.sh','uninstall.sh']:(bundle/name).chmod(0o755)
    (bundle/'share/applications/BrickSuite.desktop').write_text('[Desktop Entry]\nType=Application\nName=BrickSuite\nExec=BrickSuite\nIcon=BrickSuite\nTerminal=false\nCategories=Utility;\n')
    ico=(args.source/'resources/icons/bricksuite_multi.ico').read_bytes()
    pngs=[]
    for i in range(struct.unpack_from('<H',ico,4)[0]):
        width,height,_,_,_,_,size,offset=struct.unpack_from('<BBBBHHII',ico,6+16*i)
        if ico[offset:offset+8]==b'\x89PNG\r\n\x1a\n':pngs.append(((width or 256)*(height or 256),ico[offset:offset+size]))
    if not pngs:raise ValueError('No PNG icon in repository ICO')
    (bundle/'share/icons/hicolor/256x256/apps/BrickSuite.png').write_bytes(max(pngs)[1])
    required=notices.stage(bundle,args.source,args.build,args.qt,args.sources,args.openssl)
    shutil.copyfile(Path(__file__).with_name('normalize_qt_diagnostics.py'),bundle/'share/licenses/Qt/normalize_qt_diagnostics.py')
    write_json(bundle/'share/licenses/Qt/diagnostic-path-remapping.json',remappings)
    required += ['Qt/normalize_qt_diagnostics.py','Qt/diagnostic-path-remapping.json']
    metadata={**record,'source_repository':'https://github.com/RayJr2/BrickSuite','version':'0.4.0','architecture':'x86_64','build_distro':'Ubuntu 22.04.5','glibc_floor':'2.35','compiler':subprocess.check_output(['g++-11','-dumpfullversion'],text=True).strip(),'qt':lock['qt']['version'],'openssl':lock['openssl']['version'],'required_licenses':required,'test_gate':'pending; staging only' if gate is None else 'passed','packaging_tool_hashes':{p.name:digest(p) for p in sorted(Path(__file__).parent.glob('*')) if p.is_file() and p.name not in ['README.md','VALIDATION.md']}}
    write_json(bundle/'share/build-metadata.json',metadata)
    write_json(bundle/'share/source-lock.json',lock)
    write_json(bundle/'share/dependencies.json',{'private':origin,'system_cxx_runtime':POLICY['cxx_runtime_evidence'],'system_allowed_sonames':POLICY['system_sonames'],'system_direct_needed':sorted({name for binary in elf_files(bundle) for name in inspect(binary)['needed'] if name in POLICY['system_sonames']}),'plugins':POLICY['plugins'],'lib3mf':{'version':lock['lib3mf']['version'],'linkage':'static including pinned bundled dependencies','submodules':subprocess.check_output(['git','-C',str(args.build/'_deps/lib3mf-src'),'submodule','status','--recursive'],text=True).splitlines()},'mcut_queue_overlay':{'reason':'Prevent lost wakeup between empty check and queue wait on Linux','regression':'McutQueueWakeup','sha256':digest(args.source/lock['mcut']['overlay']),'generated_header_sha256':digest(args.build/'mcut-linux-include/mcut/internal/tpool.h')}})
    shutil.copy2('/work/ubuntu-packages.tsv',bundle/'share/ubuntu-build-packages.tsv')
    shutil.copy2(Path(__file__).with_name('RUNTIME.md'),bundle/'README.md')
    if gate is not None:write_json(bundle/'share/test-summary.json',gate)
    report=audit(bundle,sha)
    write_json(bundle/'share/abi-audit.json',report)
    for path in bundle.rglob('*'):
        if not path.is_symlink():path.chmod(0o755 if path.is_dir() or os.access(path,os.X_OK) else 0o644)
    if args.stage_only:
        print(json.dumps({'staging_only':str(bundle),'ceilings':report['ceilings'],'elf_count':report['elf_count']},indent=2));return
    epoch=int(subprocess.check_output(['git','-C',str(args.source),'show','-s','--format=%ct','HEAD'],text=True))
    archive=args.output/'BrickSuite-v0.4.0-Linux-x86_64.tar.gz'
    archive_tree(bundle,archive,epoch)
    (args.output/(archive.name+'.sha256')).write_text(digest(archive)+'  '+archive.name+'\n')
    for name in ['build-metadata.json','dependencies.json','abi-audit.json']:shutil.copy2(bundle/'share'/name,args.output/name)
    print(json.dumps({'archive':str(archive),'bytes':archive.stat().st_size,'sha256':digest(archive),'ceilings':report['ceilings'],'elf_count':report['elf_count']},indent=2))


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--stage-only',action='store_true',help='Audit staging without creating a release archive; allows pending test gate')
    for name,default in [('source','/src'),('build','/work/build'),('qt','/opt/qt'),('openssl','/work/openssl-prefix/usr'),('sources','/work/sources'),('output','/work/release')]:p.add_argument('--'+name,type=Path,default=Path(default))
    stage(p.parse_args())
if __name__=='__main__':main()
