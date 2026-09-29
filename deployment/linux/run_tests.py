#!/usr/bin/env python3
"""Run serial full and focused CTest gates and reject skipped registrations."""
import argparse
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--build',type=Path,default=Path('/work/build'));p.add_argument('--output',type=Path,default=Path('/work/evidence/tests'));args=p.parse_args()
    args.output.mkdir(parents=True,exist_ok=True)
    configured=json.loads((args.build/'configured-tests.json').read_text())
    focused='McutMeshBoolean|McutQueueWakeup|LocalPrintableOverrideService|CredentialStoreLinux|PrintCompositionRouting|PrintPreparation3021|Host|Remote|PairingFoundation'
    reports={}
    for name,extra in [('full',[]),('focused',['-R',focused])]:
        xml=args.output/(name+'.xml')
        with (args.output/(name+'.log')).open('w') as log:
            result=subprocess.run(['ctest','--test-dir',str(args.build),'-j','1','--output-on-failure','--output-junit',str(xml),*extra],stdout=log,stderr=subprocess.STDOUT)
        if not xml.exists():raise RuntimeError('CTest did not produce report')
        cases=ET.parse(xml).getroot().findall('.//testcase')
        bad=[c.attrib['name'] for c in cases if c.find('failure') is not None or c.find('skipped') is not None or c.find('error') is not None or c.attrib.get('status','run')!='run']
        if result.returncode or bad:raise RuntimeError(name+' failed/skipped: '+str(bad))
        if name=='full' and {c.attrib['name'] for c in cases}!=set(configured['tests']):raise RuntimeError('Registration/execution mismatch')
        reports[name]={'passed':len(cases),'failed':0,'skipped':0}
        print(name,reports[name],flush=True)
    (args.output/'summary.json').write_text(json.dumps(reports,indent=2)+'\n')
if __name__=='__main__':main()
