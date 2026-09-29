#!/usr/bin/env python3
"""Fail closed on Linux bundle ABI, dependency, provenance and relocation defects."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

POLICY = json.loads(Path(__file__).with_name('policy.json').read_text())


def version(value):
    return tuple(int(v) for v in value.split('.'))


def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024*1024), b''): h.update(chunk)
    return h.hexdigest()


def elf_files(root):
    for p in sorted(Path(root).rglob('*')):
        if p.is_file() and not p.is_symlink():
            with p.open('rb') as f:
                if f.read(4) == b'\x7fELF': yield p


def inspect(path):
    header = subprocess.check_output(['readelf','-h',str(path)], text=True)
    dynamic = subprocess.check_output(['readelf','-d',str(path)], text=True)
    symbols = subprocess.check_output(['objdump','-T',str(path)], text=True)
    needs = {}
    for prefix in POLICY['ceilings']:
        tags = re.findall(r'\*UND\*[^\n]*?\b'+prefix+r'_([A-Za-z0-9_.]+)', symbols)
        if any(not re.fullmatch(r'\d+(?:\.\d+)+', tag) for tag in tags):
            raise ValueError(f'Unrecognized ABI requirement in {path}: {tags}')
        values = re.findall(r'\*UND\*[^\n]*?\b'+prefix+r'_(\d+(?:\.\d+)+)', symbols)
        needs[prefix] = max(values, key=version, default='0')
    return {'machine':re.search(r'Machine:\s*(.+)',header)[1].strip(),
            'class':re.search(r'Class:\s*(.+)',header)[1].strip(),
            'needed':re.findall(r'\(NEEDED\).*?\[(.*?)\]',dynamic),
            'rpaths':re.findall(r'\((?:RUNPATH|RPATH)\).*?\[(.*?)\]',dynamic),
            'soname':next(iter(re.findall(r'\(SONAME\).*?\[(.*?)\]',dynamic)),None),
            'requires':needs}


def check_info(info, relative, expected_rpath, ceilings=None):
    if info['machine'] != POLICY['architecture'] or info['class'] != 'ELF64':
        raise ValueError(f'Wrong architecture: {relative}')
    for name, ceiling in (ceilings or POLICY['ceilings']).items():
        if version(info['requires'][name]) > version(ceiling):
            raise ValueError(f'{relative}: {name}_{info["requires"][name]} exceeds {ceiling}')
    if info['rpaths'] != [expected_rpath]:
        raise ValueError(f'Non-relative or unexpected RUNPATH: {relative}: {info["rpaths"]}')


def audit(bundle, expected_sha, resolve=True, ceilings=None, forbidden_prefixes=()):
    bundle = Path(bundle).resolve()
    metadata = json.loads((bundle/'share/build-metadata.json').read_text())
    if metadata['source_sha'] != expected_sha or not re.fullmatch('[0-9a-f]{40}', expected_sha):
        raise ValueError('Source SHA mismatch')
    for name in ['BrickSuite','BrickSuiteMeshBooleanWorker']:
        path = bundle/'bin'/name
        if not path.is_file() or not os.access(path,os.X_OK): raise ValueError('Missing executable helper/app: '+name)
    expected = set(POLICY['plugins'])
    actual = {str(p.relative_to(bundle/'plugins')) for p in (bundle/'plugins').rglob('*.so')}
    if actual != expected: raise ValueError(f'Plugin allowlist mismatch: missing={expected-actual}, extra={actual-expected}')
    if not metadata.get('required_licenses'): raise ValueError('Missing license inventory')
    for name in metadata['required_licenses']:
        p=bundle/'share/licenses'/name
        if not p.is_file() or not p.stat().st_size: raise ValueError('Missing license: '+name)
    if list((bundle/'lib').glob('libstdc++*')) or list((bundle/'lib').glob('libgcc_s*')):
        raise ValueError('Private C++ runtime is forbidden')
    if (bundle/'bin/qt.conf').read_text() != '[Paths]\nPrefix=..\nLibraries=lib\nPlugins=plugins\n':
        raise ValueError('Invalid qt.conf')
    for p in bundle.rglob('*'):
        if p.is_symlink() and (os.path.isabs(os.readlink(p)) or not p.resolve().is_relative_to(bundle) or not p.exists()):
            raise ValueError('Unsafe or broken bundle symlink: '+str(p))
    binaries=list(elf_files(bundle))
    mandatory=[bundle/'bin/BrickSuite',bundle/'bin/BrickSuiteMeshBooleanWorker',*[bundle/'plugins'/p for p in POLICY['plugins']]]
    if any(p not in binaries for p in mandatory): raise ValueError('Required payload is not an ELF binary')
    for name in ['libssl.so.3','libcrypto.so.3']:
        if not (bundle/'lib'/name).is_file(): raise ValueError('Missing TLS runtime: '+name)
    report=[]; maxima={key:'0' for key in POLICY['ceilings']}
    for binary in binaries:
        relative=str(binary.relative_to(bundle)); info=inspect(binary)
        librel=os.path.relpath(bundle/'lib',binary.parent)
        rpath='$ORIGIN' + ('/'+librel if librel != '.' else '')
        check_info(info,relative,rpath,ceilings)
        raw=binary.read_bytes()
        for prefix in [b'/home/ray/',b'/home/qt/',b'/work/build/',b'/src/',b'/opt/qt/',b'/tmp/bricksuite-build',*forbidden_prefixes]:
            # An interior component of a relative diagnostic path is not an
            # absolute development prefix (e.g. qtbase/src/widgets).
            if re.search(rb'(?<![A-Za-z0-9_./-])'+re.escape(prefix),raw):
                raise ValueError(f'Developer path {prefix!r} in {relative}')
        for dependency in info['needed']:
            if '/' in dependency: raise ValueError('Absolute dependency: '+dependency)
            if not (bundle/'lib'/dependency).exists() and dependency not in POLICY['system_sonames']:
                raise ValueError(f'Unresolved or undeclared dependency: {relative}: {dependency}')
        if resolve:
            result=subprocess.run(['ldd',str(binary)],text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
            if result.returncode or 'not found' in result.stdout: raise ValueError('Unresolved dependency: '+relative+'\n'+result.stdout)
            for location in re.findall(r'=>\s+(/[^\n]+?)\s+\(0x[0-9a-f]+\)',result.stdout):
                if not (location.startswith(str(bundle)+'/') or location.startswith(('/lib/','/usr/lib/','/lib64/'))):
                    raise ValueError('Developer library resolved: '+location)
        for name,val in info['requires'].items(): maxima[name]=max(maxima[name],val,key=version)
        report.append({'path':relative,'sha256':digest(binary),**info})
    return {'source_sha':expected_sha,'ceilings':maxima,'elf_count':len(report),'elf':report}


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('bundle',type=Path);p.add_argument('--source-sha',required=True);p.add_argument('--output',type=Path)
    p.add_argument('--local',action='store_true',help='Explicitly audit a local-development package against its recorded build-host policy')
    args=p.parse_args();ceilings=None
    if args.local:
        metadata=json.loads((args.bundle/'share/build-metadata.json').read_text())
        if metadata.get('package_kind')!='local-development':raise ValueError('Not a local-development package')
        ceilings=metadata['abi_policy']
    result=audit(args.bundle,args.source_sha,ceilings=ceilings)
    if args.output:args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k!='elf'},indent=2))

if __name__=='__main__':main()
