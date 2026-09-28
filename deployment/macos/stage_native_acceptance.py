#!/usr/bin/env python3
"""Create a separate GUI acceptance copy with INI preferences (never a release)."""
import argparse
import plistlib
import re
import shutil
import subprocess
from pathlib import Path
from audit_bundle import audit

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('bundle',type=Path)
parser.add_argument('wrapper',type=Path)
parser.add_argument('output',type=Path)
args=parser.parse_args()
bundle=args.output.resolve()
if bundle.exists(): raise SystemExit('Output already exists')
shutil.copytree(args.bundle,bundle,symlinks=True)
exe=bundle/'Contents/MacOS/BrickSuite'
shutil.copy2(args.wrapper,exe)
def run(*cmd): subprocess.run([str(x) for x in cmd],check=True)
load=subprocess.check_output(['otool','-l',str(exe)],text=True)
for path in re.findall(r'cmd LC_RPATH\s+cmdsize \d+\s+path (.*?) \(offset',load):
    run('install_name_tool','-delete_rpath',path,exe)
run('install_name_tool','-add_rpath','@executable_path/../Frameworks',exe)
for line in subprocess.check_output(['otool','-L',str(exe)],text=True).splitlines()[1:]:
    dep=line.strip().split(' (compatibility')[0]
    if dep.startswith(('/System/Library/','/usr/lib/')): continue
    parts=dep.split('/')
    target='/'.join(parts[next(i for i,p in enumerate(parts) if p.endswith('.framework')):]) if '.framework/' in dep else Path(dep).name
    run('install_name_tool','-change',dep,'@rpath/'+target,exe)
p=bundle/'Contents/Info.plist'
info=plistlib.loads(p.read_bytes())
info['CFBundleIdentifier']='com.rfstateside.bricksuite.m392.nativeacceptance'
info['CFBundleName']='BrickSuite Native Acceptance'
p.write_bytes(plistlib.dumps(info))
result=audit(bundle,'arm64','13.0')
if result['errors']: raise SystemExit('\n'.join(result['errors']))
run('codesign','--force','--sign','-',exe)
run('codesign','--force','--sign','-',bundle)
run('codesign','--verify','--deep','--strict',bundle)
