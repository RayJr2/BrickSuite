#!/usr/bin/env python3
"""Configure and explicitly build all registered Linux test executables in Jammy."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess


def run(args, **kw):
    subprocess.run([str(x) for x in args], check=True, **kw)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, default=Path('/src'))
    p.add_argument('--build', type=Path, default=Path('/work/build'))
    p.add_argument('--qt', type=Path, default=Path('/opt/qt'))
    p.add_argument('--openssl', type=Path, default=Path('/work/openssl-prefix/usr'))
    p.add_argument('--ldraw', type=Path, default=Path('/work/ldraw'))
    p.add_argument('--jobs', default='6')
    args = p.parse_args()
    if 'VERSION_ID="22.04"' not in Path('/etc/os-release').read_text():
        raise SystemExit('Release builds require Ubuntu 22.04')
    if not subprocess.check_output(['g++-11', '-dumpversion'], text=True).startswith('11'):
        raise SystemExit('GCC 11 is required')
    run(['git', '-C', args.source, 'diff', '--exit-code', 'HEAD', '--', 'src', 'resources', 'cmake', 'CMakeLists.txt'])
    sha = subprocess.check_output(['git', '-C', str(args.source), 'rev-parse', 'HEAD'], text=True).strip()
    args.build.mkdir(parents=True, exist_ok=True)
    (args.build / 'source-record.json').write_text(json.dumps({'source_sha': sha, 'schema': 35, 'protocol': '1.5'}, indent=2)+'\n')
    flags = '-ffile-prefix-map=' + str(args.source) + '=. -ffile-prefix-map=/work=. -ffile-prefix-map=' + str(args.qt) + '=.'
    run(['cmake', '-S', args.source, '-B', args.build, '-G', 'Ninja',
         '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=ON',
         '-DCMAKE_PROJECT_INCLUDE='+str(args.source/'deployment/linux/BaselineTests.cmake'),
         '-DCMAKE_C_COMPILER=gcc-11', '-DCMAKE_CXX_COMPILER=g++-11',
         '-DCMAKE_C_FLAGS='+flags, '-DCMAKE_CXX_FLAGS='+flags,
         '-DCMAKE_PREFIX_PATH='+str(args.qt), '-DOPENSSL_ROOT_DIR='+str(args.openssl),
         '-DBRICKSUITE_CALIBRATION_LDRAW_ROOT='+str(args.ldraw)])
    records = re.findall(r'add_test\((\S+)\s+"([^"]+)"', (args.build/'CTestTestfile.cmake').read_text())
    required = {'PrintCompositionRouting', 'PrintPreparation3021'}
    required.update('FitCalibrationSource'+family for family in ['BallSocket','PinBarrelHinge','InterleavedFingerHinge','ClickHinge','RetainedRotatingWheel','PlainRoundBoreWheel'])
    if not required.issubset({n for n, _ in records}):
        raise SystemExit('Missing installed-LDraw test registrations')
    targets = sorted({Path(exe).name for _, exe in records})
    (args.build/'configured-tests.json').write_text(json.dumps({'count':len(records),'targets':targets,'tests':[n for n,_ in records]},indent=2)+'\n')
    print(f'Building {len(records)} configured tests / {len(targets)} test executables', flush=True)
    run(['cmake','--build',args.build,'--parallel',args.jobs,'--target','BrickSuite','BrickSuiteMeshBooleanWorker',*targets])

if __name__ == '__main__':
    main()
