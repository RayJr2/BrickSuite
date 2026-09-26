import argparse
import json
from pathlib import Path
from run_proof import run, executable

parser=argparse.ArgumentParser()
parser.add_argument('--bin',type=Path,required=True)
parser.add_argument('--out',type=Path,required=True)
args=parser.parse_args()
results={}
pairs=[(f'23422-{mode}','23422-source') for mode in ['candidate','clean','refine','fill','fill-clean','local','bambu-normalized']]
pairs += [(f'23422-{mode}','23422-bambu-normalized') for mode in ['fill','fill-clean','local']]
for a,b in pairs:
    key=a+'__'+b
    results[key]=run([executable(args.bin,'cgal_distance'),args.out/(a+'.off'),args.out/(b+'.off')],args.out/(key+'.log'))
    print(key,results[key],flush=True)
    (args.out/'bounded-distances.json').write_text(json.dumps(results,indent=2))
