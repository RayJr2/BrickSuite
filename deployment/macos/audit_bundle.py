#!/usr/bin/env python3
"""Fail closed on architecture, minimum OS, or runtime dependency leaks."""
import argparse
import json
import re
import subprocess
from pathlib import Path


def output(*args):
    return subprocess.check_output(args, text=True).strip()


def binaries(bundle):
    for path in sorted(bundle.rglob('*')):
        if path.is_file() and not path.is_symlink():
            with path.open('rb') as stream:
                magic = stream.read(4)
            if magic in (b'\xcf\xfa\xed\xfe', b'\xfe\xed\xfa\xcf',
                         b'\xce\xfa\xed\xfe', b'\xfe\xed\xfa\xce',
                         b'\xca\xfe\xba\xbe', b'\xca\xfe\xba\xbf',
                         b'\xbe\xba\xfe\xca', b'\xbf\xba\xfe\xca'):
                yield path


def audit(bundle, arch, minimum):
    bundle = bundle.resolve()
    executable = bundle / 'Contents/MacOS'
    records = []
    errors = []
    for path in bundle.rglob('*'):
        if path.is_symlink() and (not path.exists() or not path.resolve().is_relative_to(bundle)):
            errors.append(f'{path.relative_to(bundle)}: broken or external symlink')
    main = executable / 'BrickSuite'
    main_load = output('otool', '-l', str(main)) if main.is_file() else ''
    main_rpaths = re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset', main_load)
    for path in binaries(bundle):
        name = str(path.relative_to(bundle))
        slices = output('lipo', '-archs', str(path)).split()
        if slices != [arch]:
            errors.append(f'{name}: unexpected architecture {slices}')
        load = output('otool', '-l', str(path))
        mins = re.findall(r'\bminos\s+(\S+)', load)
        mins += re.findall(r'cmd LC_VERSION_MIN_MACOSX\s+cmdsize \d+\s+version (\S+)', load)
        if not mins or any(tuple((list(map(int, m.split('.'))) + [0, 0])[:3]) > tuple((list(map(int, minimum.split('.'))) + [0, 0])[:3]) for m in mins):
            errors.append(f'{name}: unsupported minimum macOS {mins}')
        rpaths = re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset', load)
        def expand(value):
            return Path(value.replace('@loader_path', str(path.parent)).replace('@executable_path', str(executable)))
        for rpath in rpaths:
            if not rpath.startswith(('@loader_path/', '@executable_path/')) or not expand(rpath).resolve().is_relative_to(bundle):
                errors.append(f'{name}: non-relative rpath {rpath}')
        deps = [line.strip().split(' (compatibility')[0] for line in output('otool', '-L', str(path)).splitlines()[1:]]
        for dep in deps:
            if dep.startswith(('/System/Library/', '/usr/lib/')):
                continue
            if dep.startswith('@rpath/'):
                candidates = [expand(p) / dep[7:] for p in rpaths]
                # dyld also searches the main executable's run-path stack.
                candidates.extend(Path(p.replace('@loader_path', str(executable)).replace('@executable_path', str(executable))) / dep[7:]
                                  for p in main_rpaths if p.startswith(('@loader_path/', '@executable_path/')))
            elif dep.startswith(('@loader_path/', '@executable_path/')):
                candidates = [expand(dep)]
            else:
                errors.append(f'{name}: absolute or unsupported dependency {dep}')
                continue
            if not any(p.exists() and p.resolve().is_relative_to(bundle) for p in candidates):
                errors.append(f'{name}: unresolved dependency {dep}')
        records.append(dict(path=name, architectures=slices, minimum_macos=mins, rpaths=rpaths, dependencies=deps))
    if not records:
        errors.append('No Mach-O binaries found')
    return dict(architecture=arch, maximum_minimum_macos=minimum, binaries=records, errors=errors)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('bundle', type=Path)
    parser.add_argument('--arch', required=True, choices=('arm64', 'x86_64'))
    parser.add_argument('--minimum', default='13.0')
    parser.add_argument('--report', type=Path, required=True)
    args = parser.parse_args()
    result = audit(args.bundle, args.arch, args.minimum)
    args.report.write_text(json.dumps(result, indent=2) + '\n')
    print(f"Audited {len(result['binaries'])} Mach-O files; {len(result['errors'])} errors")
    for error in result['errors']:
        print(error)
    raise SystemExit(bool(result['errors']))
