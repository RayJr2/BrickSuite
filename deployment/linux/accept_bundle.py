#!/usr/bin/env python3
"""Exercise extracted-release probes and write a compact acceptance result."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time



def main_executable(bundle, base):
    output=base/'main-executable';output.mkdir(exist_ok=True)
    env=os.environ.copy()
    for key,name in [('XDG_DATA_HOME','data'),('XDG_CONFIG_HOME','config'),('XDG_CACHE_HOME','cache')]:
        path=output/name;path.mkdir(exist_ok=True);env[key]=str(path)
    with (output/'console.log').open('w') as console:
        process=subprocess.Popen([str(bundle/'BrickSuite')],env=env,stdout=console,stderr=subprocess.STDOUT)
        try:
            started=False
            for _ in range(300):
                logs=list((output/'data').rglob('BrickSuite.log'))
                if any('BrickSuite startup completed' in log.read_text(errors='replace') for log in logs):started=True;break
                if process.poll() is not None:break
                time.sleep(.1)
            if not started or process.poll() is not None:raise RuntimeError('Actual packaged launcher/app startup failed')
            mappings=Path(f'/proc/{process.pid}/maps').read_text()
            images=set()
            for line in mappings.splitlines():
                fields=line.split(maxsplit=5)
                if len(fields)!=6 or not fields[5].startswith('/') or '.so' not in fields[5]:continue
                path=fields[5];images.add(path)
                private=path.startswith(str(bundle)+'/')
                if not private and not path.startswith(('/usr/lib/','/lib/','/lib64/')):raise RuntimeError('Unexpected app runtime library: '+path)
                if any(name in path for name in ['libQt6','libmcut','libssl.so','libcrypto.so']) and not private:raise RuntimeError('Private app library loaded outside bundle: '+path)
            (output/'loaded-images.txt').write_text('\n'.join(sorted(images))+'\n')
            print('actual packaged launcher/startup/loaded-library closure PASS',flush=True)
        finally:
            if process.poll() is None:process.terminate();process.wait(timeout=10)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--bundle',type=Path,default=Path('/mnt/relocated bundle/BrickSuite'))
    p.add_argument('--source-workflows',action='store_true')
    p.add_argument('--native',action='store_true')
    p.add_argument('--prepare',action='store_true')
    args=p.parse_args();base=Path(os.environ['M39_AUDIT_OUTPUT']);base.mkdir(parents=True,exist_ok=True)
    main_executable(args.bundle,base)
    commands=[('core',['PackageProbe']),('queue',['McutQueueWakeupTest'])]
    if args.source_workflows:commands += [('routing-2456-3037',['LDrawPrintPreparationServiceTest','--routing-only',os.environ['BRICKSUITE_TEST_LDRAW']]),('bounded-3021',['LDrawPrintPreparationServiceTest','--3021-only',os.environ['BRICKSUITE_TEST_LDRAW']]),('calibration',['FitCalibrationGenerationServiceTest']),('secure-host',['SecureHostFoundationTest'])]
    if args.native:commands.append(('native',['NativeProbe',*(['--prepare'] if args.prepare else [])]))
    results=[{'check':'actual-main-startup-and-runtime-closure','passed':True,'shutdown':'terminated by harness after startup marker'}]
    for name,command in commands:
        output=base/name;output.mkdir(exist_ok=True);env={**os.environ,'M39_AUDIT_OUTPUT':str(output)}
        with (output/'output.log').open('w') as log:
            result=subprocess.run([str(args.bundle/'bin'/command[0]),*command[1:]],env=env,stdout=log,stderr=subprocess.STDOUT,timeout=900)
        results.append({'check':name,'exit_code':result.returncode});(base/'acceptance.json').write_text(json.dumps(results,indent=2)+'\n')
        print(name,'PASS' if result.returncode==0 else 'FAIL',flush=True)
        if result.returncode:raise RuntimeError('Acceptance failed: '+name+'; inspect '+str(output/'output.log'))
if __name__=='__main__':main()
